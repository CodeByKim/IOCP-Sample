#pragma comment(lib, "ws2_32.lib")

#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <thread>
#include <chrono>
#include <vector>
#include <unordered_map>

#include "RingBuffer.h"

#define SERVER_PORT 20000

class Connection;

void Initialize();
void Listen();
void Accept();
void WorkerThread();
void Receive(Connection* conn);
void Send(Connection* conn, char* data, int size);

enum class eIoOperationType
{
	Receive,
	Send
};

class AsyncIoInfo
{
public:
	AsyncIoInfo(eIoOperationType type)
		: mIoType(type)
	{
		ZeroMemory(&mOverlapped, sizeof(OVERLAPPED));		
	}

	OVERLAPPED mOverlapped;
	WSABUF mWsaBuf;

	eIoOperationType mIoType;
};

class Connection
{
public:
	Connection(SOCKET socket, SOCKADDR_IN addr)
		: mSocket(socket)
		, mAddr(addr)
		, mRecvBuffer(512)
		, mSendBuffer(512)
		, mRecvInfo(eIoOperationType::Receive)
		, mSendInfo(eIoOperationType::Send)		
		, mIsSending(false)
		, mIoCount(0)
	{						
	}

	~Connection()
	{
		closesocket(mSocket);
	}

	SOCKET mSocket;
	SOCKADDR_IN mAddr;

	RingBuffer mRecvBuffer;
	RingBuffer mSendBuffer;	

	AsyncIoInfo mRecvInfo;
	AsyncIoInfo mSendInfo;
	
	bool mIsSending;
	int mIoCount;
};

SOCKET gListenSocket;
HANDLE gIocp;

std::thread gAcceptThread;
std::vector<std::thread> gWorkerThreads;

void Initialize()
{
	WSAData data;
	if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
	{
		std::cout << "WSAStartup Error" << std::endl;
		exit(-1);
	}		

	gListenSocket = socket(AF_INET, 
						   SOCK_STREAM, 
						   IPPROTO_TCP);

	SYSTEM_INFO info;
	GetSystemInfo(&info);
	int workerThreadCount = info.dwNumberOfProcessors;

	gIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
										 NULL, 
										 NULL, 
										 workerThreadCount);

	for (int i = 0; i < workerThreadCount; i++)
		gWorkerThreads.push_back(std::thread(WorkerThread));
}

void Listen()
{
	SOCKADDR_IN addr;
	addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);

	bind(gListenSocket, 
		 (SOCKADDR*)&addr, 
		 sizeof(SOCKADDR));

	listen(gListenSocket, 
		   SOMAXCONN);

	gAcceptThread = std::thread(Accept);
}

void Accept()
{
	while (true)
	{
		SOCKADDR_IN clientAddr;
		int len = sizeof(SOCKADDR_IN);
		
		SOCKET clientSocket = accept(gListenSocket, 
									 (SOCKADDR*)&clientAddr, 
									 &len);

		/*
		 * 종료 처리할 때 리슨소켓을 close
		 * accept는 INVALID_SOCKET을 리턴하면서 블럭 빠져나옴
		 */
		if (clientSocket == INVALID_SOCKET)
		{
			break;
		}

		/*
		 * 전송 버퍼 사이즈 0으로, 비동기 처리 위해
		 */
		int sendbufferSize = 0;
		setsockopt(clientSocket, 
				   SOL_SOCKET, 
				   SO_SNDBUF, 
				   (char*)&sendbufferSize, 
				   sizeof(int));

		Connection* conn = new Connection(clientSocket,
										  clientAddr);
		
		CreateIoCompletionPort((HANDLE)conn->mSocket,
							    gIocp, 
								(ULONG_PTR)conn, 
								NULL);
		
		Receive(conn);
	}
}

void WorkerThread()
{
	while (true)
	{
		Connection* conn = nullptr;
		AsyncIoInfo* ioInfo = nullptr;
		DWORD bytesTransferred = 0;

		BOOL result = GetQueuedCompletionStatus(gIocp, 
												&bytesTransferred, 
												(PULONG_PTR)&conn, 
												(OVERLAPPED**)&ioInfo, 
												INFINITE);

		if (!result)
		{
			if (ioInfo == nullptr)
			{
				std::cout << "ERROR : " << WSAGetLastError() << std::endl;
				break;
			}
			else
			{
				int error = WSAGetLastError();

				/*
				 * ERROR_NETNAME_DELETED은 클라의 강제 종료
				 * 강제 종료는 에러로 보지 않음
				 */
				if (error != ERROR_NETNAME_DELETED)
				{
					std::cout << "ERROR : " << WSAGetLastError() << std::endl;
				}
				
			}
		}		

		/*
		 * PostQueuedCompletionStatus로 key를 NULL로 넣음
		 * 종료 처리
		 */
		if (conn == nullptr)
		{			
			break;
		}

		/*
		 * 바로 끊지말고 IoCount가 0이 되는 Connection만 제거		 
		 * 끊긴 Connection이라도 이미 작업중인 IO가 있을 수 있음
		 */
		if (bytesTransferred == 0)
		{						
			/*
			 * bytesTransferred 0은 상대방의 정상 종료			 
			 * Recv로 올려놓은 IoCount 감소
			 */
			conn->mIoCount -= 1;

			/*
			 * 서버가 걸어놓은 비동기 Send가 없을 때만 Connection 제거			 
			 */
			if (conn->mIoCount <= 0 && !conn->mIsSending)
			{
				delete conn;
				std::cout << "Disconnect Client" << std::endl;

				continue;
			}
		}

		if (ioInfo->mIoType == eIoOperationType::Receive)
		{								
			/* 
			 * 실제로 얻은 데이터를 가져오려면 직접 Rear를 뒤로 옮겨줘야함
			 * IOCP가 Enqueue를 통해 넣은게 아니기 때문
			 */
			conn->mRecvBuffer.MoveRear(bytesTransferred);

			/*
			 * 받은 데이터만큼 Dequeue			 
			 */
			char message[256];
			conn->mRecvBuffer.Dequeue(message, 
									  bytesTransferred);
			std::cout << "recv data : " << message << std::endl;
			
			/*
			 * 클라로 받은 데이터 그대로 전송하고
			 */
			Send(conn,
				message,
				bytesTransferred);

			/*
			 * 다시 리시브 시작
			 */
			Receive(conn);

			conn->mIoCount -= 1;
		}
		else if(ioInfo->mIoType == eIoOperationType::Send)
		{			
			/*
			 * 데이터가 잘 전송되었다면 front를 뒤로 옮겨서 데이터 삭제 처리
			 */
			conn->mSendBuffer.MoveFront(bytesTransferred);
			conn->mIoCount -= 1;
			conn->mIsSending = false;

			std::cout << "sent data..." << std::endl;
		}
	}
}

void Receive(Connection* conn)
{	
	/*
	 * 데이터를 수신하기 위해 Recv링버퍼 바로 꽃음
	 */
	conn->mRecvInfo.mWsaBuf.buf = conn->mRecvBuffer.GetBufferRear();
	conn->mRecvInfo.mWsaBuf.len = conn->mRecvBuffer.GetDirectEnqueueSize();
	ZeroMemory(&conn->mRecvInfo.mOverlapped, 
			   sizeof(OVERLAPPED));

	/*
	 * WSARecv할 때 인자 제대로 다 안넣어주면 에러남
	 * flag를 안쓰더라도 넣어줘야 함
	 */
	DWORD flag = 0;
	int result = WSARecv(conn->mSocket,
		&conn->mRecvInfo.mWsaBuf,
		1,
		NULL,
		&flag,
		(OVERLAPPED*)&conn->mRecvInfo,
		NULL);

	/*
	 * error가 IO_PENDING이 아니면 끊어줌
	 * 단 여기서 무조건 close 하면 안됨
	 */
	if (result == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			std::cout << "ERROR : " << error << std::endl;		
			return;
		}		
	}

	conn->mIoCount += 1;
}

void Send(Connection* conn, char* data, int size)
{
	/*
	 * 전달할 데이터를 Send링버퍼에 Enqueue해서 클라로 보낼 준비
	 */
	conn->mSendBuffer.Enqueue(data, size);
	conn->mSendInfo.mWsaBuf.buf = conn->mSendBuffer.GetBufferFront();
	conn->mSendInfo.mWsaBuf.len = size;
	ZeroMemory(&conn->mSendInfo.mOverlapped, 
			   sizeof(OVERLAPPED));

	int result = WSASend(conn->mSocket,
		&conn->mSendInfo.mWsaBuf,
		1,
		NULL,
		0,
		(OVERLAPPED*)&conn->mSendInfo,
		NULL);

	/*
	 * error가 IO_PENDING이 아니면 끊어줌
	 * 단 여기서 무조건 close하면 안됨
	 */
	if (result == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			std::cout << "ERROR : " << error << std::endl;		
			return;
		}
	}

	conn->mIsSending = true;
	conn->mIoCount += 1;
}

void NotifyQuit()
{
	closesocket(gListenSocket);

	for (int i = 0; i < gWorkerThreads.size(); i++)
	{
		PostQueuedCompletionStatus(gIocp, 
								   0, 
								   NULL, 
								   NULL);
	}
		
}

void Release()
{
	gAcceptThread.join();

	for (int i = 0; i < gWorkerThreads.size(); i++)
		gWorkerThreads[i].join();

	CloseHandle(gIocp);

	std::cout << "end of program..." << std::endl;
	WSACleanup();
}

int main()
{
	Initialize();	
	Listen();

	while (true)
	{
		char ch = getchar();
		
		if (ch == 'q')
		{			
			NotifyQuit();
			break;
		}		
	}
	
	Release();	
}
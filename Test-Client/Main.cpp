#pragma comment(lib, "ws2_32.lib")


#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define SERVER_PORT 20000

#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <thread>
#include <chrono>

int main()
{
	WSAData data;
	WSAStartup(MAKEWORD(2, 2), &data);

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	SOCKADDR_IN addr;
	addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
	
	connect(sock, (SOCKADDR*)&addr, sizeof(SOCKADDR));

	char message[256] = "hello world";
	send(sock, message, strlen(message) + 1, 0);

	char recvMessage[256];
	recv(sock, recvMessage, 256, 0);

	std::cout << "from server : " << recvMessage << std::endl;
	
	getchar();
	closesocket(sock);
	WSACleanup();
}
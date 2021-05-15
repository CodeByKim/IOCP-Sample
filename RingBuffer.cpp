#include "RingBuffer.h"

RingBuffer::RingBuffer(int size)
	: mBuffer(nullptr)
	, mBufferFront(nullptr)
	, mBufferRear(nullptr)
	, mBufferEnd(nullptr)
	, mCapacity(size)
	, mSize(0)
{
	mBuffer = new char[mCapacity];
	mBufferFront = mBuffer;
	mBufferRear = mBuffer;
	mBufferEnd = mBuffer + mCapacity - 1;
}

RingBuffer::~RingBuffer()
{

}

bool RingBuffer::Enqueue(char* data, int size)
{
	if (size > GetRemainQueueSize())
	{
		return false;
	}

	// 넣으려는 사이즈가 버퍼의 남은 공간보다 작을 때
	// 이때는 데이터가 한번에 들어간다.
	if (size <= (mBufferEnd - mBufferRear - 1))
	{
		CopyMemory(mBufferRear, data, size);
		mBufferRear += size;
	}
	else
	{
		__int64 tempSize = mBufferEnd - mBufferRear;
		CopyMemory(mBufferRear, data, tempSize);
		mBufferRear = mBuffer;

		CopyMemory(mBufferRear, data + tempSize, size - tempSize);
		mBufferRear += (size - tempSize);
	}

	mSize += size;
	return true;
}

bool RingBuffer::Dequeue(char* outData, int size)
{
	if (size > mSize)
	{
		return false;
	}

	//데이터를 한번에 가져올 수 있는경우
	//일단 End앞에 데이터가 다 있으면 된다. 그럼 한번에 가져올 수 있음
	if (size <= (mBufferEnd - mBufferFront - 1))
	{
		CopyMemory(outData, mBufferFront, size);
		mBufferFront += size;
	}
	else
	{
		//가져올 데이터가 애매하게 end와 front에 걸친경우
		//우선 end까지 읽고
		__int64 tempSize = mBufferEnd - mBufferFront;
		CopyMemory(outData, mBufferFront, tempSize);
		mBufferFront = mBuffer;

		CopyMemory(outData + tempSize, mBufferFront, size - tempSize);
		mBufferFront += (size - tempSize);
	}

	mSize -= size;
	return true;
}

bool RingBuffer::Peek(char* outData, int size)
{
	if (size > mSize)
	{
		return false;
	}

	//데이터를 한번에 가져올 수 있는경우
	//일단 End앞에 데이터가 다 있으면 된다. 그럼 한번에 가져올 수 있음
	if (size <= (mBufferEnd - mBufferFront - 1))
	{
		CopyMemory(outData, mBufferFront, size);
	}
	else
	{
		//가져올 데이터가 애매하게 end와 front에 걸친경우
		//우선 end까지 읽고
		__int64 tempSize = mBufferEnd - mBufferFront;
		CopyMemory(outData, mBufferFront, tempSize);
		CopyMemory(outData + tempSize, mBuffer, size - tempSize);
	}

	return true;
}

void RingBuffer::MoveFront(int size)
{
	//이거 rear에서 front를 빼야하는거 아니야?
	if (size <= (mBufferEnd - mBufferFront - 1))
	{
		mBufferFront += size;
	}
	else
	{
		__int64 tempSize = mBufferEnd - mBufferFront;

		mBufferFront = mBuffer;
		mBufferFront += (size - tempSize);
	}

	mSize -= size;
}

void RingBuffer::MoveRear(int size)
{
	// 넣으려는 사이즈가 버퍼의 남은 공간보다 작을 때
	// 이때는 데이터가 한번에 들어간다.
	if (size <= (mBufferEnd - mBufferRear - 1))
	{
		mBufferRear += size;
	}
	else
	{
		__int64 tempSize = mBufferEnd - mBufferRear;
		mBufferRear = mBuffer;
		mBufferRear += (size - tempSize);
	}

	mSize += size;
}

bool RingBuffer::IsEmpty()
{
	return mBufferFront == mBufferRear;
}

int RingBuffer::GetRemainQueueSize()
{
	return mCapacity - 1 - mSize;
}

int RingBuffer::GetUseSize()
{
	return mSize;
}

char* RingBuffer::GetBufferFront()
{
	return mBufferFront;
}

char* RingBuffer::GetBufferRear()
{
	return mBufferRear;
}

int RingBuffer::GetDirectEnqueueSize()
{
	//한번에 쓸 수 있는 사이즈 (Enqueue 로직에서 가져옴)
	//그럼 얘도.. rear가 front보다 앞에 있는 상황이 있다는거 아닌가?
	
	if (mBufferFront <= mBufferRear)
	{		
		//일반적인 케이스
		return (int)(mBufferEnd - mBufferRear - 1);
	}
	else
		return (int)(mBufferFront - mBufferRear - 1);
}

int RingBuffer::GetDirectDequeueSize()
{
	//front가 rear보다 크다는건 rear가 front보다 앞에 있다는 의미
	if (mBufferFront >= mBufferRear)
	{		
		return (int)(mBufferEnd - mBufferFront - 1);
	}
	else
		return (int)(mBufferRear - mBufferFront);
}

void RingBuffer::Clear()
{
	mBufferFront = mBuffer;
	mBufferRear = mBuffer;
	mBufferEnd = mBuffer + mCapacity - 1;

	mSize = 0;
}

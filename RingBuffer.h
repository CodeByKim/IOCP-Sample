#pragma once
#include <Windows.h>

class RingBuffer
{
public:
	RingBuffer(int size);
	~RingBuffer();

	bool Enqueue(char* data, int size);
	bool Dequeue(char* outData, int size);
	bool Peek(char* outData, int size);
	void MoveFront(int size);
	void MoveRear(int size);
	bool IsEmpty();
	int GetRemainQueueSize();
	int GetUseSize();
	char* GetBufferFront();
	char* GetBufferRear();
	int GetDirectEnqueueSize();
	int GetDirectDequeueSize();
	void Clear();

private:
	char* mBuffer;
	char* mBufferFront;
	char* mBufferRear;
	char* mBufferEnd;
	int mSize;
	int mCapacity;
};


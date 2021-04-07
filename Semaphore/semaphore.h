#pragma once
#include "TCB.h"

typedef struct _semaphore {
	int lock;	//互斥锁
	int value;	//信号量值
	PLIST_ENTRY queue;	//等待队列
} Semaphore ;

void initSemaphore(Semaphore *sem, int initValue);
void getSemaphore(Semaphore *sem, TCB *tcb);
void releaseSemaphore(Semaphore *sem);
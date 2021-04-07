#pragma once
#include "TCB.h"

typedef struct _semaphore {
	int lock;	//������
	int value;	//�ź���ֵ
	PLIST_ENTRY queue;	//�ȴ�����
} Semaphore ;

void initSemaphore(Semaphore *sem, int initValue);
void getSemaphore(Semaphore *sem, TCB *tcb);
void releaseSemaphore(Semaphore *sem);
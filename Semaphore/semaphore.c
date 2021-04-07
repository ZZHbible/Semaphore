#include <stdio.h>
#include "UserLock.h"
#include "semaphore.h"

void initSemaphore(Semaphore *sem, int initValue)
{
	sem->lock = 0;
	sem->value = initValue;
	sem->queue = (PLIST_ENTRY)malloc(sizeof(LIST_ENTRY));
	InitializeListHead(sem->queue);
}
void getSemaphore(Semaphore *sem, TCB *tcb)
{
	/*�ڴ˴�ʵ�ֻ�ȡ�ź����Ĵ����߼�*/
	//��ȡ������
	while (!getLock(&sem->lock));
	//value��һ
	sem->value--;
	//���valueС��0
	if (sem->value < 0) 
	{	
		//����ǰ�̲߳��뵽queue��
		InsertTailList(sem->queue, &currentTCB->List);
		//InsertTailList(sem->queue, &tcb->List);
		//�ͷŻ�����
		sem->lock = 0;	
		//��ȡ������
		while (!getLock(&g_ScheduleLock));
		//����ǰ�߳�״̬��Ϊ����
		currentTCB->State = BLOCKED;
		//�Ƴٵ��������ͷ�
		((TCB*)tcb)->Lock[((TCB*)tcb)->postLockCount] = &g_ScheduleLock;
		((TCB*)tcb)->postLockCount++;
		//����
		schedule();
	}
	else
	{	//����
		//�ͷŻ�����
		sem->lock = 0;
	}
}

void releaseSemaphore(Semaphore *sem)
{
	/*�ڴ˴�ʵ���ͷ��ź����Ĵ����߼�*/

	//��ȡ������
	while (!getLock(&sem->lock));
	//value��һ
	sem->value++;
	//���valueС�ڵ���0
	if (sem->value <= 0)
	{	
		//��ȡ������
		while (!getLock(&g_ScheduleLock));
		//ȡqueue�ײ���TCB���޸���״̬λ�����������뵽��������
		PTCB tcb = (PTCB)RemoveHeadList(sem->queue);
		tcb->State = READY;
		InsertTailList(CurrentArrayList[0], &tcb->List);
		//�ͷŵ�����
		g_ScheduleLock = 0;
	}
	//�ͷŻ�����
	sem->lock = 0;

}
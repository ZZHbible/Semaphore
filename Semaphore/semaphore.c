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
	/*在此处实现获取信号量的处理逻辑*/
	//获取互斥锁
	while (!getLock(&sem->lock));
	//value减一
	sem->value--;
	//如果value小于0
	if (sem->value < 0) 
	{	
		//将当前线程插入到queue中
		InsertTailList(sem->queue, &currentTCB->List);
		//InsertTailList(sem->queue, &tcb->List);
		//释放互斥锁
		sem->lock = 0;	
		//获取调度锁
		while (!getLock(&g_ScheduleLock));
		//将当前线程状态改为阻塞
		currentTCB->State = BLOCKED;
		//推迟调度锁的释放
		((TCB*)tcb)->Lock[((TCB*)tcb)->postLockCount] = &g_ScheduleLock;
		((TCB*)tcb)->postLockCount++;
		//调度
		schedule();
	}
	else
	{	//否则
		//释放互斥锁
		sem->lock = 0;
	}
}

void releaseSemaphore(Semaphore *sem)
{
	/*在此处实现释放信号量的处理逻辑*/

	//获取互斥锁
	while (!getLock(&sem->lock));
	//value加一
	sem->value++;
	//如果value小于等于0
	if (sem->value <= 0)
	{	
		//获取调度锁
		while (!getLock(&g_ScheduleLock));
		//取queue首部的TCB，修改其状态位就绪，并加入到就绪队列
		PTCB tcb = (PTCB)RemoveHeadList(sem->queue);
		tcb->State = READY;
		InsertTailList(CurrentArrayList[0], &tcb->List);
		//释放调度锁
		g_ScheduleLock = 0;
	}
	//释放互斥锁
	sem->lock = 0;

}
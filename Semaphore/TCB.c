

#include "TCB.h"
#include <stdarg.h>
#include <stdio.h>
#include <strsafe.h>
#include "UserLock.h"


extern void threadFunc(DUMMYARGS void *p);

void ErrorExit(LPTSTR lpszFunction)
{
	// Retrieve the system error message for the last-error code

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	// Display the error message and exit the process

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"),
		lpszFunction, dw, lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
}


/* fast/assembly/non-portable version of the return function */
#define make_return_func_fast(NUM_ARGS)                                          \
static __declspec(noinline) __declspec(naked) void return_func##NUM_ARGS()		 \
{                                                                                \
    /* we're returning here from "threadFunc" (see input param to makeContext) */ \
    static size_t tcb = 0;														 \
																				 \
    /* clean up the input arguments to func now, first POP is the pointer to TCB */ \
	_asm { _asm pop eax }														 \
    _asm { _asm mov [tcb], eax }				                                 \
	_asm { _asm pop eax }														 \
													\
	InsertTailList(DeadList, &((TCB *)tcb)->List);								 \
	((TCB *)tcb)->State = DEAD;													 \
	/* current thread finished, we transfer CPU to idle thread */				 \
	/*setContext(idleTCB);		*/												 \
	while (!getLock(&g_ScheduleLock));												\
	((TCB *)tcb)->Lock[((TCB *)tcb)->postLockCount] = &g_ScheduleLock;				\
	((TCB *)tcb)->postLockCount++;													\
	schedule();																	\
}
make_return_func_fast(0);

void initTHREADList(LIST_ENTRY *L)
{
	InitializeListHead(L);
}

void initTCB(TCB *tcb, int ID, int priority, int duration)
{
	tcb->TID = ID;
	tcb->Duration = duration;
	tcb->Priority = priority;
	tcb->TCB_stack.ss_size = 8192*10;//80KB for each thread
	tcb->TCB_stack.ss_sp = tcb->stack;

	tcb->TCB_stack.ss_flags = 0;
	tcb->In_IO = FALSE;
	tcb->State = READY;
	tcb->postLockCount = 0;
}

void freeTCB(TCB *tcb)
{
	if (tcb)
	{
		tcb->TCB_stack.ss_sp = NULL;
		free(tcb);
		tcb = NULL;
	}
}

//运行时动态创建新的线程
TCB * duplicateTCB()
{
	TCB *tcb = (TCB*)malloc(sizeof(TCB));
	initTCB(tcb, g_NextID++, 0, -1);

	tcb->TCB_context = newTCB->TCB_context;

	makeContext(tcb, threadFunc, 1, tcb);

	return tcb;
}

void freeDeadTCB(PLIST_ENTRY deadList)
{
	while (!IsListEmpty(deadList))
	{
		PTCB tcb = (PTCB)RemoveHeadList(deadList);
		freeTCB(tcb);
	}
}

//设置时间配额
void setQuantumn(TCB *tcb, int quantumn)
{
	tcb->Quantumn = quantumn;
	tcb->Quantumn_left = quantumn;
}


int getContext(TCB *tcb)
{
	int ret;

	/* Retrieve the full machine context */
	tcb->TCB_context.ContextFlags = CONTEXT_FULL;
	ret = GetThreadContext(g_hHandle, &tcb->TCB_context);

	return (ret == 0) ? -1: 0;
}


int setContext(const TCB *tcb)
{
	int ret;
	
	// Restore the full machine context (already set) //
	ret = SetThreadContext(g_hHandle, &tcb->TCB_context);
	return (ret == 0) ? -1: 0;
}

void myPrintf(char *str, int TID)
{
	while (!getLock(&g_PrintLock));
	printf(str, TID);
	releaseLock(&g_PrintLock);
}

__declspec(naked) void printfInRunningThread()
{
	__asm {
		push eax
		push ebx
		mov eax, dword ptr[esp + 12]	//thread id
		mov ebx, dword ptr[esp + 16]	//string address
		push eax
		push ebx
		call printf					//printf("Thread ID %d is Running!\n", TID);
		add esp, 8					// caller clearup the stack

		pop ebx
		pop eax
		ret	8						// here 16, to popup the string and thread id parameter
	}
}

void postPrintf(TCB *tcb)
{
	static char str[] = "Thread ID %d is Running!\n";

	char *sp = (char *)tcb->TCB_context.Esp;
	sp -= 8;						//for 2 parameters 
	sp -= sizeof(size_t);			//return address

	tcb->TCB_context.Esp = (DWORD)sp;		//new stack top

	*(size_t*)sp = tcb->TCB_context.Eip;	//return address
	sp += 4;
	*(size_t*)sp = (size_t)tcb->TID;	//thread ID
	sp += 4;
	*(size_t*)sp = (size_t)str;			//output string

	tcb->TCB_context.Eip = (DWORD)printfInRunningThread;	//print info in the running thread
}

void postReleaseLock(TCB *waittcb, TCB *tcb)
{
	for (int i = 0; i < waittcb->postLockCount; i++)
	{
		char *sp = (char *)tcb->TCB_context.Esp;
		sp -= 8;						//parameter
		sp -= sizeof(size_t);			//return address

		tcb->TCB_context.Esp = (DWORD)sp;		//new stack top

		*(size_t*)sp = tcb->TCB_context.Eip;	//return address
		sp += 4;
		*(size_t*)sp = (size_t)waittcb->Lock[i];	//parameter

		tcb->TCB_context.Eip = (DWORD)releaseLockFromOtherThread;	//release the Lock here
	}
	waittcb->postLockCount = 0;
}


//构建线程执行上下文
int makeContext(TCB *tcb, void (*func)(), int argc, ...)
{
	int i;
    va_list ap;
	char *sp;

	/* Stack grows down */
	sp = (char *) (size_t)tcb->TCB_stack.ss_sp + tcb->TCB_stack.ss_size;

	/* Reserve stack space for the arguments (maximum possible: argc*(8 bytes per argument)) */
	sp -= argc * 8;

	/* Reserve stack space for the return function address (called after func returns) */
	sp -= sizeof(size_t);

	if ( sp < (char *)tcb->TCB_stack.ss_sp) {
		/* errno = ENOMEM;*/
		return -1;
	}

	/* Set the instruction and the stack pointer */
#if defined(_X86_)
	tcb->TCB_context.Eip = (unsigned long long) func;
	tcb->TCB_context.Esp = (unsigned long long) sp;
#else
	tcb->TCB_mcontext.Rip = (unsigned long long) func;
	tcb->TCB_mcontext.Rsp = (unsigned long long) (sp - 40);
#endif
	/* Save/Restore the full machine context */
	tcb->TCB_context.ContextFlags = CONTEXT_FULL;

	/* Copy the return func that will handle context switch when this thread finish */
	*(size_t*)sp = (size_t)return_func0;
	sp += sizeof(size_t);

	/* Copy the arguments */
	va_start(ap, argc);
	for (i = 0; i<argc; i++) {
		memcpy(sp, ap, 8);
		ap += 8;
		sp += 8;
	}
	va_end(ap);

	return 0;
}

//线程上下文切换
int swapContext(TCB *otcb, const TCB *tcb)
{
	int ret;

	if ((otcb == NULL) || (tcb == NULL)) {
		/*errno = EINVAL;*/
		return -1;
	}

	ret = getContext(otcb);
	if (ret == 0) {
		ret = setContext(tcb);
	}
	return ret;
}

//更新当前执行线程的剩余执行时间和当前执行轮可用剩余CPU时间配额
int updateQuantumn(PTCB tcb)
{
	tcb->Quantumn_left -= g_basicQuantumn;
	if (tcb->Duration == -1)
	{
		return tcb->Quantumn_left > 0 ? 1 : 0;
	}
	else
	{
		tcb->Duration -= g_basicQuantumn;
		if (tcb->Duration > 0)
		{
			return tcb->Quantumn_left > 0 ? 1 : 0;
		}
		else
		{
			tcb->Duration = 0;
			return 1;//最后一次调度，使得线程正常退出
		}
	}
}

//更新当前执行线程的剩余执行时间和当前执行轮可用剩余CPU时间配额
//与updateQuantumn不同，线程到时会自动退出，不需要计算Duration和最后一次调度
int updateQuantumn1(PTCB tcb)
{
	tcb->Quantumn_left -= g_basicQuantumn;
	return tcb->Quantumn_left > 0 ? 1 : 0;
}

//新一轮调度，重置时间配额
void resetQuantumn(PLIST_ENTRY *list)
{
	for (int i = 0; i < NLists; i++)
	{
		if (!IsListEmpty(CurrentArrayList[i]))
		{
			PLIST_ENTRY l = CurrentArrayList[i]->Flink;
			while (l != CurrentArrayList[i])
			{
				PTCB tcb = (PTCB)l;
				tcb->Quantumn_left = tcb->Quantumn;
				l = l->Flink;
			}
		}
	}
}

/*在schedule中实现具体的调度算法*/
/* 参数bySelf，指明是否线程主动切换*/
void schedule()
{
	/////////////////////////////////////////////////////////////////////////////////////
	//为了演示，此处只实现了针对一个队列的时间片轮转算法
	/////////////////////////////////////////////////////////////////////////////////////
	if (currentTCB != idleTCB && currentTCB != mainTCB && currentTCB->State == RUNNING)
	{
		if (updateQuantumn1(currentTCB))
		{
			return;	//时间配额没有用完，继续
		}
		currentTCB->State = READY;
		InsertTailList(ReserveArrayList[currentTCB->Priority], &currentTCB->List);
	}

	PTCB waitPCB = currentTCB;

	//遍历当前的就绪队列
	for (int i = 0; i < NLists; i++)
	{
		if (!IsListEmpty(CurrentArrayList[i]))
		{
			currentTCB = (PTCB)RemoveHeadList(CurrentArrayList[i]);
			//postPrintf(currentTCB);
			//printf("Thread %d is Running\n", currentTCB->TID);
			
			postReleaseLock(waitPCB, currentTCB);//推迟当前阻塞线程的锁释动作放到另一个活动线程
			currentTCB->State = RUNNING;
			swapContext(waitPCB, currentTCB);

			return;
		}
	}
	//当前就绪队列为空，交换CurrentArrayList和ReserveArrayList，再遍历一遍
	PLIST_ENTRY *tmp = CurrentArrayList;
	CurrentArrayList = ReserveArrayList;
	ReserveArrayList = tmp;

	resetQuantumn(CurrentArrayList);//重置时间配额
	for (int i = 0; i < NLists; i++)
	{
		if (!IsListEmpty(CurrentArrayList[i]))
		{
			currentTCB = (PTCB)RemoveHeadList(CurrentArrayList[i]);
			//postPrintf(currentTCB);
			//printf("Thread %d is Running\n", currentTCB->TID);
			postReleaseLock(waitPCB, currentTCB);//推迟当前线程的锁释放到另一个线程
			currentTCB->State = RUNNING;
			swapContext(waitPCB, currentTCB);
			return;
		}
	}

	//没有就绪线程，选择idle
	currentTCB = idleTCB;
	postReleaseLock(waitPCB, currentTCB);//推迟当前线程的锁释放到另一个线程
	printf("Idle thread is Running\n");
	swapContext(waitPCB, currentTCB);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

}

/* 调度选中Main执行时，系统将要关机 */
void scheduleMain()
{

	if (currentTCB != idleTCB  && currentTCB->State == RUNNING)
	{
		currentTCB->State = READY;
		//挂接到就绪队列首部，时间配额未用完
		InsertHeadList(CurrentArrayList[currentTCB->Priority], &currentTCB->List);
	}
	
	PTCB waitPCB = currentTCB;

	currentTCB = mainTCB;
	printf("Main thread is Running\n");
	swapContext(waitPCB, currentTCB);
}

void outputAllTask()
{
	printf("Current List:");
	for (int i = 0; i < NLists; i++)
	{
		if (!IsListEmpty(CurrentArrayList[i]))
		{
			PLIST_ENTRY l = CurrentArrayList[i]->Flink;
			while (l != CurrentArrayList[i])
			{
				PTCB tcb = (PTCB)l;
				printf(" %d ", tcb->TID);
				l = l->Flink;
			}
		}
	}

	printf("	Reserve List:");
	for (int i = 0; i < NLists; i++)
	{
		if (!IsListEmpty(ReserveArrayList[i]))
		{
			PLIST_ENTRY l = ReserveArrayList[i]->Flink;
			while (l != ReserveArrayList[i])
			{
				PTCB tcb = (PTCB)l;
				printf(" %d ", tcb->TID);
				l = l->Flink;
			}
		}
	}

	printf("\n");
}
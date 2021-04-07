// UserThread.cpp : 定义控制台应用程序的入口点。
//


#include <stdio.h>
#include <conio.h>
#include <time.h>
#include "UserLock.h"
#include "TCB.h"
#include "semaphore.h"
//#include "mutex.h"

Semaphore S1;
Semaphore S2;
Semaphore mutex;
int in = 0;
int out = 0;
int data[7];

/* 模拟硬件计时器中断，进行线程调度*/
DWORD WINAPI TimerInterrupt(PVOID pvParam) {

	int nbasicQuantumn = PtrToUlong(pvParam);
	static LONG		nTimes = 0;
	g_hHandle = OpenThread(THREAD_ALL_ACCESS, FALSE, g_ID);
	while (!g_Finish) 
	{
		nTimes++;
		SleepEx(nbasicQuantumn, FALSE);//每隔nbasicQuantumn，处理一次始终中断
		if (g_Start)
		{
			//while (!getLock(&g_PrintLock));
			while (!getLock(&g_ScheduleLock));
			SuspendThread(g_hHandle);
			schedule();
			ResumeThread(g_hHandle);
			releaseLock(&g_ScheduleLock);
			//releaseLock(&g_PrintLock);
		}
		if(nTimes % 100 == 0)
		{ 
			//每间隔100次中断，释放处于僵死状态的线程
			freeDeadTCB(DeadList);
		}
	}
	//到达此处，说明准备shutdown，需要切换到main thread，释放资源
	while (!getLock(&g_ScheduleLock));
	SuspendThread(g_hHandle);
	scheduleMain();
	ResumeThread(g_hHandle);
	releaseLock(&g_ScheduleLock);
	return(0);
}

/* 线程执行体，可以自己定义新的函数，在创建线程时，以指针传递 */
/* makeContext(tcb, threadFunc, 1, tcb); */
void threadFunc(DUMMYARGS void *p)
{
	PTCB tcb = (PTCB)p;
	
	if(tcb->Duration == -1)
	{
		while (1);
		//printf("Thread %d end for!\n", tcb->TID);
	}
	else
	{
		while(tcb->Duration != 0);
	}

	printf("Thread %d Finished!\n", tcb->TID);
}


/* 线程执行体，可以自己定义新的函数，在创建线程时，以指针传递 */
/* 根据循环次数，确定生存周期 */
void threadFunc1(DUMMYARGS void *p, UINT nTimes)
{
	PTCB tcb = (PTCB)p;

	if (tcb->Duration == -1)
	{
		while (1);
		//printf("Thread %d end for!\n", tcb->TID);
	}
	else
	{
		for (UINT i = 0; i < nTimes; i++);
	}

	printf("Thread %d Finished!\n", tcb->TID);
}


void producer(DUMMYARGS void *p)
{
	PTCB tcb = (PTCB)p;

	if (tcb->Duration == -1)
	{
		while (1)
		{

			/*实现生产者执行逻辑*/
			getSemaphore(&S1, tcb);
			getSemaphore(&mutex, tcb);
			//生产一个物品时间（随机）
			int produceTime = rand() % (1200 - 100 + 1) + 100;
			for (UINT i = 0; i < produceTime * g_nLoopsMiliSecond; i++);
			data[in] = tcb->TID;
			in = (in + 1) % 7;
			printf("Thread %d produced data!\n", tcb->TID);
			releaseSemaphore(&mutex);
			releaseSemaphore(&S2);
			
		}
	}
}

void consummer(DUMMYARGS void *p)
{
	PTCB tcb = (PTCB)p;

	if (tcb->Duration == -1)
	{
		int d;
		while (1)
		{						
			//消费一个物品时间（随机）
			int consummeTime = rand() % (1200 - 100 + 1) + 100;
			for (UINT i = 0; i < consummeTime * g_nLoopsMiliSecond; i++);
			/*实现消费者执行逻辑*/
			getSemaphore(&S2, tcb);
			getSemaphore(&mutex, tcb);
			d = data[out];
			out = (out + 1) % 7;
			printf("Thread %d get data from Thread %d\n", tcb->TID,d);
			releaseSemaphore(&mutex);
			releaseSemaphore(&S1);

		}
	}
}

/* idle线程，目前空转，就绪队列为空时执行 */
void idleFunc(DUMMYARGS void *p)
{
	g_Start = TRUE;
	PTCB tcb = (PTCB)p;

	for (;;)
	{
		for (int i = 0; i < 1000000000; i++);
		printf("Thread idle end for! \n");
	}
}

/* 系统关闭，释放资源*/
void shutDown()
{
	for (int i = 0; i < NLists; i++)
	{
		while (!IsListEmpty(CurrentArrayList[i]))
		{
			PTCB tcb = (PTCB)RemoveHeadList(CurrentArrayList[i]);
			freeTCB(tcb);
		}

		while (!IsListEmpty(ReserveArrayList[i]))
		{
			PTCB tcb = (PTCB)RemoveHeadList(ReserveArrayList[i]);
			freeTCB(tcb);
		}
	}

	freeDeadTCB(DeadList);

	freeTCB(idleTCB);
	freeTCB(newTCB);
	free(mainTCB);

	for (int i = 0; i < NLists; i++)
	{
		free(CurrentArrayList[i]);
		free(ReserveArrayList[i]);
	}
	free(DeadList);

	printf("\n OS shut down!\n");  /* Execution control returned to main thread */
}

/* 关闭控制台时，执行关机操作，释放资源*/
BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
	switch (CEvent)
	{
		/*输入任意按键退出*/
		case CTRL_C_EVENT:
		case CTRL_CLOSE_EVENT:
		//case CTRL_SHUTDOWN_EVENT:
			g_Finish = TRUE;
			SleepEx(Quantumn[0], FALSE);
			shutDown();
			break;
	}
	return TRUE;
}

//主要用于防止debug模式下，CTRL+C产生的异常
void disableCTRLC()
{
	/*The CTRL+C and CTRL+BREAK key combinations receive special handling by console processes.
	By default, when a console window has the keyboard focus, CTRL+C or CTRL+BREAK is treated
	as a signal (SIGINT or SIGBREAK) and not as keyboard input. By default, these signals are
	passed to all console processes that are attached to the console.
	(Detached processes are not affected.) The system creates a new thread in each client
	process to handle the event. The thread raises an exception if the process is being debugged.
	The debugger can handle the exception or continue with the exception unhandled.*/

	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	if (hStdin == INVALID_HANDLE_VALUE)
		perror("GetStdHandle");
	//ENABLE_PROCESSED_INPUT

	DWORD dwMode;
	// Save the current input mode, to be restored on exit.
	if (!GetConsoleMode(hStdin, &dwMode))
		perror("GetConsoleMode");

	// Enable the window and mouse input events.
	dwMode ^= ENABLE_PROCESSED_INPUT;
	if (!SetConsoleMode(hStdin, dwMode))
		perror("SetConsoleMode");

}

int main(void)
{
	//控制台初始化
	/////////////////////////////////////////////////////////////////////////////////////////
#ifdef _DEBUG
	disableCTRLC();
#endif
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE))
	{
		// unable to install handler... 
		// display message to the user
		printf("Unable to install handler!\n");
		return -1;
	}

	//计算每毫秒循环的次数
	clock_t start = clock();
	for (UINT i = 0; i < 100000000; i++);
	g_nLoopsMiliSecond = 100000000 / (clock() - start); 

	//随机数初始化
	//rand init for different threads
	LARGE_INTEGER nFrequency;
	if (QueryPerformanceFrequency(&nFrequency))
	{
		LARGE_INTEGER nStartCounter;
		QueryPerformanceCounter(&nStartCounter);
		srand((unsigned)nStartCounter.LowPart);
	}
	///////////////////////////////////////////////////////////////////////////////////////
	//线程相关队列初始化
	//////////////////////////////////////////////////////////////////////////////////////
	CurrentArrayList = ReadyList;
	ReserveArrayList = ReserveList;

	g_ID = GetCurrentThreadId();
	g_hHandle = GetCurrentThread();
	
	//基本时间片，系统每经过此时间片，产生一个硬件计时器中断
	g_basicQuantumn = 300;

	/* 根据Nlists的数量，初始化不同优先级的时间配额 */
	/* 例如：Quantumn[0]是基本时间片的5倍，Quantumn[1]是基本时间片的4倍，.... */
	/* 需要与NLists的值对应 */
	//此处需要同学们根据实验要求修改
	Quantumn[0] = 2 * g_basicQuantumn;
	//Quantumn[1] = 2 * g_basicQuantumn;
	//Quantumn[2] = ? * g_basicQuantumn;

	/* 初始化基本线程队列 */
	for (int i = 0; i < NLists; i++)
	{
		PLIST_ENTRY pLpcb = (PLIST_ENTRY)malloc(sizeof(LIST_ENTRY));
		initTHREADList(pLpcb);
		CurrentArrayList[i] = pLpcb;

		pLpcb = (PLIST_ENTRY)malloc(sizeof(LIST_ENTRY));
		initTHREADList(pLpcb);
		ReserveArrayList[i] = pLpcb;
	}

	//僵死线程队列，定时清除
	DeadList = (PLIST_ENTRY)malloc(sizeof(LIST_ENTRY));
	initTHREADList(DeadList);

	//////////////////////////////////////////////////////////////////////////////////////////
	//线程初始化
	//需要同学们根据实验要求修改下面的初始化内容
	/////////////////////////////////////////////////////////////////////////////////////////
	/* 线程ID*/
	g_NextID++;//从1开始编号
	/* 初始线程的数量、类型（常驻或固定生存时间）、时间配额*/
	/* 可以根据需要自由组合 */
	int duration;
	for (int i = 0; i < NLists; i++)
	{
		/*每个优先级队列5个线程，可以根据需要修改*/
		for (int j = 0; j < 10; j++)
		{
			TCB *tcb = (TCB*)malloc(sizeof(TCB));
			getContext(tcb);

			/* 常驻线程初始化 */
			duration = -1;
			initTCB(tcb, g_NextID++, i, duration);

			/* 随机初始化线程生存时间，毫秒*/
			/* 同时需要设置每个优先级的不同配额*/
			//duration = rand() % (2000 - 700 + 1) + 700;
			//initTCB(tcb, g_NextID++, i, duration);


			setQuantumn(tcb, Quantumn[i]); //设置时间配额

			//线程执行的函数可以自己定义，可以传递更多参数
			//如：void threadFunc(DUMMYARGS void *p, int a, int b, int c)
			//makeContext(tcb, threadFunc, 1, tcb, a, b, c)
			//makeContext(tcb, threadFunc, 1, tcb);
			//makeContext(tcb, threadFunc1, 2, tcb, duration * g_nLoopsMiliSecond);
			if (j < 5)
				makeContext(tcb, producer, 1, tcb);
			else
				makeContext(tcb, consummer, 1, tcb);

			InsertTailList(CurrentArrayList[i], &tcb->List);
		}
	}
	
	//初始化信号量
	initSemaphore(&S1, 7);
	initSemaphore(&S2, 0);
	initSemaphore(&mutex, 1);

	//以下不需要修改
	//////////////////////////////////////////////////////////////////////////////////////
	//全局线程初始化
	//////////////////////////////////////////////////////////////////////////////////////
	idleTCB = (TCB*)malloc(sizeof(TCB));
	mainTCB = (TCB*)malloc(sizeof(TCB));
	newTCB = (TCB*)malloc(sizeof(TCB));//作为运行时动态创建线程的上下文模板
	getContext(newTCB);
	
	/*设置idle线程*/
	getContext(idleTCB);
	initTCB(idleTCB, 0, 0, -1);
	makeContext(idleTCB, idleFunc, 1, idleTCB);

	//////////////////////////////////////////////////////////////////////////////////////
	//启动
	//////////////////////////////////////////////////////////////////////////////////////	
	printf("OS start！ \n");                  /* main thread starts */

	DWORD dwThreadID;
	HANDLE hThread = chBEGINTHREADEX(NULL, 0, TimerInterrupt, (PVOID)(INT_PTR)g_basicQuantumn,	0, &dwThreadID);
	
	initTCB(mainTCB, MAXINT32, 0, -1);
	getContext(mainTCB);			/* Save the context of main thread */
	currentTCB = idleTCB;			/* ilde线程最先开始执行*/
	swapContext(mainTCB, idleTCB);	/* 切换到idle线程*/

	while (1);
	
	//应该永远不会到达这里
	return 0;
}

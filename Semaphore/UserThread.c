// UserThread.cpp : �������̨Ӧ�ó������ڵ㡣
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

/* ģ��Ӳ����ʱ���жϣ������̵߳���*/
DWORD WINAPI TimerInterrupt(PVOID pvParam) {

	int nbasicQuantumn = PtrToUlong(pvParam);
	static LONG		nTimes = 0;
	g_hHandle = OpenThread(THREAD_ALL_ACCESS, FALSE, g_ID);
	while (!g_Finish) 
	{
		nTimes++;
		SleepEx(nbasicQuantumn, FALSE);//ÿ��nbasicQuantumn������һ��ʼ���ж�
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
			//ÿ���100���жϣ��ͷŴ��ڽ���״̬���߳�
			freeDeadTCB(DeadList);
		}
	}
	//����˴���˵��׼��shutdown����Ҫ�л���main thread���ͷ���Դ
	while (!getLock(&g_ScheduleLock));
	SuspendThread(g_hHandle);
	scheduleMain();
	ResumeThread(g_hHandle);
	releaseLock(&g_ScheduleLock);
	return(0);
}

/* �߳�ִ���壬�����Լ������µĺ������ڴ����߳�ʱ����ָ�봫�� */
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


/* �߳�ִ���壬�����Լ������µĺ������ڴ����߳�ʱ����ָ�봫�� */
/* ����ѭ��������ȷ���������� */
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

			/*ʵ��������ִ���߼�*/
			getSemaphore(&S1, tcb);
			getSemaphore(&mutex, tcb);
			//����һ����Ʒʱ�䣨�����
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
			//����һ����Ʒʱ�䣨�����
			int consummeTime = rand() % (1200 - 100 + 1) + 100;
			for (UINT i = 0; i < consummeTime * g_nLoopsMiliSecond; i++);
			/*ʵ��������ִ���߼�*/
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

/* idle�̣߳�Ŀǰ��ת����������Ϊ��ʱִ�� */
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

/* ϵͳ�رգ��ͷ���Դ*/
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

/* �رտ���̨ʱ��ִ�йػ��������ͷ���Դ*/
BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
	switch (CEvent)
	{
		/*�������ⰴ���˳�*/
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

//��Ҫ���ڷ�ֹdebugģʽ�£�CTRL+C�������쳣
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
	//����̨��ʼ��
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

	//����ÿ����ѭ���Ĵ���
	clock_t start = clock();
	for (UINT i = 0; i < 100000000; i++);
	g_nLoopsMiliSecond = 100000000 / (clock() - start); 

	//�������ʼ��
	//rand init for different threads
	LARGE_INTEGER nFrequency;
	if (QueryPerformanceFrequency(&nFrequency))
	{
		LARGE_INTEGER nStartCounter;
		QueryPerformanceCounter(&nStartCounter);
		srand((unsigned)nStartCounter.LowPart);
	}
	///////////////////////////////////////////////////////////////////////////////////////
	//�߳���ض��г�ʼ��
	//////////////////////////////////////////////////////////////////////////////////////
	CurrentArrayList = ReadyList;
	ReserveArrayList = ReserveList;

	g_ID = GetCurrentThreadId();
	g_hHandle = GetCurrentThread();
	
	//����ʱ��Ƭ��ϵͳÿ������ʱ��Ƭ������һ��Ӳ����ʱ���ж�
	g_basicQuantumn = 300;

	/* ����Nlists����������ʼ����ͬ���ȼ���ʱ����� */
	/* ���磺Quantumn[0]�ǻ���ʱ��Ƭ��5����Quantumn[1]�ǻ���ʱ��Ƭ��4����.... */
	/* ��Ҫ��NLists��ֵ��Ӧ */
	//�˴���Ҫͬѧ�Ǹ���ʵ��Ҫ���޸�
	Quantumn[0] = 2 * g_basicQuantumn;
	//Quantumn[1] = 2 * g_basicQuantumn;
	//Quantumn[2] = ? * g_basicQuantumn;

	/* ��ʼ�������̶߳��� */
	for (int i = 0; i < NLists; i++)
	{
		PLIST_ENTRY pLpcb = (PLIST_ENTRY)malloc(sizeof(LIST_ENTRY));
		initTHREADList(pLpcb);
		CurrentArrayList[i] = pLpcb;

		pLpcb = (PLIST_ENTRY)malloc(sizeof(LIST_ENTRY));
		initTHREADList(pLpcb);
		ReserveArrayList[i] = pLpcb;
	}

	//�����̶߳��У���ʱ���
	DeadList = (PLIST_ENTRY)malloc(sizeof(LIST_ENTRY));
	initTHREADList(DeadList);

	//////////////////////////////////////////////////////////////////////////////////////////
	//�̳߳�ʼ��
	//��Ҫͬѧ�Ǹ���ʵ��Ҫ���޸�����ĳ�ʼ������
	/////////////////////////////////////////////////////////////////////////////////////////
	/* �߳�ID*/
	g_NextID++;//��1��ʼ���
	/* ��ʼ�̵߳����������ͣ���פ��̶�����ʱ�䣩��ʱ�����*/
	/* ���Ը�����Ҫ������� */
	int duration;
	for (int i = 0; i < NLists; i++)
	{
		/*ÿ�����ȼ�����5���̣߳����Ը�����Ҫ�޸�*/
		for (int j = 0; j < 10; j++)
		{
			TCB *tcb = (TCB*)malloc(sizeof(TCB));
			getContext(tcb);

			/* ��פ�̳߳�ʼ�� */
			duration = -1;
			initTCB(tcb, g_NextID++, i, duration);

			/* �����ʼ���߳�����ʱ�䣬����*/
			/* ͬʱ��Ҫ����ÿ�����ȼ��Ĳ�ͬ���*/
			//duration = rand() % (2000 - 700 + 1) + 700;
			//initTCB(tcb, g_NextID++, i, duration);


			setQuantumn(tcb, Quantumn[i]); //����ʱ�����

			//�߳�ִ�еĺ��������Լ����壬���Դ��ݸ������
			//�磺void threadFunc(DUMMYARGS void *p, int a, int b, int c)
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
	
	//��ʼ���ź���
	initSemaphore(&S1, 7);
	initSemaphore(&S2, 0);
	initSemaphore(&mutex, 1);

	//���²���Ҫ�޸�
	//////////////////////////////////////////////////////////////////////////////////////
	//ȫ���̳߳�ʼ��
	//////////////////////////////////////////////////////////////////////////////////////
	idleTCB = (TCB*)malloc(sizeof(TCB));
	mainTCB = (TCB*)malloc(sizeof(TCB));
	newTCB = (TCB*)malloc(sizeof(TCB));//��Ϊ����ʱ��̬�����̵߳�������ģ��
	getContext(newTCB);
	
	/*����idle�߳�*/
	getContext(idleTCB);
	initTCB(idleTCB, 0, 0, -1);
	makeContext(idleTCB, idleFunc, 1, idleTCB);

	//////////////////////////////////////////////////////////////////////////////////////
	//����
	//////////////////////////////////////////////////////////////////////////////////////	
	printf("OS start�� \n");                  /* main thread starts */

	DWORD dwThreadID;
	HANDLE hThread = chBEGINTHREADEX(NULL, 0, TimerInterrupt, (PVOID)(INT_PTR)g_basicQuantumn,	0, &dwThreadID);
	
	initTCB(mainTCB, MAXINT32, 0, -1);
	getContext(mainTCB);			/* Save the context of main thread */
	currentTCB = idleTCB;			/* ilde�߳����ȿ�ʼִ��*/
	swapContext(mainTCB, idleTCB);	/* �л���idle�߳�*/

	while (1);
	
	//Ӧ����Զ���ᵽ������
	return 0;
}

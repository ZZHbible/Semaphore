
#ifndef TCB_H
#define TCB_H

#include <windows.h>
#include <process.h>       // For _beginthreadex

// This macro function calls the C runtime's _beginthreadex function. 
// The C runtime library doesn't want to have any reliance on Windows' data 
// types such as HANDLE. This means that a Windows programmer needs to cast
// values when using _beginthreadex. Since this is terribly inconvenient, 
// I created this macro to perform the casting.
typedef unsigned(__stdcall *PTHREAD_START) (void *);

#define chBEGINTHREADEX(psa, cbStackSize, pfnStartAddr, \
   pvParam, dwCreateFlags, pdwThreadId)                 \
      ((HANDLE)_beginthreadex(                          \
         (void *)        (psa),                         \
         (unsigned)      (cbStackSize),                 \
         (PTHREAD_START) (pfnStartAddr),                \
         (void *)        (pvParam),                     \
         (unsigned)      (dwCreateFlags),               \
         (unsigned *)    (pdwThreadId)))


#if defined(_X86_)
#define DUMMYARGS
#else
#define DUMMYARGS long dummy0, long dummy1, long dummy2, long dummy3, 
#endif

#define malloc(x)	_aligned_malloc(x,64)
#define free(x)		_aligned_free(x)


/*
typedef struct _LIST_ENTRY{
	struct _LIST_ENTRY *Flink;
	struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY;
*/

#define InitializeListHead(PLH__) ((PLH__)->Flink = (PLH__)->Blink = (PLH__))

#define IsListEmpty(PLH__) ((PLH__)->Flink == (PVOID)(PLH__))

#define RemoveEntryList(PLE__) \
{ \
 PLIST_ENTRY pleBack__ = (PLIST_ENTRY)((PLE__)->Blink); \
 PLIST_ENTRY pleForward__ = (PLIST_ENTRY)((PLE__)->Flink); \
 \
 pleBack__->Flink = pleForward__; \
 pleForward__->Blink = pleBack__; \
}

#define InsertHeadList(PLH__, PLE__) \
{ \
 PLIST_ENTRY pleListHead__ = (PLH__); \
 PLIST_ENTRY pleFlink__ = (PLIST_ENTRY)((PLH__)->Flink); \
 \
 (PLE__)->Blink = pleListHead__; \
 (PLE__)->Flink = pleFlink__; \
 pleFlink__->Blink = (PLE__); \
 pleListHead__->Flink = (PLE__); \
}

#define InsertTailList(PLH__, PLE__) \
{ \
 PLIST_ENTRY pleListHead__ = (PLH__); \
 PLIST_ENTRY pleBlink__ = (PLIST_ENTRY)((PLH__)->Blink); \
 \
 (PLE__)->Flink = pleListHead__; \
 (PLE__)->Blink = pleBlink__; \
 pleBlink__->Flink = (PLE__); \
 pleListHead__->Blink = (PLE__); \
}

#define RemoveHeadList(PLH__) \
 (PLIST_ENTRY)((PLH__)->Flink); \
 RemoveEntryList((PLIST_ENTRY)((PLH__)->Flink));


/* Returns the base address of a structure from a structure member */
//#define CONTAINING_RECORD(address, type, field) \
//   ((type *)(((ULONG_PTR)address) - (ULONG_PTR)(&(((type *)0)->field))))


//��ջ
typedef struct __stack {
	void *ss_sp;
	size_t ss_size;
	int ss_flags;
} stack_t;

//�źţ���ʵ��
typedef unsigned long __sigset_t;

enum _STATE {READY, RUNNING, BLOCKED, DEAD};

typedef struct _TCB {
	LIST_ENTRY			List;
	//unsigned long int	TCB_flags;
	stack_t				TCB_stack;
	CONTEXT				TCB_context;
	int					TID;
	enum _STATE			State;
	
	int					Duration;	//�߳�����ʱ�䣬��λms���������Ϊ-1,����פ�߳�
	//__sigset_t		Sigmask;

	/*���������ֶΣ�ʵ�����ȼ����Ⱥ�IO����*/
	int					Priority;
	int					Quantumn;		//ʱ������ͬ���ȼ�����ͬ
	int					Quantumn_left;	//ʣ�����ʱ��
	int					postLockCount;	//�ӳ��ͷ���������
	int					*Lock[2];		//�ӳ��ͷŵ���
	int					IOTime;			//��ʵ��
	int					IOTme_left;		//��ʵ��
	BOOL				In_IO;
	//������schedule�п��ܵ���printf���˴���Ҫ16K��ջ�ռ䣬�����������������ǰ���ֶε�����
	char				stack[8192*10];	

} TCB, *PTCB;

DWORD g_NextID;		//giving an ID for each new thread
DWORD g_ID;			//for schedule control
HANDLE g_hHandle;	//for schedule control
PTCB mainTCB;		//main thread
PTCB idleTCB;		//idle thread
PTCB newTCB;		//context template for dynamic created thread
PTCB currentTCB;	//current active thread
BOOL g_Start;		//for system start
BOOL g_Finish;		//for system shutdown

UINT g_nLoopsMiliSecond; //ÿ����ѭ������

//������
int g_ScheduleLock;
int g_PrintLock;

//�����ȼ��������飬���Ը�����Ҫ���ӻ���ٲ�ͬ���ȼ����е�����
#define NLists	1

//���ȼ���������Ԫ��˳�����У���ReadyList[0] > ReadyList[1] > ...
//����ʱ���������ȼ�������ReadyList��ÿ���߳�ʱ��Ƭ��ʱ��ת��ReserveList
//ReadyListΪ��ʱ������ReadyList��ReserveListָ��
PLIST_ENTRY ReadyList[NLists];
PLIST_ENTRY ReserveList[NLists];

PLIST_ENTRY *CurrentArrayList;
PLIST_ENTRY *ReserveArrayList;
PLIST_ENTRY DeadList;	//�����߳��������ʵ���ʱ���ͷ������Դ

int g_basicQuantumn;		//ģ��Ӳ����ʱ�������ж�ʱ�䣬����ʱ����ʱ�򣬿��ܻ������̵߳���
int	Quantumn[NLists];		//��ͬ���ȼ������У��̵߳�ʱ��Ƭ���

void initTHREADList(LIST_ENTRY *L);
void initTCB(TCB *tcb, int ID, int priority, int duration);
void freeTCB(TCB *tcb);
void freeDeadTCB(PLIST_ENTRY deadList);
TCB * duplicateTCB();
void setQuantumn(TCB *tcb, int quantumn);
int getContext(TCB *tcb);
int setContext(const TCB *tcb);
int makeContext(TCB *, void (*)(), int, ...);
int swapContext(TCB *, const TCB *);
void postReleaseLock(TCB *, TCB *);
void schedule();
void scheduleMain();
void outputAllTask();

#endif /* TCB_H */
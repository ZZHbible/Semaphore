#ifndef USERLOCK_H
#define USERLOCK_H

/* an ASM spinlock from wiki */
/*
; In C: while(!__sync_bool_compare_and_swap(&locked, 0, 1)) while(locked) __builtin_ia32_pause();
spin_lock:
mov     ecx, 1             ; Set the ECX register to 1.
retry:
xor     eax, eax           ; Zero out EAX, because cmpxchg compares against EAX.
XACQUIRE lock cmpxchg ecx, [locked]
; atomically decide: if locked is zero, write ECX to it.
;  XACQUIRE hints to the processor that we are acquiring a lock.
je      out                ; If we locked it (old value equal to EAX: 0), return.
pause:
mov     eax, [locked]      ; Read locked into EAX.
test    eax, eax           ; Perform the zero-test as before.
jz      retry              ; If it's zero, we can retry.
rep nop                    ; Tell the CPU that we are waiting in a spinloop, so it can
;  work on the other thread now. Also written as the "pause".
jmp     pause              ; Keep check-pausing.
out:
ret                        ; All done.

spin_unlock:
XRELEASE mov [locked], 0   ; Assuming the memory ordering rules apply, release the
;  lock variable with a "lock release" hint.
ret                        ; The lock has been released.


#define Lock __asm										\
{															\
__asm		push eax										\
__asm		push ecx										\
__asm		mov		ecx, 1									\
__asm	retry:												\
__asm		xor     eax, eax								\
__asm		XACQUIRE lock cmpxchg dword ptr [g_Lock], ecx	\
__asm		je      out1									\
__asm	pause1:												\
__asm		mov     eax, dword ptr [g_Lock]					\
__asm		test    eax, eax								\
__asm		jz      retry									\
__asm		rep		nop										\
__asm	jmp     pause1										\
__asm	out1:												\
__asm		pop ecx											\
__asm		pop eax											\
}											


#define Unlock	__asm{										\
__asm	XRELEASE mov dword ptr [g_Lock],0					\
}
*/

static  __declspec(naked) int getLock(int *locked)
{
	__asm {
			push ecx
			push edx
			mov	ecx, 1
			xor eax, eax
			mov edx, dword ptr[esp + 0Ch]
			XACQUIRE lock cmpxchg dword ptr[edx], ecx
			je      succ
			mov eax, 0
			jmp out1
		succ :
			mov eax, 1
		out1 :
			pop edx
			pop ecx
			ret
	}
}

static  __declspec(naked) void releaseLock(int *locked)
{
	__asm {
		mov eax, dword ptr[esp + 4h]
		mov dword ptr[eax], 0
		ret							
	}
}

//never call this function directly, it is pushed to the scheduled thread's stack for releasing the lock which 
//is held by the blocking thread
static  __declspec(naked) void releaseLockFromOtherThread(int *locked)
{
	__asm {
		push eax
		mov eax, dword ptr[esp + 8h]
		mov dword ptr[eax], 0
		pop eax
		ret	8h						// here 8h, to popup the locked parameter
	}
}


#endif /* USERLOCK_H */
#ifndef __KERNEL_SCHEDULER_H
#define __KERNEL_SCHEDULER_H



#ifndef DEFAULT_THREAD_PRIORITY
	#define DEFAULT_THREAD_PRIORITY normal // Default is used when a priority value is not given on task creation
#endif
#ifndef MAX_THREAD_NAME_LEN
	#define MAX_THREAD_NAME_LEN 16 // 
#endif

#include <vector>
#include <algorithm>
// #include <stdint.h>
#include <string>
#include <cstring>

namespace {
	switch_to_thread_mode(void*) {
		asm("svc #0");
	}
}

void PendSV_Trigger();

extern loop();

TCB* CurrentTCB; // Pointer to the current TCB

class RTOS {
private:
	typedef struct TCB_Stack_Frame {
		// Initial stack contents
		uint32_t r7, r6, r5, r4; // Pushed and popped by PendSV_Handler() to save and restore context
		uint32_t r0, r1, r2, r3; // Return value register, scratch register, or argument register
		uint32_t r12;  // Intra-Procedure-call scratch register
		uint32_t lr;   // Link register
		uint32_t pc;   // Program counter
		uint32_t xPSR; // Program status register
	} stack_t;

	typedef enum ThreadState {
		// States that can be run
		active,   // Thread is currently running (Rename to Running?)
		paused,   // Rename Preempted, Stopped???
		queued,   // Has not started yet (rename to ready, available???)
		// States prevent the thread from running (Blocked)
		sleeping, // Blocked for a specific period of time and will unblock when the timer expires.
		blocked   // Blocked by a user program or thread and can only be unblocked manually by a user program or thread
	} TState_t;

	struct sleep_t {
		uint32_t start; // Start time
		uint32_t delay; // Delay in milliseconds
		bool operator==(bool) { return (millis() - start) >= delay; }
		operator bool() { return (millis() - start) >= delay; }
	};

	// void purgeThreads(); // Remove expired threads
	static void updateThreads(); /** TODO: Remove?? Replace?? */ // Updates blocked threads if there condition/delay/wait is over
	static TCB* setActiveThread(); // Returns the highest priority thread that is not blocked

public:
	typedef enum ThreadPriority { // Priority levels for tasks
		uninterruptible = -1,
		critical,
		high,
		moderate,
		normal,
		low,
		none
	} TPri_t;

	typedef struct TCB { // Thread control block
		volatile uint32_t sp; // Stack pointer (Does it need to be volatile???)
		int (*func)(void); // Pointer to thread code
		ThreadPriority priority; // Lower priority value means a higher priority.
		ThreadState state; // active, paused, blocked, queued
		Sleep_t sleep;
		std::string name; // User defined name for the thread. Used to get a handle to the thread.
		void* stack; // Stack with initialization values for the thread

		TCB(std::string _name, TPri_t _priority, TState_t _state, int (*_func)(void), uint32_t _stackSize) {
			name = _name; // Set name
			func = _func; // Set func
			priority = _priority; // Set priority
			state = _state; // Set state manager

			if (_stackSize < 48) _stackSize = 48; // Ensure that the stack is at least 48 bytes to accommodate the compiler saving registers on function entry (I need to find a way to tell gcc not to save registers on function entry because the context switch does it for us)
			stack = malloc(_stackSize + sizeof(stack_t)); // Allocate stack size plus space for the stack frame
			if (stack == NULL) { /** TODO: Add error handling in case memory is not allocated */ }
			stack_t* frame = (stack_t*)((uint32_t)stack + _stackSize); // Get the stack frame from the top of the stack

			sp = (uint32_t)frame;         // Set SP to the start of the stack frame
			frame->pc = (uint32_t)func;            // Set PC to the function address
			frame->xPSR = 0x01000000;              // Set xPSR to 0x01000000 (the default value)
			frame->lr = (uint32_t)&onReturn(); // Set LR to a return handler function
		}
	} TCB_t;

	static bool enabled = false; // If the scheduler is running
	static std::vector<TCB*> threads; // Holds pointers to all threads

	static void init(void) {
		// Configure the priority of Systick and PendSV
		nvic_setpriority(PendSV_IRQn, 3);  // Set PendSV interrupt to the lowest priority
		nvic_setpriority(SysTick_IRQn, 0); // Set Systick interrupt to the highest priority

		threads.push_back(new TCB("_MAIN", none, active, nullptr, 0));
		CurrentTCB = thread("_MAIN");

		void* kernel_stack = malloc(256); // Allocate a stack for the kernel

		asm("dsb"); // Data synchronization barrier
		asm("isb"); // Instruction synchronization barrier

		switch_to_thread_mode(kernel_stack);
	}

	static int create(std::string name, int stackSize, int (*_func)(void), TPri_t priority = DEFAULT_THREAD_PRIORITY) {
		if (name.length()>MAX_THREAD_NAME_LEN) { return -1; }
		if (thread(name.c_str()) == nullptr) { // Check that name does not already exist
			threads.push_back(new TCB(name, priority, queued, *_func, stackSize));
			return 0;
		} else {
			return -1;
		}
	}
	
	/** TODO: Remove?? Replace?? */
	static void onReturn() { // (Rename???) Called when active thread returns
		free(CurrentTCB->stack); // Free the stack
		threads.erase( std::find(threads.begin(),threads.end(),CurrentTCB) ); // Finds and removes the activeThread from the threads vector
		CurrentTCB = main(); // needed???
		PendSV_Trigger(); // Call PendSV to run the next thread
		/** TODO: 
		 * When a thread is removed from running (ends??), check that the stack pointer is within the valid range
		 * (on context switch, check for stack overflow)
		 */
	}

	// Find the thread with the given name and return its pointer
	static TCB* thread(const char *name) {
		if (strlen(name) > MAX_THREAD_NAME_LEN) return nullptr;
		for (uint16_t i = 0; i < threads.size(); i++)
			if (threads[i]->name == name) return threads[i];
		return nullptr;
	}
	// Return a pointer to the currently running thread
	static TCB* self() { return (TCB*)CurrentTCB; }
	// Return a pointer to the main thread
	static TCB* main() { return (TCB*)threads[0]; }

	static void block();                // Set the current thread's state to blocked and call PendSV
	static void block(TCB* thread);     // Set the given thread's state to blocked
	static void unblock(TCB* thread);   // Set the given thread's state to paused
	static void sleep(uint32_t millis); // Sleep the current thread for _ms milliseconds
	
	/** yield()
	 * @short Immediately invoke a context switch.
	 * This can be used to force the status of blocked/sleeping/waiting threads to be updated.
	 * If a thread is unblocked and has a higher priority than the current thread, it will be
	 * run. Otherwise the current thread will be resumed.
	 */
	static void yield() { PendSV_Trigger(); }

}; /** END: Class Scheduler */


#endif /** END: __KERNEL_SCHEDULER_H */
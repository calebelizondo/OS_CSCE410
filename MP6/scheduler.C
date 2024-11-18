/*
 File: scheduler.C
 
 Author:
 Date  :
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler::Scheduler() : threadQueue() {
  Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield() {
  if (Machine::interrupts_enabled()) Machine::disable_interrupts();

  if (!threadQueue.isEmpty()) {
    Thread* incoming_thread = threadQueue.dequeue();
    Thread::dispatch_to(incoming_thread);
  }

  if (!Machine::interrupts_enabled()) Machine::enable_interrupts();
}

void Scheduler::resume(Thread * _thread) {
  if (Machine::interrupts_enabled()) Machine::disable_interrupts();
  threadQueue.enqueue(_thread);
  if (!Machine::interrupts_enabled()) Machine::enable_interrupts();
}

void Scheduler::add(Thread * _thread) {
  if (Machine::interrupts_enabled()) Machine::disable_interrupts();
  threadQueue.enqueue(_thread);
  if (!Machine::interrupts_enabled()) Machine::enable_interrupts();
}

void Scheduler::terminate(Thread * _thread) {
  if (Machine::interrupts_enabled()) Machine::disable_interrupts();
  bool thread_found = threadQueue.terminate(_thread);
  if (!Machine::interrupts_enabled()) Machine::enable_interrupts();
}

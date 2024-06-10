#include "uthreads.h"
#include <iostream>
#include <queue>
#include <set>
#include <thread>
#include <csignal>
#include <setjmp.h>

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
      : "=g" (ret)
      : "0" (addr));
  return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}


#endif
#define SECOND 1000000
#define STACK_SIZE 4096

typedef void (*thread_entry_point)(void);


enum class State { READY, BLOCKED, RUNNING };

class Thread {
public:
  int tid;
  int quantumsAlive;
  sigjmp_buf env;

  int sleepQuantums;
  thread_entry_point entry_point;
  State state;
  // Constructor to initialize tid
  // TODO: recreate constructor with new fields.
  Thread(int id, thread_entry_point entry_point)
      : tid(id), quantumsAlive(1),
        state(State::READY) {
    char* stack = new char[STACK_SIZE];
    sigsetjmp(env, 1);
    (env->__jmpbuf)[JB_SP] = translate_address ((address_t) stack + STACK_SIZE - sizeof(address_t));
    (env->__jmpbuf)[JB_PC] = translate_address ((address_t )entry_point);
  }
};

// Use typedef to create an alias for the class
void freeMemory();
void setRunningThread();
void removeFromReadyQueue(int);

// Global variables:
int quantomUsecs;
std::queue<int> readyQueue;
std::set<int> blockedSet;
std::set<int> sleepingSet;
int runningThread;
int totalQuantums;
std::vector<Thread *> threads(MAX_THREAD_NUM, nullptr);


void scheduled_controller(){
  sigsetjmp(threads[runningThread]->env,1);
  threads[runningThread]->quantumsAlive ++;
  for (const int &thread : sleepingSet){
    threads[thread]->sleepQuantums --;
    if (threads[thread]->sleepQuantums <= 0){
      sleepingSet.erase (runningThread);
    }
    threads[runningThread]-> state = State::READY;
    setRunningThread();
  }
}

/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as
 * RUNNING. There is no need to provide an entry_point or to create a stack for
 * the main thread - it will be using the "regular" stack and PC. You may assume
 * that this function is called before any other thread library function, and
 * that it is called exactly once. The input to the function is the length of a
 * quantum in micro-seconds. It is an error to call this function with
 * non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
 */
int uthread_init(int quantum_usecs) {
  if (quantum_usecs < 0) {
    std::cerr << "thread library error: quantom time is negative.\n";
    return -1;
  }
  // Todo how do I fix this leak?
  Thread *mainThread = new (std::nothrow) Thread(0, nullptr);
  if (!mainThread) {
    // System call failed.
    std::cerr << "system error: [uthread_init] memory allocation failed.\n";
    return -1;
    // TODO: exit properly.
  }
  threads[0] = mainThread;
  runningThread = 0;
  quantomUsecs = quantum_usecs;
  return 0;
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point
 * with the signature void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of
 * concurrent threads to exceed the limit (MAX_THREAD_NUM). Each thread should
 * be allocated with a stack of size STACK_SIZE bytes. It is an error to call
 * this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return
 * -1.
 */
int uthread_spawn(thread_entry_point entry_point) {
  int curID = 0;
  for (; threads[curID]; curID++) {
  }
  if (curID == MAX_THREAD_NUM) {
    std::cerr << "thread library error: max amount of threads reached.\n";
    // TODO: do we exit?
    return -1;
  }
  Thread *newThread = new Thread(curID, entry_point);
  threads[curID] = newThread;
  return curID;
}

/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant
 * control structures.
 *
 * All the resources allocated by the library for this thread should be
 * released. If no thread with ID tid exists it is considered an error.
 * Terminating the main thread (tid == 0) will result in the termination of the
 * entire process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and
 * -1 otherwise. If a thread terminates itself or the main thread is terminated,
 * the function does not return.
 */
int uthread_terminate(int tid) {
  if (!threads[tid]) {
    std::cerr << "thread library error: no thread with ID tid exists.\n";
    return -1;
  }

  if (tid == 0) {
    freeMemory();
    exit(0);
  }

  if (threads[tid]->state == State::READY) {
    removeFromReadyQueue(tid);
  }

  if (threads[tid]->state == State::BLOCKED) {
    // Remove from blocked set.
    // Make sure it is indeed in blocked set:
    if (blockedSet.find(tid) == blockedSet.end()) {
      // TODO: handle this.
    }
    blockedSet.erase(tid);
  }
  if (threads[tid]->state == State::RUNNING) {
    // Handle the fact that it is currently running:
    // I think there has to be something in readyQueue - either 0 is there or it
    // is terminated (and then we would not reach this code).
    if (readyQueue.empty()) {
      // TODO: handle this, I don't believe we should reach here.
    }
    setRunningThread();

    // TODO: Reset timer.
  }
  delete threads[tid];
  threads[tid] = nullptr;

  return 0;
}

/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using
 * uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it
 * is an error to try blocking the main thread (tid == 0). If a thread blocks
 * itself, a scheduling decision should be made. Blocking a thread in BLOCKED
 * state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
 */
int uthread_block(int tid) {
  if (tid == 0) {
    std::cerr << "thread library error: [uthread_block] it is an error to "
                 "block the main thread.\n";
    return -1;
  }
  if (!threads[tid]) {
    std::cerr
        << "thread library error: [uthread_block] thread does not exist.\n";
    return -1;
  }
  // TODO: what happens if we are blocking a sleeping thread?
  if (uthread_get_tid() == tid) {
    threads[tid]->state = State::BLOCKED;
    setRunningThread();
    return 0;
  }
  // Now we may assume that we are removing a thread that is ready.
  removeFromReadyQueue(tid);
  blockedSet.insert(tid);
  threads[tid]->state = State::BLOCKED;
  return 0;
}

/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not
 * considered as an error. If no thread with ID tid exists it is considered an
 * error.
 *
 * @return On success, return 0. On failure, return -1.
 */
int uthread_resume(int tid) {
  if (!threads[tid]) {
    std::cerr
        << "thread library error: [uthread_resume] thread does not exist.\n";
    return -1;
  }

  State curState = threads[tid]->state;
  if (curState == State::RUNNING || curState == State::READY) {
    // No need to do anything.
    return 0;
  }

  // Set state to ready and move to ready queue.
  // TODO what if it is sleeping? can we resume a sleeping set?
  // (Currently we will not resume a sleeping set).
  threads[tid]->state = State::READY;
  blockedSet.erase(tid);
  readyQueue.push(tid);
  return 0;
}

/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a
 * scheduling decision should be made. After the sleeping time is over, the
 * thread should go back to the end of the READY queue. If the thread which was
 * just RUNNING should also be added to the READY queue, or if multiple threads
 * wake up at the same time, the order in which they're added to the end of the
 * READY queue doesn't matter. The number of quantums refers to the number of
 * times a new quantum starts, regardless of the reason. Specifically, the
 * quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
 */
int uthread_sleep(int num_quantums) {
  int tid = uthread_get_tid();
  if (tid == 0) {
    std::cerr << "thread library error: cannot put main thread to sleep.\n";
    return -1;
  }

  threads[tid]->sleepQuantums = num_quantums;
  threads[tid]->state = State::BLOCKED;
  sleepingSet.insert(tid);
  setRunningThread();
  return 0;
}

/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
 */
int uthread_get_tid() { return runningThread; }

/**
 * @brief Returns the total number of quantums since the library was
 * initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should
 * be increased by 1.
 *
 * @return The total number of quantums.
 */
int uthread_get_total_quantums() { return totalQuantums; }

/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING
 * state.
 *
 * On the first time a thread runs, the function should return 1. Every
 * additional quantum that the thread starts should increase this value by 1 (so
 * if the thread with ID tid is in RUNNING state when this function is called,
 * include also the current quantum). If no thread with ID tid exists it is
 * considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid.
 * On failure, return -1.
 */
int uthread_get_quantums(int tid) {
  // TODO: make sure this quantumsAlive is properly maintinaed @nahtomi(?)
  if (!threads[tid]) {
    // Thread does not exist, error:
    std::cerr << "thread library error: thread does not exist.\n";
    return -1;
  }

  return threads[tid]->quantumsAlive;
}

/**
 * This function frees all memory allocated by the program.
 * Should be called before exit.
 */
void freeMemory() {
  for (int i = 0; i < MAX_THREAD_NUM; i++) {
    if (!threads[i]) {
      continue;
    }
    delete threads[i];
  }
}

/**
 * This function sets the next thread in readyQueue to running.
 */
void setRunningThread() {
  const int nextRunningThread = readyQueue.front();
  readyQueue.pop();
  threads[nextRunningThread]->state = State::RUNNING;
  runningThread = nextRunningThread;
}

void removeFromReadyQueue(int tid) {
  std::queue<int> tmpQueue;
  // Copy queue to tmpQueue, not copying the unwanted tid.
  while (!readyQueue.empty()) {
    int cur = readyQueue.front();
    readyQueue.pop();
    if (cur == tid) {
      continue;
    }
    tmpQueue.push(cur);
  }
  // Copy back to readyQueue.
  while (!tmpQueue.empty()) {
    int cur = tmpQueue.front();
    tmpQueue.pop();
    readyQueue.push(cur);
  }
}
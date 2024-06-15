#include "uthreads.h"
#include <csignal>
#include <iostream>
#include <queue>
#include <set>
#include <setjmp.h>
#include <sys/time.h>
#include <thread>

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
        : "=g"(ret)
        : "0"(addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr) {
  address_t ret;
  asm volatile("xor    %%gs:0x18,%0\n"
               "rol    $0x9,%0\n"
               : "=g"(ret)
               : "0"(addr));
  return ret;
}

#endif
#define SECOND 1000000
#define STACK_SIZE 4096

typedef void (*thread_entry_point)(void);

enum class State { READY, BLOCKED, RUNNING };

class Thread
{
public:
    int tid;
    int quantumsAlive;
    sigjmp_buf env;

    int sleepQuantums;
    thread_entry_point entry_point;
    State state;
    // Constructor to initialize tid
    Thread(int id, thread_entry_point entry_point)
        : tid(id), quantumsAlive(1), state(State::READY)
    {
        char* stack = new char[STACK_SIZE];

        sigsetjmp(env, 1);
        (env->__jmpbuf)[JB_SP] =
            translate_address((address_t)stack + STACK_SIZE - sizeof(address_t));
        (env->__jmpbuf)[JB_PC] = translate_address((address_t)entry_point);
    }
};

// Use typedef to create an alias for the class
void freeMemory();
void setRunningThread();
bool removeFromReadyQueue(int);
bool validateTID(int);

// Global variables:
int quantomUsecs;
std::queue<int> readyQueue;
std::set<int> blockedSet;
std::set<int> sleepingSet;
int runningThread;
int totalQuantums;
std::vector<Thread*> threads(MAX_THREAD_NUM, nullptr);
sigset_t vtalarm_block_set;


void scheduledController(int sig)
{
    // TODO: add block and unblock
    threads[runningThread]->quantumsAlive++;
    totalQuantums++;
    std::set<int> wakingUpThreads;

    for (const int& thread : sleepingSet)
    {
        threads[thread]->sleepQuantums--;
        if (threads[thread]->sleepQuantums <=
            0)
        {
            // if a thread finished its sleeping time put it in ready node
            wakingUpThreads.insert(thread);
            if (blockedSet.count(thread) == 0) // check if the thread that woke up is
            // blocked: if not put it in ready
            {
                readyQueue.push(thread);
            }
        }
    }
    for (const int& thread : wakingUpThreads)
    {
        sleepingSet.erase(thread);
    }
    // TODO: is this redundent? @nahtomi
    threads[runningThread]->state = State::READY;
    readyQueue.push(runningThread);
    setRunningThread();
}

void startTimer()
{
    struct itimerval timer{};
    timer.it_value.tv_sec =
        quantomUsecs / 1000000; // first time interval, seconds part
    timer.it_value.tv_usec =
        quantomUsecs % 1000000; // first time interval, microseconds part

    // configure the timer to expire every 3 sec after that.
    timer.it_interval.tv_sec =
        quantomUsecs / 1000000; // following time intervals, seconds part
    timer.it_interval.tv_usec =
        quantomUsecs % 1000000; // following time intervals, microseconds part

    // Start a virtual timer. It counts down whenever this process is executing.
    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr))
    {
        std::cerr << "system error: [start_timer] setitimer failed.\n";
        freeMemory();
        exit(1);
    }
}

/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be vtalarm_block_set as
 * RUNNING. There is no need to provide an entry_point or to create a stack for
 * the main thread - it will be using the "regular" stack and PC. You may assume
 * that this function is called before any other thread library function, and
 * that it is called exactly once. The input to the function is the length of a
 * quantum in micro-seconds. It is an error to call this function with
 * non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
 */
int uthread_init(int quantum_usecs)
{
    if (quantum_usecs <= 0)
    {
        std::cerr << "thread library error: quantom time should be positive.\n";
        return -1;
    }
    Thread* mainThread = new(std::nothrow) Thread(0, nullptr);
    if (!mainThread)
    {
        // System call failed.
        std::cerr << "system error: [uthread_init] memory allocation failed.\n";
        freeMemory();
        exit(1);
    }
    sigemptyset(&vtalarm_block_set);
    sigaddset(&vtalarm_block_set, SIGVTALRM);
    threads[0] = mainThread;
    runningThread = 0;
    quantomUsecs = quantum_usecs;
    totalQuantums = 1;

    struct sigaction sa = {nullptr};
    // Install timer_handler as the signal handler for SIGVTALRM.
    sa.sa_handler = &scheduledController;
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
    {
        std::cerr << "system error: [uthread_init] sigaction failed.\n";
        freeMemory();
        exit(1);
    }
    startTimer();
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
int uthread_spawn(thread_entry_point entry_point)
{
    int curID = 0;
    for (; threads[curID]; curID++)
    {
    }
    if (curID == MAX_THREAD_NUM)
    {
        std::cerr << "thread library error: max amount of threads reached.\n";
        return -1;
    }
    Thread* newThread = new Thread(curID, entry_point);
    threads[curID] = newThread;
    readyQueue.push(curID);
    totalQuantums++;
    startTimer();
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
int uthread_terminate(int tid)
{
    if (sigprocmask(SIG_BLOCK, &vtalarm_block_set, nullptr) == -1)
    {
        std::cerr << "system error: [uthread_terminate] sigprocmask failed.\n";
        freeMemory();
        exit(1);
    }


    if (!validateTID(tid))
    {
        std::cerr << "thread library error: [uthread_terminate] invalid tid - no "
            "thread with this tid exists.\n";
        return -1;
    }

    if (tid == 0)
    {
        freeMemory();
        exit(0);
    }

    if (threads[tid]->state == State::READY)
    {
        removeFromReadyQueue(tid);
    }

    // Anyway, remove it from sleepingSet and blockedSet. No harm-no foul.
    sleepingSet.erase(tid);
    blockedSet.erase(tid);
    bool is_running = threads[tid]->state == State::RUNNING;
    delete threads[tid];
    threads[tid] = nullptr;


    if (is_running)
    {
        // Handle the fact that it is currently running:
        // I think there has to be something in readyQueue - either 0 is there or it
        // is terminated (and then we would not reach this code).
        if (readyQueue.empty())
        {
            // TODO: handle this, I don't believe we should reach here.
            //  Answer: - the main thread can never be blocked :)
        }


        const int nextRunningThread = readyQueue.front();
        readyQueue.pop();
        runningThread = nextRunningThread;
        threads[nextRunningThread]->state = State::RUNNING;
        if (sigprocmask(SIG_UNBLOCK, &vtalarm_block_set, NULL) == -1)
        {
            std::cerr << "system error: [uthread_terminate] sigprocmask failed.\n";
            freeMemory();
            exit(1);
        }
        startTimer();
        siglongjmp(threads[nextRunningThread]->env, 1);
    }
    else
    {
        if (sigprocmask(SIG_UNBLOCK, &vtalarm_block_set, NULL) == -1)
        {
            std::cerr << "system error: [uthread_terminate] sigprocmask failed.\n";
            freeMemory();
            exit(1);
        }
    }


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
int uthread_block(int tid)
{
    if (sigprocmask(SIG_BLOCK, &vtalarm_block_set, nullptr) == -1)
    {
        std::cerr << "system error: [uthread_block] sigprocmask failed.\n";
        freeMemory();
        exit(1);
    }

    if (tid == 0)
    {
        std::cerr << "thread library error: [uthread_block] it is an error to "
            "block the main thread.\n";
        return -1;
    }
    if (!validateTID(tid))
    {
        std::cerr << "thread library error: [uthread_block] invalid tid - no "
            "thread with this tid exists.\n";
        return -1;
    }

    if (uthread_get_tid() == tid)
    {
        threads[tid]->state = State::BLOCKED;
        setRunningThread();
        return 0;
    }
    // Now we may assume that we are removing a thread that is ready or sleeping.
    // removeFromReadyQueue also works if not in ready queue.
    bool didRemove = removeFromReadyQueue(tid);
    // TODO: do we want to uncomment this? This is a kind of error check.
    // if (didRemove && threads[tid]->sleepQuantums>0) {}
    blockedSet.insert(tid);
    threads[tid]->state = State::BLOCKED;
    if (sigprocmask(SIG_UNBLOCK, &vtalarm_block_set, NULL) == -1)
    {
        std::cerr << "system error: [uthread_block] sigprocmask failed.\n";
        freeMemory();
        exit(1);
    }
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
int uthread_resume(int tid)
{
    if (sigprocmask(SIG_BLOCK, &vtalarm_block_set, nullptr) == -1)
    {
        std::cerr << "system error: [uthread_resume] sigprocmask failed.\n";
        freeMemory();
        exit(1);
    }

    if (!validateTID(tid))
    {
        std::cerr << "thread library error: [uthread_resume] invalid tid - no "
            "thread with this tid exists.\n";
        return -1;
    }

    State curState = threads[tid]->state;
    if (curState == State::RUNNING || curState == State::READY)
    {
        // No need to do anything.
        return 0;
    }

    // Set state to ready and move to ready queue.
    // TODO what if it is sleeping? can we resume a sleeping vtalarm_block_set? no!
    // (Currently we will not resume a sleeping vtalarm_block_set).
    blockedSet.erase(tid);
    if (threads[tid]->sleepQuantums == 0)
    {
        readyQueue.push(tid);
        threads[tid]->state = State::READY;
    }
    if (sigprocmask(SIG_UNBLOCK, &vtalarm_block_set, NULL) == -1)
    {
        std::cerr << "system error: [uthread_resume] sigprocmask failed.\n";
        freeMemory();
        exit(1);
    }
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
int uthread_sleep(int num_quantums)
{
    if (sigprocmask(SIG_BLOCK, &vtalarm_block_set, nullptr) == -1)
    {
        std::cerr << "system error: [uthread_sleep] sigprocmask failed.\n";
        freeMemory();
        exit(1);
    }

    int tid = uthread_get_tid();
    if (tid == 0)
    {
        std::cerr << "thread library error: [uthread_sleep] cannot put main thread "
            "to sleep.\n";
        return -1;
    }
    // A sleeping vtalarm_block_set is not blocked. But is removed from readyQueue.
    threads[tid]->sleepQuantums = num_quantums;
    sleepingSet.insert(tid);

    // Make sure we are not in readyQueue.
    removeFromReadyQueue(tid);
    startTimer();
  if (sigprocmask(SIG_UNBLOCK, &vtalarm_block_set, NULL) == -1) {
      return -1; //TODO: throw error
    }
    // Set new running thread as we have vtalarm_block_set ourselves to sleep.
    // e.g. increment quantums and decrement sleep.
    scheduledController(0);
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
int uthread_get_quantums(int tid)
{
    // TODO: make sure this quantumsAlive is properly maintinaed @nahtomi(?)
    if (!validateTID(tid))
    {
        std::cerr << "thread library error: [uthread_get_quantums] invalid tid - "
            "no thread with this tid exists.\n";
        return -1;
    }

    return threads[tid]->quantumsAlive;
}

/**
 * This function frees all memory allocated by the program.
 * Should be called before exit.
 */
void freeMemory()
{
    for (int i = 0; i < MAX_THREAD_NUM; i++)
    {
        if (!threads[i])
        {
            continue;
        }
        delete threads[i];
    }
}

/**
 * This function sets the next thread in readyQueue to running.
 */
void setRunningThread()
{
    const int nextRunningThread = readyQueue.front();
    readyQueue.pop();
    threads[nextRunningThread]->state = State::RUNNING;
    int to_jump =
        sigsetjmp(threads[runningThread]->env,
                  1); // saving the env for the current thread if we jumped here
    // the value of to_jump will be 1, o.w 0
    if (!to_jump)
    {
        runningThread = nextRunningThread;
        siglongjmp(threads[nextRunningThread]->env,
                   1); // if to_jump = 0 then this is the first time the thread
        // reach this line, so we need to jump
    }
}

bool removeFromReadyQueue(int tid)
{
    std::queue<int> tmpQueue;
    bool ret = false;
    // Copy queue to tmpQueue, not copying the unwanted tid.
    while (!readyQueue.empty())
    {
        int cur = readyQueue.front();
        readyQueue.pop();
        if (cur == tid)
        {
            ret = true;
            continue;
        }
        tmpQueue.push(cur);
    }
    // Copy back to readyQueue.
    while (!tmpQueue.empty())
    {
        int cur = tmpQueue.front();
        tmpQueue.pop();
        readyQueue.push(cur);
    }
    return ret;
}

bool validateTID(int tid)
{
    if (tid < 0)
    {
        return false;
    }
    if (tid >= MAX_THREAD_NUM)
    {
        return false;
    }
    if (!threads[tid])
    {
        return false;
    }
    return true;
}

//int block_vtalarm(){

//}

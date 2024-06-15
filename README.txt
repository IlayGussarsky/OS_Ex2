omri.nahtomi, ilay_gussarsky
omri nahtomi(213072317), ilay gussarsky (214713091)
EX: 2

FILES:
uthreads.cpp -- a file with some code

REMARKS:
These are some remarks that
I want the graders to know
about this submission.

ANSWERS:

Q1:
a. sigsetjmp - this func receive an enviroment variable and saves the current state of the thread to the env.
   some of the things it saves is the pointer to the stack of the current running thread and the line in the code it is executed in (the current line)

   siglongjmp - this func receive an enviroment and switch its current env to it.
   it makes the stack of the env the one used and jumps to the line in the code the env stoped at (the last sigsetjmp)

b. sigsetjmp has an argumnet 'savemask'. if savemask != 0 the env is saved with its signal mask. o.w the mask is not saved.
   siglongjmp only restores the mask if it was saves (in the last sisetjmp).



Q2:

Q3:
Some advantages of using different processes for different tabs are:
 -  Security - even if a malicious entity has managed to "take over" a certain tab, it would still not have access to other tabs
        (i.e. the bank's website) and cause further damage.
 -  Isolation - problems encountered in one tab won't affect other tabs. (I.e. if one tab encounters an error).
 -  Resource management - each tab can be allocated its own memory and compute power, and it can be controlled based on
        The machine's resources and current state.
Some disadvantages of using different processes for different tabs are:
 -  Memory usage - each tab (even two instances of the same website) use different memory addresses. Indeed we see that chrome is
        extremely memory intensive.
 -  Efficiency regarding time spent running OS activities - if the implementation gives a process for each tab, the computing
        time spent on chrome now splits over several processes, increasing time spent on context switching etc.
 -  Communication between processes - kernel-level threads can communicate with each-other much more naturally and efficiently than
        subprocesses of the same process.
Q4:

Q5:
The difference between 'real' and 'virtual' time (in the context of timers) is that virtual time counts only the time spent in the timer's process
     (i.e. if after the timer started there was a context switch the time spent on the other process is not counted for the timer).
     Real time counts the time a user would have counted if he had a stopwatch that starts when the timer starts
     (i.e. if after the timer started there was a context switch the time spent on the other process is counter for the timer).


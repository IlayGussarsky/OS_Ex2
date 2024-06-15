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


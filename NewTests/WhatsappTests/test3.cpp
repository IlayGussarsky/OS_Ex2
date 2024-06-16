#include "../../uthreads.h"
#include "stdio.h"
#include <signal.h>
#include <unistd.h>

void g()
{
  printf ("%d ", uthread_get_tid());
  fflush(stdout);
  uthread_sleep (1);
  printf ("%d ", uthread_get_tid());
  fflush(stdout);

  uthread_terminate (1);
}

void f()
{
  printf ("%d ", uthread_get_tid());
  fflush(stdout);

  uthread_block (1);
  uthread_terminate(uthread_get_tid());
}

int main(int argc, char **argv)
{
  uthread_init (999999);
  uthread_spawn (g);
  uthread_spawn (f);
  kill(getpid(),SIGVTALRM);
  printf ("%d ", uthread_get_tid());
  fflush(stdout);

  kill(getpid(),SIGVTALRM);
  printf ("\nYou should see: 1 2 0\n");
  fflush(stdout);

  uthread_terminate(0);
}
#include "../../uthreads.h"
#include "stdio.h"
#include <signal.h>
#include <unistd.h>
#include <iostream>


void g()
{
  printf ("%d ", uthread_get_tid());
  fflush(stdout);

  // std::cout << uthread_get_tid();
  uthread_sleep (1);
  printf ("%d ", uthread_get_tid());
  fflush(stdout);

  // std::cout << uthread_get_tid();
  uthread_terminate (1);
}

void f()
{
  printf ("%d ", uthread_get_tid());
  fflush(stdout);
  uthread_terminate(uthread_get_tid());
}

int main(int argc, char **argv)
{
  uthread_init (999999);
  uthread_spawn (g);
  uthread_spawn (f);
  kill(getpid(),SIGVTALRM);
  printf ("%d ", uthread_get_tid());
  kill(getpid(),SIGVTALRM);
  printf ("\nYou should see: 1 2 0 1\n");
  uthread_terminate(0);
}
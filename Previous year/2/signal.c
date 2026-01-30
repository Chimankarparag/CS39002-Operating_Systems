/***********************************************************/
/*** Sample program demonstrating the sending of signals ***/
/*** Last updated by Abhijit Das, 13-Jan-2026            ***/
/***********************************************************/

/***********************************************************/
/*** When this program runs, look at the status of the   ***/
/*** parent and the child in another shell.              ***/
/*** To do so, keep on running the following command:    ***/
/*** ps af | grep a.out                                  ***/
/***********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

/* Customized sleep function */
void mysleep ( )
{
   int i, t;

   usleep(100000);
   t = 5 + rand() % 5;
   printf("+++ Parent: Going to sleep for %d seconds:", t);
   fflush(stdout);
   for (i=1; i<=t; ++i) {
      sleep(1);
      printf(" %d", i);
      fflush(stdout);
   }
   usleep(250000);
   printf("\n");
}

/* The signal handler for the child process */
void childSigHandler ( int sig )
{
   if (sig == SIGUSR1) {
      printf("\n\t\t\t\t\t+++ Child : Received signal SIGUSR1 from parent...\n\n");
   } else if (sig == SIGUSR2) {
      printf("\n\t\t\t\t\t+++ Child : Received signal SIGUSR2 from parent...\n\n");
   } else if (sig == SIGINT) {
      printf("\n\t\t\t\t\t+++ Child : Received signal SIGINT from parent...\n\n");
   } else if (sig == SIGKILL) {
      printf("\n\t\t\t\t\t+++ Child : Received signal SIGKILL from parent...\n\n");
   }
}

int main ()
{
   int pid;

   srand((unsigned int)time(NULL));

   pid = fork();                                   /* Spawn the child process */
   if (pid) {
                                                            /* Parent process */
      int t;

      mysleep();      /* Sleep for some time before sending a signal to child */

      t = 1 + rand() % 2;
      printf("+++ Parent: Going to send signal SIGUSR%d to child\n", t);
      kill(pid, (t == 1) ? SIGUSR1 : SIGUSR2);        /* Send signal to child */

      mysleep();           /* Sleep for some time before suspending the child */
      printf("+++ Parent: Going to suspend child\n");
      kill(pid, SIGTSTP);
      
      mysleep();            /* Sleep for some time before waking up the child */
      printf("+++ Parent: Going to wake up child\n");
      kill(pid, SIGCONT);
      
      mysleep();  /* Sleep for some time before trying to terminate the child */
      printf("+++ Parent: Trying to terminate child by SIGINT\n");
      kill(pid, SIGINT);

      mysleep();  /* Sleep for some time before trying to terminate the child */
      if (waitpid(pid, NULL, WNOHANG) == 0)
         printf("+++ Parent: Could not terminate child\n");

      printf("+++ Parent: Trying to terminate child by SIGKILL\n");
      kill(pid, SIGKILL);

      /* Child is terminated, but the parent has not yet wait()-ed for it.    */
      /* So the child is now a zombie process. ps af will show this           */
      /* STAT Z and <defunct>.                                                */
      
      mysleep();              /* Sleep for some time before waiting for child */
      waitpid(pid, NULL, 0);                        /* Wait for child to exit */
      printf("+++ Parent: Child exited\n");

      /* Child is now completely removed from the system as ps af will show.  */

      mysleep();                        /* Sleep for some time before exiting */

   } else {
                                                             /* Child process */

      /* The child will receive several signals from its parent. The first    */
      /* signal is randomly chosen between SIGUSR1 abd SIGUSR2. The default   */
      /* handlers of these signals would terminate the child. But the child   */
      /* refuses to die by registering the following custom-made handler      */
      /* for these two signals.                                               */

      /* The child also tries to disable the following termination signals.   */
      /* SIGINT  : Interrupt and terminate a process (same as ^C from user)   */
      /* SIGKILL : Kill a process (cannot be caught)                          */
      signal(SIGUSR1, childSigHandler);           /* Register SIGUSR1 handler */
      signal(SIGUSR2, childSigHandler);           /* Register SIGUSR2 handler */
      signal(SIGINT,  childSigHandler);           /* Register SIGINT  handler */

      signal(SIGKILL, childSigHandler);           /* Register SIGKILL handler */
                                                  /* No warning, no effect    */

      /* The child will also receive the following signals from the parent.   */
      /* The child does not register signal handlers for these signals,       */
      /* so the default actions will be taken for these signals.              */
      /* SIGTSTP : Suspend a process (same as ^Z sent to the child)           */
      /* SIGCONT : Make a suspended process alive again                       */

      while (1) pause();      /* Pause until a signal is received from parent */

      /* Eventually the child is terminated by SIGKILL from parent.           */

   }

   exit(0);
}

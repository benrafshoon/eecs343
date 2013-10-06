/***************************************************************************
 *  Title: Runtime environment
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2005/10/13 05:24:59 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
	#define __RUNTIME_IMPL__
	#define BACKGROUND_JOB_RUNNING 1
	#define BACKGROUND_JOB_STOPPED 0

  /************System include***********************************************/
	#include <assert.h>
	#include <errno.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <sys/wait.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
	#include <fcntl.h>
	#include <signal.h>

  /************Private include**********************************************/
	#include "runtime.h"
	#include "io.h"
	#include "tsh.h"

  /************Defines and Typedefs*****************************************/
    /*  #defines and typedefs should have their names in all caps.
     *  Global variables begin with g. Global constants with k. Local
     *  variables should be in all lower case. When initializing
     *  structures and arrays, line everything up in neat columns.
     */

  /************Global Variables*********************************************/

	#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

	typedef struct _BackgroundJob {
		pid_t pid;
		struct _BackgroundJob* next;
		char running;
	} BackgroundJob;

	/* the pids of the background processes */
	BackgroundJob* backgroundJobListHead = NULL;
	BackgroundJob* backgroundJobListTail = NULL;
	int backgroundJobListSize = 0;

	pid_t foregroundPID = -1;


  /************Function Prototypes******************************************/
	/* run command */
	static void RunCmdFork(commandT*, bool);
	/* runs an external program command after some checks */
	static void RunExternalCmd(commandT*, bool);
	/* resolves the path and checks for exutable flag */
	static bool ResolveExternalCmd(commandT*);
	/* forks and runs a external program */
	static void Exec(commandT*, bool);
	/* runs a builtin command */
	static bool RunBuiltInCmd(commandT*);

	static int AddBackgroundJob(pid_t toAdd, char running);

	static pid_t RemoveBackgroundJob(pid_t toDelete);

	static pid_t GetAtBackgroundJob(int position);

	static pid_t SetRunningBackgroundJob(int position, char running);

	static void printJob(int position, pid_t pid, char running);

	static void printBackgroundJobs();


  /************External Declaration*****************************************/

/**************Implementation***********************************************/
    int total_task;
	void RunCmd(commandT** cmd, int n)
	{
        int i;
        total_task = n;
        if(n == 1) {
            commandT* currentCommand = cmd[0];
            printf("Command name: %s\n", currentCommand->argv[0]);
            int argvCounter;
            for(argvCounter = 1; argvCounter < currentCommand->argc; argvCounter++) {
                printf("Argument %i: %s\n", argvCounter, currentCommand->argv[argvCounter]);
            }
            if(currentCommand->bg) {
                printf("Background\n");
            }
            RunCmdFork(cmd[0], TRUE);
        }

        else {
            RunCmdPipe(cmd[0], cmd[1]);
            for(i = 0; i < n; i++) {
                ReleaseCmdT(&cmd[i]);
            }
        }
	}

	void RunCmdFork(commandT* cmd, bool fork)
	{
		if (cmd->argc<=0) {
		    return;
		}
        if(!RunBuiltInCmd(cmd)) {
            RunExternalCmd(cmd, fork);
        }
	}

	void RunCmdBg(commandT* cmd)
	{
		// TODO
	}

	void RunCmdPipe(commandT* cmd1, commandT* cmd2)
	{
	}

	void RunCmdRedirOut(commandT* cmd, char* file)
	{
	}

	void RunCmdRedirIn(commandT* cmd, char* file)
	{
	}


/*Try to run an external command*/
static void RunExternalCmd(commandT* cmd, bool fork)
{
  if (ResolveExternalCmd(cmd)){
    Exec(cmd, fork);
  }
  else {
    printf("%s: command not found\n", cmd->argv[0]);
    fflush(stdout);
    ReleaseCmdT(&cmd);
  }
}

/*Find the executable based on search list provided by environment variable PATH*/
static bool ResolveExternalCmd(commandT* cmd)
{
  char *pathlist, *c;
  char buf[1024];
  int i, j;
  struct stat fs;

  if(strchr(cmd->argv[0],'/') != NULL){
    if(stat(cmd->argv[0], &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(cmd->argv[0],X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(cmd->argv[0]);
          return TRUE;
      }
    }
    return FALSE;
  }
  pathlist = getenv("PATH");
  if(pathlist == NULL) return FALSE;
  i = 0;
  while(i<strlen(pathlist)){
    c = strchr(&(pathlist[i]),':');
    if(c != NULL){
      for(j = 0; c != &(pathlist[i]); i++, j++)
	buf[j] = pathlist[i];
      i++;
    }
    else{
      for(j = 0; i < strlen(pathlist); i++, j++)
	buf[j] = pathlist[i];
    }
    buf[j] = '\0';
    strcat(buf, "/");
    strcat(buf,cmd->argv[0]);
    if(stat(buf, &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
	if(access(buf,X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
	  cmd->name = strdup(buf);
	  return TRUE;
	}
    }
  }
  return FALSE; /*The command is not found or the user don't have enough priority to run.*/
}

    static inline int isForegroundProcessRunning() {
        return foregroundPID != -1;
    }
    static void Exec(commandT* cmd, bool forceFork)
	{
	    if(cmd->bg) {
	        printf("Background processes not yet implemented\n");
	    } else {
	        sigset_t sigchld;
	        sigemptyset(&sigchld);
	        sigaddset(&sigchld, SIGCHLD);
	        sigprocmask(SIG_BLOCK, &sigchld, NULL);
            pid_t newPID = fork();
            if(newPID < 0)
            {
                printf("Error: could not create new process\n");
            }
            else if(newPID == 0)
            {
                sigprocmask(SIG_UNBLOCK, &sigchld, NULL);
                setpgid(0, 0);
                newPID = getpid();
                printf("Beginning execution of process %i\n", newPID);
                execvp(cmd->argv[0], cmd->argv);

            } else {
                sigprocmask(SIG_UNBLOCK, &sigchld, NULL);
                foregroundPID = newPID;
                printf("Foreground process %i created\n", foregroundPID);
                sigset_t signalsToWaitFor;
                sigemptyset(&signalsToWaitFor);
                sigaddset(&signalsToWaitFor, SIGCHLD);
                while(isForegroundProcessRunning())
                {
                    printf("Foreground process running, shell waiting for sigchld\n");
                    int signal;
                    sigwait(&signalsToWaitFor, &signal);
                    if(signal == SIGCHLD)
                    {
                        int status;
                        pid_t terminatedPID = waitpid(-1, &status, WUNTRACED);
                        if(terminatedPID == foregroundPID) {
                            if(WIFEXITED(status)) {
                            printf("Foreground process %i returned %i\n", foregroundPID, WEXITSTATUS(status));
                            }
                            if(WIFSIGNALED(status)) {
                                switch(WTERMSIG(status)) {
                                    case SIGINT:
                                        printf("Foreground process %i terminated due to uncaught SIGINT\n", foregroundPID);
                                        break;
                                    default:
                                        printf("Foreground process %i terminated due to uncaught signal %i\n", foregroundPID, WTERMSIG(status));
                                        break;
                                }
                            }
                            if(WIFSTOPPED(status)) {

                                printf("Foreground process %i stopped (not terminated) due to signal %i\n", foregroundPID, WSTOPSIG(status));
                                AddBackgroundJob(foregroundPID, BACKGROUND_JOB_STOPPED);
                            }
                            foregroundPID = -1;
                            //Foreground process terminated


                        } else {
                            printf("Background process pid=%i exited\n", terminatedPID);
                        }
                    }
                }
            }
	    }

	}



	static bool RunBuiltInCmd(commandT* cmd) {
	    if(strcmp(cmd->argv[0], "jobs") == 0) {
	        printBackgroundJobs();
	        return TRUE;
	    }
	    return FALSE;
	}

    void CheckJobs() {
 	}


commandT* CreateCmdT(int n)
{
  int i;
  commandT * cd = malloc(sizeof(commandT) + sizeof(char *) * (n + 1));
  cd -> name = NULL;
  cd -> cmdline = NULL;
  cd -> is_redirect_in = cd -> is_redirect_out = 0;
  cd -> redirect_in = cd -> redirect_out = NULL;
  cd -> argc = n;
  for(i = 0; i <=n; i++)
    cd -> argv[i] = NULL;
  return cd;
}

/*Release and collect the space of a commandT struct*/
void ReleaseCmdT(commandT **cmd){
  int i;
  if((*cmd)->name != NULL) free((*cmd)->name);
  if((*cmd)->cmdline != NULL) free((*cmd)->cmdline);
  if((*cmd)->redirect_in != NULL) free((*cmd)->redirect_in);
  if((*cmd)->redirect_out != NULL) free((*cmd)->redirect_out);
  for(i = 0; i < (*cmd)->argc; i++)
    if((*cmd)->argv[i] != NULL) free((*cmd)->argv[i]);
  free(*cmd);
}

static int AddBackgroundJob(pid_t toAdd, char running) {
    BackgroundJob* newJob = (BackgroundJob* )malloc(sizeof(BackgroundJob));
    newJob->pid = toAdd;
    newJob->next = NULL;
    newJob->running = running;
    if(backgroundJobListHead == NULL) {
        backgroundJobListHead = newJob;
        backgroundJobListTail = newJob;
    } else {
        backgroundJobListTail->next = newJob;
        backgroundJobListTail = newJob;
    }
    backgroundJobListSize++;
    printBackgroundJobs();
    return backgroundJobListSize;
}

static pid_t RemoveBackgroundJob(pid_t toDelete) {
    BackgroundJob* job = backgroundJobListHead;
    BackgroundJob* previousJob = NULL;
    int jobNumber = 1;
    while(job != NULL && job->pid != toDelete) {
        previousJob = job;
        job = job->next;
        jobNumber++;
    }
    if(job == NULL) {
        return -1;
    }
    if(job == backgroundJobListHead) {
        backgroundJobListHead = job->next;
    }
    if(job == backgroundJobListTail) {
        backgroundJobListTail = previousJob;
    }
    if(previousJob != NULL) {
        previousJob->next = job->next;
    }
    backgroundJobListSize--;
    free(job);
    return toDelete;
}
static pid_t GetAtBackgroundJob(int position) {
    BackgroundJob* job = backgroundJobListHead;
    int i;
    for(i = 1; i < position; i++) {
        if(job == NULL) {
            return -1;
        } else {
            job = job->next;
        }
    }
    if(job == NULL) {
        return -1;
    } else {
        return job->pid;
    }
}

static pid_t SetRunningBackgroundJob(int position, char running) {
    BackgroundJob* job = backgroundJobListHead;
    int i;
    for(i = 1; i < position; i++) {
        if(job == NULL) {
            return -1;
        } else {
            job = job->next;
        }
    }
    if(job == NULL) {
        return -1;
    } else {
        job->running = running;
        return job->pid;
    }
}

static void printJob(int position, pid_t pid, char running) {
    printf("[%i] pid=%i ", position, pid);
    if(running) {
        printf("Running");
    } else {
        printf("Stopped");
    }
    printf("\n");
}

static void printBackgroundJobs() {
    BackgroundJob* job = backgroundJobListHead;
    int counter = 1;
    while(job != NULL) {
        printJob(counter, job->pid, job->running);
        job = job->next;
        counter++;
    }
}

void SignalHandler(int signalNumber) {
    if(foregroundPID != -1) {
        switch(signalNumber) {
            case SIGTSTP:
                printf("SIGTSP received, stopping (not terminating) process %i\n", foregroundPID);
                killpg(foregroundPID, SIGSTOP);
                break;
            case SIGINT:
                printf("SIGINT received, forwarding to process %i\n", foregroundPID);
                killpg(foregroundPID, signalNumber);
                break;
            case SIGCHLD:
                printf("Sigchld received in signal handler\n");
                break;
            default:
                printf("Unknown signal %i received\n", signalNumber);
                break;
        }
    } else {
        switch(signalNumber) {
            case SIGCHLD:
                printf("SIGCHLD received for background process\n");
                int status;
                pid_t terminatedPID = waitpid(-1, &status, WUNTRACED);
                printf("Background process %i terminated\n", terminatedPID);
                RemoveBackgroundJob(terminatedPID);
                break;
            default:
                printf("Unknown signal %i received for background process\n", signalNumber);
        }

    }
}

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
	#include "jobs.h"

  /************Defines and Typedefs*****************************************/
    /*  #defines and typedefs should have their names in all caps.
     *  Global variables begin with g. Global constants with k. Local
     *  variables should be in all lower case. When initializing
     *  structures and arrays, line everything up in neat columns.
     */

  /************Global Variables*********************************************/

	#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

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

	static void MoveBackgroundJobToForeground(int jobNumber);

	static void WaitForForegroundProcess();

    static void ChangeDirectory(char* pathToNewDirectory);

  /************External Declaration*****************************************/

/**************Implementation***********************************************/
    int total_task;
	void RunCmd(commandT** cmd, int n)
	{
        int i;
        total_task = n;
        if(n == 1) {
            commandT* currentCommand = cmd[0];
            //printf("Command name: %s\n", currentCommand->argv[0]);
            int argvCounter;
            for(argvCounter = 1; argvCounter < currentCommand->argc; argvCounter++) {
            //    printf("Argument %i: %s\n", argvCounter, currentCommand->argv[argvCounter]);
            }
            if(currentCommand->bg) {
            //    printf("Background\n");
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
            RunCmdFork(cmd, FALSE);
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

static void Exec(commandT* cmd, bool forceFork)
{
    sigset_t sigchld;
    sigemptyset(&sigchld);
    sigaddset(&sigchld, SIGCHLD);
    //sigprocmask(SIG_BLOCK, &sigchld, NULL);
    pid_t newPID = fork();
    if(newPID < 0)
    {
        printf("Error: could not create new process\n");
    }
    else if(newPID == 0)
    {
        //sigprocmask(SIG_UNBLOCK, &sigchld, NULL);
        setpgid(0, 0);
        newPID = getpid();

        if(cmd->is_redirect_out) {
            printf("Redirecting output to %s\n", cmd->redirect_out);
            int outputFileDescriptor = open(cmd->redirect_out, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
            if(outputFileDescriptor == -1) {
                printf("Could not redirect output to %s\n", cmd->redirect_out);
                exit(1);
            } else {
                dup2(outputFileDescriptor, 1);
                close(outputFileDescriptor);
            }
        }

        if(cmd->is_redirect_in) {
            printf("Redirecting input from %s\n", cmd->redirect_in);
            int inputFileDescriptor = open(cmd->redirect_in, O_RDONLY);
            if(inputFileDescriptor == -1) {
                printf("Could not redirect input from %s\n", cmd->redirect_out);
                exit(1);
            } else {
                dup2(inputFileDescriptor, 0);
                close(inputFileDescriptor);
            }
        }

        printf("Beginning execution of process %i\n", newPID);
        execvp(cmd->argv[0], cmd->argv);
    } else {
        //sigprocmask(SIG_UNBLOCK, &sigchld, NULL);
        printf("Process %i created\n", newPID);
        if(cmd->bg) {
            printf("Adding new process to background\n");
            int jobNumber = AddJob(newPID, JOB_RUNNING_BACKGROUND);
            PrintJob(jobNumber, newPID, JOB_RUNNING_BACKGROUND);
        } else {
            printf("New process running in foreground\n");
            int jobNumber = AddJob(newPID, JOB_RUNNING_FOREGROUND);
            PrintJob(jobNumber, newPID, JOB_RUNNING_FOREGROUND);
            WaitForForegroundProcess();
        }
    }
}

static void MoveBackgroundJobToForeground(int jobNumber) {
    if(IsForegroundProcessRunning()) {
        printf("Somehow a foreground process %i is already running, this should never occur\n", GetForegroundJob());
        return;
    }
    if(jobNumber == -1) {
        printf("No such job\n");
            return;
    }
    pid_t foregroundPID = SetJobRunningStateByJobNumber(jobNumber, JOB_RUNNING_FOREGROUND);
    if(foregroundPID == -1) {
        printf("No such job\n");
        return;
    } else {
        killpg(foregroundPID, SIGCONT);
        printf("Moving task %i (pid %i) to foreground\n", jobNumber, foregroundPID);
        WaitForForegroundProcess();
    }

}

static void WaitForForegroundProcess() {
    while(IsForegroundProcessRunning()) {
        printf("Foreground process running, shell waiting for sigchld\n");
        int status;
        pid_t terminatedPID = waitpid(-1, &status, WUNTRACED);
        printf("PID %i changed state\n", terminatedPID);
        pid_t foregroundPID = GetForegroundJob();
        if(terminatedPID == foregroundPID) {
            if(WIFEXITED(status)) {
                printf("Foreground process %i returned %i\n", foregroundPID, WEXITSTATUS(status));
                RemoveJobByPID(foregroundPID);
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
                RemoveJobByPID(foregroundPID);
            }
            if(WIFSTOPPED(status)) {

                printf("Foreground process %i stopped (not terminated) due to signal %i\n", foregroundPID, WSTOPSIG(status));
                int jobNumber = SetJobRunningStateByPID(foregroundPID, JOB_STOPPED);
                PrintJob(jobNumber, foregroundPID, JOB_STOPPED);

            }
        } else {
            printf("Background process pid=%i exited\n", terminatedPID);
            SetJobRunningStateByPID(terminatedPID, JOB_BACKGROUND_DONE);
        }
    }
}



	static bool RunBuiltInCmd(commandT* cmd) {
	    if(strcmp(cmd->argv[0], "jobs") == 0) {
	        PrintAllJobsAndRemoveDoneJobs();
	        return TRUE;
	    }

            if(strcmp(cmd->argv[0], "bg") == 0) {

                if(cmd->argc < 2) {
                    printf("Must specify a job number to put in background\n");
                }

                else{
                    int pID = SetJobRunningStateByJobNumber(atoi(cmd->argv[1]), JOB_RUNNING_BACKGROUND);
                    killpg(pID,SIGCONT);
                }
                return TRUE;
            }

	    if(strcmp(cmd->argv[0], "fg") == 0) {
            if(cmd->argc < 2) {
                MoveBackgroundJobToForeground(-1);
            } else {
                MoveBackgroundJobToForeground(atoi(cmd->argv[1]));
            }

            return TRUE;
	    }
	    if(strcmp(cmd->argv[0], "cd") == 0) {
	        if(cmd->argc >= 2) {
	            ChangeDirectory(cmd->argv[1]);
	        } else {
	            ChangeDirectory(NULL);
	        }
	        return TRUE;
	    }
	    return FALSE;
	}

void CheckJobs() {
    PrintAndRemoveDoneJobs();
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


void SignalHandler(int signalNumber) {
    if(IsForegroundProcessRunning()) {
        switch(signalNumber) {
            case SIGTSTP:
                printf("SIGTSP received, stopping (not terminating) process %i\n", GetForegroundJob());
                killpg(GetForegroundJob(), SIGTSTP);
                break;
            case SIGINT:
                printf("SIGINT received, forwarding to process %i\n", GetForegroundJob());
                killpg(GetForegroundJob(), signalNumber);
                break;
            case SIGCHLD:
                printf("Sigchld received in signal handler\n");
                printf("This should never occur\n");
                break;
            default:
                printf("Unknown signal %i received\n", signalNumber);
                break;
        }
    } else {
        switch(signalNumber) {
            case SIGINT:
                printf("SIGINT received, no foreground process running\n");
                break;
            case SIGTSTP:
                printf("SIGTSTP received, no foreground process running\n");
                break;
            case SIGCHLD:
                printf("SIGCHLD received for background process\n");
                int status;
                pid_t terminatedPID = waitpid(-1, &status, WUNTRACED | WNOHANG);
                if(terminatedPID > 0) {
                    printf("Background process %i exited\n", terminatedPID);
                    SetJobRunningStateByPID(terminatedPID, JOB_BACKGROUND_DONE);
                }

                break;
            default:
                printf("Unknown signal %i received for background process\n", signalNumber);

        }

    }
}

static void ChangeDirectory(char* pathToNewDirectory) {
    if(pathToNewDirectory != NULL) {
        if(strcmp(pathToNewDirectory, "~") == 0 || strstr(pathToNewDirectory, "~/") == pathToNewDirectory) {
            char* home = getenv("HOME");
            char* homeRelativePathToNewDirectory = (char *)malloc(sizeof(char) * (strlen(home) + strlen(pathToNewDirectory)));
            strcpy(homeRelativePathToNewDirectory, home);
            if(strlen(pathToNewDirectory) > 1) {
                strcat(homeRelativePathToNewDirectory, pathToNewDirectory + 1);
            }
            if(chdir(homeRelativePathToNewDirectory) == -1) {
                printf("Could not change directory to %s\n", homeRelativePathToNewDirectory);
            }
            free(homeRelativePathToNewDirectory);
        } else {
            if(chdir(pathToNewDirectory) == -1) {
                printf("Could not change directory to %s\n", pathToNewDirectory);
            }
        }

    }

}

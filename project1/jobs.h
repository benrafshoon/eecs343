#ifndef __JOBS_H__
#define __JOBS_H__

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#define JOB_STOPPED 0
#define JOB_RUNNING_BACKGROUND 1
#define JOB_RUNNING_FOREGROUND 2
#define JOB_BACKGROUND_DONE 3



typedef struct _Job {
    pid_t pid;
    int jobNumber;
    struct _Job* next;
    char runningState;
    char* commandName;
} Job;


//Adds a process to the end of list of background processes.  The job number is assigned to be
//the current largest job number + 1
int AddJob(pid_t toAdd, char runningState);

//Removes the background job specified by pid toDelete from the list of background jobs
//On success, returns the pid toDelete.  On fail, returns -1
pid_t RemoveJobByPID(pid_t pidToDelete);

//Removes the background job specified by jobNumber from the list of background jobs
//On success, returns the pid of the job.  On fail, returns -1
pid_t RemoveJobByJobNumber(int jobNumberToDelete);

//Returns the pid of the background job specified by jobNumber.  On fail, returns -1
pid_t GetJobByJobNumber(int jobNumber);

pid_t GetForegroundJob();

//Sets the running status of the job specified by jobNumber.  On fail, returns -1
//On success, returns the pid of the job specified by jobNumber
pid_t SetJobRunningStateByJobNumber(int jobNumber, char runningState);

int SetJobRunningStateByPID(pid_t pid, char runningState);

inline int IsForegroundProcessRunning();

void PrintJob(int jobNumber, pid_t pid, char running);

void PrintAllJobs();

void PrintAndRemoveDoneJobs();

#endif

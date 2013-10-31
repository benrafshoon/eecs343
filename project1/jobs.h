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
//the current largest job number + 1.  Returns the job number.  commandName is the name that is printed
//when jobs is called
int AddJob(pid_t toAdd, char runningState, const char* commandName);

//Removes the background job specified by pid toDelete from the list of background jobs
//On success, returns the pid toDelete.  If there is no job with that pid, returns -1
pid_t RemoveJobByPID(pid_t pidToDelete);

//Removes the background job specified by jobNumber from the list of background jobs
//On success, returns the pid of the job.  If there is no job with that jobNumber, returns -1
pid_t RemoveJobByJobNumber(int jobNumberToDelete);

//Returns the pid of the background job specified by jobNumber.  If there is no job with that jobNumber
//returns -1
pid_t GetJobByJobNumber(int jobNumber);

//Returns the pid of the foreground job, or -1 if no foreground job is running
pid_t GetForegroundJob();

//Sets the running status of the job specified by jobNumber.  On fail, returns -1
//On success, returns the pid of the job specified by jobNumber.  If the runningState
//is JOB_RUNNING_FOREGROUND, the job is set to the foreground job
pid_t SetJobRunningStateByJobNumber(int jobNumber, char runningState);

//Sets the running status of the job specified by jobNumber.  On fail, returns -1
//On success, returns the pid of the job specified by jobNumber.  If the runningState
//is JOB_RUNNING_FOREGROUND, the job is set to the foreground job
int SetJobRunningStateByPID(pid_t pid, char runningState);

//Returns true if there is a foreground process running, false otherwise
inline int IsForegroundProcessRunning();

//Prints the pid of the job specified by jobNumber, if it exists
void PrintPID(int jobNumber, pid_t pid);

//Prints the details of a job
void PrintJob(int jobNumber, pid_t pid, char runningState, const char* commandName);

//Searches the job list and prints the job specified by pid if it exists
void FindAndPrintJobByPID(pid_t pid);

//Searches the job list and prints the job specified by jobNumber is if exists
void FindAndPrintJobByJobNumber(int jobNumber);

//Prints all jobs.  Any job with runningState JOB_BACKGROUND_DONE is removed from the list of jobs
//after printing
void PrintAllJobsAndRemoveDoneJobs();

//Prints all jobs with runningState JOB_BACKGROUND_DONE, then removes thhose jobs from the list of jobs
void PrintAndRemoveDoneJobs();

#endif

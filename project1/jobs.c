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

#include "jobs.h"

static Job* jobListHead = NULL;
static Job* jobListTail = NULL;
static Job* foregroundJob = NULL;

static void RemoveJobFromList(Job* job, Job* previousJob);

inline int IsForegroundProcessRunning() {
    return foregroundJob != NULL;
}

int AddJob(pid_t toAdd, char runningState, const char* commandName) {
    Job* newJob = (Job* )malloc(sizeof(Job));
    newJob->pid = toAdd;
    newJob->next = NULL;
    newJob->runningState = runningState;
    newJob->commandName = (char*)malloc(sizeof(char) * (strlen(commandName) + 1));
    strcpy(newJob->commandName, commandName);

    if(jobListHead == NULL) {
        newJob->jobNumber = 1;
        jobListHead = newJob;
        jobListTail = newJob;
    } else {
        jobListTail->next = newJob;
        newJob->jobNumber = jobListTail->jobNumber + 1;
        jobListTail = newJob;
    }
    if(runningState == JOB_RUNNING_FOREGROUND) {
        foregroundJob = newJob;
    }
    return jobListTail->jobNumber;
}

pid_t GetJobByJobNumber(int jobNumber) {
    Job* job = jobListHead;
    while(job != NULL && job->jobNumber != jobNumber) {
        job = job->next;
    }

    if(job == NULL) {
        return -1;
    } else {
        return job->pid;
    }
}

pid_t GetForegroundJob() {
    if(foregroundJob != NULL) {
        return foregroundJob->pid;
    } else {
        return -1;
    }
}

pid_t SetJobRunningStateByJobNumber(int jobNumber, char runningState) {
    Job* job = jobListHead;
    while(job != NULL && job->jobNumber != jobNumber) {
        job = job->next;
    }

    if(job == NULL) {
        return -1;
    } else {
        if(job == foregroundJob) {
            foregroundJob = NULL;
        }
        job->runningState = runningState;
        if(runningState == JOB_RUNNING_FOREGROUND) {
            foregroundJob = job;
        }
        return job->pid;
    }
}

int SetJobRunningStateByPID(pid_t pid, char runningState) {
    Job* job = jobListHead;
    while(job != NULL && job->pid != pid) {
        job = job->next;
    }
    if(job == NULL) {
        return -1;
    } else {
        if(job == foregroundJob) {
            foregroundJob = NULL;
        }
        job->runningState = runningState;
        if(runningState == JOB_RUNNING_FOREGROUND) {
            foregroundJob = job;
        }
        return job->jobNumber;
    }
}

void FindAndPrintJobByPID(pid_t pid) {
    Job* job = jobListHead;
    while(job != NULL && job->pid != pid) {
        job = job->next;
    }
    if(job != NULL) {
        PrintJob(job->jobNumber, job->pid, job->runningState, job->commandName);
    }
}

void FindAndPrintJobByJobNumber(int jobNumber) {
    Job* job = jobListHead;
    while(job != NULL && job->jobNumber != jobNumber) {
        job = job->next;
    }
    if(job != NULL) {
        PrintJob(job->jobNumber, job->pid, job->runningState, job->commandName);
    }
}

void PrintPID(int jobNumber, pid_t pid) {
    printf("[%i] %i\n", jobNumber, pid);
}

void PrintJob(int jobNumber, pid_t pid, char runningState, const char* commandName) {
    printf("[%i]   ", jobNumber);
    switch(runningState) {
        case JOB_STOPPED:
            printf("Stopped");
            break;
        case JOB_RUNNING_BACKGROUND:
            printf("Running");
            break;
        case JOB_RUNNING_FOREGROUND:
            printf("Running (Foreground)");
            break;
        case JOB_BACKGROUND_DONE:
            printf("Done   ");
            break;
        default:
            printf("Unknown running state");
            break;
    }
    printf("                 %s\n", commandName);
}

static void RemoveJobFromList(Job* job, Job* previousJob) {
    if(job == jobListHead) {
        jobListHead = job->next;
    }
    if(job == jobListTail) {
        jobListTail = previousJob;
    }
    if(previousJob != NULL) {
        previousJob->next = job->next;
    }
    if(job == foregroundJob) {
        foregroundJob = NULL;
    }

    free(job->commandName);
    free(job);
}

pid_t RemoveJobByPID(pid_t pidToDelete) {
    Job* job = jobListHead;
    Job* previousJob = NULL;
    while(job != NULL && job->pid != pidToDelete) {
        previousJob = job;
        job = job->next;
    }
    if(job == NULL) {
        return -1;
    }
    RemoveJobFromList(job, previousJob);
    return pidToDelete;
}

pid_t RemoveJobByJobNumber(int jobNumberToDelete) {
    Job* job = jobListHead;
    Job* previousJob = NULL;
    while(job != NULL && job->jobNumber != jobNumberToDelete) {
        previousJob = job;
        job = job->next;
    }
    if(job == NULL) {
        return -1;
    }
    pid_t pidOfDeletedJob = job->pid;
    RemoveJobFromList(job, previousJob);
    return pidOfDeletedJob;
}



void PrintAllJobsAndRemoveDoneJobs() {
    Job* job = jobListHead;
    Job* previousJob = NULL;
    while(job != NULL) {
        PrintJob(job->jobNumber, job->pid, job->runningState, job->commandName);
        if(job->runningState == JOB_BACKGROUND_DONE) {
            Job* jobToDelete = job;
            job = job->next;
            RemoveJobFromList(jobToDelete, previousJob);
        } else {
            previousJob = job;
            job = job->next;
        }
    }
}

void PrintAndRemoveDoneJobs() {
    Job* job = jobListHead;
    Job* previousJob = NULL;
    while(job != NULL) {
        if(job->runningState == JOB_BACKGROUND_DONE) {
            PrintJob(job->jobNumber, job->pid, job->runningState, job->commandName);
            Job* jobToDelete = job;
            job = job->next;
            RemoveJobFromList(jobToDelete, previousJob);
        } else {
            previousJob = job;
            job = job->next;
        }
    }
}

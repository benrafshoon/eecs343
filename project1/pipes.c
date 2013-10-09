#define READ_END 0
#define WRITE_END 1

int[2] currentPipe;
int[2] previousPipe;
int lastCommand = numCommands - 1;
for(int i = 0; i < numCommands; i++) {
    if(i != lastCommand) {
        pipe(currentPipe);
    }
    pid_t pid = fork();
    if(pid == 0)  {
        //Child process
        if(i != 0) {
            dup2(previousPipe[READ_END], STDIN_FILENO);
            close(previousPipe[READ_END]);
        }
        if(i != lastCommand) {
            dup2(currentPipe[WRITE_END], STDOUT_FILENO);
            close(currentPipe[WRITE_END]);
        }
        exec(...);
    } else {
        if(i == lastCommand) {
            pid_t toWatch = pid;
            if(command[i]->bg) {
                AddJob(toWatch, JOB_RUNNING_BACKGROUND, command[i]->cmdline);
            } else {
                AddJob(toWatch, JOB_RUNNING_FOREGROUND, command[i]->cmdline);
                WaitForForegroundProcess();
            }
        }
    }
}

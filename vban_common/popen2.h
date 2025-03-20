#ifndef POPEN2_H
#define POPEN2_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#define READ 0
#define WRITE 1


static inline pid_t popen2(const char *command, int *infp, int *outfp)
{
    int p_stdin[2], p_stdout[2];
    pid_t pid;

    if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
        return -1;

    pid = fork();

    if (pid < 0)
        return pid;
    else if (pid == 0)
    {
        dup2(p_stdin[READ], STDIN_FILENO);
        dup2(p_stdout[WRITE], STDOUT_FILENO);

        //close unuse descriptors on child process.
        close(p_stdin[READ]);
        close(p_stdin[WRITE]);
        close(p_stdout[READ]);
        close(p_stdout[WRITE]);

        //can change to any exec* function family.
        execl("/bin/bash", "bash", "-c", command, NULL);
        perror("execl");
        exit(1);
    }

    // close unused descriptors on parent process.
    close(p_stdin[READ]);
    close(p_stdout[WRITE]);

    if (infp == NULL)
        close(p_stdin[WRITE]);
    else
        *infp = p_stdin[WRITE];

    if (outfp == NULL)
        close(p_stdout[READ]);
    else
        *outfp = p_stdout[READ];

    return pid;
}


static inline int pclose2(pid_t pid)
{
    int internal_stat;
    waitpid(pid, &internal_stat, 0);
    return WEXITSTATUS(internal_stat);
}


#endif // POPEN2_H

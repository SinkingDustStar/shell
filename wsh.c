
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>   // chdir, execvp
#include <sys/wait.h> // waitpid
#include <sys/types.h>
#include <stdint.h>
#include "wsh.h"

static jobObject *jobList[100]; // record jobs
static int maxjob = 0;
static pid_t child_pid = -1;
static char *parsed_line[100];
static int parsed_number;

void handle_signal_main(int sig)
{
    (void)sig;
    if (child_pid != -1)
    {

        kill(child_pid, SIGINT); // Terminate the child process
    }
    fflush(stdout);
}

void handle_signal_main2(int sig)
{
    (void)sig;
    // printf("handle_signal_main2\n");
    if (child_pid != -1)
    {
        setpgid(child_pid, child_pid);
        add_job(child_pid, parsed_line, parsed_number);
        // printf("receive ctrl + z, getpid = %d, childpid = %d, parsed_line = %s %s, maxjob = %d \n", getpid(), child_pid, parsed_line[0], parsed_line[1], maxjob);
        kill(child_pid, SIGTSTP); // Terminate the child process
    }
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    if (argc > 2)
    { // error
        exit(-1);
    }

    for (int i = 0; i < 100; i++)
    {
        jobList[i] = NULL;
    }

    FILE *input = stdin; // from user input or file, at frst set as interactive mode, user can type commands directly

    // read from line
    char *line = NULL;
    size_t line_len = 0;

    // Background Job
    pid_t pid;

    // check & and |
    bool isBackground = false;
    bool isPipe = false;
    char linePipe[256];

    // batch modes,  reads input from a batch file and executes commands from therein
    //  update file pointer
    if (argc == 2)
    {
        input = fopen(argv[1], "r");
        if (input == NULL)
        {
            exit(-1);
        }
    }

    while (1)
    {

        signal(SIGINT, handle_signal_main);
        signal(SIGTSTP, handle_signal_main2);

        if (input == stdin)
        {
            printf("wsh> ");
            // printf("%ld", (intmax_t)getpid());
        }
        if (getline(&line, &line_len, input) == -1)
        { // fail to read from line: EoF; input exit
            if (input != stdin)
            { // remember to close the file
                fclose(input);
            }
            break;
        }

        // strsep() would change ilne

        isBackground = false;
        isPipe = false;
        if (strstr(line, "&"))
        {
            isBackground = true;
        }
        if (strstr(line, "|"))
        {
            isPipe = true;
            strcpy(linePipe, line); //?strdup
        }

        // parse input
        parsed_number = parse(line, parsed_line);

        if (strcmp(parsed_line[0], "exit") == 0)
        {
            if (parsed_number == 1)
            {
                exit(0);
            }
            else
            {
                exit(-1);
            }
        }

        if (isBackground) // Background Job
        {
            parsed_line[parsed_number - 1] = NULL;
            parsed_number = parsed_number - 1;
            // printf("in background\n");
            setpgid(0, 0);
            linePipe[strlen(linePipe) - 2] = '\0';
            int index = add_job(getpid(), parsed_line, parsed_number);

            // printf("in back ground, %s \n", linePipe);

            // When you call execvp for any command, it replaces the shell itself,
            // and if the command completes successfully or fails, it will never return to your shell.
            if ((pid = fork()) == 0)
            { // run cmd in child
                if (isPipe)
                {
                    handle_pipe(linePipe, index);
                }
                else if (execvp(parsed_line[0], parsed_line) == -1)
                {
                    exit(-1);
                }
            }
            else if (pid < 0)
            {
                exit(-1);
            }
            else // parent don't wait there
            {
                tcsetpgrp(STDIN_FILENO, getpgrp());
            }
        }
        else if (isPipe) // use linePipe, as line is changed when parsing
        {                // Pipes:process is input argument; inout file descrioter
            handle_pipe(linePipe, 0);
        }
        else
        {
            exe_command(parsed_line, parsed_number);
        }
    }
    exit(0);
}

int parse(char *line, char **parsed_line)
{
    char *token;
    int i = 0;
    int j = 0;
    for (j = strlen(line) - 1; j >= 0; j--)
    {
        if (line[j] == ' ')
        {
            line[j] = '\0';
        }
        else
        {
            break;
        }
    }
    while ((token = strsep(&line, " ")) != NULL)
    {
        parsed_line[i] = token;
        i++;
    }
    // eliminate \n in the end
    if (parsed_line[i - 1][strlen(parsed_line[i - 1]) - 1] == '\n')
    {
        parsed_line[i - 1][strlen(parsed_line[i - 1]) - 1] = '\0';
    }
    // parsed_line should be NULL-terminated
    parsed_line[i] = NULL;
    return i;
}

void exe_command(char **parsed_line, int parsed_number)
{

    if (strcmp(parsed_line[0], "cd") == 0)
    {
        if (parsed_number == 2)
        {
            if (chdir(parsed_line[1]) != 0)
            {
                exit(-1);
            }
        }
        else
        {
            exit(-1);
        }
    }
    else if (strcmp(parsed_line[0], "jobs") == 0)
    {
        // printf("enter jobs ");
        handle_jobs();
        // printf("leave jobs ");~cs537-1/tests/P3/test-job-control.csh -v -c
    }
    else if (strcmp(parsed_line[0], "fg") == 0)
    {
        pid_t fg_pid;
        int i = 0;
        if (parsed_number == 2)
        {
            i = atoi(parsed_line[1]);
        }
        else if (parsed_number == 1)
        {
            pid_t return_pid;
            bool ismax = true;
            // printf("enter addjob ");
            for (int id = maxjob; id >= 1; id--)
            {
                if (jobList[id] != NULL)
                {
                    return_pid = kill(jobList[id]->jobpid, 0);
                    if (return_pid != 0)
                    { // it no longer exist, we should remove it from list
                        jobList[id] = NULL;
                        if (ismax)
                        {
                            ismax = false;
                            maxjob = id;
                        }
                    }
                }
            }
            i = maxjob;
        }
        else
        {
            exit(-1);
        }

        printf("0.pgid = %d\n", getpgrp());
        fg_pid = jobList[i]->jobpid;
        parsed_number = jobList[i]->argc;
        for (int j = 0; j < jobList[i]->argc; j++)
        {
            parsed_line[j] = strdup(jobList[i]->argv[j]);
        }
        jobList[i] = NULL;
        signal(SIGINT, handle_signal_main);
        signal(SIGTSTP, handle_signal_main2);
        printf("in fg, i = %d, current pid = %ld, fgpid = %ld , pgid of fgpid = %ld, pgid of current = %ld\n", i, (intmax_t)getpid(), (intmax_t)fg_pid, (intmax_t)getpgid(fg_pid), (intmax_t)getpgid(getpid()));
        tcsetpgrp(STDIN_FILENO, fg_pid); // don't change current pid, only set fg_pid as foreend
        printf("1.pgid = %ld, fgpid = %d\n", (intmax_t)getpgrp(), fg_pid);
        child_pid = fg_pid;

        kill(fg_pid, SIGCONT);
        // printf("2.pid = %ld\n", (intmax_t)getpid());
        // int status;
        waitpid(fg_pid, 0, WUNTRACED);
        // do {
        //     waitpid(fg_pid, &status, WUNTRACED);
        // } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));
        // add_job(fg_pid, parsed_line, parsed_number); // Add it to the job list

        // printf("3.pid = %ld\n", (intmax_t)getpid());
        signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpgrp());
        signal(SIGTTOU, SIG_DFL);
        // printf("4.pid = %ld\n", (intmax_t)getpid());
        child_pid = -1;
        // printf("in fg ");
    }
    else if (strcmp(parsed_line[0], "bg") == 0)
    {
        if (parsed_number == 2)
        {
            kill(jobList[atoi(parsed_line[1])]->jobpid, SIGCONT);
        }
        else if (parsed_number == 1)
        {
            pid_t return_pid;
            bool ismax = true;
            // printf("enter addjob ");
            for (int id = maxjob; id >= 1; id--)
            {
                if (jobList[id] != NULL)
                {
                    return_pid = kill(jobList[id]->jobpid, 0);
                    if (return_pid != 0)
                    { // it no longer exist, we should remove it from list
                        jobList[id] = NULL;
                        if (ismax)
                        {
                            ismax = false;
                            maxjob = id;
                        }
                    }
                }
            }
            // printf("in bg ");
            kill(jobList[maxjob]->jobpid, SIGCONT); // if pid is running, it will just continue running
        }
        else
        {
            exit(-1);
        }
    }
    else
    {

        pid_t pid;
        int status;
        if ((pid = fork()) == 0)
        { // run cmd in child
            // printf("process that execuating is %ld\n", (intmax_t)getpid());
            setpgid(0, 0);
            if (execvp(parsed_line[0], parsed_line) == -1)
            {
                exit(-1);
            }
        }
        else if (pid < 0)
        {
            exit(-1);
        }
        else
        {                                     // at this part, parent should wait
            child_pid = pid;                  // Update global variable with the child's process ID
            waitpid(pid, &status, WUNTRACED); // This will return if the child is stopped or terminated
            child_pid = -1;
            // waitpid(pid, &status, 0);
            // tcsetpgrp(STDIN_FILENO, getpgrp());
        }
    }
}

void handle_jobs()
{ // print out jobs, if it stops, just remove it
    pid_t return_pid;
    // printf("enter handle_jobs ");
    for (int i = 1; i <= maxjob; i++)
    {
        if (jobList[i] != NULL)
        {
            return_pid = kill(jobList[i]->jobpid, 0);
            // printf("in handle_job,getpid = %d,jobpid = %d, parsed_line = %s %s, maxjob = %d \n", getpid(), jobList[i]->jobpid, parsed_line[0], parsed_line[1], maxjob);

            if (return_pid != 0)
            { // it not exits
                free(jobList[i]->argv);
                free(jobList[i]);
                jobList[i] = NULL;
            }
            else if (return_pid == 0)
            { //  it's still existing, run or stop
                printf("%d:", i);
                for (int index = 0; index < jobList[i]->argc; index++)
                {
                    printf(" %s", jobList[i]->argv[index]);
                }
                printf("\n");
            }
            else
            {
                printf("d");
                exit(-1);
            }
        }
    }
    // printf("finish handle_jobs ");
}

int add_job(pid_t jobpid, char **argv, int argc)
{
    pid_t return_pid;
    bool ismax = true;

    // printf("in add_job,getpid = %d,jobpid = %d, parsed_line = %s %s, maxjob = %d , argc = %d\n", getpid(), jobpid, parsed_line[0], parsed_line[1], maxjob, argc);
    // if(jobList[1] == NULL){
    //     printf("jobList[1] == NULL \n");
    // }
    for (int id = maxjob; id >= 1; id--)
    {
        if (jobList[id] != NULL)
        {
            return_pid = kill(jobList[id]->jobpid, 0);
            if (return_pid != 0)
            { // it no longer exist, we should remove it from list
                free(jobList[id]->argv);
                free(jobList[id]);
                jobList[id] = NULL;
                if (ismax)
                {
                    ismax = false;
                    maxjob = id;
                }
            }
        }
    }
    int id = 1;
    for (; id <= maxjob + 1; id++)
    {
        if (jobList[id] == NULL)
        {
            // printf("add_job\n");
            jobList[id] = malloc(sizeof(jobObject));
            jobList[id]->argc = argc;
            jobList[id]->argv = malloc((argc + 1) * sizeof(char *));
            for (int i = 0; i < argc; i++)
            {
                jobList[id]->argv[i] = strdup(argv[i]);
            }
            jobList[id]->argv[argc] = NULL;
            jobList[id]->jobid = id;
            jobList[id]->jobpid = jobpid;
            maxjob = id;
            break;
        }
    }
    return id;
    // printf("leave addjob ");
}

void handle_pipe(char *linePipe, int ind)
{
    // printf("in pipe/n");
    char *token = NULL;
    int index = 0;
    pid_t pid;
    char *command[100];
    char *commands[100];
    int command_number = 0;
    while ((token = strsep(&linePipe, "|")) != NULL)
    {
        index = 0;
        while (token[index] == ' ')
        {
            index++;
        }
        commands[command_number++] = strdup(token + index);
    }

    int pipefd[command_number][2];
    for (int i = 0; i < command_number - 1; i++)
    {
        if (pipe(pipefd[i]) == -1)
        {
            exit(-1);
        }
    }

    for (int i = 0; i < command_number; i++)
    {

        parse(commands[i], command);
        // use a list of pipefd, hou STDIN_FILENO ->0, qian STDOUT_FILENO->1
        // close after set
        // close at last

        if ((pid = fork()) == 0)
        { // in child, write to pipe
            // printf("child\n");
            if (i != 0)
            { // not the first commad
                // redirect the standard input of the current process, read from the previous pipe
                dup2(pipefd[i - 1][0], STDIN_FILENO);
                close(pipefd[i - 1][0]);
                close(pipefd[i - 1][1]); // after redirecting, we no longer need the original file descriptor
            }
            if (i < command_number - 1) // if there are remaining commands, redirect it to write end
            {
                dup2(pipefd[i][1], STDOUT_FILENO); // then what execvp output would not be in terminal, instead, it would in the wrte end of pipe
                close(pipefd[i][1]);
            }

            // execuate command
            execvp(command[0], command);
        }
        else if (pid < 0)
        {
            exit(-1);
        }
        else // parent: read
        {
            wait(NULL); // wait for child
            close(pipefd[i][1]);
        }
    }
    jobList[ind] = NULL;
    for (int i = 0; i < command_number; i++)
    {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
        free(commands[i]); // free allocated memory
    }
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

int contains_pipe(char **, int);

int contains_redirect(char **, int);

static int curr_pid = 1; //static variable so we can tell what process is running or not. child pid for shell and background and 0 for child.

void mySignalHandler(int signo) {
    if (curr_pid > 0) {
        //if we are the shell we dont want to terminate
        return;
    } else if(curr_pid == 0) { //else we are a child and we want to terminate
        exit(1);
    }


}

void child_handler(int signo){
    if(curr_pid > 0)
        while ( waitpid(-1, NULL, WNOHANG) > 0 );
}

int prepare(void) {

    struct sigaction newAction = {
            .sa_handler = mySignalHandler,
            .sa_flags = SA_RESTART
    };
    if (sigaction(SIGINT, &newAction, NULL) == -1) {
        perror("Signal handle registration failed");
        exit(EXIT_FAILURE);
    }
    struct sigaction child = {
            .sa_handler = child_handler,
            .sa_flags = SA_RESTART
    };
    if(sigaction(SIGCHLD, &child, NULL)){
        perror("Signal handle registration failed");
        exit(EXIT_FAILURE);
    }

    return 0;
}

int finalize(void) {
    return 0;
}

int process_arglist(int count, char **arglist) {

    int pipe_index = contains_pipe(arglist, count);
    int redirect_index = contains_redirect(arglist, count);
    int status;

    if (strcmp(arglist[count - 1], "&") == 0) { // handle background

        arglist[count - 1] = NULL; // get rid of ampersand
        int pid = fork();
        if (pid == -1) {
            perror("Failed forking!");
            return 0;
        }
        if (pid == 0) // child code
        {
            curr_pid = 1;
            if(execvp(arglist[0], arglist) == -1){
                perror("execvp failed!");
            }
            exit(1);
        } else { // parent code

            return 1;
        }
    }

    else if (pipe_index != -1) { // handle pipe

        int pid1, pid2;
        int pfds[2];
        char **args1;
        char **args2;
        args1 = arglist; // get the args before the |
        args1[pipe_index] = NULL;
        args2 = arglist + (pipe_index + 1); // get the args after the |

        if (pipe(pfds) == -1) {
            perror("Failed piping!");
            return 0;
        }
        pid1 = fork();
        if (pid1 == -1) {
            perror("Failed forking!");
            return 0;
        }
        if (pid1 > 0) { // parent code
            curr_pid = pid1;


            pid2 = fork();
            if (pid2 == -1) {
                perror("Failed forking!");
                return 0;
            }
            if (pid2 == 0) { // child2 code - reads from pipe
                curr_pid = pid2;
                close(pfds[1]);
                int dup_exit = dup2(pfds[0], STDIN_FILENO);
                close(pfds[0]);
                if (dup_exit == -1) {
                    perror("dup error");
                    exit(1);
                }
                if(execvp(args2[0], args2) == -1){
                    perror("execvp failed!");
                    exit(1);
                };
            }

            close(pfds[0]);
            close(pfds[1]);
            while ((waitpid(pid1, &status, 0) == -1) &&
                   (errno == ECHILD || errno == EINTR)); // wait for child1 to write
            while ((waitpid(pid2, &status, 0) == -1) && (errno == ECHILD || errno == EINTR));

        } else { // child1 code - writes to pipe
            curr_pid = pid1;
            close(pfds[0]);
            int dup_exit = dup2(pfds[1], STDOUT_FILENO);
            close(pfds[1]);
            if (dup_exit == -1) {
                perror("dup2 error!");
                exit(1);
            }
            if(execvp(args1[0], args1) == -1){
                perror("execvp failed!");
                exit(1);
            }
        }

        return 1;
    }

    else if (redirect_index != -1) { // handle redirect
        char **args1 = arglist;
        char *filename = arglist[redirect_index + 1];
        arglist[redirect_index] = NULL;
        int fd = open(filename, O_RDONLY);
        if (fd == -1) {
            perror("failed opening file!");
            return 0;
        }
        int pid = fork();
        if (pid == -1) {
            perror("Failed forking!");
            return 0;
        }
        if (pid > 0) {
            while ((waitpid(pid, &status, 0) == -1) &&
                   (errno == ECHILD || errno == EINTR)); //wait for the child to finish
            if (close(fd) == -1) {
                perror("error closing file");
                return 0;
            }
            return 1;
        } else {
            curr_pid = pid;
            int dup_exit = dup2(fd, STDIN_FILENO);
            if (dup_exit == -1) {
                perror("dup2 error!");
                return 0;
            }
            if(execvp(args1[0], args1) == -1){
                perror("execvp failed!");
                exit(1);
            }
        }
    }
    else{ // handle basic command
        int pid = fork();

        if (pid == -1) {
            perror("Failed forking!");
            return 0;
        }
        if (pid == 0) // child code
        {
            curr_pid = 0;
            if(execvp(arglist[0], arglist) == -1){
                perror("execvp failed!");
                exit(1);
            }
        } else { // parent code
            while ((waitpid(pid, &status, 0) == -1) && (errno == ECHILD || errno == EINTR));
        }
    }

    return 1;
}

int contains_pipe(char **arglist, int count) {
    int i;
    for (i = 0; i < count; i++) {
        if (strcmp(arglist[i], "|") == 0)
            return i;
    }
    return -1;
}

int contains_redirect(char **arglist, int count) {
    int i;
    for (i = 0; i < count; i++) {
        if (strcmp(arglist[i], "<") == 0)
            return i;
    }
    return -1;
}

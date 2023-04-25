#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int contains_pipe(char **, int);

int contains_redirect(char **, int);

int prepare(void) {
    return 0;
}

int process_arglist(int count, char **arglist) {

    int index;
    int status;


    if (strcmp(arglist[count - 1], "&") == 0) { //handle background

        arglist[count - 1] = NULL; //get rid of ampersand
        int pid = fork();
        if (pid == -1) {
            perror("Failed forking!");
            return 0;
        }
        if (pid == 0) //son code
        {
            execvp(arglist[0], arglist);
            exit(1);
        }
        else{ //parent code
            return 1;
        }
    }

    else if ((index = contains_pipe(arglist, count)) != -1) { //handle pipe

        int pid1, pid2;
        int pfds[2];
        int status;
        char **args1;
        char **args2;
        args1 = arglist; //get the args before the |
        args1[index] = NULL;
        args2 = arglist + ((index + 1) * sizeof(char *)); //get the args after the |

        if (pipe(pfds) == -1) {
            perror("Failed piping!");
            return 0;
        }

        pid1 = fork();
        if (pid1 == -1) {
            perror("Failed forking!");
            return 0;
        }
        if (pid1 > 0) {//parent code
            while((waitpid(pid1, &status, 0) == -1) && (errno == ECHILD || errno == EINTR)); // wait for child1 to write
            pid2 = fork();
            if (pid2 == -1) {
                perror("Failed forking!");
                return 0;
            }
            if (pid2 == 0) {//child2 code - reads from pipe
                close(pfds[1]);
                int dup_exit = dup2(pfds[0], STDIN_FILENO);
                if (dup_exit == -1) {
                    perror("dup error");
                    exit(1);
                }
                execvp(args2[0], args2);
            }



        }
        else {//child1 code - writes to pipe
            close(pfds[0]);
            dup2( pfds[1], STDOUT_FILENO);
            execvp(args1[0], args1);
        }

        while((waitpid(pid2, &status, 0) == -1) && (errno == ECHILD || errno == EINTR));
        close(pfds[0]);
        close(pfds[1]);




    }

    else if ((index = contains_redirect(arglist, count)) != -1) { //handle redirect


    }

    else { //handle basic command
        int pid = fork();


        if (pid == -1) {
            perror("Failed forking!");
            return 0;
        }
        if (pid == 0) //child code
        {
            execvp(arglist[0], arglist);
        }
        else{ //parent code
                while((waitpid(pid, &status, 0) == -1) && (errno == ECHILD || errno == EINTR));
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

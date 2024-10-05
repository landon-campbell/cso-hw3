#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>


char *getoutput(const char *command){

    // instantiate pipe
    int pipefd[2];
    
    // check to make sure pipe succeeded, if not, return NULL
    if (pipe(pipefd) == -1){
        return NULL;
    }

    // fork and create parent process and child process
    pid_t pid = fork();

    // check to make sure fork succeeded, if not, close pipe, close fds, and return NULL
    if (pid == -1){
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    // if child process
    if (pid == 0){
        
        // set STDOUT to write to pipe file descripter index 1 (write end) and check to make sure dup2 succeeded, if not, exit
        if (dup2(pipefd[1], STDOUT_FILENO) == -1){
            _exit(127);
        }
        // close both pipe file descriptors
        close(pipefd[0]);
        close(pipefd[1]);
        // execl as done in my_system.c
        execl("/bin/sh", "sh", "-c", command, (char *) NULL);
        _exit(127);
    }

    // else, parent process
    else {
        // close write end of pipe since only child needs it
        close(pipefd[1]);

        // read all contents from the read end of the pipe, mallocing enough space to store it all.
        // create buffer, define buffer as malloc
        char *buffer = NULL;
        size_t buff_size = 1024;
        size_t tot_size = 0;
        buffer = malloc(buff_size);

        // make sure malloc succeeded
        if (buffer == NULL){
            // close pipe, then return
            close(pipefd[0]);
            waitpid(pid, NULL, 0);
            return NULL;
        }

        // handle a read-in while dynamically adjusting buffer size, double buffer size each time capacity is reached
        ssize_t num_r;
        while ((num_r = read(pipefd[0], buffer + tot_size, buff_size - tot_size)) > 0) {
            tot_size += num_r;
            if (tot_size >= buff_size) {
                buff_size *= 2;
                char *new_buff = realloc(buffer, buff_size);

                // check to make sure realloc succeeded
                if (new_buff == NULL){
                    free(buffer);
                    close(pipefd[0]);
                    waitpid(pid, NULL, 0);
                    return NULL;
                }
                buffer = new_buff;
            }
        }

        // close pipe and wait for child process to finish
        close(pipefd[0]);
        waitpid(pid, NULL, 0);

        // null term buffer
        buffer[tot_size] = '\0';

        // return buffer
        return buffer;
    }
}

char *parallelgetoutput(int count, const char **argv_base) {

    // instantiate pipe write end
    int pipefd[2];

    // check to make sure pipe succeeded, if not, return NULL
    if (pipe(pipefd) == -1){
        return NULL;
    }

    // create pointer to array of child pids and malloc space for it
    pid_t *pids = malloc(count * sizeof(pid_t));

    // check to make sure malloc succeeded, if not, close pipe and return NULL
    if (pids == NULL){
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    // iterate through count, forking and creating child processes
    for (int i = 0; i < count; i++){
        pid_t pid = fork();

        // check to make sure fork succeeded, if not, close pipe, close fds, and return NULL
        if (pid == -1){
            for (int j = 0; j < i; j++){
                waitpid(pids[j], NULL, 0);
            }
            free(pids);
            close(pipefd[0]);
            close(pipefd[1]);
            return NULL;
        }

        // if child process
        if (pid == 0){

            int argv_length = 0;
            // iterate through argv_base to get length
            while (argv_base[argv_length] != NULL){
                argv_length++;
            }

            // create new argv array new idx and malloc space for it
            char **argv = malloc(sizeof(char *) * (argv_length + 2));
            // check to make sure malloc succeeded, if not, exit
            if (argv == NULL){
                _exit(127);
            }

            for (int j = 0; j < argv_length; j++){
                argv[j] = (char *)argv_base[j];
            }

            char idx_string[10];
            snprintf(idx_string, sizeof(idx_string), "%d", i);
            argv[argv_length] = idx_string;
            argv[argv_length + 1] = NULL;

            // set STDOUT to write to pipe file descripter index 1 (write end) and check to make sure dup2 succeeded, if not, exit
            if (dup2(pipefd[1], STDOUT_FILENO) == -1){
                _exit(127);
            }
            // close both pipe file descriptors
            close(pipefd[0]);
            close(pipefd[1]);

            // exec, but differen than getoutput
            execv(argv_base[0], argv);
            _exit(127);
        }

        // else, parent process
        else {
            // store pid in pids array
            pids[i] = pid;
        }
    }

    // close write end of pipe since only child needs it
    close(pipefd[1]);

    // create buffer, define buffer as malloc
    char *buffer = NULL;
    size_t buff_size = 1024;
    size_t tot_size = 0;
    buffer = malloc(buff_size);

    // make sure malloc succeeded
    if (buffer == NULL){
        // close pipe, then return
        close(pipefd[0]);
        for (int i = 0; i < count; i++){
            waitpid(pids[i], NULL, 0);
        }
        free(pids);
        return NULL;
    }

    // handle a read-in while dynamically adjusting buffer size, double buffer size each time capacity is reached
    ssize_t num_r;
    while ((num_r = read(pipefd[0], buffer + tot_size, buff_size - tot_size)) > 0) {
        tot_size += num_r;
        if (tot_size >= buff_size) {
            buff_size *= 2;
            char *new_buff = realloc(buffer, buff_size);

            // check to make sure realloc succeeded
            if (new_buff == NULL){
                free(buffer);
                close(pipefd[0]);
                for (int i = 0; i < count; i++){
                    waitpid(pids[i], NULL, 0);
                }
                free(pids);
                return NULL;
            }
            buffer = new_buff;
        }
    }

    // close pipe and wait for child process to finish
    close(pipefd[0]);
    for (int i = 0; i < count; i++){
        waitpid(pids[i], NULL, 0);
    }

    // null term buffer
    buffer[tot_size] = '\0';

    // free pids and return buffer
    free(pids);
    return buffer;
}

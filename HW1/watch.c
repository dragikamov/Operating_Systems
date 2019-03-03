/**
 * @author Dragi Kamov
 * 
 * Assignment 1
 * Problem 2
 * Course: Operating Systems
 * Professor: Jurgen Schoenwaelder
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <getopt.h>


int watch(int argc, char* argv[], int s_time, int fail_stop, int special_value)
{
    while(1)
    {
        /* Creating a process */
        pid_t pid = fork();

        /* Checking if the creation was successful */
        if(pid < 0)
        {
            perror("Error creating a child process. \n");
            exit(1);
        }
        /* If it is a new process */
        else if(pid == 0)
        {
            char *new_argv[argc - optind + 1];
            for(int i = optind; i < argc; i++)
            {
                new_argv[i - optind] = argv[i];
            }
            new_argv[argc-optind] = '\0';

            /* Executing the commands and checking if they crash */
            if(execvp(new_argv[0], new_argv) == -1)
            {
                if(special_value)
                {
                    perror("a\n");
                    perror("Error with special value case.\n");
                }
                else
                {
                    perror("Error!\n");
                }
                exit(1);
            }
            exit(0);
        }
        /* The waiting process */
        else
        {
            int wait_process;
            (void) waitpid(-1, &wait_process, WUNTRACED);

            if(fail_stop && wait_process)
            {
                exit(1);
            }
            (void) sleep(s_time);
        }
    }
    return 1;
}

int main(int argc, char* argv[])
{
    int s_time = 2;
    int n, fail_stop = 0;
    int special_value = 0;

    /* Processing all the options */
    while((n = getopt(argc, argv, "+n:eb")) != -1)
    {
        if(n == 'n')
        {
            s_time = atoi(optarg);
            if(s_time <= 0)
            {
                perror("Error, invalid time value.\n");
                exit(1);
            }
        }
        else if(n == 'b')
        {
            special_value = 1;
        }
        else if(n == 'e')
        {
            fail_stop = 1;
        }
        else
        {
            perror("Error, invalid option.\n");
            exit(1);
        }
    }

    /* Running the watch function and checking if it crashes */
    if(watch(argc, argv, s_time, fail_stop, special_value))
    {
        perror("Error while running watch.\n");
        exit(1);
    }

    return 0;
}
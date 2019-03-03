/**
 * @author Dragi Kamov
 * 
 * Assignment 2
 * Problem 1
 * Course: Operating Systems
 * Professor: Jurgen Schoenwaelder
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <time.h>

int persons = 100;
int n = 10000;
char coins[] = {'O','O','O','O','O','O','O','O','O','O','X','X','X','X','X','X','X','X','X','X'};
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Got this function from the course source codes and then adjusted it */
static int dflag = 0; //unused
static const char *progname = "pthread";
void run_threads(unsigned int nn, void* (*proc)(void *))
{
    pthread_t *thread;
    int rc;

    thread = calloc(nn, sizeof(pthread_t));
    if (!thread) 
    {
        fprintf(stderr, "%s: %s: %s\n", progname, __func__, strerror(errno));
        exit(1);
    }

    for (int i = 0; i < nn; i++) 
    {
        rc = pthread_create(&thread[i], NULL, proc, NULL);
        if (rc) 
        {
            fprintf(stderr, "%s: %s: unable to create thread %d: %s\n",
                    progname, __func__, i, strerror(rc));
        }
    }

    for (int i = 0; i < nn; i++) 
    {
        if (thread[i]) 
        {
            (void) pthread_join(thread[i], NULL);
	    }
    }

    (void) free(thread);
}

/* Got this function from the problem sheet */
static double timeit(int n, void* (*proc)(void *))
{
    clock_t t1, t2;
    t1 = clock();
    run_threads(n, proc);
    t2 = clock();
    return ((double) t2 - (double) t1) / CLOCKS_PER_SEC * 1000;
}

/* Function taking the current state of the coin and returning the opposite */
char flip(char c)
{
    if(c == 'O')
    {
        return 'X';
    }
    else
    {
        return 'O';
    }
}

/* This is the function for the first scenario */
static void *f1(void *data)
{
    pthread_mutex_lock(&mutex);
    for(int i = 0; i < n; i++)
    {
        for(int j = 0; j < 20; j++)
        {
            coins[j] = flip(coins[j]);
        }
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}

/* This is the function for the second scenario */
static void *f2(void *data)
{
    for(int i = 0; i < n; i++)
    {
        pthread_mutex_lock(&mutex);
        for(int j = 0; j < 20; j++)
        {
            coins[j] = flip(coins[j]);
        }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

/* This is the function for the third scenario */
static void *f3(void *data)
{
    for(int i = 0; i < n; i++)
    {
        for(int j = 0; j < 20; j++)
        {
            pthread_mutex_lock(&mutex);
            coins[j] = flip(coins[j]);
            pthread_mutex_unlock(&mutex);
        }
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    int i;

    /* Getting the command line options */
    while((i = getopt(argc, argv, "p:n:")) != -1)
    {
        if(i == 'p')
        {
            int persons_temp;
            persons_temp = atoi(optarg);
            if(persons_temp <= 0)
            {
                perror("Error, invalid number of people.\n");
                exit(1);
            }
            persons = persons_temp;
        }
        else if(i == 'n')
        {
            int n_temp;
            n_temp = atoi(optarg);
            if(n_temp <= 0)
            {
                perror("Error, invalid number of flips.\n");
                exit(1);
            }
            n = n_temp;
        }
        else
        {
            perror("Error, invalid command.\n");
            exit(1);
        }
    }
    double t1, t2, t3;

    /* Running the scenarios and printing the results */
    printf("coins: %s (start - global lock)\n", coins);
    t1 = timeit(persons, f1);
    printf("coins: %s (end - global lock)\n", coins);
    printf("%d threads x %d flips: %.3lf ms\n\n", persons, n, t1);

    printf("coins: %s (start - iteration lock)\n", coins);
    t2 = timeit(persons, f2);
    printf("coins: %s (end - table lock)\n", coins);
    printf("%d threads x %d flips: %.3lf ms\n\n", persons, n, t2);

    printf("coins: %s (start - coin lock)\n", coins);
    t3 = timeit(persons, f3);
    printf("coins: %s (end - coin lock)\n", coins);
    printf("%d threads x %d flips: %.3lf ms\n\n", persons, n, t3);

    return 0;
}
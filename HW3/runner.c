/*
 * p3-runner/runner-template.c --
 *
 *      Runners meeting regularly (if they manage). See Operating
 *      Systems '2018 assignment #3 for more details.
 */

// #define _REENTRANT commented because of a warning
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>

#define LATE_THRESHOLD          5
#define LONELY_THRESHOLD        10

#define GROUP_SLEEPING          0x01
#define GROUP_ASSEMBLING        0x02
#define GROUP_RUNNING           0x03
#define GROUP_FINISHING         0x04

#define RUNNER_SLEEPING         0x01
#define RUNNER_LEADING          0x02
#define RUNNER_ASSEMBLING       0x03
#define RUNNER_RUNNING          0x04
#define RUNNER_WAITING          0x05

typedef struct group {
    unsigned        state;      /* the state of the running group */
    unsigned        arriving;   /* number of runnners arriving */
    unsigned        running;    /* number of runnners running */
} group_t;

typedef struct runner {
    unsigned        id;         /* identity of the runner */
    pthread_t       tid;        /* thread identifier */
    unsigned        state;      /* state of the runner */
    unsigned        late;       /* number of runs missed (late arrival) */
    unsigned        lonely;     /* number of runs without any other runners */
    unsigned        runs;       /* number of runs completed */
    group_t        *group;      /* the group this runner belongs to */
} runner_t;

static const char *progname = "runner";

static pthread_mutex_t group_mut;
static pthread_cond_t runners;

static void*
runners_life(void *data)
{
    runner_t *runner = (runner_t *) data;
    group_t *group = runner->group;
    
    assert(runner && runner->group);
        
    while (runner->late < LATE_THRESHOLD
           && runner->lonely < LONELY_THRESHOLD) {

        runner->state = RUNNER_SLEEPING;
        (void) fprintf(stderr, "r%d sleeping\n", runner->id);

        usleep(172000+random()%10000);

        /* Locking the mutex while checking the states of the group */
        pthread_mutex_lock(&group_mut);
        if(group->state == GROUP_RUNNING || group->state == GROUP_FINISHING)
        {
            pthread_mutex_unlock(&group_mut);
            runner->late++;
            continue;
        }
        if(group->arriving == 0)
        {
            group->state = GROUP_ASSEMBLING;
            runner->state = RUNNER_LEADING;
            group->running++;
        }
        else
        {
            runner->state = RUNNER_ASSEMBLING;
            group->running++;
        }
        pthread_mutex_unlock(&group_mut);

        /* The session leader waiting for some time for additional 
        runners to arrive and join the session */
        if(runner->state == RUNNER_ASSEMBLING)
        {
            /* Waiting for someone to show up */
            pthread_mutex_lock(&group_mut);
            while(group->state != GROUP_RUNNING)
            {
                pthread_cond_wait(&runners, &group_mut);
            }
            pthread_mutex_unlock(&group_mut);
        }
        else
        {
            usleep(3600+random()%100);
            pthread_mutex_lock(&group_mut);
            /* Checking if someone actually came */
            if(group->running == 1)
            {
                runner->lonely++;
            }
            group->state = GROUP_RUNNING;
            pthread_mutex_unlock(&group_mut);
            pthread_cond_broadcast(&runners);
        }
        runner->state = RUNNER_RUNNING;
        
        pthread_mutex_lock(&group_mut);
        /* If more people are running it has to wait for everyone to finish */
        if(group->running > 1)
        {
            group->arriving++;
            group->running--;
            while(group->state != GROUP_FINISHING)
            {
                pthread_cond_wait(&runners, &group_mut);
            }
            pthread_mutex_unlock(&group_mut);
        }
        else
        {
            group->arriving++;
            group->running--;
            group->state = GROUP_FINISHING;
            pthread_mutex_unlock(&group_mut);
            pthread_cond_broadcast(&runners);
        }

        pthread_mutex_lock(&group_mut);
        group->arriving--;
        /* After everyone arrives back we can put them to sleep */
        if(group->arriving == 0)
        {
            group->state = GROUP_SLEEPING;
        }
        pthread_mutex_unlock(&group_mut);
        runner->state = RUNNER_WAITING;
        runner->runs++;
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int c, n = 1;
    runner_t *runner = NULL;
    int rc = EXIT_SUCCESS;

    group_t group = {
        .state      = GROUP_SLEEPING,
        .arriving   = 0,
        .running    = 0,
    };

    while ((c = getopt(argc, argv, "n:h")) >= 0) 
    {
        switch (c) 
        {
        case 'n':
            if ((n = atoi(optarg)) <= 0) 
            {
                (void) fprintf(stderr, "number of runners must be > 0\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'h':
            (void) printf("Usage: %s [-n runners] [-h]\n", progname);
            exit(EXIT_SUCCESS);
        }
    }

    runner = calloc(n, sizeof(runner_t));
    if (! runner) 
    {
        (void) fprintf(stderr, "%s: calloc() failed\n", progname);
        rc = EXIT_FAILURE;
        goto cleanup;
    }

    for (int i = 0; i < n; i++) 
    {
        runner[i].id = i;
        runner[i].state = RUNNER_SLEEPING;
        runner[i].late = 0;
        runner[i].runs = 0;
        runner[i].group = &group;
        
        /* Creating thread for runner[i] */
        pthread_create(&runner[i].tid, NULL, runners_life, &runner[i]);
        
    }

    for (int i = 0; i < n; i++) 
    {
        /* Joining thread for runner[i] */
        pthread_join(runner[i].tid, NULL);
    }

    for (int i = 0; i < n; i++) 
    {
        (void) printf("r%d stopped after %3d run(s): "
                      "%3d run(s) missed, %3d run(s) alone\n",
                      runner[i].id, runner[i].runs,
                      runner[i].late, runner[i].lonely);
    }

cleanup:

    if (runner) { (void) free(runner); }
    
    return rc;
}
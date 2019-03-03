/*
* Server/client
*/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>

#include "tcp.h"
#include <event2/event.h>

typedef struct game_status
{
    int total_guesses;
    int correct_guesses;
    char current_word[33];
    char current_missing_word[33];
} game_status;

typedef struct node
{
    int cfd;
    game_status *status;
    struct event *cev;
    struct node *next;
} tcp_node;

tcp_node *add_new_tcp_node(tcp_node **head, int cfd)
{
    tcp_node *current_node = NULL;
    game_status *current_game_status = NULL;

    if (*head == NULL)
    {
        *head = (tcp_node *)malloc(sizeof(tcp_node));
        (*head)->next = NULL;
        current_node = *head;
        goto reset_node_info;
    }
    else
    {
        current_node = *head;
        while (current_node->next != NULL)
        {
            current_node = current_node->next;
        }
        current_node->next = (tcp_node *)malloc(sizeof(tcp_node));
        current_node = current_node->next;
        current_node->next = NULL;
    }

reset_node_info:
    current_node->cfd = cfd;
    current_node->cev = NULL;
    current_node->status = (game_status *)malloc(sizeof(game_status));
    current_game_status = current_node->status;

    current_game_status->total_guesses = 0;
    current_game_status->correct_guesses = 0;
    current_game_status->current_word[0] = '\0';
    current_game_status->current_missing_word[0] = '\0';

    return current_node;
}

void remove_tcp_node(tcp_node **head, int cfd)
{
    tcp_node *current_node = *head;
    if (current_node->cfd == cfd)
    {
        *head = (*head)->next;
        event_free(current_node->cev);
        free(current_node);
        return;
    }

    while (current_node->next != NULL)
    {
        if (current_node->next->cfd == cfd)
        {
            tcp_node *back_up = current_node->next;
            current_node->next = current_node->next->next;
            event_free(current_node->cev);
            free(back_up);
            break;
        }
        current_node = current_node->next;
    }
}

/* Global variables */

static const char *progname = "game_server";
static struct event_base *evb;
static tcp_node *connections_list = NULL;
typedef enum
{
    OK,
    CHALLENGE,
    GENERIC,
    UNKNOWN,
    FAIL
} reply_type;

/* end global variables */

/* region callbacks and helpers */

static void send_to_client(tcp_node *tcp_connection, reply_type rt, char *generic_msg)
{
    char message[200];
    switch (rt)
    {
    case CHALLENGE:
        sprintf(message, "C: %s", tcp_connection->status->current_word);
        break;
    case OK:
        strcpy(message, "O: Congratulations - challenge passed!\n");
        break;
    case FAIL:
        sprintf(message, "F: Wrong guess - expected: %s\n", tcp_connection->status->current_missing_word);
        break;
    case GENERIC:
        sprintf(message, "M: %s", generic_msg);
        break;
    case UNKNOWN:
    default:
        strcpy(message, "M: unrecognized input from client \n");
    }

    tcp_write(tcp_connection->cfd, message, strlen(message));
}

static void underscore_word(game_status *status)
{
    int count, word_no;
    char *token, *position;
    char game[33];

    // POSIX whitespace characters and other interpunction characters
    char delimit[] = " \t\r\n\v\f.,;!~`_-";
    
    // count number of tokens
    position = status->current_word;
    while ((position = strpbrk(position, delimit)) != NULL)
    {
        position++;
        count++;
    }

hide_word:
    // initializing randomizer to choose a word
    srand((unsigned int)time(NULL));
    word_no = rand() % count;
    count = 0;

    // going word by word through a phrase
    strcpy(game, status->current_word);
    token = strtok(game, delimit);
    while (token)
    {
        if (count == word_no)
        {
            // this word gets removed
            position = strstr(status->current_word, token);
            if (!position)
            {
                // somehow an error occured so we need to start again
                goto hide_word;
            }
            memset(position, '_', strlen(token));
            strcpy(status->current_missing_word, token);
            break;
        }
        count++;
        token = strtok(NULL, delimit);
    }
    if (token == NULL)
    {
        // this means that no word was chosen
        goto hide_word;
    }
}

static void read_game(evutil_socket_t evfd, short evwhat, void *evarg)
{
    int nbytes, pipe_fd = evfd;
    tcp_node *current_connection = (tcp_node *)evarg;

    // reading from the pipe and storing in the buffer
    nbytes = read(pipe_fd, current_connection->status->current_word, 32);
    if (nbytes == -1)
    {
        syslog(LOG_ERR, "failed to read from game");
        current_connection->status->current_word[0] = '\0';
    }
    else
    {
        current_connection->status->current_word[nbytes] = '\0';
    }

    // remove word and send the challenge back
    underscore_word(current_connection->status);
    send_to_client(current_connection, CHALLENGE, NULL);
}

static void get_game(tcp_node *tcp_connection)
{
    int pipe_fd[2];
    struct event *fev;
    pid_t child;

    pipe(pipe_fd);
    fev = event_new(evb, pipe_fd[0], EV_READ, read_game, tcp_connection);
    event_add(fev, NULL);

    child = fork();
    if (child == -1)
    {
        syslog(LOG_ERR, "failed to fork process");
        tcp_connection->status->current_word[0] = '\0';
    }
    else if (child == 0)
    {
        // redirecting child's output to the pipe
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);

        execlp("game", "game", "-n", "32", "-s", NULL);
        syslog(LOG_ERR, "failed to start game");
        exit(EXIT_FAILURE);
    }
    else
    {
        // reading from the child's output
        close(pipe_fd[1]);
    }
}


static void read_from_client(evutil_socket_t evfd, short evwhat, void *evarg)
{
    tcp_node *current_connection = (tcp_node *)evarg;
    int nbytes, cfd = evfd;
    char message[200], *token;

    // POSIX whitespace characters
    char delimit[] = " \t\r\n\v\f";

    // reading from the client
    nbytes = read(cfd, message, sizeof(message));
    if (nbytes < 0)
    {
        syslog(LOG_ERR, "error reading from client");
        tcp_close(cfd);
        remove_tcp_node(&connections_list, cfd);
        return;
    }

    // figuring out the type of message
    if (strstr(message, "Q:") == message)
    {
        // closing connection
        sprintf(message, "You mastered %d/%d challenges. Good bye!\n",
                current_connection->status->correct_guesses,
                current_connection->status->total_guesses);
        send_to_client(current_connection, GENERIC, message);
        tcp_close(cfd);
        remove_tcp_node(&connections_list, cfd);
    }
    else
    {
        if (strstr(message, "R:") == message)
        {
            current_connection->status->total_guesses++;
            memmove(message, message + 2, strlen(message) - 2);
            token = strtok(message, delimit);
            if (token == NULL || strcmp(token, current_connection->status->current_missing_word) != 0)
            {
                send_to_client(current_connection, FAIL, NULL);
            }
            else
            {
                current_connection->status->correct_guesses++;
                send_to_client(current_connection, OK, NULL);
            }
            // sending out new challenge
            get_game(current_connection);
        }
        else
        {
            send_to_client(current_connection, GENERIC, "Command unrecognized. Please try again!\n");
        }
    }
}

static void new_connection_callback(evutil_socket_t evfd, short evwhat, void *evarg)
{
    size_t n;
    int cfd;
    char welcome_msg[200];
    tcp_node *new_connection;

    // accepting and opening a TCP connection
    cfd = tcp_accept((int)evfd);
    if (cfd == -1)
    {
        return;
    }

    // sending welcome message
    strcpy(welcome_msg, "M: Guess the missing ____!\n");
    strcat(welcome_msg, "M: Send your guess in the form ’R: word\\r\\n’.\n");

    n = tcp_write(cfd, welcome_msg, strlen(welcome_msg));
    if (n != strlen(welcome_msg))
    {
        syslog(LOG_ERR, "write failed");
        tcp_close(cfd);
    }

    // storing connection file descriptor in linked list
    new_connection = add_new_tcp_node(&connections_list, cfd);

    // reacting to client's input
    new_connection->cev = event_new(evb, cfd, EV_READ | EV_PERSIST, read_from_client, new_connection);
    event_add(new_connection->cev, NULL);

    // outputting
    get_game(new_connection);
}

int main(int argc, char **argv)
{
    int tfd;
    struct event *tev;
    const char *interfaces[] = {"0.0.0.0", "::", NULL};

    // check for arguments
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s port\n", progname);
        exit(EXIT_FAILURE);
    }

    // getting child status codes
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
    {
        fprintf(stderr, "%s: signal() %s \n", progname, strerror(errno));
        return EXIT_FAILURE;
    }

    // daemon-ize this process
    (void)daemon(0, 0);

    // creating the event base
    openlog(progname, LOG_PID, LOG_DAEMON);
    evb = event_base_new();
    if (!evb)
    {
        syslog(LOG_ERR, "creating event base failed");
        return EXIT_FAILURE;
    }

    // registering events
    for (int i = 0; interfaces[i]; i++)
    {
        tfd = tcp_listen(interfaces[i], argv[1]);
        if (tfd > -1)
        {
            tev = event_new(evb, tfd, EV_READ | EV_PERSIST, new_connection_callback, NULL);
            event_add(tev, NULL);
        }
    }

    // reaction to these events happening
    if (event_base_loop(evb, 0) == -1)
    {
        syslog(LOG_ERR, "event loop failed");
        event_base_free(evb);
        return EXIT_FAILURE;
    }

    // successful termination of the program
    closelog();
    event_base_free(evb);

    return EXIT_SUCCESS;
}
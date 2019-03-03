/**
 * @author Dragi Kamov
 * 
 * Assignment 1
 * Problem 1c
 * Course: Operating Systems
 * Professor: Jurgen Schoenwaelder
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sendfile.h>

#define SYS_CALL 0
#define LIB_CALL 1
#define LINUX_CALL 2
#define BYTES 4096

int main(int argc, char* argv[])
{
    int n, call = -1;

    /* Checking whether the call is library, system or linux call */
    while((n = getopt(argc, argv, "lsp")) != -1)
    {
        if(n == 's')
        {
            call = SYS_CALL;
        }
        else if(n == 'p')
        {
            call = LINUX_CALL;
        }
        else
        {
            call = LIB_CALL;
        }
    }
    if(call == -1)
    {
        exit(1);
    }

    char ch;
    if(call == SYS_CALL)
    {
        /* System call */
        ssize_t sc;
        while((sc = read(0, &ch, 1)) > 0)
        {
            sc = write(1, &ch, 1);
            if(sc < 0)
            {
                exit(1);
            }
        }
        if(sc < 0)
        {
            exit(1);
        }
    }
    else if(call == LINUX_CALL)
    {
        /* Linux call */
        ssize_t lc;
        while((lc = sendfile(1, 0, 0, BYTES)) > 0)
        {
            if(lc < 0)
            {
                exit(1);
            }
        }
        if(lc < 0)
        {
            exit(1);
        }
    }
    else
    {
        /* Library call */
        while((ch = getc(stdin)) != EOF)
        {
            if(putc(ch, stdout) == EOF)
            {
                exit(1);
            }
        }
    }
    return 0;
}
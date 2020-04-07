#include <stdio.h>
#include "receiver.h"  /* Include the header here, to obtain the function declaration */
#define BUFSIZE 1024

void main(void)
{
    int n;
    char buf[BUFSIZE];
    int y = getConnection();  /* Use the function here */
    printf("Completed Socket connection\n");

    while(1){
        /* print the server's reply */
        bzero(buf, BUFSIZE);
        n = read(y, buf, BUFSIZE);
        if (n < 0)
          error("ERROR reading from socket");
        printf("Echo from server: %s", buf);
    }
    close(y);
 
}


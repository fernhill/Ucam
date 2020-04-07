#ifndef __interface__
#define __interface__

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
static int clientSocketSender;
static int clientSocketReceiver;
char buffer[1024];
char send_buffer[1024];
char recv_buffer[1024];
struct sockaddr_in serverAddr;
socklen_t addr_size;

void connectToServer();
void setChannelRoles();
void fullDuplexConnection();
void sendMessage(char *message);
char *readMessage();
int readData(char *b);
#endif

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>

#include "interface.h"

void connectToServer(){
  /*---- Create the socket. The three arguments are: ----*/
  /* 1) Internet domain 2) Stream socket 3) Default protocol (TCP in this case) */
  clientSocketSender = socket(PF_INET, SOCK_STREAM, 0);
  clientSocketReceiver = socket(PF_INET, SOCK_STREAM, 0);

  /*---- Configure settings of the server address struct ----*/
  /* Address family = Internet */
  serverAddr.sin_family = AF_INET;
  /* Set port number, using htons function to use proper byte order */
  serverAddr.sin_port = htons(7891);
  /* Set IP address to localhost */
  serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  /* Set all bits of the padding field to 0 */
  memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

  /*---- Connect the socket to the server using the address struct ----*/
  addr_size = sizeof serverAddr;
  connect(clientSocketSender, (struct sockaddr *) &serverAddr, addr_size);
  printf("Sender connect complete\n");
  connect(clientSocketReceiver, (struct sockaddr *) &serverAddr, addr_size);
  printf("Receiver connect complete\n");

}

void sendMessage(char *message){
  size_t buf_size= strlen(message) + 1;
  //printf("Size is --> %d\n", buf_size);
  //printf("Message is --> %s\n",message);
  strcpy(buffer,message);
  send(clientSocketSender,buffer,buf_size,0);
}

char* receive(){
  recv(clientSocketReceiver, recv_buffer, 1024, 0);
  printf("Received --> %s\n", buffer);
  return recv_buffer;
}

int readData(char *b){
  return recv(clientSocketSender, b, 1024, 0);
}

void setChannelRoles(){
  char *msg;
  msg = "SENDER";
  strcpy(send_buffer,msg);
  send(clientSocketSender,send_buffer,strlen(msg),0);
  msg = "RECEIVER";
  strcpy(send_buffer,msg);
  send(clientSocketReceiver,send_buffer,strlen(msg),0);
}

void fullDuplexConnection(){
  connectToServer();
  setChannelRoles();
}

/*
void main(){
	printf("Setup Full Duplex Connection\n");
	fullDuplexConnection();
	while(1){
		usleep(1000000);		
	}
}*/

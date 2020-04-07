#include <stdio.h>

void main(){
	int i = 5000;
	printf("Values -> %d\n",i*1000);
	usleep(i*1000);
	printf("Sleep Done..\n");
}

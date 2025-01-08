#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "msg.h"
#include <string.h>

int main()
{
	int msgq;
	int ret;
	int key = 0x12345;
	msgq = msgget( key, IPC_CREAT | 0666);
	printf("msgq id: %d\n", msgq);

	struct my_msgbuf msg;
	memset(&msg, 0, sizeof(msg));
	ret = msgrcv(msgq, &msg, sizeof(msg) - sizeof(long), 0, 0);
	printf("msgsnd ret: %d\n", ret);
	printf("msg.mtype: %ld\n", msg.mtype);
	printf("msg.pid: %d\n", msg.pid);
	printf("msg.io_time: %d\n", msg.io_time);
	

	if (msgctl(msgq, IPC_RMID, NULL) == -1) {
		perror("msgctl failed");
		return 1;
	}
	return 0;
}

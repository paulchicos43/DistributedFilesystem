#include <stdio.h>
#include "mfs.h"
#include "udp.h"

int sd; // Keeps track of open connection
struct sockaddr_in addrSnd; // socket used for the connection

int MFS_Init(char *hostname, int port) {
	sd = UDP_Open(20000);
	int rc = UDP_FillSockAddr(&addrSnd, hostname, port);
	if (rc < 0) {
		return -1;
	}
	
	return 0;
}

int MFS_Lookup(int pinum, char *name) {
	message* msg = malloc(sizeof(struct message));
	response* resp = malloc(sizeof(struct response));

	strcpy(msg->command, "LOOKUP");
	msg->inum = pinum;
	strcpy(msg->name, name);
	Send_Message(msg, resp);
	printf("client:: resp->succ=%d\n", resp->succ);

	return resp->succ;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
	message* msg = malloc(sizeof(struct message));
	response* resp = malloc(sizeof(struct response));

	strcpy(msg->command, "STAT");
	msg->inum = inum;
	Send_Message(msg, resp);

	memcpy(m, &resp->stats, sizeof(MFS_Stat_t));
	m = &resp->stats;
	
	return 0;
}

int MFS_Write(int inum, char *buffer, int block) {
	message* msg = malloc(sizeof(struct message));
	response* resp = malloc(sizeof(struct response));

	strcpy(msg->command, "WRITE");
	msg->inum = inum;
	msg->block_offset = block;
	memcpy(msg->block, &buffer, sizeof(char[MFS_BLOCK_SIZE]));
	Send_Message(msg, resp);
	
	return 0;
}

int MFS_Read(int inum, char *buffer, int block) {
	message* msg = malloc(sizeof(struct message));
	response* resp = malloc(sizeof(struct response));

	strcpy(msg->command, "READ");
	msg->inum = inum;
	msg->block_offset = block;
	Send_Message(msg, resp);

	memcpy(buffer, &resp->block, sizeof(char[MFS_BLOCK_SIZE]));
	
	return 0;
}

int MFS_Creat(int pinum, int type, char *name) {
	message* msg = malloc(sizeof(struct message));
	response* resp = malloc(sizeof(struct response));

	strcpy(msg->command, "CREAT");
	msg->inum = pinum;
	msg->type = type;
	strcpy(msg->name, name);
	Send_Message(msg, resp);

	return 0;
}

int MFS_Unlink(int pinum, char *name) {
	message* msg = malloc(sizeof(struct message));
	response* resp = malloc(sizeof(struct response));

	strcpy(msg->command, "UNLINK");
	msg->inum = pinum;
	strcpy(msg->name, name);
	Send_Message(msg, resp);

	return 0;
}

int MFS_Shutdown() {
	message* msg = malloc(sizeof(struct message));
	response* resp = malloc(sizeof(struct response));

	strcpy(msg->command, "SHUTDOWN");
	Send_Message(msg, resp);

	return 0;
}

// Helper method to send commands
int Send_Message(message* msg, response* resp) {
	printf("client:: sending message...\n");
	int rc = UDP_Write(sd, &addrSnd, (char*) msg, sizeof(struct message));
	if (rc < 0) {
		printf("client:: failed to send\n");
		return -1;
	}
	printf("client:: awaiting server response...\n");
	char buffer[sizeof(struct response)];
	UDP_Read(sd, &addrSnd, &buffer[0], sizeof(struct response));
	printf("client:: response recieved!\n");
	*resp = *(response*) buffer;
	return 0;
}

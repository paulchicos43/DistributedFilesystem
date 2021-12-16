#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "udp.h"

#define BLOCK_SIZE 4096


int initialized = 0;
char *fileName;

typedef struct inode {
	int size;
	int type; //0 for a file and 1 for a directory
	int pointers[14];
} inode;

typedef struct __MFS_DirEnt_t {
    char name[28];  // up to 28 bytes of name in directory (including \0)
    int  inum;      // inode number of entry (-1 means entry not used)
} MFS_DirEnt_t;

typedef struct info {
	int size;
	int inodes;
} Info;

int server_write(void *buffer, int offset, int bytes) {
	if(initialized != 1) {
		return 1;
	}

	int fd = open(fileName,O_WRONLY);
	lseek(fd,offset+sizeof(Info),SEEK_SET);
	write(fd,buffer,bytes);
	close(fd);
	return 0;
}

int server_read(void *buffer, int offset, int bytes) {
	if(initialized != 1) {
		return 1;
	}

	int fd = open(fileName,O_RDONLY);
	lseek(fd,offset+sizeof(Info),SEEK_SET);
	read(fd,buffer,bytes);
	close(fd);
	return 0;
}



int server_init(char *name) {
        fileName = name;
        initialized = 1;
        if(access(fileName,F_OK) == 0) {
                return 0;
        }
        int fd = open(fileName,O_CREAT);
        
	Info info = { 4,0 };
	server_write(&info,-8,sizeof(int));
	int p[14];
	p[0] = 4096 * sizeof(inode);
	inode first = { 4096,1,{*p} };
	server_write(&first,0,sizeof(inode));
	
		
	for(int i = 1; i < 4096; i++) {
		int pointers[14];
		inode in = { 0,-1,{*pointers} };
		server_write(&in,i * sizeof(inode),sizeof(inode));

	}
	MFS_DirEnt_t rootDir[128];
	rootDir[0].inum = 0;
	strcpy(rootDir[0].name,".");
	rootDir[1].inum = 0;
	strcpy(rootDir[1].name,"..");

	for(int i = 2; i < 128; i++) {
		rootDir[i].inum = -1;
	}
	
	server_write(&rootDir,4096 * sizeof(inode),BLOCK_SIZE);

	int size = 4096 * sizeof(inode) + BLOCK_SIZE + 4;
	info.size = size;
	info.inodes = 1;
	server_write(&info,-8,sizeof(Info));
        close(fd);
        return 0;
}

int MFS_Lookup(int pinum, char *name) {
	inode *in = malloc(sizeof(inode));
	server_read(in,pinum * sizeof(inode),sizeof(inode));
	for(int i = 0; i < 14; i++) {
		if(in->pointers[i] == 0) {
			break;
		}
		MFS_DirEnt_t dir[128];
		server_read(&dir,in->pointers[i],BLOCK_SIZE);
		for(int j = 0; j < 128; j++) {
			if(strcmp(dir[j].name,name) == 0) {
				return dir[j].inum;
			}
			if(dir[j].inum == -1) {
				break;
			}
		}	
	}
	return -1;
}

int MFS_Write(int inum, char *buffer, int block) {
	inode *in = malloc(sizeof(inode));
	server_read(in,inum * sizeof(inode),sizeof(inode));
	if(in->pointers[block] == 0) {
		Info *info = malloc(sizeof(Info));
		server_read(info,-8,sizeof(Info));
		in->pointers[block] = info->size;
		in->size += BLOCK_SIZE;
		info->size += BLOCK_SIZE;
		server_write(info,-8,sizeof(Info));
	}

	server_write(buffer,in->pointers[block],BLOCK_SIZE);
	server_write(in, inum * sizeof(inode),sizeof(inode));
	return 0;
}

int MFS_Read(int inum, char *buffer, int block) {
	inode *in = malloc(sizeof(inode));
	server_read(in,inum * sizeof(inode),sizeof(inode));
	server_read(buffer,in->pointers[block],BLOCK_SIZE);

	return 0;
}

int MFS_Creat(int pinum,int type, char *name) {
	Info *info = malloc(sizeof(Info));
	server_read(info,-8,sizeof(Info));

	inode *in = malloc(sizeof(inode));
	server_read(in,pinum * sizeof(inode),sizeof(inode));
	int found = 0;
	for(int i = 0; i < 14; i++) {
		MFS_DirEnt_t dir[128];
		server_read(&dir,in->pointers[i],BLOCK_SIZE);
		for(int j = 0; j < 128; j++) {
			if(dir[j].inum == -1) {
				dir[j].inum = info->inodes;
				strcpy(dir[j].name,name);
				info->inodes += 1;
				found = 1;
				break;
			}
		}
		if(found == 1) {
			server_write(&dir,in->pointers[i],BLOCK_SIZE);
			server_write(info,-8,sizeof(Info));
			break;
		}
	}
	return 0;
}


int MFS_Unlink(int pinum, char *name) {
	inode *in = malloc(sizeof(inode));
	server_read(in,pinum * sizeof(inode),sizeof(inode));

	for(int i = 0; i < 14; i++) {
		MFS_DirEnt_t dir[128];
		server_read(&dir,in->pointers[i],BLOCK_SIZE);
		int done = 0;
		for(int j = 0; j < 128; j++) {
			if(strcmp(dir[j].name,name) == 0) {
				dir[j].inum = -1;
				done = 1;
				server_write(&dir,in->pointers[i],BLOCK_SIZE);
				break;
			}
		}
		if(done == 1) {
			break;
		}
	}
	return 0;
}

int main(int argc, char *argv[]) {
	/*server_init("storage");
	int fn = MFS_Lookup(0,"testing");
	char buffer1[4096] = "whats up";

	char buffer2[4096];
	MFS_Write(fn,buffer1,2);

	MFS_Read(fn,buffer2,2);

	printf("%s\n",buffer2);*/
    int sd = UDP_Open(10000);
    assert(sd > -1);
    while (1) {
        struct sockaddr_in addr;
        char message[BUFFER_SIZE];
        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, message, BUFFER_SIZE);
        printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
        if (rc > 0) {
            char reply[BUFFER_SIZE];
            sprintf(reply, "goodbye world");
            rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
            printf("server:: reply\n");
        }
    }
	
	return 0;

}
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "udp.h"
#include "mfs.h"

#define BLOCK_SIZE 4096


int initialized = 0;
char *fileName;

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
	inode first = { 4096,1,*p };
	server_write(&first,0,sizeof(inode));
	
		
	for(int i = 1; i < 4096; i++) {
		int pointers[14];
		pointers[0] = 4096 * sizeof(inode) + BLOCK_SIZE + 4;
		inode in = { 0,-1,*pointers };
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


int MFS_Lookup_loc(int pinum, char *name) {
	struct inode *in = malloc(sizeof(struct inode));
	server_read(in,pinum * sizeof(struct inode),sizeof(struct inode));
	for(int i = 0; i < 14; i++) {
		if(in->pointers[i] == 0) {
			break;
		}
		MFS_DirEnt_t dir[128];
		server_read(&dir,in->pointers[i],BLOCK_SIZE);
		for(int j = 0; j < 128; j++) {
			if(strcmp(dir[j].name, name) == 0) {
				return dir[j].inum;
			}
			if(dir[j].inum == -1) {
				break;
			}
		}	
	}
	return -1;
}

int MFS_Stat_loc(int inum, MFS_Stat_t *m) {
	if(inum >= 4096) {
		return -1;
	}
        struct inode *in = malloc(sizeof(struct inode));
        server_read(in,inum * sizeof(struct inode),sizeof(struct inode));
	if(in->active == 0) {
		return -1;
	}
        m->size = in->size;
        m->type = in->type;
        return 0;
}



int MFS_Write_loc(int inum, char *buffer, int block) {
	if(inum >= 4096 || inum < 0 || block >= 14 || block < 0) {
		return -1;
	}
	inode *in = malloc(sizeof(inode));
	server_read(in,inum * sizeof(inode),sizeof(inode));
	if(in->active == 0 || in->type != MFS_REGULAR_FILE) {
		return -1;
	}
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

int MFS_Read_loc(int inum, char *buffer, int block) {
	if(inum >= 4096 || inum < 0 || block >= 14 || block < 0) {
                return -1;
        }
	inode *in = malloc(sizeof(inode));
	server_read(in,inum * sizeof(inode),sizeof(inode));
	if(in->active == 0 || in->pointers[block] == 0) {
                return -1;
        }
	server_read(buffer,in->pointers[block],BLOCK_SIZE);

	return 0;
}


int MFS_Create_dir_loc(int pinum, char *name) {
	Info *info = malloc(sizeof(Info));
	server_read(info,-8,sizeof(Info));

	inode *currentDir = malloc(sizeof(inode));
	server_read(currentDir,pinum * sizeof(inode),sizeof(inode));

	for(int i = 0; i < 14; i++) {
		MFS_DirEnt_t entries[128];
		if(currentDir->pointers[i] != 0) {
			server_read(&entries,currentDir->pointers[i],BLOCK_SIZE);
		} else {
			currentDir->pointers[i] = info->size;
			info->size += MFS_BLOCK_SIZE;
			for(int j = 0; j < 128; j++) {
				entries[j].inum = -1;
			}
		}

		int found = 0;
		for(int j = 0; j < 128; j++) {
			if(entries[j].inum == -1) {
				entries[j].inum = info->inodes;
				strcpy(entries[j].name,name);
				
				MFS_DirEnt_t newDirectoryEntries[128];
				newDirectoryEntries[0].inum = entries[j].inum;
				strcpy(newDirectoryEntries[0].name,".");
				newDirectoryEntries[1].inum = pinum;
                                strcpy(newDirectoryEntries[1].name,"..");
				server_write(&newDirectoryEntries,info->size,BLOCK_SIZE);
				
				info->inodes += 1;
				info->size += BLOCK_SIZE;
				found = 1;
				break;
			}
		}

		if(found == 1) {
                        server_write(&entries,currentDir->pointers[i],BLOCK_SIZE);
                        server_write(info,-8,sizeof(Info));

                        inode *new_inode = malloc(sizeof(inode));
                        new_inode->type = MFS_DIRECTORY;
                        new_inode->active = 1;
			new_inode->pointers[0] = info->size - BLOCK_SIZE;
                        server_write(new_inode,info->inodes-1 * sizeof(inode),sizeof(inode));
                        break;
                }


	}
	return 0;
}

int MFS_Create_file_loc(int pinum, char *name) {
	Info *info = malloc(sizeof(Info));
	server_read(info,-8,sizeof(Info));

	inode *currentDirectory = malloc(sizeof(inode));
	server_read(currentDirectory,pinum * sizeof(inode),sizeof(inode));
	int found = 0;
	printf("\n%d\n",currentDirectory->pointers[0]);
	for(int i = 0; i < 14; i++) {
		MFS_DirEnt_t entries[128];
		if(currentDirectory->pointers[i] != 0) {
			printf("HAPPENING\n");
                        server_read(&entries,currentDirectory->pointers[i],BLOCK_SIZE);
                } else {
                        currentDirectory->pointers[i] = info->size;
                        info->size += MFS_BLOCK_SIZE;
                        for(int j = 0; j < 128; j++) {
                                entries[j].inum = -1;
                        }
                }
		
		for(int j = 0; j < 128; j++) {
			if(entries[j].inum == -1) {
				entries[j].inum = info->inodes;
				strcpy(entries[j].name,name);
				info->inodes += 1;
				found = 1;
				break;
			}
		}
		if(found == 1) {
			server_write(&entries,currentDirectory->pointers[i],BLOCK_SIZE);
			server_write(info,-8,sizeof(Info));

			inode *new_inode = malloc(sizeof(inode));
			new_inode->type = MFS_REGULAR_FILE;
			new_inode->active = 1;
			server_write(new_inode,info->inodes-1 * sizeof(inode),sizeof(inode));
			break;
		}
	}
	return 0;
}

int MFS_Creat_loc(int pinum,int type, char *name) {
        if(pinum >= 4096 || pinum < 0) {
                return -1;
        }
	if(type == MFS_REGULAR_FILE) {
		MFS_Create_file_loc(pinum,name);
	} else {
		MFS_Create_dir_loc(pinum,name);
	}
}

int is_directory_empty(int inum) {
	inode *in = malloc(sizeof(inode));
	server_read(in,inum * sizeof(inode),sizeof(inode));
        for(int i = 0; i < 14; i++) {
		if(in->pointers[i] == 0) {
			break;
		}
                MFS_DirEnt_t dir[128];
                server_read(&dir,in->pointers[i],BLOCK_SIZE);
                for(int j = 0; j < 128; j++) {
                        if(dir[j].inum != -1) {
				return 1;
			}
                }
        }
	return 0;
}


int MFS_Unlink_loc(int pinum, char *name) {
	if(pinum >= 4096 || pinum < 0) {
		return -1;
	}
	if(is_directory_empty(pinum) == 1) {
		return -1;
	}
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



int MFS_Shutdown_loc() {
	exit(0);
}

int main(int argc, char *argv[]) {
	/*server_init("storage");
	int fn = MFS_Lookup_loc(0,"testing");
	char buffer1[4096] = "whats up";

	char buffer2[4096];
	MFS_Write_loc(fn,buffer1,2);

	MFS_Read_loc(fn,buffer2,2);

	printf("%s\n",buffer2);*/
    if (argc != 3)
    {
        perror("server:: Invocation of server contains incorrect params\n");
        return 0;
    }

    int sd = UDP_Open(atoi(argv[1]));
    printf("server:: file_image: %s\n", argv[2]);
    server_init(argv[2]);
    assert(sd > -1);
    printf("server:: Server initialized!\n");
    while (1) {
	    struct sockaddr_in addr;
	    struct message* msg = malloc(sizeof(struct message));
	    char buffer[sizeof(struct message)];
	    printf("server:: waiting...\n");
	    int rc = UDP_Read(sd, &addr, &buffer[0], sizeof(struct message));
	    printf("server:: message recieved!\n");
	    if (rc > 0) {
	        //msg = (message*) buffer;
		struct message* msg = malloc(sizeof(struct message));
		memcpy(msg,(struct message*) buffer, sizeof(struct message));
	        printf("server:: read command: %s\n", msg->command);
		if (strcmp(msg->command, "LOOKUP") == 0) {
		    int succ = MFS_Lookup_loc(msg->inum, msg->name);
		    struct response* resp = malloc(sizeof(struct response));
		    resp->succ = succ;
		    printf("server:: starting to send reply\n");
		    UDP_Write(sd, &addr, (char*) resp, sizeof(struct response));
		    printf("server:: LOOKUP reply sent\n");
		    free(resp);
		}
		else if (strcmp(msg->command, "STAT") == 0) {
			int succ = MFS_Stat_loc(msg->inum,&msg->stats);
			struct response* resp = malloc(sizeof(struct response));
			resp->succ = succ;
			resp->stats = resp->stats;
			UDP_Write(sd, &addr, (char*) resp, sizeof(struct response));
			printf("server:: STAT reply sent\n");
			free(resp);
		}
		else if (strcmp(msg->command, "WRITE") == 0) {
		   int succ = MFS_Write_loc(msg->inum, msg->block, msg->block_offset);
		   struct response* resp = malloc(sizeof(struct response));
		   resp->succ = succ; 
		   UDP_Write(sd, &addr, (char*) resp, sizeof(struct response));
		   printf("server:: WRITE reply sent\n");
		   free(resp);
		}
		else if (strcmp(msg->command, "READ") == 0) {
		   int succ = MFS_Read_loc(msg->inum, msg->block, msg->block_offset);
		   struct response* resp = malloc(sizeof(struct response));
		   resp->succ = succ;
		   //memcpy(resp->block, msg->block, sizeof(char[MFS_BLOCK_SIZE]));
		   strcpy(resp->block,msg->block);
		   //printf("HERE %s\n",resp->block);
		   UDP_Write(sd, &addr, (char*) resp, sizeof(struct response));
		   printf("server:: READ reply sent\n");
		   free(resp);
		}
		else if (strcmp(msg->command, "CREAT") == 0) {
		    int succ = MFS_Creat_loc(msg->inum, msg->type, msg->name);
	    	    struct response* resp = malloc(sizeof(struct response));
		    resp->succ = succ;
		    UDP_Write(sd, &addr, (char*) resp, sizeof(struct response));
		    printf("server:: CREAT reply sent\n");
		    free(resp);	    
		}
		else if (strcmp(msg->command, "UNLINK") == 0) {
		    int succ = MFS_Unlink_loc(msg->inum, msg->name);
		    struct response* resp = malloc(sizeof(struct response));
		    resp->succ = succ;
		    UDP_Write(sd, &addr, (char*) resp, sizeof(struct response));
		    printf("server:: UNLINK reply sent\n");
		    free(resp);
		}
		else if (strcmp(msg->command, "SHUTDOWN") == 0) {
		    MFS_Shutdown_loc();
		}
	    }
    }
	
        /*struct sockaddr_in addr;
        char message[BUFFER_SIZE];
        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, message, BUFFER_SIZE);
        printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
        if (rc > 0) {
            char reply[BUFFER_SIZE];
            sprintf(reply, "goodbye world");
            rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
            printf("server:: reply\n");
        }*/
    return 0;

}

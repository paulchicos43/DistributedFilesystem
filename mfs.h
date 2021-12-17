#ifndef __MFS_h__
#define __MFS_h__

#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)

#define MFS_BLOCK_SIZE   (4096)
#define BUFFER_SIZE (1000)

typedef struct __MFS_Stat_t {
    int type;   // MFS_DIRECTORY or MFS_REGULAR
    int size;   // bytes
    // note: no permissions, access times, etc.
} MFS_Stat_t;

typedef struct __MFS_DirEnt_t {
    char name[28];  // up to 28 bytes of name in directory (including \0)
    int  inum;      // inode number of entry (-1 means entry not used)
} MFS_DirEnt_t;

typedef struct inode {
    int active; //0 not active, 1 active
    int size;
    int type; // 0 for a directory, 1 for a regular file
    int pointers[14];
} inode;

typedef struct message {
    char command[20];
    char block[MFS_BLOCK_SIZE];
    int inum;
    int block_offset;
    int type;
    char name[50];
    MFS_Stat_t stats;
} message;

typedef struct response {
    int succ; //
    char block[MFS_BLOCK_SIZE];
    MFS_Stat_t stats;
} response;

int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int block);
int MFS_Read(int inum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);
int MFS_Shutdown();

int Send_Message(struct message* msg, struct response* resp);

#endif // __MFS_h__


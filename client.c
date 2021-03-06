#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (1000)

// client code
int main(int argc, char *argv[]) {
    
    /*struct sockaddr_in addrSnd, addrRcv;

    int sd = UDP_Open(20000);
    int rc = UDP_FillSockAddr(&addrSnd, "localhost", 10000);

    char message[BUFFER_SIZE];
    sprintf(message, "hello world");

    printf("client:: send message [%s]\n", message);
    rc = UDP_Write(sd, &addrSnd, message, BUFFER_SIZE);
    if (rc < 0) {
        printf("client:: failed to send\n");
        exit(1);
    }

    printf("client:: wait for reply...\n");
    rc = UDP_Read(sd, &addrRcv, message, BUFFER_SIZE);
    printf("client:: got reply [size:%d contents:(%s)\n", rc, message);*/

    MFS_Init("localhost", 10000);
    MFS_Creat(0,MFS_REGULAR_FILE,"TESTING");
    int rc = MFS_Lookup(0, "TESTING");
    printf("Lookup result: %d\n",rc);
    char buffer1[4096] = "hello world";
    MFS_Write(rc,buffer1,0);

    char buffer2[4096];
    MFS_Read(rc,buffer2,0);
    printf("client:: Read results: %s\n", buffer2);
    
    return 0;
}


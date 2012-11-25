/*
 * Filename: test.c
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>   // for mmap
#include "myioctl.h"

int main(void)
{
    int fd, i;
    char pixel[4] = {0x00, 0x00, 0x00, 0xff};
    unsigned char *fb;

    //pid_t child;
    
    //child = fork();

    fd = open("/dev/cdata", O_RDWR);
    fb = (unsigned char *)mmap(0, 240*320*4, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    for(i=0; i<500; i++) {
       *fb=0xff; fb++;
       *fb=0x00; fb++;
       *fb=0x00; fb++;
       *fb=0x00; fb++;
    }

/*
    while (1) {
       write(fd, pixel, 4);
    }
*/

/*
    if (child != 0) {	//parent
       write(fd, "ABCDE", 5);
    } else {		// child
       write(fd, "12345", 5);
    }

    ioctl(fd, CDATA_SYNC, 0);
*/
    close(fd);
}

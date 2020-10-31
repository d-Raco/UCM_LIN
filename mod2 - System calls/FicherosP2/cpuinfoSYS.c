#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>

#define BUFSIZE 512

#ifndef SYS_read 
#define SYS_read          0 
#define SYS_write         1 
#define SYS_open          2 
#define SYS_close         3 
#endif

/* gcc -Wall -g cpuinfoSYS.c -o cpuinfoSYS */

int main(void)
{
    int read_chars=0;
    int fd=0, standardOutput=1;
    char buf[BUFSIZE+1];
    
    /* Open /proc entry in read-only mode */
    fd=syscall(SYS_open,"/proc/cpuinfo",O_RDONLY);
    
    if (fd<0){
        fprintf(stderr,"Can't open the file\n");
        exit(1);
    }
    
    /* Loop that reads data from the file and prints its contents to stdout */
    while((read_chars=syscall(SYS_read,fd,buf,BUFSIZE))>0){
        buf[read_chars++]='\0';
        syscall(SYS_write,standardOutput,buf,read_chars);      
    }
     
    if (read_chars<0){
        fprintf(stderr,"Error while reading the file\n");
        exit(1);        
    }
    
    	
    /* Close the file and exit */ 
    syscall(SYS_close,fd);
    return 0;
}

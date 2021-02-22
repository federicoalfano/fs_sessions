#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define CHAR_DEVICE "/dev/session-module"
 
#define SESSION_MAGIC 'S'
#define SESSION_OPEN_SEQ_NO 0X1
#define SESSION_CLOSE_SEQ_NO 0X2

#define SESSION_OPEN _IOW(SESSION_MAGIC, SESSION_OPEN_SEQ_NO, unsigned int)
#define SESSION_CLOSE _IOW(SESSION_MAGIC, SESSION_CLOSE_SEQ_NO,   int)

int session_init()
{
	return open(CHAR_DEVICE, O_RDWR);
}
 
int session_open(int session_id, int fd)
{  
    return ioctl(session_id, SESSION_OPEN, &fd);  
}

int session_close(int session_id, int fd)
{	
    return ioctl(session_id, SESSION_CLOSE, &fd); 
}

int session_exit(int session_id)
{
		return close(session_id);
}
	


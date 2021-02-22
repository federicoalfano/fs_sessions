#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>


#define TRIES 10000

extern int session_init(void);
extern int session_open(int , int );
extern int session_close(int , int );
extern int session_exit(int );

int write_with_session(char *filename)
{
	int fd = open(filename, O_APPEND | O_RDWR);
	int session_id = session_init();
	const char *to_write = "New string";
	
	session_open(session_id, fd);
	write(fd, to_write, strlen(to_write));
	session_close(session_id, fd);
	session_exit(session_id);
	close(fd);
	return 0;
}

int write_without_session(char *filename)
{
	const char *to_write = "New string";
	int fd = open(filename, O_APPEND | O_RDWR);
	write(fd, to_write, strlen(to_write));
	close(fd);
	return 0;
}


int main(int argc, char *argv[])
{
	if(argc<3)
		printf("[-] Usage: perf_test.c file1 file2");
	int i;
	
	for(i=0; i<TRIES;i++)
		write_with_session(argv[1]);
	for(i=0; i<TRIES;i++)
		write_without_session(argv[2]);
	return 0;
}

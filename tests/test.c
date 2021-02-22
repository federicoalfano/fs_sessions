#include <check.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>




#define MAX_LINE 512
#define BUFFER_LENGTH 256
#define FILENAME_SIZE 128
#define CHILDREN_NUMBER 10


extern int session_init(void);
extern int session_open(int , int );
extern int session_close(int , int );
extern int session_exit(int );


int get_counter(char*, char*);

/* Set here an invalid path with write permissions in order to test the module */
#define INVALID_PATH "/home/osboxes/Documenti/AOSV"

/* Test about the initialization of the module */

START_TEST(test_session_init)
{
	int session_id = session_init();

	// The init should return a positive value
	ck_assert_int_ge(session_id, 0);

	// A second init has to return the -EEXIST value
	ck_assert_int_lt(session_init(), 0);
	ck_assert_int_eq(errno, EEXIST);
	
	// The module builds the tree into sysfs check the existence of all files
	ck_assert_int_eq(access("/sys/kernel/session-module/proc_counter", F_OK), 0);
	ck_assert_int_eq(access("/sys/kernel/session-module/file_counter", F_OK), 0);
	ck_assert_int_eq(access("/sys/kernel/session-module/session_counter", F_OK), 0);
	ck_assert_int_eq(access("/sys/kernel/session-module/path", F_OK), 0);
	
	// Check the exit
	ck_assert_int_eq(session_exit(session_id), 0); 
	
}
END_TEST

START_TEST(test_file_op)
{
	char base_addr[BUFFER_LENGTH]; 
	char file_addr[BUFFER_LENGTH];
	char read_buffer_nosession[BUFFER_LENGTH];
	char read_buffer[BUFFER_LENGTH];
	ssize_t rw_result;
	
	/* For simplicity we will check for just strings */
	char *write_string = "This is just a string";
	
	int session_id = session_init(); 
	int path_fd = open("/sys/kernel/session-module/path", O_RDONLY);
	
	/* 
	 * Testing if the path is set and then stores it into base_addr in order to create a test_file into a valid path
	 * then stores the result and append a name that has to be an empty file, if it exists, it will be unlinked before
	 * */
	ck_assert_int_ge(read(path_fd, base_addr, sizeof(base_addr)), 0);
	close(path_fd); 
	sprintf(file_addr, "%s/test_file", base_addr);
	if(access(file_addr, F_OK)== 0)
		unlink(file_addr);

	/* 
	 * Creating and opening a valid a test file two times, the first will have session opened and the second won't,
	 * let's check the differences
	 */
	int fd = open(file_addr, O_CREAT | O_RDWR, 0644);
	ck_assert_int_ge(fd, 0);
	session_open(session_id, fd);

	//Trying to write a string a
	rw_result = write(fd, write_string, strlen(write_string));
	ck_assert_int_ge(rw_result, 0);

	lseek(fd, 0, SEEK_SET);

	// Tries to read the file and compare with session and no_session
	rw_result = read(fd, read_buffer, BUFFER_LENGTH);
	ck_assert_int_gt(rw_result, 0);
	ck_assert(strcmp(read_buffer, write_string)==0);
	memset(read_buffer, 0, BUFFER_LENGTH);
	lseek(fd, 2, SEEK_SET);
	ck_assert_int_ge(errno, 0);
	rw_result = read(fd, read_buffer, BUFFER_LENGTH);
	
	ck_assert(strcmp(read_buffer, &write_string[2])==0);
	
	lseek(fd, 0, SEEK_SET);
	rw_result = read(fd, read_buffer, BUFFER_LENGTH);
	
	// Open the same file without sessions and checks if it's written before close
	int fd2  = open(file_addr, O_RDONLY);
	ck_assert_int_gt(fd2, 0);
	memset(read_buffer_nosession, 0, BUFFER_LENGTH);

	rw_result = read(fd2,read_buffer_nosession,  BUFFER_LENGTH);

	ck_assert_int_ne(strcmp(read_buffer_nosession, read_buffer), 0);
	ck_assert_int_eq(session_close(session_id, fd),0);
	ck_assert_int_eq(session_exit(session_id), 0);
	
	// After closing session the content has to be flushed, check it!
	memset(read_buffer_nosession, 0, BUFFER_LENGTH);
	rw_result = read(fd2, read_buffer_nosession, BUFFER_LENGTH);
	ck_assert_int_gt(rw_result, 0);
	ck_assert(strcmp(read_buffer_nosession, read_buffer)==0);
	
	/*
	 * Try a write with the O_APPEND flag
	 */
	 
	fd = open(file_addr, O_APPEND | O_RDWR, 0644);
	ck_assert_int_ge(fd, 0);
	session_open(session_id, fd);
	rw_result = write(fd, write_string, strlen(write_string));
	ck_assert_int_eq(rw_result, strlen(write_string));
	lseek(fd, 0, SEEK_SET);
	rw_result = read(fd, read_buffer, BUFFER_LENGTH);
	ck_assert_int_gt(rw_result, 0);
	ck_assert_int_eq(rw_result, 2*strlen(write_string));
	
	
}
END_TEST
START_TEST(test_errors)
{
	char file_addr[BUFFER_LENGTH];
	char read_buffer[BUFFER_LENGTH];
	unsigned int invalid_command = 0XCF;
	ssize_t rw_result;
	

	
	int session_id = session_init(); 
	/* Check an invalid ioctl command */
	ioctl(session_id, invalid_command); 
	ck_assert_int_eq(errno, EINVAL);
	
	/* Check an invalid file descriptor */
	int invalid_fd = -1;
	session_open(session_id, invalid_fd);
	ck_assert_int_eq(errno, EBADF);
	session_close(session_id, invalid_fd);
	ck_assert_int_eq(errno, EBADF);
	invalid_fd = 15;
	ck_assert_int_eq(errno, EBADF);
	
	/*Let's test to open a session onto a file created in an invalid path */
	
	sprintf(file_addr, "%s/test_file", INVALID_PATH);
	
	invalid_fd = open(file_addr, O_CREAT | O_RDWR, 0644);
	session_open(session_id, invalid_fd);
	if(access(file_addr, F_OK)== 0)
		unlink(file_addr);
	ck_assert_int_eq(errno, EACCES);
	session_close(session_id, invalid_fd);
	close(invalid_fd);
	session_exit(session_id);
	
	
}
END_TEST
START_TEST(test_sysfs)
{
	
	char base_addr[BUFFER_LENGTH]; 
	char file_addr[BUFFER_LENGTH];
	char *file_counter_addr = "/sys/kernel/session-module/file_counter";
	char *proc_counter_addr ="/sys/kernel/session-module/proc_counter";
	char *session_counter_addr = "/sys/kernel/session-module/session_counter";
	char sys_result[8];
	char pid[8];
	int result;
	
	sprintf(pid, "%d", getpid());

	int session_id = session_init(); 
	int path_fd = open("/sys/kernel/session-module/path", O_RDONLY);
	read(path_fd, base_addr, sizeof(base_addr));
	close(path_fd);
	sprintf(file_addr, "%s/test_file", base_addr);
	if(access(file_addr, F_OK)== 0)
		unlink(file_addr);

	int fd = open(file_addr, O_CREAT , 0644);
	int fd2 = open(file_addr, O_RDONLY);

	ck_assert_int_ge(fd, 0);
	session_open(session_id, fd);
	int c_fd = open(session_counter_addr, O_RDONLY);
	read(c_fd, sys_result, 1);

	sys_result[1] = '\0';
	ck_assert_int_eq(atoi(sys_result), 1);		
		
	ck_assert_int_eq(get_counter(file_counter_addr, file_addr),1);
	ck_assert_int_eq(get_counter(proc_counter_addr, pid),1);
	
	session_open(session_id, fd2);
	ck_assert_int_eq(get_counter(file_counter_addr, file_addr),2);
	ck_assert_int_eq(get_counter(proc_counter_addr, pid),2);
	
	lseek(c_fd, 0, SEEK_SET);
	read(c_fd, sys_result, sizeof(result));

	sys_result[1] = '\0';
	ck_assert_int_eq(atoi(sys_result), 2);
	
	session_close(session_id, fd);
	ck_assert_int_eq(get_counter(file_counter_addr, file_addr),1);
	ck_assert_int_eq(get_counter(proc_counter_addr, pid),1);
	
	session_close(session_id, fd2);
	ck_assert_int_eq(get_counter(file_counter_addr, file_addr),0);
	ck_assert_int_eq(get_counter(proc_counter_addr, pid),0);
	
	lseek(c_fd, 0, SEEK_SET);
	read(c_fd, sys_result, sizeof(result));
	sys_result[1] = '\0';
	ck_assert_int_eq(atoi(sys_result), 0);

	close(fd);
	close(fd2);
	close(c_fd);
	session_exit(session_id);
	
}
END_TEST

START_TEST(test_fork_processes)
{
	char filename[FILENAME_SIZE];
	char base_addr[BUFFER_LENGTH];
	char file_addr[BUFFER_LENGTH];
	const char *strings[CHILDREN_NUMBER];
	int session_id;
	int fd;
	int rw_result;
	pid_t processes[CHILDREN_NUMBER];
	pid_t pid;
	int i;
	/* Initializing the array of strings */
	strings[0] = "Lorem ipsum dolor sit amet quam.";
	strings[1] = "Lorem ipsum dolor sit amet nisi.";
	strings[2] = "Praesent nec mi sed felis morbi.";
	strings[3] = "Donec ac leo et nisl erat curae.";
	strings[4] = "Vivamus tristique dolor sit sed.";
	
	 /* Getting a valid path for test file */
	int path_fd = open("/sys/kernel/session-module/path", O_RDONLY);
	ck_assert_int_ge(read(path_fd, base_addr, sizeof(base_addr)), 0);
	close(path_fd); 
	sprintf(file_addr, "%s/test_file", base_addr);
	
	/* In this for the process is forked and they try to write everyone on its session */
	
	for(i=0;i<CHILDREN_NUMBER;i++)
	{
		switch(pid = processes[i] = fork()){
			case 0:
						session_id = session_init(); 

						fd = open(file_addr, O_CREAT | O_RDWR, 0644);
						ck_assert_int_ge(fd, 0);
						session_open(session_id, fd);
						rw_result = write(fd, strings[i%5], strlen(strings[i%5]));
						ck_assert_int_ge(rw_result, 0);
						session_close(session_id, fd);
						session_exit(session_id);

						exit(0);
				break;
			
			default:
				break;
			
		}
	}
	/* The parent waits for the termination of all children and then checks the file */
	for(i=0;i<CHILDREN_NUMBER;i++) {
		waitpid(processes[i], NULL, 0);
	}
	fd = open(file_addr, O_RDONLY);
	char buffer[BUFFER_LENGTH];
	ck_assert_int_ge(read(fd, buffer, BUFFER_LENGTH), 0);
	int presence = 0;
	for(i=0;i<5;i++)
	{
		if(strcmp(buffer, strings[i])==0)
		{
			presence=1;
			break;
		}
	}
	ck_assert_int_eq(presence, 1);
	
	
}
END_TEST
START_TEST(test_signal)
{
int fd, fd2, session_id;
	char buf[8];

		session_id = session_init();
		fd = open("test_file", O_CREAT | O_RDWR, 0644);
		
		session_open(session_id, fd);
		write(fd, "fd1\0", 4);
		fd2 = open("test_file", O_RDONLY);

		session_open(session_id, fd2);
		session_close(session_id, fd);
		unlink("test_file");
		write(fd, "fd2\0", 4);
		session_close(session_id, fd2);
		printf("Errore %s\n", strerror(errno));
		session_exit(session_id);
		
}
END_TEST
Suite *session_suite(void)
{
	Suite *s;
	TCase *tc_core;
	
	s = suite_create("Core functionalities Tests");
	tc_core = tcase_create("Core");
	
	tcase_add_test(tc_core, test_session_init);
	tcase_add_test(tc_core, test_file_op);
	tcase_add_test(tc_core, test_errors);
	tcase_add_test(tc_core, test_sysfs);
	tcase_add_test(tc_core, test_fork_processes);
	tcase_add_test_raise_signal(tc_core, test_signal, SIGPIPE);
	suite_add_tcase(s, tc_core);
	return s;
}


 int main(void)
 {
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = session_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
 }

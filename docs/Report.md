# Advanced Operating Systems and Virtualization - A.A. 2018/2019
# Final Project Report

**Author:** Federico Alfano



## Introduction

It's a subsystem that makes the user able to access a file using sessions. In this way every modification is not visible until the session is close. Many processes can access concurrently a given file.
The system provides also some statistical information into the /sys/kernel/session-module directory where the user can:
-  See the number of total open sessions
-  See the number of sessions per-process and per-file
-  See and change the path where the user can open sessions

The idea is to create a new file, copy the old one into it and then redirect all the VFS operations on the session file overwriting the file operations. At the close, the session file is will be copied back into the original one and then is unlinked.

## Usage

The module needs to be compiled and then inserted, so with a terminal open into the module directory we need to launch the following commands:

```
make
sudo insmod session-module.ko
```
On the contrary if we want to remove the module this is the right command:
```
sudo rmmod session-module
```
The user can be allowed to use the module's facilities by installing the userspace library into the libsession folder with those commands
```
make
sudo make install
```
and then copying this code inside the source of your program

```
#ifndef SESSION
#define SESSION


extern int session_init(void);

extern int session_open(int , int );

extern int session_close(int , int );

extern int session_exit(int );

#endif
```
finally you need to compile with the -lsession flag

## User Space Library

The userspace library is a simple interface that provides the user a set of operations to interact with the module. In particular the operations needed are:
- session_init: it starts the session and provides a session_id that is the file descriptor of the module
- session_open: it calls the library in order to open a session on a given file, it needs as parameters the session_id and the file descriptor onto we want to open a session and it returns 0 if no error occurs and an error code if it fails.
- session_close: it calls the library in order to close a session on a given file, it needs as parameters the session_id and the file descriptor onto we want to close a session.
- session_exit: it ends the session closing the module.

All the operations are a call to the facilities provided by the module
```
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
```

## Kernel-Level Data Structures

The subsystem has two global rbtrees that are represented respectively by:
- struct session_proc_node
- struct session_file_node

They contain the counter for the per-process sessions and the per-file sessions. 
The system relies also on two global variables:
- The base_path
- session_counter

The former is used to store the current base path where the sessions can live, the latter keeps track of the total number of sessions.
All data can be read into the /sys directory and the path can also be modified from the user.

## Kernel-Level Subsystem Implementation

### Initialization
The initialization allocates dynamically a MAJOR and a single MINOR for the char device and then initializes that and set the global variable *my_cdev* that is a struct defined into the header,
containing the char device and a kobject. The next step is the build of the sysfs tree that contains all the files needed for keeping track of sessions and for managing the path


![Sysfs structure](../sysfs.png)


The last step is to set the PWD as default base path

```

get_fs_pwd(current->fs ,&base_path);

```

### Module operations
The device initialization wants file_operations structure in order to control the behaviour of the module; the following implementations only need three of them:
- open 
- release 
- ioctl_unlocked 

The third one is the essence of the module, so it deserves a dedicated paragraph.
### module_open
It's called when the user wants to start to work with sessions, so the first thing to do is to check if a session is already open and return an EEXIST error if it's so
```
if((node = rb_search(&root, pid, &tree_rwlock)))
		return -EEXIST;
```

after that, it will create a node into the rbtree containing the processes:
```
node = kmalloc(sizeof(struct session_proc_node), GFP_KERNEL);
	scnprintf(node->key, PROC_NAME_LENGTH, "%d", current->pid);
	atomic_set(&node->session_counter, 0);
	rb_insert(&root, node, &tree_rwlock);
``` 
The insertion is sensitive to race conditions, so it keeps a rwlock that is managed into the helper function in *tree_utils.c*.
And finally it creates a file into  /sys/kernel/session_module/proc directory with the number of open sessions.

```
proc_counter_attr = get_attribute(node->key, 0444, proc_counter_show, NULL);
	node->attr = (const struct attribute*) &proc_counter_attr->attr;
	sysfs_create_file(proc_kobj, &proc_counter_attr->attr);
```

### module_release

Basically, the release method is a check to verify if the user has closed the module in the right way.
```

	if(current_node!=NULL)
	{
		clear_proc_node();					
		send_sig(SIGPIPE, current, 0);
	}
```

### module_ioctl
The function is in charge to manage all the operations on the file, let's see them in detail:

#### OPEN_SESSION
This is the operation called at the opening of the session. After some checks on the validity of the file and of permissions it performs the following operations:

- Creates a temporary file with the same content and flags of the original one

```

			session_filp = filp_open(cur_addr, O_TMPFILE | O_RDWR  , 0644);
			down_read(priv_data->rw_sem);
			err = vfs_copy_file_range(original_filp, 0, session_filp, 0, i_size_read(original_filp->f_inode), 0);
			up_read(priv_data->rw_sem);
			session_filp->f_flags = original_filp->f_flags;
```
this file will be pointed out by the *private_data*  field which is located into the original file. This one is a special structure defined in the header which contains the session file pointer, the absolute path and a semaphore
that is used to protect sessions during the close.
- It creates a node needed to keep track of opened file, if it doesn't exists. Otherwise it increments atomically the counter:

```
tmp_key = d_path(&original_filp->f_path, tmp_addr, MAX_PATH_SIZE);
			f_node = rb_search_file(&root_files, tmp_key, &tree_rwlock_files);
			if(f_node== NULL)
				{

				f_node = kmalloc(sizeof(struct session_file_node), GFP_KERNEL);
				
				atomic_set(&f_node->counter,1);
				strcpy(f_node->key, tmp_key);
				rb_insert_file(&root_files, f_node, &tree_rwlock_files);
			}
			else
			{
				atomic_inc(&f_node->counter);
				
			}
```

- Creates a **struct file_operations** based on the one present in the *original_filp* and replaces the needed operations with the new ones

```
fops_replacement = kmalloc(sizeof(struct file_operations), GFP_KERNEL);
								
			*fops_replacement = (struct file_operations) *original_filp->f_op;
			fops_replacement->write = session_write;
			fops_replacement->read = session_read;
			fops_replacement->llseek = session_llseek;
			fops_replacement->release = session_release;  
			replace_fops(original_filp, fops_replacement);
```

And finally it increments the global counter and the counter of the sessions opened by the process

#### CLOSE_SESSION
The close_session is responsible of closing the file and of copying the content of the session into the original file. All is done thanks to the function  *flush* that after
some operations makes its job:
```
	down_write(priv_data->rw_sem);
	vfs_truncate(&filp->f_path, 0);
	err = vfs_copy_file_range(session_file, 0, filp, 0, file_size, 0); 
	up_write(priv_data->rw_sem);
```

after that it restores the default file operations
```
replace_fops(filp, inodep->i_fop);
```

The flush returns an EPIPE error if the given file is removed from the filesystem, checking at the variable *i_nlink*
```
	if(inodep->i_nlink==0)
	{
		send_sig(SIGPIPE, current, 1);
		up_write(priv_data->rw_sem);
		printk(KERN_ERR "Sigpipe is sent");
		return -EPIPE;
	}
```

## Testcase and Benchmark

### Correctness
The whole developement had a test driven approach, all tests are contained into the tests/unit_tests folder, they were performed throught the  *check*, a unit test framework. In particular
the file test.c contains the main functions tested during the development, looking the code it's easy to understand that the tests were about:
- Initialization
- Session operations on the file
- Main errors
- Sysfs correctness
- Some basic tests on correctness with concurrency
- Test of the correct raise of SIGPIPE

```
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
```
### Performance

The file perf_test compiled into the make with the -pg option tries many times a write operation with and without sessions:
```
	if(argc<3)
		printf("Usage: perf_test.c file1 file2");
	int i;
	
	for(i=0; i<TRIES;i++)
		write_with_session(argv[1]);
	for(i=0; i<TRIES;i++)
		write_without_session(argv[2]);
	return 0;
```

The tests are performed initially with empty file and they shows no significant drop of performances with a relative low number of try.
When  tries are increased with the O_APPEND mode  (more than 10k) the write with session takes more than the 99% of the time.
The analysis is made with **gproof** with the following command into the terminal after the execution:
```
gprof perf_test gmon.out > little_file_analysis

```
so the file *little_file_analysis* contains the report of the previous test.

the second test is performed with a medium file (2MB instead of 100byte) and 1k tries and with 10k tries. We can see that the drop of performace
always belongs to the vfs_copy_file_range call into the kernel as we expected.
All analysis are provided into the test folder.

### Note:
All the tests were performed on a LUbuntu 16 distro with kernel version: 4.15.0.


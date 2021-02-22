/**
 * @file session_module.h
 * @author Federico Alfano
 * @date 2 May, 2020
 * @brief The header of the main functions that allows the user a session-based file access semantic
 *
 */
#ifndef SESSION_HEADER
#define SESSION_HEADER

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/fdtable.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/kobject.h> 
#include <linux/sysfs.h>
#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/dcache.h>
#include <linux/sched/signal.h>
#include <linux/file.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "session-module"

#define SESSION_INSTANCES 1

#define SESSION_MAJOR_NUMBER 0	/**< The Major number of the driver if 0 it's chosen dynamically by the system, please change only if you know what you are doing*/
#define SESSION_BASE_MINOR 0			/**< This is the base minor, it's not critical as the driver is designed to work with just one instance */

#define SESSION_CLASS "session-class"

#define SESSION_MAGIC 'S'
#define SESSION_OPEN_SEQ_NO 0X1
#define SESSION_CLOSE_SEQ_NO 0X2

#define MAX_FILENAME_SIZE 128			/**< The maximum filename length, you can compile it with different values if you know the maximum length of your files */
#define MAX_PATH_SIZE 256					/**< The maximum length of the path */
#define MAX_FILE_NUM 256						/**< It indicates how many files can be represented, this macro is useful to indicate to the file_counter how many lines can show */
#define FILE_SESSIONS_SIZE (MAX_PATH_SIZE*MAX_FILE_NUM)
#define N_SESS_DIGITS 8							/**< Indicates of how many digits the session_counter can have, (e.g. setting at 3, the subsystem cannot show more than 999 session) */
#define N_PROC_DIGITS 6							/**< Indicates of how many digits the proc_counter can have, in other words are maximum number of digits needed to represent the number of per-process sessions */

#define PROC_NAME_LENGTH 8	


#define SESSION_OPEN _IOW(SESSION_MAGIC, SESSION_OPEN_SEQ_NO, unsigned int)
#define SESSION_CLOSE _IOW(SESSION_MAGIC, SESSION_CLOSE_SEQ_NO,  unsigned int)


/**
 *  @struct session_cdev
 *  @brief The wrapper containing the char device used to manage sessions plus the kobject needed into the /sys to get info about the open sessions
 */ 

struct session_cdev {
	struct cdev cdev;
	struct kobject *kobj;

	};

/**
 * @struct session_file_node
 * @brief This is the structure into the rb_tree that contains all the open files with their counter
 * 
 */
struct session_file_node
{
	char key[MAX_PATH_SIZE];		 /**<  the key on which the search method will find the needed node, it's the absolute path of the file */
	struct rb_node node;			 		
	struct rw_semaphore *rw_sem;	/**<   the semaphore that protects the file during the read and the write into ioctl and flush operations. */
	atomic_t counter;							/**<   the field that stores an atomic value indicating how many times the file is opened with sessions. */
	
};

/**
 * @struct session_proc_node
 * @brief This is the structure into the rb_tree that contains all the processes which are currently using the sessions
 * 
 */
struct session_proc_node
{
	int key;								 /**<  the key on which the search method will find the needed node, its the pid*/
	struct rb_node node;
	atomic_t session_counter;   /**< the field that stores an atomi value indicating how many sessions are opened by the process */
};
/**
 * @struct session_priv_data
 * @brief This structure is linked to the original file through the field **private_data** and the main purpose is to keep the pointer to the session file
 * 
 */
struct session_priv_data
{
	struct rw_semaphore *rw_sem;	/**< This is a redundance, is a copy of the one present into the struct session_file_node, prevents a search into the rb_tree*/
	struct file *session_filp;					/**< This is the real sense of this structure, it's a pointer pointing to the session_file, the incarnation of the original one and the core of the sessions*/
	char *abs_path;								/**< This is a redundance, it avoid the use of the d_path */
	
};


int module_open(struct inode *, struct file *);
int module_release(struct inode *, struct file *);
long module_ioctl(struct file *, unsigned int, unsigned long );

/**
 * @brief Closes the session related to filp, unlinking the temporary session node,
 * it's called just after an unexpected close. The right way to close a session is
 * with the SESSION_CLOSE from ioctl that replaces the file operations from the 
 * original file
 * 
 * @param inodep:	the inode structure pointer referred by the call
 * @param filp:	the file structure pointer referred by the call
 * @return 0 or error code
 */
int session_release(struct inode *, struct file *);

/**
 *  @brief It reads is onto the session file, every concurrent
 * change onto the original file between the session open and the session release
 * are invisible to the read
 * 
 * @param filp:	the file structure pointer referred by the call
 * @param buff:	the destination buffer with data to write
 * @param count:	maximum number of bytes to read
 * @param offp:	where the read has to start
 * @return return  the number of bytes read or error code 
 */
ssize_t session_read(struct file *, char __user *, size_t ,loff_t *);

/**
 * @brief It seeks the position into the session file
 * 
 * @param filp:	the file structure pointer referred by the call
 * @param off:	the seek offset
 * @param whence:	the seek method
 * @return returns the resulting offset  location  as  measured  in bytes from the beginning of the file or an error code
 */
loff_t session_llseek(struct file *, loff_t, int);

/**
 * @brief It writes only onto session file keeping immutate
 * the original one
 * 
 * @param filp:	the file structure pointer referred by the call
 * @param buff:	the buffer containing the data to write
 * @param count:	maximum number of bytes to write
 * @param offp:	where the write has to start
 * @return  the resulting offset  location  as  measured  in bytes from the beginning of the file or an error code
 */
ssize_t session_write(struct file *, const char __user *, size_t, loff_t *);

int clear_proc_node(void);

void clear_sysfs_file(struct file *);

void traverse(struct rb_node*, char*);
void traverse_proc(struct rb_node *, char *);

void read_file(struct file *);
int rb_insert(struct rb_root *, struct session_proc_node *,  rwlock_t *);
int rb_insert_file(struct rb_root *, struct session_file_node *,  rwlock_t *);
struct session_proc_node *rb_search(struct rb_root *, int,  rwlock_t *);
struct session_file_node *rb_search_file(struct rb_root *, char*,  rwlock_t *);
int session_exit(struct session_proc_node *);
int setup_sys_tree(void);
int flush(struct inode *, struct file *);
ssize_t file_counter_show(struct kobject *, struct kobj_attribute *, char *);
ssize_t proc_counter_show(struct kobject *, struct kobj_attribute *, char *);
ssize_t path_show(struct kobject *, struct kobj_attribute *, char *);
ssize_t path_store(struct kobject *, struct kobj_attribute *, const char *, size_t );
ssize_t session_counter_show(struct kobject *, struct kobj_attribute *, char *);


#endif


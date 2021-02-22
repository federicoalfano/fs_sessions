#include "session_module.h"

dev_t dev;

static struct class* session_class; 
static struct session_cdev* my_cdev;

static struct rb_root root = RB_ROOT;
static struct rb_root root_files = RB_ROOT;

struct path base_path;

static atomic_t session_counter = ATOMIC_INIT(0);

DEFINE_RWLOCK(tree_rwlock);
DEFINE_RWLOCK(tree_rwlock_files);	

struct kobj_attribute path_attr = __ATTR_RW(path); 
struct kobj_attribute counter_attr = __ATTR_RO(session_counter);
struct kobj_attribute file_counter_attr = __ATTR_RO(file_counter);
struct kobj_attribute proc_counter_attr = __ATTR_RO(proc_counter);


struct file_operations device_fops = {
	
	.owner = THIS_MODULE,
	.open = module_open,
	.release = module_release,
	.unlocked_ioctl = module_ioctl,
	
};


ssize_t session_write(struct file *filp, const char __user *buff, size_t count, loff_t *offp)
{	
	struct session_priv_data *priv_data;
	struct file *session_file;
	size_t ret;
	
	priv_data = (struct session_priv_data *) filp->private_data;
	ret=0;
	if(IS_ERR(priv_data->session_filp))
			return -ENOENT;
	session_file = priv_data->session_filp;
	ret = vfs_write(session_file, buff, count, &session_file->f_pos);
	if(ret>0){
			filp->f_pos += ret;
		}
		
	return ret;
}

ssize_t session_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{	

	struct session_priv_data *priv_data;
	struct file *session_file;
	size_t ret;
	
	priv_data = (struct session_priv_data *) filp->private_data;
	ret = 0;
	
	if(IS_ERR(priv_data->session_filp))
			return -ENOENT;
	session_file = priv_data->session_filp;

	ret = vfs_read(session_file, buff, count, &session_file->f_pos);

	return ret;
			

}

loff_t session_llseek(struct file *filp, loff_t off, int whence)
{
	struct session_priv_data *priv_data;
	struct file *session_file;
	size_t ret;
	
	priv_data = (struct session_priv_data *) filp->private_data;
	
	
	if(IS_ERR(priv_data->session_filp))
			return -ENOENT;
	session_file = priv_data->session_filp;
	ret = vfs_llseek(session_file, off, whence);
	return ret;
	
}

void clear_sysfs_file(struct file *filp)
{	
	struct session_priv_data *priv_data;
	struct session_file_node *sysfs_data;
	char *file_key;
	
	priv_data = (struct session_priv_data *) filp->private_data;
	file_key = priv_data->abs_path;
	
	if((sysfs_data= rb_search_file(&root_files, file_key, &tree_rwlock_files)) && (atomic_dec_return(&sysfs_data->counter) ==0))
		{

			write_lock(&tree_rwlock_files);

			rb_erase(&sysfs_data->node, &root_files);

			write_unlock(&tree_rwlock_files);
			kfree(sysfs_data);
			

		}
	atomic_dec(&session_counter);
} 

int clear_proc_node(){
	struct session_proc_node *current_node;
	pid_t pid;
	
	pid = current->pid;

	current_node = rb_search(&root, pid, &tree_rwlock);

	if(current_node)
		{
			write_lock(&tree_rwlock);

			rb_erase(&current_node->node, &root);

			write_unlock(&tree_rwlock);	
		}
					
	return 0;
}

int flush(struct inode *inodep, struct file *filp)
{
	int err;
			
	struct session_priv_data *priv_data;
	struct file *session_file;
	struct inode *parent;
	size_t file_size;
	
	priv_data = (struct session_priv_data *) filp->private_data;
	session_file = priv_data->session_filp;
	file_size = i_size_read(session_file->f_inode);
		
	session_file->f_inode->i_mode = inodep->i_mode;
	parent = filp->f_path.dentry->d_parent->d_inode;

	/* file_out must have the O_APPEND flag unset in order to use vfs_copy_file_range*/

	filp->f_flags = filp->f_flags & ~O_APPEND;
	
	/* write lock onto the original file to protect from other sessions 
	 *  If the number of hard_links is 0 the file is unlinked, so it raises a sigpipe
	  */
	
	down_write(priv_data->rw_sem);
	if(inodep->i_nlink==0)
	{
		send_sig(SIGPIPE, current, 1);
		up_write(priv_data->rw_sem);
		return -EPIPE;
	}
	vfs_truncate(&filp->f_path, 0);
	err = vfs_copy_file_range(session_file, 0, filp, 0, file_size, 0); 
	up_write(priv_data->rw_sem);

	kfree(filp->private_data);
	replace_fops(filp, inodep->i_fop);	

	return 0;
}

int session_release(struct inode * inodep, struct file * filp)
{		
	
	struct session_priv_data *priv_data;	
		
	if(!IS_ERR(filp)) {
	priv_data = (struct session_priv_data *) filp->private_data;
	clear_sysfs_file(filp);
	
	kfree(priv_data);
	
	
	}
	inodep->i_fop->release(inodep, filp);
	return 0;
}

long module_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{	
	
	int err;

	struct file *original_filp = NULL;
	struct file *session_filp;
	struct session_file_node *f_node;
	struct path dir_path;
	struct file_operations *fops_replacement;

	pid_t pid;
	char *cur_addr;
	char *tmp_key;
	char tmp_addr[MAX_PATH_SIZE] = {0};

	struct session_proc_node *current_node;
	int o_fd;
	
	
	pid = current->pid;
	current_node = rb_search(&root, pid, &tree_rwlock);	

	get_user(o_fd,  (unsigned long*)arg);



	switch(cmd){
		case SESSION_OPEN:
					
			if(o_fd<=0){
						
				return -EBADF;
			}
			original_filp = fcheck(o_fd);
			if(IS_ERR(original_filp))
			{
				return -EBADF;
						
			}
				
			/* Building the path structure of the path directory, in order to have the session file with the same parent of the original one */
			dir_path.mnt = original_filp->f_path.mnt;
			dir_path.dentry = original_filp->f_path.dentry->d_parent;

			/* Building the address where to create the session file */		
			cur_addr = d_path(&dir_path, tmp_addr, MAX_PATH_SIZE);

			 if(!path_is_under(&dir_path, &base_path))
			{
				return -EACCES;
			} 

			/* Creating the session file */
			session_filp = filp_open(cur_addr, O_TMPFILE | O_RDWR  , 0644);
			if(IS_ERR(session_filp))
				return -EIO;
			
			/* The key is the path to the original file */
			tmp_key = d_path(&original_filp->f_path, tmp_addr, MAX_PATH_SIZE);
			f_node = rb_search_file(&root_files, tmp_key, &tree_rwlock_files);
			if(f_node== NULL)
				{

				f_node = kmalloc(sizeof(struct session_file_node), GFP_KERNEL);
				
				atomic_set(&f_node->counter,1);
				strcpy(f_node->key, tmp_key);
				f_node->rw_sem = kmalloc(sizeof(struct rw_semaphore), GFP_KERNEL);
				init_rwsem(f_node->rw_sem);
				rb_insert_file(&root_files, f_node, &tree_rwlock_files);
			}
			else
			{
				atomic_inc(&f_node->counter);	
			}

			/* Creation of the session file */

			down_read(f_node->rw_sem);
			err = vfs_copy_file_range(original_filp, 0, session_filp, 0, i_size_read(original_filp->f_inode), 0);
			up_read(f_node->rw_sem);
			
			session_filp->f_flags = original_filp->f_flags;
			
			original_filp->private_data = kmalloc(sizeof(struct session_priv_data), GFP_KERNEL);
			
			((struct session_priv_data*)original_filp->private_data)->session_filp = session_filp;
			((struct session_priv_data*)original_filp->private_data)->rw_sem = f_node->rw_sem;
			((struct session_priv_data*)original_filp->private_data)->abs_path = f_node->key;
					
			/* Setting the new temporary operations on the open file */
			
			fops_replacement = kmalloc(sizeof(struct file_operations), GFP_KERNEL);
								
			*fops_replacement = (struct file_operations) *original_filp->f_op;
			fops_replacement->write = session_write;
			fops_replacement->read = session_read;
			fops_replacement->llseek = session_llseek;
			fops_replacement->release = session_release;  
			replace_fops(original_filp, fops_replacement);
			atomic_inc(&current_node->session_counter);
			atomic_inc(&session_counter);
			break;
			
		case SESSION_CLOSE:
		
			if(o_fd<=0){
					
			return -EBADF;
			}	
			if(o_fd<=0){
						
				return -EBADF;
			}
			original_filp = fcheck(o_fd);
			if(IS_ERR(original_filp))
			{
				return -EBADF;
						
			}
			
			
			if (!current_node)
				return -EPERM;
		
			/* Check if the file has an active session */
			if(!original_filp->private_data)
				return -EPERM;

			err = flush(original_filp->f_inode, original_filp);
			if(err<0)
				return err;
			clear_sysfs_file(original_filp);

			atomic_dec(&current_node->session_counter);
			break;
		

		default:
			return -EINVAL;
				
	}
	return 0;
}



int module_open(struct inode * inodep, struct file * filp)
{
	struct session_proc_node *node;

	if((node = rb_search(&root, current->pid, &tree_rwlock)))
		return -EEXIST;
	
	node = kmalloc(sizeof(struct session_proc_node), GFP_KERNEL);
	node->key = current->pid;
	atomic_set(&node->session_counter, 0);
	rb_insert(&root, node, &tree_rwlock);
	return 0;
}

int module_release(struct inode * inodep, struct file * filp)
{
	struct session_proc_node *current_node;
	pid_t pid;

	pid = current->pid;
	
	current_node = rb_search(&root, pid, &tree_rwlock);
	
	if(current_node!=NULL)
	{
		clear_proc_node();	
	}

	return 0;
}

 static char *mydevnode(struct device *dev, umode_t *mode)
  {
      if (mode)
          *mode = 0666; /* Permission needed for device node */
      return NULL; /* could override /dev name here too */
  }

ssize_t path_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
				char tmp[MAX_PATH_SIZE];
				char *path = d_path(&base_path, tmp, MAX_PATH_SIZE);

				return scnprintf(buf, MAX_PATH_SIZE, "%s", path);
			}
ssize_t path_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t len){
	
				int err;
				size_t count;
				char path[MAX_PATH_SIZE];
				
				count = len;

				scnprintf(path, len, "%s", buf);
				if(len>0 && path[len-1]=='\n')
					--count;
				path[count] = '\0';

				err = kern_path(path, LOOKUP_FOLLOW | LOOKUP_DIRECTORY, &base_path);
				
				if(err)
					return err;
				
				return len;
			}

ssize_t session_counter_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
	int read_value = atomic_read(&session_counter);
	return scnprintf(buf,N_SESS_DIGITS, "%d", read_value);
}

ssize_t proc_counter_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
	
	char *out = vmalloc(FILE_SESSIONS_SIZE);
	strcpy(out, "");
	traverse_proc(root.rb_node, out);
	return scnprintf(buf, FILE_SESSIONS_SIZE, "%s", out);
	
}

ssize_t file_counter_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
	
	char *out = vmalloc(FILE_SESSIONS_SIZE);
	strcpy(out, "");
	traverse(root_files.rb_node, out);
	return scnprintf(buf, FILE_SESSIONS_SIZE, "%s", out);
	
}

static int __init init_session_module(void)
{
	int error;
	struct device* device_error;

	pr_info( "%s: Module inserted\n", DEVICE_NAME);
	
	error = alloc_chrdev_region(&dev, SESSION_BASE_MINOR, SESSION_INSTANCES, DEVICE_NAME);
	
	if(error < 0)
	{
		pr_err( "Failed to allocate region, can't get major\n");
		error = -ENOMEM;
		goto fail_alloc;

	}
		
	my_cdev = kmalloc(sizeof(struct session_cdev)*SESSION_INSTANCES, GFP_KERNEL);
	
	cdev_init(&my_cdev->cdev, &device_fops);

    error = cdev_add(&my_cdev->cdev, dev, SESSION_INSTANCES );

	if ( error ) {
        pr_err( "Registration has failed \n" );
        goto fail_cdev_add;
    }
      
    session_class = class_create(THIS_MODULE, SESSION_CLASS);

    session_class->devnode = mydevnode;
    
    device_error= device_create(	session_class,
										NULL,
										dev,
										NULL,
										DEVICE_NAME);
										
	if(IS_ERR(device_error)) {
		pr_err( "Device creation has failed \n" );
		goto fail_device_creation;

	}
	
	setup_sys_tree();
	
	/* The first legal path is the one from where the module has started by default */
	get_fs_pwd(current->fs ,&base_path);
	return 0;
	
	fail_device_creation: 	device_destroy(session_class, dev);
							class_destroy(session_class);						
	
	fail_cdev_add:		kfree(my_cdev);

	fail_alloc:	unregister_chrdev_region(dev, SESSION_INSTANCES);
	
	return error;
	
}

int setup_sys_tree()
{

	my_cdev->kobj = kobject_create_and_add("session-module", kernel_kobj);

	sysfs_create_file(my_cdev->kobj, &path_attr.attr);
	sysfs_create_file(my_cdev->kobj, &counter_attr.attr);
	sysfs_create_file(my_cdev->kobj, &file_counter_attr.attr);
	sysfs_create_file(my_cdev->kobj, &proc_counter_attr.attr);
	
	return 0;
}

static void __exit exit_session_module(void)
{
	kobject_put(my_cdev->kobj);
	device_destroy(session_class, dev);
	class_destroy(session_class);
	kfree(my_cdev);
	unregister_chrdev_region( dev, SESSION_INSTANCES );
	pr_info( "%s: Module removed\n", DEVICE_NAME);
}

module_init(init_session_module);
module_exit(exit_session_module);

MODULE_AUTHOR("Federico Alfano");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Linux module which allows to access files in a specific directory of the VFS using sessions.");


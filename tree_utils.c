#include "session_module.h"

  struct session_proc_node *rb_search(struct rb_root *root, int key , rwlock_t *tree_rwlock)
  {
  	struct rb_node *node = root->rb_node;
  	read_lock(tree_rwlock);
  	while (node) {
  		struct session_proc_node *data = container_of(node, struct session_proc_node, node);
		int result;
		result = data->key - key;
		if (result < 0)
  			node = node->rb_left;
		else if (result > 0)
  			node = node->rb_right;
		else
		{
			read_unlock(tree_rwlock);
  			return data;
		}
	}
	read_unlock(tree_rwlock);
	return NULL;
  }
  
    struct session_file_node *rb_search_file(struct rb_root *root, char* key , rwlock_t *tree_rwlock)
  {
  	struct rb_node *node;
  	read_lock(tree_rwlock);
  	
  	node = root->rb_node;
	
  	while (node) {
  		struct session_file_node *data = container_of(node, struct session_file_node, node);
		int result;
		result = strcmp(data->key, key);
		if (result < 0){
  			node = node->rb_left;
  			
		}
		else if (result > 0){
  			node = node->rb_right;		
		}
		else
		{
			read_unlock(tree_rwlock);
  			return data;
		}
	}

	read_unlock(tree_rwlock);
	return NULL;
  }
  
    int rb_insert(struct rb_root *root, struct session_proc_node *data, rwlock_t *tree_rwlock)
  {
  	struct rb_node **new = &(root->rb_node), *parent = NULL;


  	write_lock(tree_rwlock);

  	while (*new) {
  		struct session_proc_node *this = container_of(*new, struct session_proc_node, node);
		int result = this->key - data->key;
		parent = *new;
  		if (result < 0)
  			new = &((*new)->rb_left);
  		else if (result > 0)
  			new = &((*new)->rb_right);
  		else
  		{
			read_unlock(tree_rwlock);
  			return 0;
		}
  	}

  	rb_link_node(&data->node, parent, new);
  	rb_insert_color(&data->node, root);
  	write_unlock(tree_rwlock);
	return 1;
  }

 int rb_insert_file(struct rb_root *root, struct session_file_node *data, rwlock_t *tree_rwlock)
  {
  	struct rb_node **new = &(root->rb_node), *parent = NULL;


  	write_lock(tree_rwlock);

  	while (*new) {
  		struct session_file_node *this = container_of(*new, struct session_file_node, node);
  		int result = strcmp(this->key, data->key);
		parent = *new;
  		if (result < 0)
  			new = &((*new)->rb_left);
  		else if (result > 0)
  			new = &((*new)->rb_right);
  		else
  		{
			read_unlock(tree_rwlock);
  			return 0;
		}
  	}

  	rb_link_node(&data->node, parent, new);
  	rb_insert_color(&data->node, root);
  	write_unlock(tree_rwlock);
	return 1;
  }


  
void traverse(struct rb_node *node, char *out)
  {
	  
	if(!node)
		return;
	else
		{
  		struct session_file_node *data = container_of(node, struct session_file_node, node);
		char node_line[MAX_PATH_SIZE+2];
		sprintf(node_line, "%s\t%d\n", data->key, atomic_read(&data->counter));	
		traverse(node->rb_left, out);
		strcat(out, node_line);
  		traverse(node->rb_right, out);	
	}
  }
  
  
  void traverse_proc(struct rb_node *node, char *out)
  {
	  
	if(!node)
		return;
	else
		{
  		 struct session_proc_node *data = container_of(node, struct  session_proc_node, node);
		char node_line[MAX_PATH_SIZE+2];
		sprintf(node_line, "%d\t%d\n", data->key, atomic_read(&data->session_counter));	
		traverse_proc(node->rb_left, out);
		strcat(out, node_line);
  		traverse_proc(node->rb_right, out);	
	}
  }

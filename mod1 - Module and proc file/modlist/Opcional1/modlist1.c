#include <linux/module.h> 	/* Modules */
#include <linux/kernel.h> 	/* KERNINFO */
#include <linux/vmalloc.h> 	/* vmalloc() */
#include <linux/proc_fs.h>	/* /proc files */
#include <linux/list.h>		/* list macros */
#include <linux/uaccess.h> 	/* copy_from_user() & copy_to_user() */
#include <linux/string.h>	/* string manipulation */

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Linked List Module - Arquitectura Interna de Linux y Android UCM");
MODULE_AUTHOR("Ramón Costales de Ledesma, José Ignacio Daguerre Garrido");

/* Usage:
	> make										//compiles program (int list)
	> sudo insmod modlist1.ko 					//loads module
	> echo add 345 > /proc/modlist1				//adds 345 to the list
	> echo add 2 > /proc/modlist1 				//adds 2 to the list
	> echo remove 2 > /proc/modlist1 			//removes all 2's from the list
	> echo cleanup > /proc/modlist1 			//deletes everything from the list
	> sudo rmmod modlist1 						//unloads module
	> make clean 								//deletes compilation objects

	> make EXTRA_CFLAGS=-DPARTE_OPCIONAL		//compiles program (string list)
	> sudo insmod modlist1.ko 					//loads module
	> echo add 345 > /proc/modlist1				//adds 345 to the list
	> echo add asdfogsgd > /proc/modlist1 		//adds asdfogsgd to the list
	> echo add asdfogsgd > /proc/modlist1 		//adds asdfogsgd to the list
	> echo remove asdfogsgd > /proc/modlist1	//removes all asdfogsgd's from the list
	> echo cleanup > /proc/modlist1 			//deletes everything from the list
	> sudo rmmod modlist1 						//unloads module
	> make clean 								//deletes compilation objects
*/

#define MAX_CHARS 50
#define MAX_DIGS 128

/* /proc entry */
static struct proc_dir_entry *proc_entry;

/* Linked list */
struct list_head mylist; 

/* List nodes int */
struct list_item {
	int data;
	struct list_head links;
};

/* List nodes String */
struct list_item_string {
	char *data;
	struct list_head links;
};



/* Adds num to the list */
void add(int num){
	/* Initialize node */
	struct list_item *node = vmalloc(sizeof(struct list_item));
	node->data = num;

	/* Adds the node at the end of the list */
	list_add_tail(&node->links, &mylist);
	printk(KERN_INFO "Modlist1: Added %d to the list.\n", num);
}

/* Adds string to the list */
void addString(char *str){
	/* Initialize node */
	struct list_item_string *node = vmalloc(sizeof(struct list_item_string));
	node->data = vmalloc(MAX_CHARS);
	strcpy(node->data, str);

	/* Adds the node at the end of the list */
	list_add_tail(&node->links, &mylist);
	printk(KERN_INFO "Modlist1: Added %s to the list.\n", str);
}

/* Removes all occurences of num from the list */
void remove(int num){
	/* Auxiliar pointers for the for loop */
	struct list_head *aux = NULL, *pos = NULL;
	/* Node to be freed */
	struct list_item *freeNode = NULL;
	
	/* Delete all occurences of num from the list and frees space */
	list_for_each_safe(pos, aux, &mylist){
		freeNode = list_entry(pos, struct list_item, links);
		if(freeNode->data == num){
			list_del(pos);
			vfree(freeNode);
			printk(KERN_INFO "Modlist1: Removed %d from the list.\n", num);
		}
	}
}

/* Removes all occurences of string from the list */
void removeString(char *str){
	/* Auxiliar pointers for the for loop */
	struct list_head *aux = NULL, *pos = NULL;
	/* Node to be freed */
	struct list_item_string *freeNode = NULL;
	
	/* Delete all occurences of str from the list and frees space */
	list_for_each_safe(pos, aux, &mylist){
		freeNode = list_entry(pos, struct list_item_string, links);
		if(strcmp(freeNode->data, str) == 0){
			list_del(pos);
			vfree(freeNode->data);
			vfree(freeNode);
			printk(KERN_INFO "Modlist1: Removed %s from the list.\n", str);
		}
	}
}

/* Empties the list */
void cleanup(void){
	/* Variables needed for the for loop */
	struct list_head *aux = NULL, *pos = NULL;
	/* Node to be freed */
	struct list_item *freeNode = NULL;
	
	/* Delete all nodes from the list and frees space */
	list_for_each_safe(pos, aux, &mylist){
		freeNode = list_entry(pos, struct list_item, links);
		list_del(pos);
		printk(KERN_INFO "Modlist1: Removed %d from the list.\n", freeNode->data);
		vfree(freeNode);
	}

	printk(KERN_INFO "Modlist1: Cleaned the whole list.\n");
}

/* Empties the list */
void cleanupString(void){
	/* Variables needed for the for loop */
	struct list_head *aux = NULL, *pos = NULL;
	/* Node to be freed */
	struct list_item_string *freeNode = NULL;
	
	/* Delete all nodes from the list and frees space */
	list_for_each_safe(pos, aux, &mylist){
		freeNode = list_entry(pos, struct list_item_string, links);
		list_del(pos);
		printk(KERN_INFO "Modlist1: Removed %s from the list.\n", freeNode->data);
		vfree(freeNode->data);
		vfree(freeNode);
	}

	printk(KERN_INFO "Modlist1: Cleaned the whole list.\n");
}



/* /proc read operation. 
 * Arguments:
	filp: open file structure.
	buf: pointer to the bytes array where we'll write.
	len: number of maximum bytes we can write to buf.
	off: position pointer (input and output parameter).

 * Return values:
 	0: end of file.
 	<0: error.
 	>0: number of bytes read (not arrived yet to the EOF).
 */
static ssize_t modlist_read(struct file *filp, char __user *buf, size_t len, loff_t *off){	
	/* Variable needed for the loop */
	struct list_head *pos = NULL;

	/* Variable where all nums will be stored and it's counter */
	char kbuf[MAX_DIGS];
	int nr_bytes = 0;

	/* Tell the application that there is nothing left to read */
	 if ((*off) > 0) 
      return 0;

	#ifdef PARTE_OPCIONAL
		/* Variable that will retrieve the node */
		struct list_item_string *nodeStr = NULL;

		/* Traverse the hole list and stores the data.
		The data is stored by digits, and a \n when a number ends. */
		list_for_each(pos, &mylist) {
			nodeStr = list_entry(pos, struct list_item_string, links);
			strcpy(kbuf+nr_bytes, nodeStr->data);
			nr_bytes += strlen(nodeStr->data);
		    kbuf[nr_bytes++] = '\n';
		}
	#else	
		/* Variable that will retrieve the node */
		struct list_item *node = NULL;
		int data = 0;

		/* Traverse the hole list and stores the data.
		The data is stored by digits, and a \n when a number ends. */
		list_for_each(pos, &mylist) {
			node = list_entry(pos, struct list_item, links);
			data = node->data;
			snprintf((kbuf+nr_bytes), MAX_DIGS, "%d", data);
			/* Minus sign */
			if(data < 0)
				nr_bytes++;
			while (data != 0) {
		        data /= 10;
		        nr_bytes++;
		    }
		    kbuf[nr_bytes++] = '\n';
		}
	#endif

	kbuf[nr_bytes++] = '\0';

	if(len < nr_bytes)
		return -ENOSPC;

	if(copy_to_user(buf, kbuf, nr_bytes))
		return -EFAULT;
	
	*off += nr_bytes;

	printk(KERN_INFO "Modlist1: Read.\n");

	return nr_bytes;
}



/* /proc write operation. 
 * Arguments:
	filp: open file structure.
	buf: pointer to the bytes array where the data is stored.
	len: number of bytes stored in buf.
	off: position pointer (input and output parameter).

 * Return values:
 	<0: error.
 	>0: number of bytes written.
 */
static ssize_t modlist_write(struct file *filp, const char __user *buf, size_t len, loff_t *off){
	/* Private copy of the data in kernel space */
	char kbuf[MAX_CHARS];
	/* Used to make sure the exact number of arguments have been entered */
	char aux[MAX_CHARS];

	if(len > MAX_CHARS)
		return -ENOSPC;

	/* Transfer user data to kernel space */
	if(copy_from_user(kbuf, buf, len))
		return -EFAULT;

	kbuf[len] = '\0';

	#ifdef PARTE_OPCIONAL

		char str[MAX_CHARS];

		/* Parsing the operation */
		if(strcmp(kbuf, "cleanup\n") == 0)
			cleanupString();
		else if(sscanf(kbuf, "add %s %s", str, aux) == 1)
			addString(str);
		else if(sscanf(kbuf, "remove %s %s", str, aux) == 1)
			removeString(str);
		else
			return -EINVAL;

	#else

		int num = 0;

		/* Parsing the operation */
		if(strcmp(kbuf, "cleanup\n") == 0)
			cleanup();
		else if(sscanf(kbuf, "add %d %s", &num, aux) == 1)
			add(num);
		else if(sscanf(kbuf, "remove %d %s", &num, aux) == 1)
			remove(num);
		else
			return -EINVAL;

	#endif
	
	*off += len;

	return len;
}





/* /proc file operations */
static const struct file_operations proc_fops = {
	.read = modlist_read,
	.write = modlist_write,
};





/* Function called when modlist module is loaded */
int list_module_init(void){
	/* Initialize the list */
	INIT_LIST_HEAD(&mylist); 
	/* Create /proc file entry */
	proc_entry = proc_create("modlist1", 0666, NULL, &proc_fops);

	/* If there's not enough memory for the /proc entry, free memory */
	if(proc_entry == NULL)
		return -ENOMEM;
	
	printk(KERN_INFO "Modlist1: Module loaded.\n");

	return 0;
}

/* Function called when modlist module is unloaded */
void list_module_close(void){
	#ifdef PARTE_OPCIONAL
		/* Erase all the elements of the list */
		cleanupString();
	#else
		/* Erase all the elements of the list */
		cleanup();
	#endif
	/* Create /proc file entry */
	remove_proc_entry("modlist1", NULL);
	printk(KERN_INFO "Modlist1: Module unloaded.\n");
}

module_init(list_module_init);
module_exit(list_module_close);
#include <linux/module.h> 	/* Modules */
#include <linux/kernel.h> 	/* KERNINFO */
#include <linux/vmalloc.h> 	/* vmalloc() */
#include <linux/proc_fs.h>	/* /proc files */
#include <linux/list.h>		/* list macros */
#include <linux/uaccess.h> 	/* copy_from_user() & copy_to_user() */
#include <linux/string.h>	/* string manipulation */
#include <linux/spinlock.h> /* spinlocks */

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SMP-safe Linked List Module - Arquitectura Interna de Linux y Android UCM");
MODULE_AUTHOR("Ramón Costales de Ledesma, José Ignacio Daguerre Garrido");

/* Usage:
	> make								//compiles program
	> sudo insmod modlist.ko 			//loads module
	> echo add 345 > /proc/modlist 		//adds 345 to the list
	> echo add 2 > /proc/modlist 		//adds 2 to the list
	> echo remove 2 > /proc/modlist 	//removes all 2's from the list
	> echo cleanup > /proc/modlist 		//deletes everything from the list
	> cat /proc/modlist 				//reads all the list elements
	> sudo rmmod modlist 				//unloads module
	> make clean 						//deletes compilation objects
*/


#define MAX_CHARS 20
#define MAX_DIGS 128

/* spinlock */
DEFINE_SPINLOCK(sp);

/* /proc entry */
static struct proc_dir_entry *proc_entry;

/* Linked list */
struct list_head mylist; 

/* List nodes */
struct list_item {
	int data;
	struct list_head links;
};



/* Adds num to the list */
void add(int num){
	/* Initialize node */
	struct list_item *node = vmalloc(sizeof(struct list_item));
	node->data = num;

	/* Acquire the spin lock (Beginning of the critical section) */
	spin_lock(&sp);
	/* Adds the node at the end of the list */
	list_add_tail(&node->links, &mylist);
	/* Free the spin lock */
	spin_unlock(&sp);

	printk(KERN_INFO "Modlist: Added %d to the list.\n", num);
}

/* Removes all occurences of num from the list */
void remove(int num){
	/* Auxiliar pointers for the for loop */
	struct list_head *aux = NULL, *pos = NULL;
	/* Node to be freed */
	struct list_item *freeNode = NULL;
	
	/* Acquire the spin lock (Beginning of the critical section) */
	spin_lock(&sp);
	/* Delete all occurences of num from the list and frees space */
	list_for_each_safe(pos, aux, &mylist){
		freeNode = list_entry(pos, struct list_item, links);
		if(freeNode-> data == num){
			list_del(pos);
			vfree(freeNode);
			printk(KERN_INFO "Modlist: Removed %d from the list.\n", num);
		}
	}
	/* Free the spin lock */
	spin_unlock(&sp);
}

/* Empties the list */
void cleanup(void){
	/* Variables needed for the for loop */
	struct list_head *aux = NULL, *pos = NULL;
	/* Node to be freed */
	struct list_item *freeNode = NULL;
	
	/* Acquire the spin lock (Beginning of the critical section) */
	spin_lock(&sp);
	/* Delete all nodes from the list and frees space */
	list_for_each_safe(pos, aux, &mylist){
		freeNode = list_entry(pos, struct list_item, links);
		list_del(pos);
		printk(KERN_INFO "Modlist: Removed %d from the list.\n", freeNode->data);
		vfree(freeNode);
	}
	/* Free the spin lock */
	spin_unlock(&sp);

	printk(KERN_INFO "Modlist: Cleaned the whole list.\n");
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
	/* Variable that will retrieve the node */
	struct list_item *node = NULL;

	/* Variable where all nums will be stored and it's counter */
	char kbuf[MAX_DIGS];
	char aux[MAX_CHARS];
	int nr_bytes = 0, data = 0, bytesToCopy;

	/* Tell the application that there is nothing left to read */
	if ((*off) > 0) 
      return 0;

	/* Acquire the spin lock (Beginning of the critical section) */
	spin_lock(&sp);
	/* Traverse the hole list and stores the data.
	The data is stored by digits, and a \n when a number ends. */
	list_for_each(pos, &mylist) {
		node = list_entry(pos, struct list_item, links);
		data = node->data;
		if((bytesToCopy = sprintf(aux, "%d\n", data)) < 0){
			spin_unlock(&sp);
			return -ENOSPC;
		}
		if(bytesToCopy > MAX_DIGS-nr_bytes){
			spin_unlock(&sp);
			return -ENOSPC;
		}

		nr_bytes += snprintf((kbuf+nr_bytes), MAX_DIGS, "%d\n", data);
	}
	/* Free the spin lock */
	spin_unlock(&sp);

	if(len < nr_bytes)
		return -ENOSPC;

	if(copy_to_user(buf, kbuf, nr_bytes))
		return -EFAULT;
	
	*off += nr_bytes;

	printk(KERN_INFO "Modlist: Read.\n");

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
	/* Will store the int */
	int num;

	if(len > MAX_CHARS)
		return -ENOSPC;

	/* Transfer user data to kernel space */
	if(copy_from_user(kbuf, buf, len))
		return -EFAULT;

	kbuf[len] = '\0';

	/* Parsing the operation */
	if(strcmp(kbuf, "cleanup\n") == 0)
		cleanup();
	else if(sscanf(kbuf, "add %d %s", &num, aux) == 1)
		add(num);
	else if(sscanf(kbuf, "remove %d %s", &num, aux) == 1)
		remove(num);
	else
		return -EINVAL;
	
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
	proc_entry = proc_create("modlist", 0666, NULL, &proc_fops);

	/* If there's not enough memory for the /proc entry, free memory */
	if(proc_entry == NULL)
		return -ENOMEM;
	
	printk(KERN_INFO "Modlist: Module loaded.\n");

	return 0;
}

/* Function called when modlist module is unloaded */
void list_module_close(void){
	/* Erase all the elements of the list */
	cleanup();
	/* Remove /proc file entry */
	remove_proc_entry("modlist", NULL);
	printk(KERN_INFO "Modlist: Module unloaded.\n");
}

module_init(list_module_init);
module_exit(list_module_close);
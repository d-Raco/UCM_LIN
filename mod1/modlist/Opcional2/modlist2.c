#include <linux/module.h> 	/* Modules */
#include <linux/kernel.h> 	/* KERNINFO */
#include <linux/vmalloc.h> 	/* vmalloc() */
#include <linux/proc_fs.h>	/* /proc files */
#include <linux/list.h>		/* list macros */
#include <linux/uaccess.h> 	/* copy_from_user() & copy_to_user() */
#include <linux/string.h>	/* string manipulation */
#include <linux/seq_file.h>	/* Sequence file */

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Linked List Module - Arquitectura Interna de Linux y Android UCM");
MODULE_AUTHOR("Ramón Costales de Ledesma, José Ignacio Daguerre Garrido");

/* Usage:
	> make								//compiles program
	> sudo insmod modlist2.ko 			//loads module
	> echo add 345 > /proc/modlist2 	//adds 345 to the list
	> echo add 2 > /proc/modlist2 		//adds 2 to the list
	> echo remove 2 > /proc/modlist2 	//removes all 2's from the list
	> echo cleanup > /proc/modlist2 	//deletes everything from the list
	> sudo rmmod modlist2 				//unloads module
	> make clean 						//deletes compilation objects
*/


#define MAX_CHARS 20
#define MAX_DIGS 128

/* number of elements in the list */
static int numElems;

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
	numElems++;

	/* Adds the node at the end of the list */
	list_add_tail(&node->links, &mylist);
	printk(KERN_INFO "Modlist2: Added %d to the list.\n", num);
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
		if(freeNode-> data == num){
			list_del(pos);
			vfree(freeNode);
			numElems--;
			printk(KERN_INFO "Modlist2: Removed %d from the list.\n", num);
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
		numElems--;
		printk(KERN_INFO "Modlist2: Removed %d from the list.\n", freeNode->data);
		vfree(freeNode);
	}

	printk(KERN_INFO "Modlist2: Cleaned the whole list.\n");
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



/* Seq file start operation */
static void *modlist_seq_start(struct seq_file *f, loff_t *pos) {
	printk(KERN_INFO "Modlist2: Seq start.\n");
	if(*pos >= numElems)
		return NULL;
	else
		return mylist.next;
}

/* Seq file next operation */
static void *modlist_seq_next(struct seq_file *s, void *v, loff_t *pos) {
	struct list_head *node = (struct list_head*)v;
	(*pos)++;

	printk(KERN_INFO "Modlist2: Seq next.\n");

	if(*pos >= numElems)
		return NULL;
	else
		return node->next;
}

/* Seq file stop operation */
static void modlist_seq_stop(struct seq_file *s, void *v) {
	printk(KERN_INFO "Modlist2: Seq stop.\n");
}

/* Seq file show operation */
static int modlist_seq_show(struct seq_file *s, void *v) {
	struct list_item *node;

	node = list_entry(v, struct list_item, links);
	seq_printf(s, "%d\n", node->data);
	printk(KERN_INFO "Modlist2: Seq show.\n");

	return 0;
}



/* seq operations */
static struct seq_operations modlist_seq_ops = {
        .start = modlist_seq_start,
        .next = modlist_seq_next,
        .stop = modlist_seq_stop,
        .show = modlist_seq_show
};


/* Seq file open operation */
static int modlist_seq_open(struct inode *inode, struct file *f) {
    return seq_open(f, &modlist_seq_ops);
}






/* /proc file operations */
static const struct file_operations proc_fops = {
	.open = modlist_seq_open,
	.read = seq_read,
	.write = modlist_write,
};





/* Function called when modlist module is loaded */
int list_module_init(void){
	/* Initialize the list */
	INIT_LIST_HEAD(&mylist); 
	numElems = 0;

	/* Create /proc file entry */
	proc_entry = proc_create("modlist2", 0666, NULL, &proc_fops);

	/* If there's not enough memory for the /proc entry, free memory */
	if(proc_entry == NULL)
		return -ENOMEM;
	
	printk(KERN_INFO "Modlist2: Module loaded.\n");

	return 0;
}

/* Function called when modlist module is unloaded */
void list_module_close(void){
	/* Erase all the elements of the list */
	cleanup();
	/* Create /proc file entry */
	remove_proc_entry("modlist2", NULL);
	printk(KERN_INFO "Modlist2: Module unloaded.\n");
}

module_init(list_module_init);
module_exit(list_module_close);
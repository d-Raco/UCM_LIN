#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <asm-generic/errno.h>
#include <linux/semaphore.h>
#include <linux/kfifo.h>


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Multi producer-consumer Kernel Module - Arquitectura Interna de Linux y Android UCM");
MODULE_AUTHOR("Rymond3");

#define MAX_CHARS_KBUF	20
#define MAX_CHARS_ADMIN 20

/* Params */
static int max_entries = 5;
static unsigned int entries = 0;
static int max_size = 32;

module_param(max_entries, int, 0644);
MODULE_PARM_DESC(max_entries, "An unsigned integer");
module_param(max_size, int, 0644);
MODULE_PARM_DESC(max_size, "An unsigned integer");

typedef struct {
	char name[MAX_CHARS_ADMIN]; /* Name of the /proc module */
	int isInt; /* Distinguish between integer and string buffers */
	struct kfifo cbuf; /* Shared circular buffer */
	struct semaphore elements, gaps; /* Producer and consumer semaphores */
	struct semaphore mtx; /* Ensures mutual exclusion while accesing the buffer */
} prodcons;

static struct proc_dir_entry *admin_entry;
struct proc_dir_entry *multipc_dir = NULL;
/* Linked list with all the data of the proc entries */
struct list_head procDataList; 
/* List nodes */
struct list_item {
	prodcons *data;
	struct list_head links;
};

struct semaphore sem_list;  /* Mutex for linked list */

int initializeProdcons(prodcons *data, int isInteger, char *name) {
	strcpy(data->name, name);

	data->isInt = isInteger;

	/* Elements semaphore, initializaed to 0 (empty buffer) */
	sema_init(&data->elements, 0);

	/* Gaps semaphore, initialized to max_size (empty buffer) */
	sema_init(&data->gaps, max_size);

	/* Semaphore for ensuring mutual exclusion */
	sema_init(&data->mtx, 1);

	/* Buffer initialization */
	if (isInteger) {
		if (kfifo_alloc(&data->cbuf, max_size*sizeof(int), GFP_KERNEL))
			return 1;
	} 
	else {
		if (kfifo_alloc(&data->cbuf, max_size*sizeof(char*), GFP_KERNEL))
			return 1;
	}

	return 0;
}

static ssize_t prodcons_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
	prodcons* data = (prodcons*)PDE_DATA(filp->f_inode);
	char kbuf[MAX_CHARS_KBUF+1];
	int val = 0;
	char *str = NULL;

	/* The application can write in this entry just once !! */
	if ((*off) > 0) 
		return 0;

	if (len > MAX_CHARS_KBUF) 
		return -ENOSPC;
	
	if (copy_from_user(kbuf, buf, len)) 
		return -EFAULT;

	kbuf[len] = '\0';
	/* Update the file pointer */
	*off += len; 

	if (data->isInt) {
		if (sscanf(kbuf, "%i", &val) != 1)
			return -EINVAL;
	}
	else {
		str = vmalloc(len+1);
		strcpy(str, kbuf);
	}

	/* Blocks yntil there's free space */
	if (down_interruptible(&data->gaps))
		return -EINTR;

	/* Enter the critical section */
	if (down_interruptible(&data->mtx)) {
		up(&data->gaps);
		return -EINTR;
	}

	/* Secure insertion in the circular buffer */
	if (data->isInt) 	
		kfifo_in(&data->cbuf, &val, sizeof(int));
	else 
		kfifo_in(&data->cbuf, &str, sizeof(char*));	

	/* Exit the critical section */
	up(&data->mtx);

	/* Increment the number of elements */
	up(&data->elements);

	if (data->isInt)
		printk(KERN_INFO "Multipc: %s produced %d\n", data->name, val);
	else
		printk(KERN_INFO "Multipc: %s produced %s", data->name, str);

	return len;
}


static ssize_t prodcons_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
	prodcons* data = (prodcons*)PDE_DATA(filp->f_inode);
	int nr_bytes = 0;
	int val = 0;
	int bytes_extracted;
	char kbuff[32] = "";
	char *str = NULL;

	if ((*off) > 0)
		return 0;

	/* Blocks until there are elements to consume */
	if (down_interruptible(&data->elements))
		return -EINTR;

	/* Enters the critical section */
	if (down_interruptible(&data->mtx)) {
		up(&data->elements);
		return -EINTR;
	}

	/* Extract the first element of the buffer */
	if (data->isInt)
		bytes_extracted = kfifo_out(&data->cbuf, &val, sizeof(int));
	else 
		bytes_extracted = kfifo_out(&data->cbuf, &str, sizeof(char*));

	/* Exit the critical section */
	up(&data->mtx);

	/* Increment the number of gaps */
	up(&data->gaps);

	if ((data->isInt && bytes_extracted != sizeof(int)) || (!data->isInt && bytes_extracted != sizeof(char*)))
		return -EINVAL;

	/* Convert to character string for the user */
	if (data->isInt)
		nr_bytes = sprintf(kbuff, "%i\n", val);
	else {
		nr_bytes = sprintf(kbuff, "%s\n", str);
		vfree(str);
	}

	if (len < nr_bytes)
		return -ENOSPC;

	if (copy_to_user(buf, kbuff, nr_bytes))
		return -EINVAL;

	(*off) += nr_bytes; 

	if (data->isInt)
		printk(KERN_INFO "Multipc: %s consumed %d\n", data->name, val);
	else
		printk(KERN_INFO "Multipc: %s consumed %s", data->name, str);

	return nr_bytes;
}

static const struct file_operations prodcons_fops = {
	.read = prodcons_read,
	.write = prodcons_write,
};


void addProc(prodcons *data) {
	int r;

	/* Initialize node */
	struct list_item *node = vmalloc(sizeof(struct list_item));
	node->data = data;

	/* "Acquires" the mutex */
	r = down_interruptible(&sem_list);

	/* Adds the node at the end of the list */
	list_add_tail(&node->links, &procDataList);

	/* "Frees" the mutex */
  	up(&sem_list);

	++entries;
}


int removeProc(char *str){
	/* Auxiliar pointers for the for loop */
	struct list_head *aux = NULL, *pos = NULL;
	/* Node to be freed */
	struct list_item *node = NULL;
	char *freestr = NULL;
	int bytes_extracted;
	int r;

	/* "Acquires" the mutex */
	r = down_interruptible(&sem_list);
	
	/* Delete and free space of /proc */
	list_for_each_safe(pos, aux, &procDataList){
		node = list_entry(pos, struct list_item, links);
		if (strcmp(str, node->data->name) == 0) {
			/* "Frees" the mutex */
  			up(&sem_list);

			if (!node->data->isInt) {
				while (kfifo_len(&node->data->cbuf) > 0) {
					/* Enters the critical section */
					if (down_interruptible(&node->data->mtx))
						return 0;

					bytes_extracted = kfifo_out(&node->data->cbuf, &freestr, sizeof(char *));

					/* Exit the critical section */
					up(&node->data->mtx);

					/* Increment the number of gaps */
					up(&node->data->gaps);

					vfree(freestr);
				}
			}
			kfifo_free(&node->data->cbuf);
			remove_proc_entry(node->data->name, multipc_dir);
			vfree(node->data);
			
			/* "Acquires" the mutex */
			r = down_interruptible(&sem_list);

			list_del(pos);
			vfree(node);
			--entries;
		}
	}

	/* "Frees" the mutex */
  	up(&sem_list);

	return 1;
}


void cleanProcs(void) {
	/* Variables needed for the for loop */
	struct list_head *aux = NULL, *pos = NULL;
	/* Node to be freed */
	struct list_item *node = NULL;
	char *str = NULL;
	int bytes_extracted, r;

	/* "Acquires" the mutex */
	r = down_interruptible(&sem_list);
	
	/* Delete and frees space of all /procs */
	list_for_each_safe(pos, aux, &procDataList){
		node = list_entry(pos, struct list_item, links);

		/* "Frees" the mutex */
  		up(&sem_list);
		if (!node->data->isInt) {
			while (kfifo_len(&node->data->cbuf) > 0) {
				/* Enters the critical section */
				r = down_interruptible(&node->data->mtx);

				bytes_extracted = kfifo_out(&node->data->cbuf, &str, sizeof(char *));

				/* Exit the critical section */
				up(&node->data->mtx);

				/* Increment the number of gaps */
				up(&node->data->gaps);

				vfree(str);
			}
		}
		kfifo_free(&node->data->cbuf);
		remove_proc_entry(node->data->name, multipc_dir);
		vfree(node->data);
		
		/* "Acquires" the mutex */
		r = down_interruptible(&sem_list);

		list_del(pos);
		vfree(node);
		--entries;
	}

	/* "Frees" the mutex */
  	up(&sem_list);
}


int exists(char *name) {
	int exists = 0; 
	/* Variable needed for the loop */
	struct list_head *pos = NULL;
	/* Variable that will retrieve the node */
	struct list_item *node = NULL;
	int r;

	/* "Acquires" the mutex */
	r = down_interruptible(&sem_list);

	list_for_each(pos, &procDataList) {
		node = list_entry(pos, struct list_item, links);
		if (!exists && strcmp(name, node->data->name) == 0)
			exists = 1;
	}

	/* "Frees" the mutex */
  	up(&sem_list);

	return exists;
}


static ssize_t admin_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
	char kbuf[MAX_CHARS_ADMIN+1];
	char name[MAX_CHARS_ADMIN];
	char type;
	static struct proc_dir_entry *proc_entry;
	prodcons* data = NULL;

	/* The application can write in this entry just once !! */
	if ((*off) > 0) 
		return 0;

	if (len > MAX_CHARS_ADMIN) 
		return -ENOSPC;
	
	if (copy_from_user(kbuf, buf, len)) 
		return -EFAULT;

	kbuf[len] = '\0';
	/* Update the file pointer */
	*off += len; 

	if (sscanf(kbuf, "new %s %c", name, &type) == 2) {
		if (entries >= max_entries)
			return -ENOSPC;

		if (exists(name))
			return -EINVAL;

		data = vmalloc(sizeof(prodcons));

		if (type == 'i') {
			if (initializeProdcons(data, 1, name))
				return -ENOMEM;
		}
    	else if (type == 's') {
    		if (initializeProdcons(data, 0, name))
				return -ENOMEM;
    	}
    	else {
    		vfree(data);
    		return -EINVAL;
    	}

    	proc_entry = proc_create_data(name, 0666, multipc_dir, &prodcons_fops, data);
    	addProc(data);

    	printk(KERN_INFO "Multipc: Added %s module (%c)\n", name, type);
	}
	else if (sscanf(kbuf, "delete %s", name) == 1) {
		if (!exists(name))
			return -EINVAL;

		if (!removeProc(name))
			return -EINTR;

		printk(KERN_INFO "Multipc: Removed %s module\n", name);
	}
	else
		return -EINVAL;

	return len;
}



static const struct file_operations admin_fops = {
	.write = admin_write,
};




int init_multipc_module(void) {
	static struct proc_dir_entry *proc_entry;
	prodcons *data;

	if (max_entries < 1 || (max_size & (max_size - 1)) != 0)
		return -EINVAL;

	/* Initialize the list */
	INIT_LIST_HEAD(&procDataList);

	/* Initializing the semaphore that allows mutual exclusion of the linked list to 1 */
    sema_init(&sem_list, 1);

    /* Create proc directory */
    multipc_dir = proc_mkdir("multipc",NULL);

    if (!multipc_dir) 
        return -ENOMEM;

    /* Create proc entry /proc/multipc/admin */
	admin_entry = proc_create("admin", 0666, multipc_dir, &admin_fops);

	if (admin_entry == NULL) {
        remove_proc_entry("multipc", NULL);
        return -ENOMEM;
    }

    data = vmalloc(sizeof(prodcons));
	
	if (initializeProdcons(data, 1, "test"))
		return -ENOMEM;

    /* Create proc entry /proc/multipc/test */
	proc_entry = proc_create_data("test", 0666, multipc_dir, &prodcons_fops, data);

    if (proc_entry == NULL) {
        remove_proc_entry("admin", multipc_dir);
        remove_proc_entry("multipc", NULL);
        kfifo_free(&data->cbuf);
        vfree(data);
        return -ENOMEM;
    }

    addProc(data);

    printk(KERN_INFO "Multipc: Module loaded\n");

    return 0;
}


void exit_multipc_module(void) {
	/* Remove all the entries of the multipc dir */
    cleanProcs();
    remove_proc_entry("admin", multipc_dir);
    remove_proc_entry("multipc", NULL);
    
    printk(KERN_INFO "Multipc: Modules removed\n");
}


module_init(init_multipc_module);
module_exit(exit_multipc_module);

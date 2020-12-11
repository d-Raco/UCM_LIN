#include <linux/kfifo.h>	/* Circular buffer kfifo */
#include <linux/semaphore.h>/* Semaphores */
#include <linux/module.h> 	/* Modules */
#include <linux/proc_fs.h>	/* /proc files */
#include <linux/uaccess.h> 	/* copy_from_user() & copy_to_user() */
#include <linux/kernel.h>	/* KERN_INFO */
#include <linux/errno.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Implementation of a FIFO pipe via /proc entry - Arquitectura Interna de Linux y Android UCM");
MODULE_AUTHOR("Ramón Costales de Ledesma, José Ignacio Daguerre Garrido");

#define MAX_KBUF	64
#define MAX_ITEMS_CBUF 64

static struct proc_dir_entry *proc_entry;
struct kfifo cbuffer; 		/* Circular buffer */
int prod_count = 0; 		/* Number of process that opened the /proc entry for writing (producers) */
int cons_count = 0; 		/* Number of process that opened the /proc entry for reading (consumers) */
struct semaphore mtx; 		/* For guaranteeing Mutual Exclusion */
struct semaphore sem_prod; 	/* Waiting queue for producer(s) */
struct semaphore sem_cons; 	/* Waiting queue for consumer(s) */
int nr_prod_waiting = 0; 	/* Number of waiting producer processes */
int nr_cons_waiting = 0; 	/* Number of waiting consumer processes */


/* Invoqued when open() is done on the /proc entry */
static int fifoproc_open(struct inode *i, struct file *file) {
	/* "Acquires" the mutex */
	if (down_interruptible(&mtx))
		return -EINTR;

	if (file->f_mode & FMODE_READ) {
		/* A consumer opened the FIFO */
		cons_count++;
		/* Wakes up one of the blocked producer threads */
		if (nr_prod_waiting > 0) {	
			up(&sem_prod);
			nr_prod_waiting--;
		}
		/* Block until a producer has opened it's writing end */
		while(prod_count == 0){
			nr_cons_waiting++;
			/* "Frees" the mutex */
			up(&mtx);
			/* Blocks a consumer in the queue */
			if (down_interruptible(&sem_cons)){
				down(&mtx);
				nr_cons_waiting--;
				cons_count--;
				up(&mtx);
				return -EINTR;
			}

			/* "Acquires" the mutex */
			if (down_interruptible(&mtx))
				return -EINTR;
		}	
	} 

	else {
		/* A producer opened the FIFO */
		prod_count++;
		/* Wakes up one of the blocked consumer threads */
		if (nr_cons_waiting > 0) {	
			up(&sem_cons);
			nr_cons_waiting--;
		}

		/* Block until a consumer has opened it's reading end */
		while(cons_count == 0){
			nr_prod_waiting++;
			/* "Frees" the mutex */
			up(&mtx);
			/* Blocks a consumer in the queue */
			if (down_interruptible(&sem_prod)){
				down(&mtx);
				nr_prod_waiting--;
				prod_count--;
				up(&mtx);
				return -EINTR;
			}

			/* "Acquires" the mutex */
			if (down_interruptible(&mtx))
				return -EINTR;
		}
	}

	/* "Frees" the mutex */
	up(&mtx);	

	return 0;
}

/* Invoqued when close() is done on the /proc entry */
static int fifoproc_release(struct inode *i, struct file *file) {
	/* "Acquires" the mutex */
	if (down_interruptible(&mtx))
		return -EINTR;

	if (file->f_mode & FMODE_READ) {
		/* Consumidor */
		cons_count--;
		/* Wakes up one of the blocked producer threads in write */
		if (nr_prod_waiting > 0) {	
			up(&sem_prod);
			nr_prod_waiting--;
		}
	}

	else{
		/* Productor */
		prod_count--;
		/* Wakes up one of the blocked consumer threads in read */
		if (nr_cons_waiting > 0) {	
			up(&sem_cons);
			nr_cons_waiting--;
		}
	}

	if (cons_count == 0 && prod_count == 0)
		kfifo_reset(&cbuffer);

	/* "Frees" the mutex */	
	up(&mtx);

	return 0;
}

/* Invoqued when read() is done on the /proc entry */
static ssize_t fifoproc_read(struct file *file, char *buff, size_t len, loff_t *off) {
	int r;
	char kbuffer[MAX_KBUF];

	if ((len > MAX_ITEMS_CBUF) || (len > MAX_KBUF))
		return -ENOSPC;

	/* "Acquires" the mutex */
	if (down_interruptible(&mtx))
		return -EINTR;

	/* Wait until there's enough data for reading (there must be producers) */
	while ((kfifo_len(&cbuffer) < len) && (prod_count > 0)) {
		nr_cons_waiting++;
		/* "Frees" the mutex */
		up(&mtx);
		/* Blocks a consumer in the queue */
		if (down_interruptible(&sem_cons)){
			down(&mtx);
			nr_cons_waiting--;
			cons_count--;
			up(&mtx);
			return -EINTR;
		}

		/* "Acquires" the mutex */
		if (down_interruptible(&mtx))
			return -EINTR;
	}

	/* Detect communication ended */
	if ((kfifo_is_empty(&cbuffer)) && (prod_count == 0)) {
		/* "Frees" the mutex */
		up(&mtx);
		return 0;
	}

	r = kfifo_out(&cbuffer, kbuffer, len);

	/* Wakes up one of the blocked producer threads */
	if (nr_prod_waiting > 0) {	
		up(&sem_prod);
		nr_prod_waiting--;
	}

	/* "Frees" the mutex */
	up(&mtx);

	if(copy_to_user(buff, kbuffer, len))
		return -EFAULT;

	return len; 
}

/* Invoqued when write() is done on the /proc entry */
static ssize_t fifoproc_write(struct file *file, const char *buff, size_t len, loff_t *off) {
	char kbuffer[MAX_KBUF];

	if ((len > MAX_ITEMS_CBUF) || (len > MAX_KBUF))
		return -ENOSPC;
	
	if (copy_from_user(kbuffer, buff, len)) 
		return -EFAULT;

	/* "Acquires" the mutex */
	if (down_interruptible(&mtx))
		return -EINTR;

	/* Wait until there's enough space for inserting (there must be consumers) */
	while ((kfifo_avail(&cbuffer) < len) && (cons_count > 0)) {
		nr_prod_waiting++;
		/* "Frees" the mutex */
		up(&mtx);
		/* Blocks a consumer in the queue */
		if (down_interruptible(&sem_prod)){
			down(&mtx);
			nr_prod_waiting--;
			prod_count--;
			up(&mtx);
			return -EINTR;
		}

		/* "Acquires" the mutex */
		if (down_interruptible(&mtx))
			return -EINTR;
	}

	/* Detect communication ended failure (consumer ends FIFO before) */
	if (cons_count == 0) {
		/* "Frees" the mutex */
		up(&mtx);
		return -EPIPE;
	}

	kfifo_in(&cbuffer, kbuffer, len);

	/* Wakes up one of the blocked consumer threads */
	if (nr_cons_waiting > 0) {	
		up(&sem_cons);
		nr_cons_waiting--;
	}

	/* "Frees" the mutex */
	up(&mtx);

	return len; 
}



/* /proc file operations */
static const struct file_operations proc_entry_fops = {
	.open = fifoproc_open,
	.release = fifoproc_release,
	.read = fifoproc_read,
	.write = fifoproc_write,
};




/* Loading module functions */
int load_fifoproc_module(void) {
	int retval;

	/* Circular buffer initialization */
	retval = kfifo_alloc(&cbuffer,MAX_ITEMS_CBUF,GFP_KERNEL);

	if (retval)
		return -ENOMEM;

	/* Initializing the waiting queue semaphores to 0 */
	sema_init(&sem_prod, 0);
	sema_init(&sem_cons, 0);

	/* Initializing the semaphore that allows mutual exclusion of the CS to 1 */
	sema_init(&mtx, 1);

	nr_prod_waiting = nr_cons_waiting = 0;

	proc_entry = proc_create_data("modfifo",0666, NULL, &proc_entry_fops, NULL);

	if (proc_entry == NULL) {
		kfifo_free(&cbuffer);
		printk(KERN_INFO "fifoproc: Coudn't create the entry in /proc.\n");
		return  -ENOMEM;
	}

	printk(KERN_INFO "fifoproc: Module loaded.\n");

	return 0;
}

/* Unloading module functions */
void unload_fifoproc_module(void) {
	kfifo_free(&cbuffer);
	remove_proc_entry("modfifo", NULL);
	printk(KERN_INFO "fifoproc: Module unloaded.\n");
}

module_init( load_fifoproc_module );
module_exit( unload_fifoproc_module );
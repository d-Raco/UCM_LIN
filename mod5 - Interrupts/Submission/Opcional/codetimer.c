#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/timer.h>
#include <linux/random.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/kfifo.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/vmalloc.h> 

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("codetimer Module - Arquitectura de Linux y Android");
MODULE_AUTHOR("José Ignacio Daguerre and Ramón Costales");

#define CBUF_SIZE 32
#define MAX_CHARS 40
#define MAX_DIGS 120
#define MAX_CODE_SIZE 8
#define NUM_OF_LETTERS 26
#define NUM_OF_DIGITS 10
#define NUM_OF_CPU 2

/* /proc entrys */
static struct proc_dir_entry *proc_timer_entry;
static struct proc_dir_entry *proc_config_entry;

struct kfifo cbuffer; /* Circular buffer */
struct list_head mylistEven; /* Even linked list */
struct list_head mylistOdd; /* Odd linked list */

/* List nodes */
struct list_item {
	unsigned char data[MAX_CODE_SIZE+1];
	struct list_head links;
};

unsigned int timer_period_ms = 1000; /* Measures the time (ms) of the code generation */
unsigned int emergency_threshold = 75; /* At which percentage of buffer size the data is going to be transferred to the workqueue */

unsigned int even = 0; /* Used for knowing when the proc even entry is ready */
unsigned int odd = 0; /* Used for knowing when the proc odd entry is ready */

DEFINE_SPINLOCK(buffer_lock); /* Spinlock for the circular buffer */
struct semaphore sem_list_even;  /* Mutex for the even linked list */
struct semaphore sem_list_odd;  /* Mutex for the odd linked list */
struct semaphore queue_even;  /* Waiting queue when even linked list is empty */
struct semaphore queue_odd;  /* Waiting queue when odd linked list is empty */
int waiting_even; /* Number of processes waiting the even list */
int waiting_odd; /* Number of processes waiting the odd list */

struct work_struct my_work; /* Work descriptor */

struct timer_list my_timer; /* Structure that describes the kernel timer */

int jobFinished = 1;




static void copy_items_into_list(struct work_struct *work) {
	unsigned long flags;
	unsigned char buffer[CBUF_SIZE], code[MAX_CODE_SIZE+1];
	int nr_bytes, i, j = 0, r;
	struct list_item *node;
	
	spin_lock_irqsave(&buffer_lock, flags);

	/* Critical section where the buffer is accessed in porcess context */
	nr_bytes = kfifo_out(&cbuffer, buffer, kfifo_len(&cbuffer));
	
	spin_unlock_irqrestore(&buffer_lock, flags);

	while(j < nr_bytes && j < CBUF_SIZE) {
		i = 0;

		while(buffer[j] != '\0'  && j < CBUF_SIZE) {
			code[i++] = buffer[j++];
		}
		code[i] = '\0';
		j++;

		node = vmalloc(sizeof(struct list_item));
		strcpy(node->data, code);

		if(i % 2 == 0 && even) {
			/* "Acquires" the mutex */
			r = down_interruptible(&sem_list_even);

			/* Adds the node at the end of the list */
			list_add_tail(&node->links, &mylistEven);

			/* "Frees" the mutex */
			up(&sem_list_even);

			printk(KERN_INFO "codetimer: Copied to even list -> %s\n", code);
		}
		else if(i % 2 != 0 && odd) {
			/* "Acquires" the mutex */
			r = down_interruptible(&sem_list_odd);

			/* Adds the node at the end of the list */
			list_add_tail(&node->links, &mylistOdd);

			/* "Frees" the mutex */
			up(&sem_list_odd);
			
			printk(KERN_INFO "codetimer: Copied to odd list -> %s\n", code);
		}
	}

	if(waiting_even > 0){
		waiting_even--;
		up(&queue_even);
	}

	if(waiting_odd > 0){
		waiting_odd--;
		up(&queue_odd);
	}

	printk(KERN_INFO "codetimer: Copied items to the list.\n");

	jobFinished = 1;
}


/* Function invoked when timer expires (fires) */
static void fire_timer(unsigned long data) {
	unsigned int random;
	unsigned char c;
	unsigned char code[MAX_CODE_SIZE+1];
	int code_length;
	int i = 0, cpu, cpu_actual = smp_processor_id(), op;

	/* Generates a random number of 32 bits */ 
	random = get_random_int();

	code_length = (random % MAX_CODE_SIZE) + 1;
	random /= MAX_CODE_SIZE;

	/* Create code from random int */
	while(i < code_length) {
		op = random % 3;
		random /= 3;
		if(op == 0) {
			/* Upper case ASCII: 65-90 (A-Z) */
			if(random < NUM_OF_LETTERS)
				random = get_random_int();
				
			c = (unsigned char)(65 + (random % NUM_OF_LETTERS));
			random /= NUM_OF_LETTERS;
		}
		else if(op == 1) {
			/* Lower case ASCII: 97-122 (a-z) */
			if(random < NUM_OF_LETTERS)
				random = get_random_int();

			c = (unsigned char)(97 + (random % NUM_OF_LETTERS));
			random /= NUM_OF_LETTERS;
		}
		else {
			/* Number ASCII: 48-57 (0-9) */
			if(random < NUM_OF_DIGITS)
				random = get_random_int();

			c = (unsigned char)(48 + (random % NUM_OF_DIGITS));
			random /= NUM_OF_DIGITS;
		}

		code[i++] = c;
	}
	code[code_length] = '\0';

	/* Acquire the spin lock */
	spin_lock(&buffer_lock);

	kfifo_in(&cbuffer, code, code_length+1);

	/* Free the spin lock */
	spin_unlock(&buffer_lock);

	/* If the buffer fills up to the emergency threshold, transfer the codes to the linked list */
	if(((CBUF_SIZE * emergency_threshold) / 100) <= (kfifo_len(&cbuffer)) && jobFinished) {
		cpu = cpu_actual + 1;
		if(cpu >= NUM_OF_CPU)
			cpu = 0;

	  	jobFinished = 0;


	  	/* Enqueue work */
	  	schedule_work_on(cpu, &my_work);

	}

	printk(KERN_INFO "codetimer: Fire timer %s\n", code);

  	/* Re-activate the timer timer_period_ms milliseconds from now */
	mod_timer(&(my_timer), jiffies + (HZ*timer_period_ms)/1000); 
}





/* Empties the list */
void cleanup(unsigned int oddL){
	int r;

	/* Variables needed for the for loop */
	struct list_head *aux = NULL, *pos = NULL;
	/* Node to be freed */
	struct list_item *freeNode = NULL;

	if(oddL) {
		/* "Acquires" the mutex */
		r = down_interruptible(&sem_list_odd);
		
		/* Delete all nodes from the list and frees space */
		list_for_each_safe(pos, aux, &mylistOdd){
			freeNode = list_entry(pos, struct list_item, links);
			list_del(pos);
			vfree(freeNode);
		}

		/* "Frees" the mutex */
	  	up(&sem_list_odd);

		printk(KERN_INFO "codetimer: Cleaned the whole odd list.\n");
	}
	else {
		/* "Acquires" the mutex */
		r = down_interruptible(&sem_list_even);
		
		/* Delete all nodes from the list and frees space */
		list_for_each_safe(pos, aux, &mylistEven){
			freeNode = list_entry(pos, struct list_item, links);
			list_del(pos);
			vfree(freeNode);
		}

		/* "Frees" the mutex */
	  	up(&sem_list_even);

		printk(KERN_INFO "codetimer: Cleaned the whole even list.\n");
	}
}







static int timerproc_open(struct inode *i, struct file *file) {
	/* Increment the Reference Counter of the module */
	try_module_get(THIS_MODULE);

    file->private_data = vmalloc(sizeof(unsigned int));
    *((unsigned int*)file->private_data) = even;

    if(even == 0)
    	even++;
    else
    	odd++;

	/* Initialize work structure (with function) */
	INIT_WORK(&my_work, copy_items_into_list);

	/* Create timer */
    init_timer(&my_timer);

    /* Initialize field */
    my_timer.data = 0;
    my_timer.function = fire_timer;
    my_timer.expires = jiffies + (HZ*timer_period_ms)/1000;  /* Activate it timer_period_ms milliseconds from now */

    if(waiting_odd <= 0) {
    	waiting_odd++;
    	if (down_interruptible(&queue_odd)) {
    		waiting_odd--;
			return -EINTR;
    	}

    	/* Activate the timer for the first time */
    	add_timer(&my_timer);
    }
    else {
    	waiting_odd--;
    	up(&queue_odd);
    } 

    printk(KERN_INFO "codetimer: Open.\n");

    return 0;
}

static int timerproc_release(struct inode *i, struct file *file) {
	/* Wait until completion of the timer function (if it's currently running) and delete timer */
  	del_timer_sync(&my_timer);

	/* Wait until all jobs scheduled so far have finished */
	flush_scheduled_work();

  	/* Delete elements of the circular buffer */
    kfifo_reset(&cbuffer);

    if(*((unsigned int*)file->private_data))
    	odd--;
    else
    	even--;

    /* Delete elements of the linked list */
    cleanup(*((unsigned int*)file->private_data));

    vfree(file->private_data);

    /* Decrement the Reference Counter of the module */
	module_put(THIS_MODULE);

	printk(KERN_INFO "codetimer: Release.\n");

	return 0;
}

static ssize_t timerproc_read(struct file *file, char *buff, size_t len, loff_t *off) {
	char kbuf[MAX_DIGS];
    int nr_bytes = 0;
    /* Variable needed for the loop */
	struct list_head *pos = NULL;
	/* Variable that will retrieve the node */
	struct list_item *node = NULL;
	unsigned char data[MAX_CODE_SIZE+1];

	if(*((unsigned int*)file->private_data)) {
		/* "Acquires" the mutex */
		if (down_interruptible(&sem_list_odd))
			return -EINTR;

		/* Blocks until the list has been filled */
	  	while(list_empty(&mylistOdd)) {
	  		waiting_odd++;

	  		/* "Frees" the mutex */
	  		up(&sem_list_odd);

	  		/* Blocks in the queue */
			if (down_interruptible(&queue_odd)){
				down(&sem_list_odd);
				waiting_odd--;
				up(&sem_list_odd);
				return -EINTR;
			}

			/* "Acquires" the mutex */
			if (down_interruptible(&sem_list_odd))
				return -EINTR;
	  	}

	  	/* Traverse the hole list and stores the data. 
		The data is stored by digits, and a \n when a number ends. */
		list_for_each(pos, &mylistOdd) {
			node = list_entry(pos, struct list_item, links);
			strncpy(data, node->data, MAX_CODE_SIZE+1);

			nr_bytes += snprintf((kbuf+nr_bytes), MAX_CODE_SIZE+2, "%s\n", data);
		}

	  	/* "Frees" the mutex */
	  	up(&sem_list_odd);
	  	
	  	cleanup(*((unsigned int*)file->private_data));

	  	printk(KERN_INFO "codetimer: Read the odd list.\n");
	}
	else {
		/* "Acquires" the mutex */
		if (down_interruptible(&sem_list_even))
			return -EINTR;

		/* Blocks until the list has been filled */
	  	while(list_empty(&mylistEven)) {
	  		waiting_even++;

	  		/* "Frees" the mutex */
	  		up(&sem_list_even);

	  		/* Blocks in the queue */
			if (down_interruptible(&queue_even)){
				down(&sem_list_even);
				waiting_even--;
				up(&sem_list_even);
				return -EINTR;
			}

			/* "Acquires" the mutex */
			if (down_interruptible(&sem_list_even))
				return -EINTR;
	  	}

	  	/* Traverse the hole list and stores the data. 
		The data is stored by digits, and a \n when a number ends. */
		list_for_each(pos, &mylistEven) {
			node = list_entry(pos, struct list_item, links);
			strncpy(data, node->data, MAX_CODE_SIZE+1);

			nr_bytes += snprintf((kbuf+nr_bytes), MAX_CODE_SIZE+2, "%s\n", data);
		}

	  	/* "Frees" the mutex */
	  	up(&sem_list_even);
	  	
	  	cleanup(*((unsigned int*)file->private_data));

	  	printk(KERN_INFO "codetimer: Read the even list.\n");
	} 	

    if(len < nr_bytes)
        return -ENOSPC;

    if(copy_to_user(buff, kbuf, nr_bytes))
        return -EFAULT;
    
    *off += nr_bytes; 

    return nr_bytes;
}

static ssize_t configproc_read(struct file *file, char *buff, size_t len, loff_t *off) {
    char kbuf[MAX_DIGS];
    int nr_bytes = 0;

    /* Tell the application that there is nothing left to read */
    if ((*off) > 0) 
      return 0;

    nr_bytes += snprintf((kbuf+nr_bytes), MAX_DIGS, "timer_period_ms = %u\n", timer_period_ms);
    nr_bytes += snprintf((kbuf+nr_bytes), MAX_DIGS, "emergency_threshold = %u\n", emergency_threshold);

    if(len < nr_bytes)
        return -ENOSPC;

    if(copy_to_user(buff, kbuf, nr_bytes))
        return -EFAULT;
    
    *off += nr_bytes;

    printk(KERN_INFO "codeconfig: Read.\n");

    return nr_bytes;
}

static ssize_t configproc_write(struct file *file, const char *buff, size_t len, loff_t *off) {
    /* Private copy of the data in kernel space */
    char kbuf[MAX_CHARS];
    unsigned int num;

    if(len > MAX_CHARS)
        return -ENOSPC;

    /* Transfer user data to kernel space */
    if(copy_from_user(kbuf, buff, len))
        return -EFAULT;

    kbuf[len] = '\0';

    /* Parsing the operation */
    if(sscanf(kbuf, "timer_period_ms %u", &num) == 1)
        timer_period_ms = num;
    else if(sscanf(kbuf, "emergency_threshold %u", &num) == 1)
        emergency_threshold = num;
    else
        return -EINVAL;
    
    *off += len;

    printk(KERN_INFO "codeconfig: Write.\n");

    return len;
}



/* /proc/codetimer file operations */
static const struct file_operations proc_timer_fops = {
    .open = timerproc_open,
    .release = timerproc_release,
    .read = timerproc_read,
};

/* /proc/codeconfig file operations */
static const struct file_operations proc_config_fops = {
    .read = configproc_read,
    .write = configproc_write,
};





int init_codetimer_module( void ) {
    int retval;

    /* Initialize the lists */
    INIT_LIST_HEAD(&mylistOdd);
    INIT_LIST_HEAD(&mylistEven);

    /* Circular buffer initialization */
    if ((retval = kfifo_alloc(&cbuffer,CBUF_SIZE,GFP_KERNEL)))
        return -ENOMEM;

    /* Initializing the waiting queue semaphore to 0 */
    sema_init(&queue_even, 0);
    sema_init(&queue_odd, 0);
    waiting_even = 0;
    waiting_odd = 0;

    /* Initializing the semaphore that allows mutual exclusion of the linked list to 1 */
    sema_init(&sem_list_even, 1);
    sema_init(&sem_list_odd, 1);

    proc_timer_entry = proc_create_data("codetimer",0666, NULL, &proc_timer_fops, NULL);
    proc_config_entry = proc_create_data("codeconfig",0666, NULL, &proc_config_fops, NULL);

    if (proc_timer_entry == NULL) {
        kfifo_free(&cbuffer);
        printk(KERN_INFO "codetimer: Coudn't create the entry in /proc.\n");
        return  -ENOMEM;
    }

    if (proc_config_entry == NULL) {
        kfifo_free(&cbuffer);
        printk(KERN_INFO "codeconfig: Coudn't create the entry in /proc.\n");
        return  -ENOMEM;
    }

    printk(KERN_INFO "codetimer: Module loaded.\n");
    printk(KERN_INFO "codeconfig: Module loaded.\n");

    return 0;
}


void cleanup_codetimer_module( void ) {
	/* Frees the circular buffer */
    kfifo_free(&cbuffer);
    
    /* Delete elements of both the linked lists */
    cleanup(0);
    cleanup(1);

    /* Remove /proc file entrys */
    remove_proc_entry("codetimer", NULL);
    remove_proc_entry("codeconfig", NULL);

    printk(KERN_INFO "codetimer: Module unloaded.\n");
    printk(KERN_INFO "codeconfig: Module unloaded.\n");

}

module_init( init_codetimer_module );
module_exit( cleanup_codetimer_module );
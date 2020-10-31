#include <linux/module.h> 
#include <linux/kernel.h> 	/* KERNINFO */
#include <linux/proc_fs.h>	/* /proc files */
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>       /* For KDSETLED */
#include <linux/vt_kern.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Leds controller via /proc file module - Arquitectura Interna de Linux y Android UCM");
MODULE_AUTHOR("Ramón Costales de Ledesma, José Ignacio Daguerre Garrido");

/* Usage:
	make
	sudo insmod modledctl.ko
	sudo echo 0x4 > /proc/modledctl
	sudo echo 0x7 > /proc/modledctl
	sudo dmesg | tail
	sudo rmmod modledctl
	make clean
*/

#define ALL_LEDS_ON 0x7
#define ALL_LEDS_OFF 0
#define MAX_CHARS 20

/* /proc entry */
static struct proc_dir_entry *proc_entry;

struct tty_driver* kbd_driver= NULL;


/* Get driver handler */
struct tty_driver* get_kbd_driver_handler(void){
   printk(KERN_INFO "Modleds: loading\n");
   printk(KERN_INFO "Modleds: fgconsole is %x\n", fg_console);
   return vc_cons[fg_console].d->port.tty->driver;
}

/* Set led state to that specified by mask */
static inline int set_leds(struct tty_driver* handler, unsigned int mask){
    return (handler->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask);
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
static ssize_t modleds_write(struct file *filp, const char __user *buf, size_t len, loff_t *off){
	/* Private copy of the data in kernel space */
	char kbuf[MAX_CHARS];
	unsigned int leds;

	if(len > MAX_CHARS)
		return -ENOSPC;

	/* Transfer user data to kernel space */
	if(copy_from_user(kbuf, buf, len))
		return -EFAULT;

	kbuf[len] = '\0';
	sscanf(kbuf, "%x", &leds);

	set_leds(kbd_driver,leds); 

	if(leds == 0x0)
		printk(KERN_INFO "Modleds: << No se deberían encender ninguno de los LEDs >>\n");
	else if(leds == 0x1)
		printk(KERN_INFO "Modleds: << Se debería encender el LED de más a la derecha >>\n");
	else if(leds == 0x2)
		printk(KERN_INFO "Modleds: << Se debería encender el LED central >>\n");
	else if(leds == 0x3)
		printk(KERN_INFO "Modleds: << Se deberían encender los dos LEDs de más a la derecha >>\n");
	else if(leds == 0x4)
		printk(KERN_INFO "Modleds: << Se debería encender el LED de más a la izquierda >>\n");
	else if(leds == 0x5)
		printk(KERN_INFO "Modleds: << Se deberían encender los LEDs de los extremos >>\n");
	else if(leds == 0x6)
		printk(KERN_INFO "Modleds: << Se deberían encender los dos LEDs de más a la izquierda >>\n");
	else 
		printk(KERN_INFO "Modleds: << Se debería encender todos los LEDs >>\n");
	
	*off += len;

	return len;
}




/* /proc file operations */
static const struct file_operations proc_fops = {
	.write = modleds_write,
};





static int __init modleds_init(void) {	
  	kbd_driver= get_kbd_driver_handler();

   	proc_entry = proc_create("modleds", 0666, NULL, &proc_fops);

   	/* If there's not enough memory for the /proc entry, free memory */
	if(proc_entry == NULL)
		return -ENOMEM;

	printk(KERN_INFO "Modleds: Module loaded.\n");

   	set_leds(kbd_driver,ALL_LEDS_ON);
  	return 0;
}

static void __exit modleds_exit(void){
    set_leds(kbd_driver,ALL_LEDS_OFF); 

    /* Remove /proc file entry */
	remove_proc_entry("modleds", NULL);
	printk(KERN_INFO "Modleds: Module unloaded.\n");
}

module_init(modleds_init);
module_exit(modleds_exit);

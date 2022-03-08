#include <linux/syscalls.h> /* For SYSCALL_DEFINEi() */
#include <linux/kernel.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>       /* For KDSETLED */
#include <linux/init.h>
#include <linux/vt_kern.h>
#include <asm-generic/errno.h>

/*
Description: Leds controller invoke programm - Arquitectura Interna de Linux y Android UCM
Authors: Rymond3
License: GPL
*/

/* Get driver handler */
struct tty_driver* get_kbd_driver_handler(void){
   printk(KERN_INFO "Ledctl: loading\n");
   printk(KERN_INFO "Ledctl: fgconsole is %x\n", fg_console);
   return vc_cons[fg_console].d->port.tty->driver;
}

/* Set led state to that specified by mask */
static inline int set_leds(struct tty_driver* handler, unsigned int mask){
	printk(KERN_INFO "Ledctl: Change led state to %u\n", mask);
    return (handler->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask);
}

/* Definition of the system call */
SYSCALL_DEFINE1(ledctl,unsigned int,leds){
	struct tty_driver* kbd_driver= NULL;

	/* Checks the argument */
	if(leds < 0x0 || leds > 0x7){
		return -EINVAL;
	}

	/* Gets the handler and turns on the leds specified by the leds variable */
	kbd_driver = get_kbd_driver_handler();	
   	
   	return set_leds(kbd_driver, (leds & 1 | ((leds & (1<<1)) << 1) | ((leds & (1<<2)) >> 1))); 
}

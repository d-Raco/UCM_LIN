#include <linux/errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>

/*
Description: Leds controller invoke programm - Arquitectura Interna de Linux y Android UCM
Authors: Ramón Costales de Ledesma, José Ignacio Daguerre Garrido
License: GPL
*/

/* Usage:  
	gcc -Wall -g ledctl_invoke.c -o ledctl_invoke   	//Compiles the programm
	sudo ./ledctl_invoke 0x5						//Turns on some leds
	sudo ./ledctl_invoke 0x0						//Turns off all leds
	sudo ./ledctl_invoke 0x7						//Turns on all leds
*/

/* Identifier of the ledctl system call (linux-4.9.111/arch/x86/entry/syscalls/syscall_64.tbl) */
#define NR_LEDS_LIN 332

/* Calls ledctl sys_call, which will turn on the leds specified in ledmask */
long lin_ledctl(unsigned int ledsmask) {
	return (long) syscall(NR_LEDS_LIN, ledsmask);
}

int main(int argc, char **argv) {
	unsigned int leds;

	/* Check if there's only  one argument */
	if(argc < 2 || argc > 2){
		printf("Usage: ./ledctl_invoke <ledmask>\n");
		return 1;
	}
	
	/* Convert into unsigned int */
	sscanf(argv[1], "%x", &leds);

	/* Calls the ledctl sys_call and checks for errors */
	if(lin_ledctl(leds) < 0){
		perror("Failed to light up the leds...");
		return 2;
	}

	/* Prints which leds have turned on */
	if(leds == 0x0)
		printf("<< No se deberían encender ninguno de los LEDs >>\n");
	else if(leds == 0x1)
		printf("<< Se debería encender el LED de más a la derecha >>\n");
	else if(leds == 0x2)
		printf("<< Se debería encender el LED central >>\n");
	else if(leds == 0x3)
		printf("<< Se deberían encender los dos LEDs de más a la derecha >>\n");
	else if(leds == 0x4)
		printf("<< Se debería encender el LED de más a la izquierda >>\n");
	else if(leds == 0x5)
		printf("<< Se deberían encender los LEDs de los extremos >>\n");
	else if(leds == 0x6)
		printf("<< Se deberían encender los dos LEDs de más a la izquierda >>\n");
	else 
		printf("<< Se debería encender todos los LEDs >>\n");

	return 0;
}

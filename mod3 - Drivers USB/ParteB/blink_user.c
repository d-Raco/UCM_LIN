#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>

/*
 * Simple user program for interacting with the driver for the Blinkstick Strip USB device (v1.0)
 *
 * Programmed by Rymond3 and joseignaciodg
 * 
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation, version 2.
 *
 * 
 */

/* 	
	sudo rmmod usbhid
	sudo modprobe usbhid quirks=0x20A0:0x41E5:0x0004
	make
	sudo insmod blinkdrv.ko
	gcc -pthread -Wall -g blink_user.c -o blink_user
	./blink_user
*/

#define LEDS 8
#define MAX_SIZE 87
#define COLOR_ARRAY 6
#define COLOR_SIZE 9

char colors[COLOR_ARRAY][COLOR_SIZE] = {
	"0x110000",
	"0x001100",
	"0x000011",
	"0x111100",
	"0x110011",
	"0x001111"
};

int fd;

void initializeToZero(char ledColors[LEDS][COLOR_SIZE]){
	for(int i = 0; i < LEDS; ++i){
		snprintf((ledColors[i]), COLOR_SIZE, "0x000000");
	}
}

int lightupLeds(char ledColors[LEDS][COLOR_SIZE]){
	char buf[MAX_SIZE];
	int i, bytes = 0;

	for(i = 0; i < LEDS-1; ++i){
		bytes += snprintf((buf + bytes), MAX_SIZE, "%d:%s,", i, ledColors[i]);
	}
	bytes += snprintf((buf + bytes), MAX_SIZE, "%d:%s", i, ledColors[i]);
	
	return write(fd, buf, bytes);
}

/* Winner led sequence */
void winnerSequence(char ledColors[LEDS][COLOR_SIZE], char singleColor[COLOR_SIZE]){
	int counter = 3;

	while(counter--){
		initializeToZero(ledColors);
		for(int i = 0; i < LEDS; i+=2)
			strncpy((ledColors[i]), singleColor, COLOR_SIZE);

		if(lightupLeds(ledColors)<0){
			perror("Error lighting up the leds");
		}
		sleep(1);

		initializeToZero(ledColors);
		for(int i = 1; i < LEDS; i+=2)
			strncpy((ledColors[i]), singleColor, COLOR_SIZE);

		if(lightupLeds(ledColors)<0){
			perror("Error lighting up the leds");
		}
		sleep(1);
	}
}

/* Option 0 */
void colorRace(){
	/* turn controls the opponent turns */
	int r1, r2, turn = 0;
	/* boolean for controling loop */
	int win = 0;
	/* Count the leds lit up for opponent 1 and 2 */
	int cnt1 = 0, cnt2 = 7;
	char aux;
	/* Stores a single color */
	char singleColor[COLOR_SIZE];
	/* Will store the colors of the leds */
	char ledColors[LEDS][COLOR_SIZE];
	initializeToZero(ledColors);
	srand(time(NULL));

	printf("Welcome to color race!\n\n");
	printf("In this game you'll have to test your luck against your partner's.\n");
	printf("You both start in one side of the blinkstick. Your goal is to reach the other side.\n");
	printf("Each turn one of you will have to throw a die. You will advance as many leds as your die's score.\n");
	printf("Have fun.\n\n");
	sleep(1);

	printf("Opponent 1 will be led 0 (beginning of the stick), opponent 2 will be led 7 (end of the stick).\n");
	printf("Opponent 1 will begin.\n\n");
	sleep(1);

	r1 = rand() % COLOR_ARRAY;
	snprintf((ledColors[cnt1]), COLOR_SIZE, colors[r1]);
	r2 = rand() % COLOR_ARRAY;
	while(r1 == r2)
		r2 = rand() % COLOR_ARRAY;
	snprintf((ledColors[cnt2]), COLOR_SIZE, colors[r2]);
	if(lightupLeds(ledColors)<0){
		perror("Error lighting up the leds");
	}
	
	getchar();
	while(!win){
		printf("Opponent %d, press enter to roll the die:", (turn+1));
		scanf("%c", &aux);
		r1 = rand() % 4;
		printf("%d\n", r1);

		for(int i = 0; i < r1; ++i){
			if(turn == 0){
				if(cnt1 < LEDS - 1){
					strncpy((ledColors[cnt1+1]), ledColors[cnt1], COLOR_SIZE);
					//snprintf((ledColors[cnt1+1]), COLOR_SIZE, ledColors[cnt1]);
					cnt1++;
					if(cnt1 == cnt2)
						cnt2++;
					if(lightupLeds(ledColors)<0){
						perror("Error lighting up the leds");
					}
					sleep(1);
				}
			}
			else{
				if(cnt2 > 0){
					strncpy((ledColors[cnt2-1]), ledColors[cnt2], COLOR_SIZE);	
					//snprintf((ledColors[cnt2-1]), COLOR_SIZE, ledColors[cnt2]);
					cnt2--;
					if(cnt1 == cnt2)
						cnt1--;
					if(lightupLeds(ledColors)<0){
						perror("Error lighting up the leds");
					}
					sleep(1);
				}
			}
		}

		if(cnt1 == LEDS-1){
			strncpy(singleColor, (ledColors[0]), COLOR_SIZE);
			win = 1;
			printf("Congratulations, opponent 1, you win!!!\n\n");
		}
		else if(cnt2 == 0){
			strncpy(singleColor, (ledColors[LEDS-1]), COLOR_SIZE);
			win = 1;
			printf("Congratulations, opponent 2, you win!!!\n\n");
		}

		turn = (turn+1) % 2;
	}

	winnerSequence(ledColors, singleColor);

	initializeToZero(ledColors);
	if(lightupLeds(ledColors)<0){
		perror("Error lighting up the leds");
	}
}

void initializeCentre(char ledColors[LEDS][COLOR_SIZE]){
	int centre = 4;
	for(int i = 0; i < LEDS; ++i){
		snprintf((ledColors[i]), COLOR_SIZE, "0x000000");
	}
	snprintf((ledColors[centre]), COLOR_SIZE, "0x110000");
}


pthread_mutex_t lock =  PTHREAD_MUTEX_INITIALIZER;
pthread_t tid;
int keypressed = 0, win = 0;
int position = 0, going = 1;


void *threadFunction(void *arg){
	int exitLoop = 1;
	/* Will store the colors of the leds */
	char ledColors[LEDS][COLOR_SIZE];

	while(!win) {
	    while(exitLoop) {
	    	usleep(5000);
	        initializeCentre(ledColors);
			snprintf((ledColors[position]), COLOR_SIZE, "0x001100");
			lightupLeds(ledColors);

			if(going){
				if(position == LEDS-1){
					going = 0;
					position--;
				}
				else
					position++;
			}
			if(!going){
				if(position == 0){
					going = 1;
					position++;
				}
				else
					position--;
			}

	        pthread_mutex_lock(&lock);
	        if(keypressed == 1)
	            exitLoop = 0;
	        pthread_mutex_unlock(&lock);
	    }
	sleep(2);
	exitLoop = 1;
	}
    return NULL;
}

void stopIt(){
	char str[10];
	/* Will store the colors of the leds */
	char ledColors[LEDS][COLOR_SIZE];

	keypressed = 0;
	win = 0;
    position = 0;
    going = 1;

	/* Start threadFunction in new thread */
	pthread_create(&tid,NULL,&threadFunction, NULL); 

	while(!win){
		keypressed = 0;
		printf("Type [stop] to stop the led.\n");
	    scanf("%s",str);
	    
	    if(strcmp(str, "stop") == 0){
	        pthread_mutex_lock(&lock);
	        /* Setting this will cause the loop in threadFunction to break */
	        keypressed = 1;
	        pthread_mutex_unlock(&lock);
	        
	        if(position == 4){
		    	win = 1;
			}
			sleep(1);
	    }
	}

	/* Wait for the threadFunction to complete execution */
	pthread_join(tid,NULL);

	printf("Congratulations, you won!!!\n\n");
	winnerSequence(ledColors, "0x001100");

	initializeToZero(ledColors);
	if(lightupLeds(ledColors)<0){
		perror("Error lighting up the leds");
	}
}

int main(){
	int option = 1;
	
	if((fd = open("/dev/usb/blinkstick0", O_WRONLY)) < 0){
		printf("Error: couldn't open the blinkstick0 file\n");
		return -1;
	}
	
	while(option != 0){
		printf("Welcome! Choose an option between:\n");
		printf("1. Color race.\n");
		printf("2. Stop it.\n");
		printf("0. Exit.\n");

		printf("\nType your option: ");
		scanf("%d", &option);

		while(option < 0 || option > 2){
			printf("Invalid option! Try again...\n");
			printf("Type your option: ");
			scanf("%d", &option);
		}

		if(option == 1)
			colorRace();
		else if(option == 2)
			stopIt();
	}

	close(fd);
	return 0;
}

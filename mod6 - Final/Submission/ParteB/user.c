#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* gcc -g -Wall -static -lpthread user.c -o user */

#define MAX 4
#define SIZE 2
#define SIZEWORDS 5

pthread_t tid[MAX][SIZE];

char procNames[MAX][SIZE] = {
	"a",
	"b",
	"c",
	"d"
};

char procTypes[2][SIZE] = {
	"i",
	"s"
};

char words[MAX][SIZEWORDS] = {
	"hola",
	"puro",
	"celo",
	"cara"
};

struct thread_args {
	char *name;
	char *type;
};

void *producer(void *arg) {
	struct thread_args* proc = arg;
	int fd;
	char str[20];

	sleep(2);

	sprintf(str, "/proc/multipc/%s", proc->name);

	for (int j = 0; j < MAX; ++j) {
		fd = open(str, 0666);

		if (strcmp(proc->type, "i") == 0) {
			write(fd, &j, sizeof(int));

			printf("%s produced %d\n", proc->name, j);
		}
		else {
			write(fd, words[j], SIZEWORDS-1);

			printf("%s produced %s\n", proc->name, words[j]);	
		}
	}

	return NULL;
}

void *consumer(void *arg) {
	struct thread_args* proc = arg;
	int fd;
	int num;
	char dir[20], str[SIZEWORDS+1];

	sleep(2);

	sprintf(dir, "/proc/multipc/%s", proc->name);

	for (int j = 0; j < MAX; ++j) {
		fd = open(dir, 0666);

		if (strcmp(proc->type, "i") == 0) {
			read(fd, &num, sizeof(int));

			printf("%s consumed %d\n", proc->name, num);
		}
		else {
			read(fd, str, SIZEWORDS-1);
			str[SIZEWORDS] = '\0';

			printf("%s consumed %s\n", proc->name, str);	
		}
	}

	return NULL;
}

int main() {
	int fd_admin;
	char str[50];
	int nr_bytes;
	struct thread_args* args[MAX];

	

	for (int cnt = 0; cnt < MAX; ++cnt) {
		if ((fd_admin = open("/proc/multipc/admin", 0666)) < 0) {
			perror("open");
			return -1;
		}	

		nr_bytes = sprintf(str, "new %s %s\n", procNames[cnt], procTypes[cnt % 2]);
		printf("%s", str);

		/* Create /porc/multipc entry */
		if (write(fd_admin, str, nr_bytes) < 0) {
			perror("write");
			return -1;
		}

		args[cnt] = malloc(sizeof *args);
		args[cnt]->name = procNames[cnt];
		args[cnt]->type = procTypes[cnt % 2];
		
		/* Start producer in new thread */
		pthread_create(&tid[cnt][0],NULL,&producer, args[cnt]); 

		/* Start consumer in new thread */
		pthread_create(&tid[cnt][1],NULL,&consumer, args[cnt]); 
	}

	for (int counter = 0; counter < MAX; ++counter) {
		pthread_join(tid[counter][0], NULL);
		pthread_join(tid[counter][1], NULL);
		free(args[counter]);

		if ((fd_admin = open("/proc/multipc/admin", 0666)) < 0) {
			perror("open");
			return -1;
		}	

		nr_bytes = sprintf(str, "delete %s\n", procNames[counter]);
		printf("%s", str);

		/* Create /porc/multipc entry */
		if (write(fd_admin, str, nr_bytes) < 0) {
			perror("write");
			return -1;
		}
	}

	return 0;
}
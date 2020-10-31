#include <linux/errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>

#define NR_HELLO 332

long lin_hello(void) {
	return (long) syscall(NR_HELLO);
}

int main(void) {
	return lin_hello();
}
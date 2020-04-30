#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <sys/time.h>
#include <linux/kernel.h>
#include <sys/syscall.h>
#include <sys/wait.h>

typedef struct children
{
	char name[32];
	int R, T, id, pid;
}children;

int compare(const void *a, const void *b);

void setaffinity(int cpu_num, int pid);

void unit_time();

void run_child(struct children *p);

int get_next(int method, int now, int N, struct children *processes, int *schedule);

bool check_finish_process(int now, struct children *processes, int *schedule);

void wake_process(int now, struct children *processes, int *schedule);

void switch_process(int now, struct children *processes, int *schedule);

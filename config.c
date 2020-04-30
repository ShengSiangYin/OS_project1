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
#include <sys/types.h>

typedef struct children
{
	char name[32];
	int R, T, id, pid;
}children;

int compare(const void *a, const void *b)
{
	children *one = (children *) a;
	children *two = (children *) b;
	if (one->R < two->R)
		return -1;
	else if (one->R > two->R)
		return 1;
	if (one->id < two->id)
		return -1;
	else if (one->id > two->id)
		return 1;
	return 0;
}

void setaffinity(int cpu_num, int pid)
{
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(cpu_num, &mask);
	sched_setaffinity(pid, sizeof(cpu_set_t), &mask);
}

void unit_time()
{
	volatile unsigned long j;
	for (j = 0; j < 1000000UL; ++j);
}

void run_child(struct children *p)
{
	pid_t pid = fork();

	if (pid < 0)
	{
		perror("fork error\n");
		exit(1);
	}
	else if (pid == 0)
	{
		p->pid = getpid();
		int exec_time = p->T;

		// create child will on cpu 0 first, move to 1 to wake up
		while (1)
			if (sched_getcpu()==1)
				break;
		struct timespec start_t, end_t;
		syscall(335, &start_t);
		//clock_gettime(CLOCK_REALTIME, &start_t);
		for (int i = 0; i < exec_time; ++i)
		{
			struct sched_param sp;
			sched_getparam(getpid(), &sp);
			if ((sp.sched_priority == 1) || (sched_getcpu() == 0)) // context switch happen
			{
				while (1)
				{
					struct sched_param sp;
					sched_getparam(getpid(), &sp);
					if ((sp.sched_priority == 99) && (sched_getcpu() == 1))
						break;
				}
			}
			unit_time();
		}
		syscall(335, &end_t);
		//clock_gettime(CLOCK_REALTIME, &end_t);
		// child finish
		printf("%s %d\n", p->name, getpid());

		long start, end;
		start = start_t.tv_sec * 1e9 + start_t.tv_nsec;
		end = end_t.tv_sec * 1e9 + end_t.tv_nsec;
		syscall(334, getpid(), start, end);
		
		struct sched_param sp;
		sp.sched_priority = 1;
		if (sched_setscheduler(p->pid, SCHED_FIFO, &sp) < 0)
			perror("child process exit error");
		exit(0);
	}
	else
	{
		p->pid = pid;
		struct sched_param sp;
		sp.sched_priority = 1;
		if (sched_setscheduler(p->pid, SCHED_FIFO, &sp) < 0)
			perror("initial child prior to 1 error");
	}
	return;
}

int get_next(int method, int now, int N, struct children *processes, int *schedule)
{
	int next = -1;
	int shortest_job = 99999999;
	switch (method)
	{
		case 0:
			now = (now + 1) % N;
			if ((schedule[now] == -1)||(processes[now].pid == -1))
				return -1;
			return now;
		case 1:
			for (int i = 0; i < N; ++i)
			{
				if (processes[i].id == (processes[now].id + 1))
				{
					if ((processes[i].pid != -1) && (schedule[i] != -1))
						return i;
					else
						return -1;
				}
			}
			if ((processes[now].pid != -1) && (schedule[now] != -1))
				return now;
			return -1;
		case 2:
			for (int i = 0; i < N; ++i)
			{
				if ((schedule[i] != -1) && (processes[i].pid != -1) && (processes[i].T < shortest_job))
				{
					shortest_job = processes[i].T;
					next = i;
				}
			}
			return next;
		case 3:
			for (int i = 0; i < N; ++i)
			{
				if ((processes[i].pid != -1) && (processes[i].T < shortest_job) && (processes[i].T != 0))
				{
					shortest_job = processes[i].T;
					next = i;
				}
			}
			return next;
	}
}

bool check_finish_process(int now, struct children *processes, int *schedule)
{
	struct sched_param sp;
	sched_getparam(processes[now].pid, &sp);
	if ((schedule[now] == -1) && (processes[now].pid != -1) && (processes[now].T == 0))// && (sp.sched_priority == 1))
	{
		waitpid(processes[now].pid, NULL, 0);
		processes[now].pid = -1;
		return true;
	}
	return false;
}

void wake_process(int now, struct children *processes, int *schedule)
{
	if ((schedule[now] != -1) && (processes[now].pid != -1))
	{
		setaffinity(1, processes[now].pid);
		struct sched_param sp;
		sp.sched_priority = 99;
		if (sched_setscheduler(processes[now].pid, SCHED_FIFO, &sp) < 0)
			perror("wake child error");
		schedule[now] = -1;
	}
	else
		perror("next can't be waked");
}

void switch_process(int now, struct children *processes, int *schedule)
{
	schedule[now] = 1;
	//setaffinity(0, processes[now].pid);
	struct sched_param sp;
	sp.sched_priority = 1;
	if (sched_setscheduler(processes[now].pid, SCHED_FIFO, &sp) < 0)
		perror("switch error");
}

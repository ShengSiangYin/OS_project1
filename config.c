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

void unit_time(int iter)
{
	for (int i = 0; i < iter; ++i)
	{
		volatile unsigned long j;
		for (j = 0; j < 1000000UL; ++j);
	}
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
		setaffinity(1, getpid());
		p->pid = getpid();
		printf("create pid: %d\n", getpid());
		struct sched_param sp;
		sp.sched_priority = 1;
		if (sched_setscheduler(p->pid, SCHED_FIFO, &sp) < 0)
			perror("when creating set to 1");

		sched_getparam(p->pid, &sp);
		int exec_time = p->T;
		while (1)
		{
			struct sched_param sp;
			sched_getparam(getpid(), &sp);
			if (sp.sched_priority == 99)
				break;
		}
		struct timespec start_t, end_t;
		syscall(335, &start_t);
		for (int i = 0; i < exec_time; ++i)
		{
			struct sched_param sp;
			sched_getparam(getpid(), &sp);
			if (sp.sched_priority == 1)
			{
				printf("pid: %d !!\n", pid);
				while (1)
				{
					struct sched_param sp;
					sched_getparam(getpid(), &sp);
					if (sp.sched_priority == 99)
						break;
				}
			}
			unit_time(1);
		}
		syscall(335, &end_t);
		
		printf("%s %d\n", p->name, getpid());

		long start, end;
		start = start_t.tv_sec * 1e9 + start_t.tv_nsec;
		end = end_t.tv_sec * 1e9 + end_t.tv_nsec;
		syscall(334, getpid(), start, end);

		sp.sched_priority = 1;
		sched_setscheduler(p->pid, SCHED_FIFO, &sp);
		exit(0);
	}
	else
	{
		p->pid = pid;
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
			now = (now + 1) % N;
			if ((schedule[now] != -1)&&(processes[now].pid != -1))
				return now;
			else
				return -1;
			/*for (int i = 0; i < N; ++i)
			{
				if ((schedule[(now + i + 1) % N] != -1)&&(processes[(now + i + 1) % N].pid != -1))
				{
					printf("now = %d, next = %d, pid = %d\n", now, (now+i+1)%N, processes[(now+i+1)%N].pid);
					next = (now + i + 1) % N;
					break;
				}
			}
			return next;*/
		case 2:
			for (int i = 0; i < N; ++i)
			{
				if ((schedule[i] != -1)&&(processes[i].pid != -1)&&(processes[i].T < shortest_job))
				{
					shortest_job = processes[i].T;
					next = i;
				}
			}
			return next;
		case 3:
			for (int i = 0; i < N; ++i)
			{
				if ((processes[i].pid != -1)&&(processes[i].T < shortest_job)&&(processes[i].T != 0))
				{
					shortest_job = processes[i].T;
					next = i;
				}
			}
			return next;
	}
}

bool check_finish_process(int N, int *now, struct children *processes, int *schedule, int method)
{
	if (*now != -1)
	{
		struct sched_param sp;
		sched_getparam(processes[*now].pid, &sp);
		if ((sp.sched_priority == 1)&&(schedule[*now] == -1)&&(processes[*now].T == 0))
		{
			printf("now: %d check wait for pid=%d\n",*now, processes[*now].pid);
			waitpid(processes[*now].pid, NULL, 0);
			processes[*now].pid = -1;
		}
		if ((processes[*now].pid == -1)&&(schedule[*now] == -1))
		{
			*now = get_next(method, *now, N, processes, schedule);
			if (*now != -1)
				return true;
		}
		return false;
	}
	else
	{
		*now = get_next(method, *now, N, processes, schedule);
		if (*now == -1)
			return false;
	}
	return true;
}

bool wake_process(int *now, struct children *processes, int *schedule)
{
	if ((schedule[*now] != -1)&&(processes[*now].pid != -1))
	{
		setaffinity(1, processes[*now].pid);
		struct sched_param sp;
		sched_getparam(processes[*now].pid, &sp);
		while (1)
		{
			sched_getparam(processes[*now].pid, &sp);
			if (sp.sched_priority == 1)
				break;
		}
		sp.sched_priority = 99;
		if (sched_setscheduler(processes[*now].pid, SCHED_FIFO, &sp) < 0)
			perror("sched_setscheduler error");

		schedule[*now] = -1;
		return true;
	}
	return false;
}

bool switch_process(int N, int *now, struct children *processes, int *schedule, int method)
{
	if (*now == -1)
		return true;
	else
	{
		if ((processes[*now].T == 0)&&(processes[*now].pid != -1))
		{
			printf("wait now = %d pid = %d\n", *now, processes[*now].pid);
			waitpid(processes[*now].pid, NULL, 0);
			processes[*now].pid == -1;
		}
	}

	setaffinity(1, processes[*now].pid);
	struct sched_param sp;
	sp.sched_priority = 1;
	if (sched_setscheduler(processes[*now].pid, SCHED_FIFO, &sp) < 0)
		perror("sched_setscheduler error(switch process)");
	if (processes[*now].pid != -1)
		schedule[*now] = processes[*now].id;
	else
		schedule[*now] = -1;
	*now = get_next(method, *now, N, processes, schedule);
	if (*now != -1)
	{
		printf("wake %d\n", *now);
		return wake_process(now, processes, schedule);
	}
	else
		return false;
}

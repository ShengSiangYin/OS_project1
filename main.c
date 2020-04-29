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
#include "config.h"

children processes[64];
int schedule[64];

int main(int argc, char const *argv[])
{
	char S[4];
	static int N;
	scanf("%s\n%d", S, &N);

	for (int i = 0; i < N; ++i)
	{
		scanf("%s %d %d", processes[i].name, &processes[i].R, &processes[i].T);
		processes[i].id = i;
		processes[i].pid = -1;
	}

	setaffinity(0, 0);
	struct sched_param sp;
	sp.sched_priority = 99;
	if (sched_setscheduler(getpid(), SCHED_FIFO, &sp)<0)
		perror("sched_setscheduler error");

	qsort(processes, N, sizeof(children), compare);

	volatile unsigned long timestamp = 0;
	static	int now = -1;
	for (int i = 0; i < N; ++i)
		schedule[i] = processes[i].id;

	if (strcmp(S, "FIFO") == 0)
	{
		while (1)
		{
			// fork child process ready at the time
			for (int i = 0; i < N; ++i)
				if (timestamp == processes[i].R)
					run_child(&processes[i]);

			if (check_finish_process(N, &now, processes, schedule, 0))
				if (wake_process(&now, processes, schedule))
					schedule[now] = -1;

			// check if there's unrun process, if no, break the loop
			int wait_num = 0;
			for (int i = 0; i < N; ++i)
				if ((schedule[i] != -1)||(processes[i].pid != -1))
					wait_num ++;
			if (wait_num == 0)
				break;

			unit_time(1);
			timestamp ++;
			if ((processes[now].T > 0)&&(schedule[now] == -1))
				processes[now].T --;
		}
		return 0;
	}
	else if (strcmp(S, "RR") == 0)
	{
		long roundtime = 500;
		long clockstart = 0;
		int now = 0;
		int tail = 0;
		int queue[N];
		queue[0] = -1;
		while (1)
		{
			// fork child process ready at the time
			for (int i = 0; i < N; ++i)
			{
				if (timestamp == processes[i].R)
				{
					run_child(&processes[i]);
					queue[tail] = processes[i].id;
					tail = (tail + 1) % N;
				}
			}
			printf("timestamp:%lu\n", timestamp);
			for (int i = 0; i < N; ++i)
				printf("queue[%d]=%d\n", i, queue[i]);

			if ((timestamp - clockstart) == roundtime)
			{
				if ((processes[queue[now]].T == 0)&&(processes[queue[now]].pid != -1))
				{
					waitpid(processes[queue[now]].pid, NULL, 0);
					processes[queue[now]].pid = -1;
					now = (now + 1) % N;
					wake_process(&queue[now], processes, schedule);
				}
				else
				{
					setaffinity(1, processes[queue[now]].pid);
					struct sched_param sp;
					sp.sched_priority = 1;
					if (sched_setscheduler(processes[queue[now]].pid, SCHED_FIFO, &sp) < 0)
						perror("sched_setscheduler error (switch)");
					queue[tail] = queue[now];
					now = (now + 1) % N;
					tail = (tail + 1) % N;
					wake_process(&queue[now], processes, schedule);
				}		
			}
			else
			{
				if ((processes[queue[now]].T == 0)&&(processes[queue[now]].pid != -1))
				{
					struct sched_param sp;
					sched_getparam(processes[queue[now]].pid, &sp);
					if ((sp.sched_priority == 1)&&(schedule[queue[now]] == -1))
					{
						waitpid(processes[queue[now]].pid, NULL, 0);
						processes[queue[now]].pid = -1;
						now = (now + 1) % N;
					}
					if (now == tail)
					{
						printf("now = tail = %d\n", now);
						break;
					}
					else
					{
						wake_process(&queue[now], processes, schedule);
					}
				}
				else if ((queue[now] == -1)&&(now != tail))
				{
					now = (now + 1) % N;
					wake_process(&queue[now], processes, schedule);
				}
			}

			// check if there's unrun process, if no, break the loop
			int wait_num = 0;
			for (int i = 0; i < N; ++i)
				if ((schedule[i] != -1)||(processes[i].pid != -1))
					wait_num ++;
			if (wait_num == 0)
				break;

			if ((now == tail)&&(queue[now] != -1))
				break;

			unit_time(1);
			timestamp ++;
			if ((processes[queue[now]].T > 0)&&(schedule[queue[now]] == -1))
				processes[queue[now]].T --;
		}
		return 0;
	}
	else if (strcmp(S, "SJF") == 0)
	{
		while (1)
		{
			for (int i = 0; i < N; ++i)
				if ((timestamp == processes[i].R)&&(schedule[i] != -1))
					run_child(&processes[i]);

			if (check_finish_process(N, &now, processes, schedule, 2))
				if (wake_process(&now, processes, schedule))
					schedule[now] = -1;

			int wait_num = 0;
			for (int i = 0; i < N; ++i)
				if ((schedule[i] != -1)||(processes[i].pid != -1))
					wait_num ++;
			if (wait_num == 0)
				break;

			unit_time(1);
			timestamp ++;
			if ((processes[now].T > 0)&&(schedule[now] == -1))
				processes[now].T --;
		}
	}
	else if (strcmp(S, "PSJF") == 0)
	{
		while (1)
		{
			for (int i = 0; i < N; ++i)
				if ((timestamp == processes[i].R)&&(schedule[i] != -1))
					run_child(&processes[i]);

			if (check_finish_process(N, &now, processes, schedule, 3))
				if (wake_process(&now, processes, schedule))
					schedule[now] = -1;
			
			if (!switch_process(N, &now, processes, schedule, 3))
				break;

			int wait_num = 0;
			for (int i = 0; i < N; ++i)
				if ((schedule[i] != -1)||(processes[i].pid != -1))
					wait_num ++;
			if (wait_num == 0)
				break;

			unit_time(1);
			timestamp ++;
			if ((processes[now].T > 0)&&(schedule[now] == -1))
				processes[now].T --;
		}
	}

	return 0;
}

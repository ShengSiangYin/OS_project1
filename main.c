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
		perror("parent sched_setscheduler error");

	qsort(processes, N, sizeof(children), compare);

	volatile unsigned long timestamp = 0;
	static int now = -1;
	for (int i = 0; i < N; ++i)
		schedule[i] = processes[i].id;

	if (strcmp(S, "FIFO") == 0)
	{
		while (1)
		{
			// fork child process ready at the time
			for (int i = 0; i < N; ++i)
			{
				if (timestamp == processes[i].R)
				{
					run_child(&processes[i]);
					if (now == -1)
					{
						now = i;
						wake_process(now, processes, schedule);
					}
				}
			}

			if (check_finish_process(now, processes, schedule))
			{
				now = get_next(0, now, N, processes, schedule);
				//if (now == -1)
				//	break;
				wake_process(now, processes, schedule);
			}

			// check if there's unrun process, if no, break the loop
			int wait_num = 0;
			for (int i = 0; i < N; ++i)
				if ((schedule[i] != -1)||(processes[i].pid != -1))
					wait_num ++;
			if (wait_num == 0)
				break;

			unit_time();
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
		int head = 0;
		while (1)
		{
			// fork child process ready at the time
			for (int i = 0; i < N; ++i)
			{
				if (timestamp == processes[i].R)
				{
					run_child(&processes[i]);
					processes[i].id = head;
					head ++;
					if (now == -1)
					{
						clockstart = timestamp;
						now = i;
						wake_process(now, processes, schedule);
					}
				}
			}
			if ((timestamp - clockstart) == roundtime)
			{
				if (now == -1)
				{
					timestamp ++;
					continue;
				}
				clockstart = timestamp;
				if (check_finish_process(now, processes, schedule))
				{
					now = get_next(1, now, N, processes, schedule);
					//if (now == -1)
					//	break;
					wake_process(now, processes, schedule);
				}
				else
				{
					switch_process(now, processes, schedule);
					int old;
					old = now;
					now = get_next(1, now, N, processes, schedule);
					processes[old].id = head;
					head ++;
					if (now == -1)
						now = old;
					wake_process(now, processes, schedule);
				}
			}
			else
			{
				if (check_finish_process(now, processes, schedule))
				{
					clockstart = timestamp;
					now = get_next(1, now, N, processes, schedule);
					//if (now == -1)
					//	break;
					wake_process(now, processes, schedule);
				}
			}

			// check if there's unrun process, if no, break the loop
			int wait_num = 0;
			for (int i = 0; i < N; ++i)
				if ((schedule[i] != -1)||(processes[i].pid != -1))
					wait_num ++;
			if (wait_num == 0)
				break;

			unit_time();
			timestamp ++;
			if ((processes[now].T > 0)&&(schedule[now] == -1))
				processes[now].T --;
		}
		return 0;
	}
	else if (strcmp(S, "SJF") == 0)
	{
		while (1)
		{
			for (int i = 0; i < N; ++i)
				if (timestamp == processes[i].R)
					run_child(&processes[i]);

			if (now == -1)
			{
				int shortest_job = 99999999;
				for (int i = 0; i < N; ++i)
				{
					if ((processes[i].pid != -1) && (processes[i].T < shortest_job))
					{
						shortest_job = processes[i].T;
						now = i;
					}
				}
				if (now != -1)
					wake_process(now, processes, schedule);
			}

			if (check_finish_process(now, processes, schedule))
			{
				now = get_next(2, now, N, processes, schedule);
				//if (now == -1)
				//	break;
				wake_process(now, processes, schedule);
			}

			int wait_num = 0;
			for (int i = 0; i < N; ++i)
				if ((schedule[i] != -1)||(processes[i].pid != -1))
					wait_num ++;
			if (wait_num == 0)
				break;

			unit_time();
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
				if (timestamp == processes[i].R)
					run_child(&processes[i]);

			if (now == -1)
			{
				int shortest_job = 99999999;
				for (int i = 0; i < N; ++i)
				{
					if ((processes[i].pid != -1) && (processes[i].T < shortest_job))
					{
						shortest_job = processes[i].T;
						now = i;
					}
				}
				if (now != -1)
					wake_process(now, processes, schedule);
			}


			if (check_finish_process(now, processes, schedule))
			{
				now = get_next(3, now, N, processes, schedule);
				//if (now == -1)
				//	break;
				wake_process(now, processes, schedule);
			}
			else if (now != -1)
			{
				switch_process(now, processes, schedule);
				now = get_next(3, now, N, processes, schedule);
				//if (now == -1)
				//	break;
				wake_process(now, processes, schedule);
			}
			
			int wait_num = 0;
			for (int i = 0; i < N; ++i)
				if ((schedule[i] != -1)||(processes[i].pid != -1))
					wait_num ++;
			if (wait_num == 0)
				break;

			unit_time();
			timestamp ++;
			if ((processes[now].T > 0)&&(schedule[now] == -1))
				processes[now].T --;
		}
	}

	return 0;
}

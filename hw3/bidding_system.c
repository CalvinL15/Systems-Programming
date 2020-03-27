#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

int priority, bidsys, conpid, sn[3], dec = 0;

void sigusr1_handler(int signal) {
	char msg_out[100], msg_in[100], errmsg[40];
	sprintf(msg_in, "receive 1 %d\n", sn[1]);
	if (write(bidsys, msg_in, strlen(msg_in)) < 0) {
		sprintf(errmsg, "Error writing to bidding_system_log\n");
		write(2, errmsg, strlen(errmsg));
	}
	struct timespec tim;
	struct timespec remtim;
	tim.tv_sec = 0;
	tim.tv_nsec = 500000000;
	while(1) {
		if (clock_nanosleep(CLOCK_REALTIME, 0, &tim, &remtim) == 0) 
			break;
		else
			tim = remtim;	
	}
	kill(conpid, SIGUSR1);
	sprintf(msg_out, "finish 1 %d\n", sn[1]);
	if (write(bidsys, msg_out, strlen(msg_out)) < 0) {
		sprintf(errmsg, "Error writing to bidding_system_log\n");
		write(2, errmsg, strlen(errmsg));
	}
	sn[1]++;
	return;
}
void sigusr2_handler(int signal) {
	char msg_out[100], msg_in[100], errmsg[40];
	sprintf(msg_in, "receive 2 %d\n", sn[2]);
	if (write(bidsys, msg_in, strlen(msg_in)) < 0) {
		sprintf(errmsg, "Error writing to bidding_system_log\n");
		write(2, errmsg, strlen(errmsg));
	}
	struct timespec tim;
	struct timespec remtim;
	tim.tv_sec = 0;
	tim.tv_nsec = 200000000;
	while(1) {
		if (clock_nanosleep(CLOCK_REALTIME, 0, &tim, &remtim) == 0) 
			break;
		else
			tim = remtim;	
	}
	kill(conpid, SIGUSR2);
	sprintf(msg_out, "finish 2 %d\n", sn[2]);
	if (write(bidsys, msg_out, strlen(msg_out)) < 0) {
		sprintf(errmsg, "Error writing to bidding_system_log\n");
		write(2, errmsg, strlen(errmsg));
	}
	else{
		dec++;
	}
	sn[2]++;
}

void util(int a[], int b[]){
	close(a[0]);
	close(a[1]);
	close(b[0]);
	close(b[1]);
}

int main(int argc, char const *argv[]) {
	int fdstdin[2], fdstdout[2];
	char test_[50];
	strcpy(test_, argv[1]);
	int i = 0;
	while(i < 3){
		sn[i] = 1;
		i++;
	}	
	if (pipe(fdstdin) < 0) {
		fprintf(stderr, "Error making pipe\n");
		exit(-1);
	}
	
	if (pipe(fdstdout) < 0) {
		fprintf(stderr, "Error making pipe\n");
		exit(-1);
	}
	conpid = fork();
	if (conpid == 0) {
		if (dup2(fdstdin[0], STDIN_FILENO) == -1) {
			fprintf(stderr, "Error duplicating stdin\n");
			exit(-1);
		}
		if (dup2(fdstdout[1], STDOUT_FILENO) == -1) {
			fprintf(stderr, "Error duplicating stdout\n");
			exit(-1);
		}
		util(fdstdin, fdstdout);
		char argument[30];
		memset(argument, '\0', sizeof(argument));
		strcpy(argument, argv[1]);
		execv("./customer", (char *[])
		{ "./customer", argument, NULL });
		perror("");
		exit(-1);
	}
	else if (conpid == -1) {
		fprintf(stderr, "Error forking\n");
		exit(-1);
	}
	else {
		close(fdstdin[0]);
		close(fdstdout[1]);
	}
	int maxfd = fdstdout[0] + 1;
	int flag = 1;
	char receive[9], finish[8];

	fd_set MASTER;
    FD_ZERO(&MASTER);
    FD_SET(fdstdout[0], &MASTER);

	strcpy(receive, "receive");
	strcpy(finish, "finish");
	bidsys = open("bidding_system_log", O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0777);
	if (bidsys < 0) {
		perror("");
		exit(-1);
	}
	struct sigaction sigh1;
	struct sigaction sigh2;
	sigh1.sa_handler = &sigusr1_handler;
	sigh1.sa_flags = SA_RESTART;
	sigemptyset(&sigh1.sa_mask);
	sigh2.sa_handler = &sigusr2_handler;
	sigh2.sa_flags = SA_RESTART;
	sigemptyset(&sigh2.sa_mask);
	sigaddset(&sigh2.sa_mask, SIGUSR1);

	if (sigaction(SIGUSR1, &sigh1, NULL) < 0) {
		fprintf(stderr, "Could not handle SIGUSR1\n");
	}
	if (sigaction(SIGUSR2, &sigh2, NULL) < 0) {
		fprintf(stderr, "Could not handle SIGUSR2\n");
	}
	while(flag > 0) {
		fd_set TEMP;
		FD_ZERO(&TEMP);
		TEMP = MASTER;
		struct timeval timout;
		timout.tv_sec = 0;
		timout.tv_usec = 10;
		int x = maxfd+1, rt = select(x, &TEMP, NULL, NULL, &timout);
		if (FD_ISSET(fdstdout[0], &TEMP)) {
			char msg_in[100], msg_out[100], errmsg[40];
			memset(msg_in, '\0', sizeof(msg_in));
			memset(msg_out, '\0', sizeof(msg_out));
			char buf[10];
			int x = read(fdstdout[0], buf, 9);
			if (x == 0) {
				flag = 0;
				break;
			}
			if (x < 0) {
				perror("");
				exit(-1);
			}
			sprintf(msg_in, "receive 0 %d\n", sn[0]);
			if (write(bidsys, msg_in, strlen(msg_in)) < 0) {
				sprintf(errmsg, "Error writing to bidding_system_log\n");
				int lenerr = strlen(errmsg);
				write(2, errmsg, lenerr);
			}
			struct timespec tim;
			struct timespec remtim;
			tim.tv_sec = 1;
			tim.tv_nsec = 0;
			while(1) {
				if (clock_nanosleep(CLOCK_REALTIME, 0, &tim, &remtim) != 0) 
					tim = remtim;
				else
					break;	
			}
			kill(conpid, SIGINT);
			sprintf(msg_out, "finish 0 %d\n", sn[0]);
			if (write(bidsys, msg_out, strlen(msg_out)) < 0) {
				sprintf(errmsg, "Error writing to bidding_system_log\n");
				write(2, errmsg, strlen(errmsg));
			}
			sn[0]++;
		}
	}
	if (flag < 0) {
		fprintf(stderr, "Error\n");
		dec--;
		exit(-1);
	}
	wait(NULL);
	char fnlmsg[100];
	sprintf(fnlmsg, "terminate\n");
	if (write(bidsys, fnlmsg, strlen(fnlmsg)) < 0) {
		char errmsg[40];
		dec--;
		sprintf(errmsg, "Error writing to bidding_system_log\n");
		int p = strlen(errmsg);
		write(2, errmsg, p);
	}
	close(bidsys);
	exit(0);
}

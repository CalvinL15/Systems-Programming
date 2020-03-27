#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#define SIGUSR3 SIGWINCH

int priority, bidsys, conpid;
int ser_num[3];
struct timeval deadlines[3];
struct timespec proses[3];
struct timespec remtim[3];
int emb[3], prosesi[3];
typedef long long int ll;

void sigusr1_handler(int signal) {
	char msgout[100], msgin[100], errmsg[40];
	gettimeofday(&deadlines[0], NULL);
	deadlines[0].tv_sec += 2;
	if (prosesi[1] == 1) {
		ll cmp0 = deadlines[0].tv_sec * 1000000 + deadlines[0].tv_usec;
		ll cmp1 = deadlines[1].tv_sec * 1000000 + deadlines[1].tv_usec;
		if (cmp0 > cmp1) {
			emb[0] = 1;
			return;
		}
	}
	prosesi[0] = 1;
	sprintf(msgin, "receive 0 %d\n", ser_num[0]);

	if (write(bidsys, msgin, strlen(msgin)) < 0) {
		sprintf(errmsg, "Error writing to bidding_system_log\n");
		write(2, errmsg, strlen(errmsg));
	}
	while(1) {
		if (clock_nanosleep(CLOCK_REALTIME, 0, &proses[0], &remtim[0]) == 0) 
			break;
		else
			proses[0] = remtim[0];	
	}
	kill(conpid, SIGUSR1);
	sprintf(msgout, "finish 0 %d\n", ser_num[0]);
	int lenout = strlen(msgout);
	if (write(bidsys, msgout, lenout) < 0) {
		sprintf(errmsg, "Error writing to bidding_system_log\n");
		int lenerr = strlen(errmsg);
		write(2, errmsg, lenerr);
	}
	proses[0].tv_nsec = 500000000;
	prosesi[0] = 0;
	ser_num[0]++;
	emb[0] = 0;
	if (emb[2] == 1)
		kill(getpid(), SIGUSR3);
	return;
}

void sigusr2_handler(int signal) {
	char msgout[100], msgin[100], errmsg[40];
	gettimeofday(&deadlines[1], NULL);
	deadlines[1].tv_sec += 3;
	prosesi[1] = 1;
	sprintf(msgin, "receive 1 %d\n", ser_num[1]);
	if (write(bidsys, msgin, strlen(msgin)) < 0) {
		sprintf(errmsg, "Error writing to bidding_system_log\n");
		write(2, errmsg, strlen(errmsg));
	}
	while(1) {
		if (clock_nanosleep(CLOCK_REALTIME, 0, &proses[1], &remtim[1]) == 0) 
			break;
		else
			proses[1] = remtim[1];	
	}
	kill(conpid, SIGUSR2);
	sprintf(msgout, "finish 1 %d\n", ser_num[1]);
	if (write(bidsys, msgout, strlen(msgout)) < 0) {
		sprintf(errmsg, "Error writing to bidding_system_log\n");
		write(2, errmsg, strlen(errmsg));
	}
	proses[1].tv_sec = 1;
	proses[1].tv_nsec = 0;
	prosesi[1] = 0;
	ser_num[1]++;
	if (emb[2] == 1)
		kill(getpid(), SIGUSR1);
	if (emb[0] == 1)
		kill(getpid(), SIGUSR3);
}

void sigusr3_handler(int signal) {
	char msgout[100], msgin[100], errmsg[40];
	gettimeofday(&deadlines[2], NULL);
	deadlines[2].tv_usec += 3;
	if (prosesi[0] == 1 || prosesi[1] == 1) {
		ll cmp0, cmp1, cmp2;
		cmp2 = deadlines[2].tv_sec * 1000000 + deadlines[2].tv_usec;
		if (prosesi[0] == 1) 
			cmp0 = deadlines[0].tv_sec * 1000000 + deadlines[0].tv_usec;
		else
			cmp0 = cmp2 + 1;
		if (prosesi[1] == 1) 
			cmp1 = deadlines[1].tv_sec * 1000000000 + deadlines[1].tv_usec;
		else
			cmp1 = cmp2 + 1;
		if (cmp2 > cmp1 || cmp2 > cmp0) {
			emb[2] = 1;
			return;
		}
	}
	prosesi[2] = 1;
	sprintf(msgin, "receive 2 %d\n", ser_num[2]);
	if (write(bidsys, msgin, strlen(msgin)) < 0) {
		sprintf(errmsg, "Error writing to bidding_system_log\n");
		write(2, errmsg, strlen(errmsg));
	}
	while (1) {
		if (clock_nanosleep(CLOCK_REALTIME, 0, &proses[2], &remtim[2]) == 0) 
			break;
		else
			proses[2] = remtim[2];
	}
	kill(conpid, SIGUSR3);
	sprintf(msgout, "finish 2 %d\n", ser_num[2]);
	if (write(bidsys, msgout, strlen(msgout)) < 0) {
		sprintf(errmsg, "Error writing to bidding_system_log\n");
		write(2, errmsg, strlen(errmsg));
	}
	proses[2].tv_nsec = 200000000;
	prosesi[2] = 0;
	ser_num[2]++;
	emb[2] = 0;
}

void util(int a[], int b[], int x){
	if(x == 1){
		close(a[0]);
		close(a[1]);
		close(b[0]);
		close(b[1]);
	}
	else{
		close(a[0]);
		close(b[1]);
	}
}

int main(int argc, char const *argv[]) {
	char test_[50];
	int fdstdin[2], fdstdout[2];
	strcpy(test_, argv[1]);
	proses[0].tv_sec = 0;
	proses[0].tv_nsec = 500000000;
	proses[1].tv_sec = 1;
	proses[1].tv_nsec = 0;
	proses[2].tv_sec = 0;
	proses[2].tv_nsec = 200000000;
	int i = 0;
	while(i < 3){
		ser_num[i] = 1;
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
		util(fdstdin, fdstdout, 1);
		char argument[30];
		memset(argument, '\0', sizeof(argument));
		strcpy(argument, argv[1]);
		execv("./customer_EDF", (char *[]){ "./customer_EDF", argument, NULL });
		perror("");
		exit(-1);
	}
	
	else if (conpid == -1) {
		fprintf(stderr, "Error forking\n");
		exit(-1);
	}
	
	else 
		util(fdstdin, fdstdout, 0);
	
	int maxfd = fdstdout[0] + 1, flag = 1;
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
	struct sigaction sigh3;
	sigh1.sa_handler = &sigusr1_handler;
	sigh1.sa_flags = SA_RESTART;
	sigemptyset(&sigh1.sa_mask);
	sigaddset(&sigh1.sa_mask, SIGUSR2);
	sigh2.sa_handler = &sigusr2_handler;
	sigh2.sa_flags = SA_RESTART;
	sigemptyset(&sigh2.sa_mask);
	sigh3.sa_handler = &sigusr3_handler;
	sigh3.sa_flags = SA_RESTART;
	sigemptyset(&sigh3.sa_mask);
	sigaddset(&sigh3.sa_mask, SIGUSR1);
	sigaddset(&sigh3.sa_mask, SIGUSR2);

	if (sigaction(SIGUSR1, &sigh1, NULL) < 0) {
		fprintf(stderr, "Could not handle SIGUSR1\n");
		exit(-1);
	}
	if (sigaction(SIGUSR2, &sigh2, NULL) < 0) {
		fprintf(stderr, "Could not handle SIGUSR2\n");
		exit(-1);
	}
	if (sigaction(SIGUSR3, &sigh3, NULL) < 0) {
		fprintf(stderr, "Could not handle SIGUSR3\n");
		exit(-1);
	}

	while(1) {
		fd_set TEMP;
		FD_ZERO(&TEMP);
		TEMP = MASTER;
		struct timeval tout;
		tout.tv_sec = 0;
		tout.tv_usec = 10;
		int x = maxfd+1;
		int rt = select(x, &TEMP, NULL, NULL, &tout);
		if (FD_ISSET(fdstdout[0], &TEMP)) {
			char buf[11];
			int szb = sizeof(buf);
			memset(buf, '\0', szb);
			if (read(fdstdout[0], buf, 10) < 0) {
				perror("");
				exit(-1);
			}
			if (strcmp(buf, "terminate\n") == 0)
				break;
		}
	}
	wait(NULL);
	char fnlmsg[100];
	sprintf(fnlmsg, "terminate\n");
	if (write(bidsys, fnlmsg, strlen(fnlmsg)) < 0) {
		char errmsg[40];
		sprintf(errmsg, "Error writing to bidding_system_log\n");
		write(2, errmsg, strlen(errmsg));
	}
	close(bidsys);
	exit(0);
}



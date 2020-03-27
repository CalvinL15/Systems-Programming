#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#define BIGNUM 100000000
#include <limits.h>
#include <sys/time.h>
#include <time.h>

int customer_log, proc, prioritas[3], nuevo;

void signal_handler(int signal) {
	char msg[100];
	int sat;
	memset(msg, '\0', sizeof(msg));
	char errmsg1[20], errmsg2[36];
	if(signal == SIGINT) 	
		sat = 0;
	else if(signal == SIGUSR1) {	
		sat = 1;
		struct itimerval waktu;
		waktu.it_interval.tv_sec = 0;
		waktu.it_interval.tv_usec = 0;
		waktu.it_value.tv_sec = 0;
		waktu.it_value.tv_usec = 0;
		if (setitimer(ITIMER_REAL, &waktu, NULL) < 0) {
			perror("");
			exit(-1);
		}
	}
	else if(signal == SIGUSR2)
		sat = 2;
	else{
		sprintf(errmsg1, "Wrong signal sent\n");
		write(2, errmsg1, sizeof(errmsg1));
		exit(-1);
		return;
	}
	
	sprintf(msg, "finish %d %d\n", sat, prioritas[sat]);
	if (write(customer_log, msg, strlen(msg)) < 0) {
		sprintf(errmsg2, "Error writing data to customer_log\n");
		write(2, errmsg2, sizeof(errmsg2));
	}
	prioritas[sat]++;
	proc--;
	return;
}

void sig_alarm_handler(int signal) {
	char msg[100];
	sprintf(msg, "timeout\n");
	int leng = strlen(msg);
	char errmsg[38];
	if (write(customer_log, msg, leng) < 0) {
		sprintf(errmsg, "Error writing data to costumer log\n");
		int len = strlen(errmsg);
		write(customer_log, errmsg, len);
		exit(-1);
	}
	exit(0);
}

void util(int a, int b){
	close(a), close(b);
}

int main(int argc, char const *argv[]) {
	proc = 0;
	customer_log = open("customer_log", O_WRONLY | O_CREAT | O_TRUNC |O_APPEND, 0777);
	if (customer_log < 0) {
		fprintf(stderr, "Error opening customer_log\n");
		exit(-1);
	}
	int test_data = open(argv[1], O_RDONLY), malf = 1;
	if (test_data < 0) {
		fprintf(stderr, "Error opening test_data\n");
		exit(-1);
	}
	pid_t parentpid = getppid();
	struct sigaction sigact;
	struct sigaction sigalrm;
	sigact.sa_handler = &signal_handler;
	sigalrm.sa_handler = &sig_alarm_handler;
	sigact.sa_flags = SA_RESTART;
	sigemptyset(&sigact.sa_mask);
	sigaddset(&sigact.sa_mask, SIGALRM);
	sigemptyset(&sigalrm.sa_mask);
	sigaddset(&sigalrm.sa_mask, SIGUSR1);
	sigaddset(&sigalrm.sa_mask, SIGUSR2);
	sigaddset(&sigalrm.sa_mask, SIGINT);
	
	
	if (sigaction(SIGUSR1, &sigact, NULL) < 0) {
		fprintf(stderr, "Could not handle SIGUSR1\n");
		exit(-1);
	}
	if (sigaction(SIGUSR2, &sigact, NULL) < 0) {
		fprintf(stderr, "Could not handle SIGUSR2\n");
		exit(-1);
	}
	if (sigaction(SIGINT, &sigact, NULL) < 0) {
		fprintf(stderr, "Could not handle SIGINT\n");
		exit(-1);
	}
	if (sigaction(SIGALRM, &sigalrm, NULL) < 0) {
		fprintf(stderr, "Could not handle SIGALARM\n");
		exit(-1);
	}
	int i = 0;
	while(i < 3){
		prioritas[i] = 1;
		i++;
	}
	int frg = 0, frg_nano = 0;
	int upc, upc_nano;
	
	while(malf > 0) {
		int code, flag = 0, sze = 0;
		char buf, buf_num[3], buf_int[20], buf_dec[3];
		nuevo++;
		memset(buf_num, '\0', sizeof(buf_num));
		memset(buf_int, '\0', sizeof(buf_int));
		memset(buf_dec, '\0', sizeof(buf_dec));
		malf = read(test_data, buf_num, 2);
		if (malf <= 0)
			break;
		code = atoi(buf_num);
		sze = 0;
		while((malf = read(test_data, &buf, 1)) > 0) {
			if (buf == '\n' || buf == '.')
				break;
			buf_int[sze++] = buf;
		}
		if (malf < 0)
			break;	
		upc = atoi(buf_int);
		upc_nano = 0;
		if (buf == '.') {
			malf = read(test_data, buf_dec, 2);
			if (malf < 0) 
				break;
			upc_nano = buf_dec[0] - '0';
		}
		struct timespec tim;
		struct timespec remtim;
		if (upc != frg) {
			if (upc_nano < frg_nano) {
				tim.tv_sec = upc - frg - 1;
				tim.tv_nsec = (upc_nano + 10 - frg_nano) * BIGNUM;
			}
			else {
				tim.tv_sec = upc - frg;
				tim.tv_nsec = (upc_nano - frg_nano) * BIGNUM;
			}
		}
		else {
			nuevo++;
			tim.tv_sec = 0;
			tim.tv_nsec = (upc_nano - frg_nano) * BIGNUM;
		}

		while(1) {
			if (clock_nanosleep(CLOCK_REALTIME, 0, &tim, &remtim) == 0)
				break;
			else tim = remtim;
		}
		frg = upc;
		frg_nano = upc_nano;
		char msg[100];
		if (code == 0) {
			flag = 0;	
			sprintf(msg, "ordinary\n");
			int len = strlen(msg);
			if (write(STDOUT_FILENO, msg, len) < 0) {
				fprintf(stderr, "Error writing to stdout\n");
				exit(-1);
			}
			fsync(STDOUT_FILENO);
		}
		else if(code == 1){
			flag = 1;
			kill(parentpid, SIGUSR1);
			struct itimerval waktu;
			waktu.it_interval.tv_sec = 0;
			waktu.it_interval.tv_usec = 0;
			waktu.it_value.tv_sec = 1;
			waktu.it_value.tv_usec = 0;
			if (setitimer(ITIMER_REAL, &waktu, NULL) < 0) {
					perror("");
					exit(-1);
				}
				
		}
		else if(code == 2){
			kill(parentpid, SIGUSR2);
			flag = 2;	
		}
				
		else {
			flag = INT_MAX;
			fprintf(stderr, "Read data is: %d\n", code);
			exit(-1);
		}
		proc++;
		memset(msg, '\0', sizeof(msg));
		sprintf(msg, "send %d %d\n", code, prioritas[code]);
		if (write(customer_log, msg, strlen(msg)) < 0) {
			fprintf(stderr, "Error writing to customer_log\n");
			exit(-1);
		}
	}
	while (proc > 0) {
		;
	}
	if (malf < 0) {
		perror("");
		exit(EXIT_FAILURE);
	}
	util(customer_log, test_data);
	exit(0);
}





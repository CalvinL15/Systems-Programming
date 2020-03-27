#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

void err_sys(const char *x) {
	perror(x); 
	exit(1);
}

char read_FIFO[50] = {}, write_FIFO[50] = {};

void create_FIFO_names(char player_index, const char* hostname) {
	int host_int = atoi(hostname);
	strcpy(read_FIFO, "host");
	strcpy(write_FIFO, "host");
	strcpy(&read_FIFO[4], hostname);
	strcpy(&write_FIFO[4], hostname);
	char temp[10] = "_X.FIFO";
	temp[1] = player_index;
	if (0 < host_int && host_int <= 9) {
		strcpy(&read_FIFO[5], temp);
		strcpy(&write_FIFO[5], ".FIFO");
	}
	else {
		strcpy(&read_FIFO[6], temp);
		strcpy(&write_FIFO[6], ".FIFO");
	}
}

int main(int argc, char const *argv[]) {
	int i;
	if (argc != 4) err_sys("usage: host_id[] player_index{A,B,C,D} randomkey[0, 65535]");
	char player_index = *argv[2]; 
	int index = player_index - 'A';
	create_FIFO_names(player_index, argv[1]);
	FILE* read_FP = fopen(read_FIFO, "r");
	FILE* write_FP = fopen(write_FIFO, "w");
	int A[4];
	for (i = 0; i < 10; ++i) {
		fscanf(read_FP, "%d%d%d%d", &A[0], &A[1], &A[2], &A[3]);
		int to_give = (index == i % 4) ? A[index] : 0;
		fprintf(write_FP, "%c %d %d\n", player_index, atoi(argv[3]), to_give);
		fflush(write_FP);
	}
	fclose(read_FP); 
	fclose(write_FP);
	exit(0);
	return 0;
}

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <stdlib.h>
#include <string.h>
#define READ_SOCKET  0
#define WRITE_SOCKET 1

int player_num[20] = {}, player_score[20] = {}, hosts[12] = {}, allr[4845][4] = {}, totalr = 0;

typedef const void cv;
typedef const char cc;

void reverse(char *s, int n) {
	int left = 0, right = n - 1;
	while (left < right) {
		char c = s[left];
		s[left] = s[right];
		s[right] = c;
		left++, right--;
	}
}

void itoa(int n, char s[]){
	int i;
	int sign;
	if ((sign = n) < 0) 
		n = (n*(-1));          
	i = 0;
	do {
		s[i++] = n % 10 + '0';   
	} while ((n /= 10) > 0);
	if (sign < 0) s[i++] = '-';
	s[i] = '\0';
	reverse(s, i);
}

void init() {
	int i;
	int j;
	for (i = 0; i < 20; i++) 
		player_num[i] = i;
	for (j = 0; j < 12; j++) 
		hosts[j] = j;
}

int pipe_1[12][2] = {}, pipe_2[12][2] = {}; 

void dhs(int data[]) {
	int i;
	for (i = 0; i < 4; ++i) 
		allr[totalr][i] = data[i]+1;
	totalr++;
	}

void actual(int A[], int data[], int start, int end, int index) {
	int i;
	if (index == 4) 
		{	dhs(data); 
			return;
		}
	for (i = start; i <= end && end-i+1 >= 4-index; ++i) {
		data[index] = A[i];
		actual(A, data, i+1, end, index+1);
	}
}

void do_combinations(int A[], int n) {
	int data[4];
	actual(A, data, 0, n - 1, 0);
}

void err_sys(cc *x) {
	perror(x); 
	exit(1);
}

void printr() {
	int i;
	int j;
	for (i = 0; i < totalr; ++i) {
		for (j = 0; j < 4; ++j) {
			printf("%d%c", allr[i][j], " \n"[j == 3]);
		}
	}
}

typedef struct {
	int fd;
	FILE *r_fp, *w_fp;
} host;

int get_max_host_fd(host host_fd[], int host_num) {
	int ret = -1;
	for (int i = 0; i < host_num; ++i) {
		ret = (host_fd[i].fd > ret) ? host_fd[i].fd : ret;
	} 
	return ret;
}

void update_stats(int n, int player_id[], int player_rank[]) {
	for (int i = 0; i < n; ++i) 
		player_score[player_id[i]-1] += 4 - player_rank[i];
}

void fork_hosts(int host_num, int player_count) {
	host host_fd[20];
	for (int i = 0; i < host_num; ++i) {
		if (pipe(pipe_1[i]) < 0 || pipe(pipe_2[i]) < 0) 
			err_sys("pipe failed\n");
		if ((host_fd[i].fd = fork()) < 0) 
			err_sys("fork failed\n");
	
		if (host_fd[i].fd == 0) {
			//close pipes that is not needed
			close(pipe_1[i][READ_SOCKET]);
			close(pipe_2[i][WRITE_SOCKET]);
			close(STDIN_FILENO);
			dup2(pipe_2[i][READ_SOCKET], STDIN_FILENO);
			close(STDOUT_FILENO);
			dup2(pipe_1[i][WRITE_SOCKET], STDOUT_FILENO);

			char host_name[5] = {};
			itoa(i+1, host_name);
			
			execl("./host", "./host", host_name, (char *)0); //go run host.c instead
			exit(0);
		}
		else { 
			close(pipe_1[i][WRITE_SOCKET]);
			close(pipe_2[i][READ_SOCKET]);
		}
	}
	for (int j = 0; j < host_num; j++) {
		host_fd[j].r_fp = fdopen(pipe_1[j][READ_SOCKET], "r");
		if (!host_fd[j].r_fp) 
			err_sys("cannot open read FP");
		host_fd[j].w_fp = fdopen(pipe_2[j][WRITE_SOCKET], "w");
		if (!host_fd[j].r_fp) 
			err_sys("cannot open write FP");
	}
	//do multiplexing
	int write_cnt = 0, max_fd = -1;
	fd_set master_set; 
	FD_ZERO(&master_set);
	
	for (int k = 0; k < host_num; k++) {
		if (max_fd < pipe_1[k][READ_SOCKET]) 
			max_fd = pipe_1[k][READ_SOCKET];
	}
	for (int k = 0; k < host_num; k++) {
		FD_SET(pipe_1[k][READ_SOCKET], &master_set);
	}

    struct timeval timeout;
    timeout.tv_sec = 0, timeout.tv_usec = 10;

    for (int k = 0; k < host_num && k < player_count && k < totalr; ++k) {
		fprintf(host_fd[k].w_fp, "%d %d %d %d\n", 
			allr[write_cnt][0], allr[write_cnt][1], 
			allr[write_cnt][2], allr[write_cnt][3]);
		fflush(host_fd[k].w_fp);
    	write_cnt++;
    }
	long long read_count = 0;
	while(read_count < totalr) {
		fd_set read_set = master_set;
		int fd_ready = select(max_fd+1, &read_set, NULL, NULL, &timeout);
		for (k= 0; k < host_num && fd_ready > 0 && read_count < totalr; ++k) {
			int current_read_fd = pipe_1[k][READ_SOCKET];
			FILE* current_host_read_fp  = host_fd[k].r_fp;
			FILE* current_host_write_fp = host_fd[k].w_fp;
			if (FD_ISSET(current_read_fd, &read_set)) {
				fd_ready--;
				int player_id[20] = {}, player_rank[20] = {};
				for (int i = 0; i < 4; ++i) {
					int arg = fscanf(current_host_read_fp, "%d%d", &player_id[i], &player_rank[i]);
					if (!(1 <= player_id[i] && player_id[i] <= player_count)) {
						fprintf(stderr, "bad id: %d\n", player_id[i]);
						fprintf(stderr, "ERROR at read count = %d, write count = %d", read_count, write_count);
						exit(1);
					}
					if (!(1 <= player_rank[i] && player_rank[i] <= 4)) {
						fprintf(stderr, "bad rank: %d\n", player_rank[i]);
						exit(1);
					}
					if (arg != 2) 
						err_sys("could not get the correct result");
				}
				read_count++;
				update_stats(player_count, player_id, player_rank);
				if (write_count < totalr) { //if there are still more rounds
					fprintf(current_host_write_fp, "%d %d %d %d\n", 
						allr[write_count][0], allr[write_count][1], 
						allr[write_count][2], allr[write_count][3]);
					fflush(current_host_write_fp);	
					write_count++;
				}
			}
		}
	}
	for (int i = 0; i < host_num; ++i) {
		fprintf(host_fd[i].w_fp, "-1 -1 -1 -1\n");
		fflush(host_fd[i].w_fp);
		waitpid(host_fd[i].fd, NULL, 0);
	}
}

typedef unsigned long long int xyz; 
void insertion_sort(int n, int keys[], int sat[], int ascending) {
	int count = 0;
	for (int i = 1; i < n; ++i) {
		count++;
		int j = i-1, key = keys[i], meow = sat[i];
		if (ascending) {
			while (j >= 0 && key > keys[j]) {
				keys[j+1] = keys[j];
				sat[j+1] = sat[j];
				j--;
			}			
		}
		else {
			while (j >= 0 && keys[j] > key) {
				keys[j+1] = keys[j];
				sat[j+1] = sat[j];
				j--;
			}
		}
		keys[j+1] = key;
		sat[j+1] = meow;
	}	
}

int main(int argc, char const *argv[]) {
	init();
	if (argc != 3) err_sys("usage: a.out host_num[1,12] player_num[4,20]");
	int host_num = atoi(argv[1]), player_count = atoi(argv[2]);
	do_combinations(player_num, player_count); 
	fork_hosts(host_num, player_count);
	int rank[20] = {1};
	insertion_sort(player_count, player_score, player_num, 1);
	int total_rank;
	for (int i = 1, total_rank = 1; i < player_count; ++i, total_rank++) {
		if (player_score[i] == player_score[i-1]){
			rank[i] = rank[i-1];	
		}
		else rank[i] = total_rank+1;
	}
	insertion_sort(player_count, player_num, rank, 0);
	for (int j = 0; j < player_count; ++j) 
		printf("%d %d\n", player_num[j]+1, rank[j]);
	return 0;
}


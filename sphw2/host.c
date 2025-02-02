#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

char write_FIFOs[4][100], read_FIFO[100];
typedef const void cv;
typedef const char cc;

void reverse(char *s, int n) {
	int left = 0, right = n - 1;
	while (left < right) {
		char c = s[left];
		s[left] = s[right], s[right] = c;
		left++, right--;
	}
}

void itoa(int n, char s[]) {
	int i;
	int sign;
	if ((sign = n) < 0) 
		n = -n;          
	i = 0;
	do {
		s[i++] = n % 10 + '0';   
	} while ((n /= 10) > 0);
	if (sign < 0) s[i++] = '-';
	s[i] = '\0';
	reverse(s, i);
}

void err_sys(cc *x) {
	perror(x); 
	exit(1);
}

void get_FIFO_names(cc *hostname);

typedef struct  {
	char id; int random_key, money;
} Return;

typedef struct {
	int money;
	int last_paid;
	int won_rounds;
	char id, won_last_round, rank, index;
} Player;

void getrand(int *A), init_players(Player P[]), update_money(Player P[]), reupd(Player P[], Return R[]), get_rank(Player P[]);

int main(int argc, char const *argv[]) {
	int i, j, q;
	srand(getpid());
	if (argc != 2) 
		err_sys("usage: host_id[1, 12]");
	get_FIFO_names(argv[1]);
	if ((q = mkfifo(read_FIFO, 0777)) != 0) err_sys("read FIFO exists");
	for (i = 0; i < 4; i++)
		if ((q = mkfifo(write_FIFOs[i], 0777)) != 0)
			err_sys("write_FIFO exists");
	FILE* read_FP;
	FILE* write_FPs[4];

	int a, b, c, d, stuff[50][4], n = 0;
	fd_set master_set; FD_ZERO(&master_set); FD_SET(STDIN_FILENO, &master_set);

    struct timeval timeout;
    timeout.tv_sec = 1, timeout.tv_usec = 500;
	int read_fd;
	//program will sometimes TLE if O_NONBLOCK is off
	if ( (read_fd = open(read_FIFO, O_RDWR | O_NONBLOCK)) < 0)
		err_sys("can't open read FIFO");
	int write_fds[4];
	for (i = 0; i < 4; ++i) {
		if ((write_fds[i] = open(write_FIFOs[i], O_RDWR | O_NONBLOCK)) < 0)
			err_sys("can't open write FIFO");
		write_FPs[i] = fdopen(write_fds[i], "w");
		if (!write_FPs[i]) 
			err_sys("can't open write FIFO stream");
	}
	
	while (scanf("%d%d%d%d", &a, &b, &c, &d) == 4) {
		if (a == -1 && b == -1 && c == -1 && d == -1) 
			break;
		Player P[4];
		init_players(P);
		P[0].id = a, P[1].id = b, P[2].id = c, P[3].id = d;
		P[0].index = 'A', P[1].index = 'B', P[2].index = 'C', P[3].index = 'D';
		int jesusweep[4];
		getrand(jesusweep);
		char A[4][20];
		for (j = 0; j < 4; j++) itoa(jesusweep[j], A[j]);
		//fork 4 players
		int pid[4];
		for (i = 0; i < 4; ++i) {
			if ((pid[i] = fork()) == 0) {
				char player_name[2] = {};
				*player_name = 'A' + i;
				execl("./player", "./player", argv[1], player_name, A[i], (char*)0);
				exit(0);
			}
			if (pid[i] < 0) err_sys("blarg error");
		}

		//for ten rounds
		Return R[4];
		int round;
		for (round = 0; round < 10; ++round) { //for each round
			update_money(P);
			int p, a;
			for (p = 0; p < 4; ++p) { //for each player
				fprintf(write_FPs[p], "%d %d %d %d\n", P[0].money, P[1].money, P[2].money, P[3].money);
				fflush(write_FPs[p]);
				fd_set master_set; FD_ZERO(&master_set); FD_SET(read_fd, &master_set); //FD_SET(0, &master_set);
				while (1) {
					fd_set read_set = master_set;
					select(read_fd+1, &read_set, NULL, NULL, &timeout);
					if (FD_ISSET(read_fd, &read_set)) {
						char buf[35] = {};
						a = read(read_fd, buf, 35); 
						if (a <= 0) err_sys("bad read");
						sscanf(buf, "%c %d %d", &R[p].id, &R[p].random_key, &R[p].money);
						break;
					}
				} 
				
			}
			reupd(P, R);
		}
		get_rank(P);
		for (i = 0; i < 4; ++i) 
			printf("%d %d\n", P[i].id, P[i].rank);
		fflush(stdout); 
		for (i = 0; i < 4; ++i) 
			waitpid(pid[i], NULL, 0);

	}
        for (i = 0; i < 4; i++) fclose(write_FPs[i]), close(write_fds[i]);

    int ret;
	if (ret = (remove(read_FIFO)) == -1) 
		err_sys("FIFO not removed");
	for (i = 0; i < 4; i++) if ((ret = remove(write_FIFOs[i]) == -1)) err_sys("write FIFO not removed");
	return 0;
}

int cmp_rounds(cv *a, cv *b) {
	Player *A = (Player *)a;
	Player *B = (Player *)b;
	if (A->won_rounds == B->won_rounds) 
		return A->id > B->id;
	return A->won_rounds < B->won_rounds;
}

int cmp_number(cv *a, cv *b) {
	Player *A = (Player *)a; 
	Player *B = (Player *)b;
	int res = A->id > B->id;
	return res;
}


void get_rank(Player P[]) {
	int total_rank;
	qsort(P, 4, sizeof(Player), cmp_rounds);

	P[0].rank = 1;
	for (int i = 1, total_rank = 1; i < 4; ++i, total_rank++) {
		if (P[i].won_rounds != P[i-1].won_rounds) 
			P[i].rank = total_rank+1;
		else 
			P[i].rank = P[i-1].rank;
	}

	qsort(P, 4, sizeof(Player), cmp_number);
}

void get_FIFO_names(cc *hostname) {
	int pos = (atoi(hostname) > 9) ? 6 : 5;
	strcpy(read_FIFO, "host"), strcpy(&read_FIFO[4], hostname), strcpy(&read_FIFO[pos], ".FIFO");

	for (int i = 0; i < 4; ++i) {
		strcpy(write_FIFOs[i], "host"), strcpy(&write_FIFOs[i][4], hostname), strcpy(&write_FIFOs[i][pos], "_X.FIFO");
		if (pos != 5) 
			write_FIFOs[i][7] = i + 'A';
		else write_FIFOs[i][6] = i + 'A';
	}
}

void getrand(int *A) {
	int is_unique;
	while (1) {
		for (int i = 0; i < 4; i++) 
			A[i] = rand() % 65536;
		is_unique = 1;
		for (int i = 0; i < 4; i++) {
			for (int j = i+1; j < 4; ++j) {
				if (A[i] == A[j]) 
					is_unique = 0;
			}
		}
		if (is_unique) break;
	}
}

void init_players(Player P[]) {
	for (int i = 0; i < 4; ++i) {
		P[i].money = P[i].won_last_round = P[i].last_paid = P[i].won_rounds = 0;
	}
}

void update_money(Player P[]) {
	int a;
	for (int i = 0; i < 4; ++i) {
		a = P[i].won_last_round ? 1 : 0;
		P[i].money = 1000 + P[i].money - (a * P[i].last_paid);
	}
}

int cmp(cv *a, cv *b) {
	Return *A = (Return *)a;
	Return *B = (Return *)b;
	int res = A->money < B->money;
	return res;
	}

int getWinner(Return R[]) {
	qsort(R, 4, sizeof(Return), cmp);
	int idx = -2;
	if (R[0].money == R[1].money && R[2].money == R[3].money) idx = -1;
	else if (R[0].money != R[1].money) idx = 0;
	else {
		if (R[1].money == R[2].money){
			if (R[2].money != R[3].money) idx = 3;
				else idx = -1;		
		} 
		else idx = 2;
	}
	if (idx == -2) 
		err_sys("edge getWinner case");	
	return idx;
}

void reupd(Player P[], Return R[]) {
	for (int i = 0; i < 4; ++i) {
		P[i].last_paid = R[i].money;
	}
	int index = getWinner(R);
	for (int i = 0; i < 4; i++) 
		P[i].won_last_round = 0;
	if (index != -1) {
		int j = 0;
		while (R[index].id != P[j].index && j < 4) 
			j++;
		if (j > 3) err_sys("bad player");
		P[j].won_last_round = 1;
		P[j].won_rounds++;
	}
}

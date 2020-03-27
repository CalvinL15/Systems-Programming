#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/select.h>
#include <time.h>

#define ENTRY_COUNT 128
#define ISLEAF -2
#define NOTLEAF -1
#define ISZERO 0
#define ISONE 1

typedef struct data {
	int id, label;
	float features[33];
} Data;

typedef volatile int vint; 

typedef struct node {
	float threshold;
	int dimension;
	short value; //0 or 1
	struct node *left;
	struct node *right;
} Node;

Node* create_node(float threshold, int dimension, short value) {
	int sz = sizeof(Node);
	Node *n = (Node *)malloc(sz);
	n->left = n->right = NULL;
	n->threshold = threshold;
	n->dimension = dimension;
	n->value = value;
	return n;
}

void print_trees();

int ones_count(Data *entries[], int left, int right) {
	int one_count = 0;
	for (int i = left; i <= right; ++i) {
		if (entries[i]->label == 1) 
			one_count++;
	}
	int equ = right-left+1;
	if (one_count == equ) 
		return -1;
	return one_count;
}

void err_sys(char *s) {perror(s); exit(1);}

float getp(Data *entries[], int idx, int start, int end){
	float left_1 = 0, left_0 = 0; 
	float right_1 = 0, right_0 = 0;
	for (int i = start; i <= idx; ++i) {
		if(entries[i]->label == 1) 
			left_1++;
		if(entries[i]->label == 0) 
			left_0++;
	}
	left_1 /= (idx - start + 1); 
	left_0 /= (idx - start + 1);
	for (int i = idx+1; i <= end; ++i) {
		if(entries[i]->label == 1) right_1++;
		if(entries[i]->label == 0) right_0++;
	}
	right_1 /= (end - idx); 
	right_0 /= (end - idx);

	float GINI = left_1 * (1-left_1) + left_0 * (1-left_0) + right_1 * (1-right_1) + right_0 * (1-right_0);
	return GINI;
}

void swap_entries(Data **a, Data **b) {
	Data *t = *a; 
	*a = *b; 
	*b = t;
}

int partition(Data *entries[], int left, int right, int dimension) {
	int pivot = right, divisor = left;
	for (int i = left; i <= right; i++) {
		if (entries[i]->features[dimension] < entries[pivot]->features[dimension]) {
			swap_entries(&entries[i], &entries[divisor++]);
		}
	}
	swap_entries(&entries[pivot], &entries[divisor]);
	return divisor;
}

void sort_quick(Data *entries[], int left, int right, int dimension) {
	int p;
	if (left < right) {
		p = partition(entries, left, right, dimension);
		sort_quick(entries, left, p-1, dimension);
		sort_quick(entries, p+1, right, dimension);
	}
}

void traverse(Node *root) {
	if (root) {
		traverse(root->left);
		if (root->dimension == ISLEAF) printf("leaf: value = %d\n", root->value);
		else printf("node: dimension = %d, threshold = %f\n", root->dimension, root->threshold);
		traverse(root->right);

	}
}

vint treeCount, threadCount, jobCount, test_cnt;

Data *train; 
int train_n;
Data *test; 
Node *Trees[40000];

void init();
void read_train();

//for training
pthread_mutex_t qqzainar = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t initialize = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t initialize2 = PTHREAD_COND_INITIALIZER;
//for testing
pthread_mutex_t kepercayaan = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t kesabaran = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t persetan = PTHREAD_COND_INITIALIZER;

typedef unsigned int ui;

ui _tid() 
{return (ui)pthread_self() % 1000;}
//divide using boundaries

Node* create_root(Data *entries[], int left, int right) { 
	int zeroes = ones_count(entries, left, right);
	if (zeroes <= 0) { //base case
		Node *leaf = create_node(ISLEAF, ISLEAF, -zeroes); 
		return leaf;
	}
	int ones = (right - left + 1) - zeroes;
	float bestp = 1000.02; 
	int best_cut = -1;, best_dim = -1;
	for (int i = 0; i < 33; ++i) { //for each dimension
		sort_quick(entries, left, right, i); //sort by dimension
		float bestlp = 532.0; 
		int bestlc = -1; 
		for (int j = left; j < right; ++j) { //for each cut position; exclude rightmost intentionally
			float lp = getp(entries, j, left, right);
			if (lp < bestlp){
				bestlp = lp; 
				bestlc = j;
			}
		}
		if (bestlp < bestp) {
			bestp = bestlp, best_cut = bestlc;
			best_dim = i;
		}
	}
	assert(best_cut != -1 && best_dim != -1);
	sort_quick(entries, left, right, best_dim);
	float chill = (entries[best_cut]->features[best_dim] + entries[best_cut+1]->features[best_dim]);
	float threshold = chill / 2.0;
	Node *new = create_node(threshold, best_dim, NOTLEAF);
	new->left = create_root(entries, left, best_cut);
	new->right = create_root(entries, best_cut+1, right);
	return new;
}

volatile int dead_threads = 0;
void* rand_tree_thread(void *arg) {
	while(1) {
		pthread_mutex_lock(&qqzainar);
		if (jobCount == treeCount) {
			pthread_mutex_lock(&initialize);
			dead_threads++;
			pthread_cond_signal(&initialize2);
			pthread_mutex_unlock(&qqzainar);
			pthread_mutex_unlock(&initialize);
			pthread_exit((void*)0); //if no more jobs, just die
		}
		int pos = jobCount; //save this pos
		jobCount++; //we've taken this job
		pthread_mutex_unlock(&qqzainar);
		Data *entries[ENTRY_COUNT] = {}; 
		for (int i = 0; i < ENTRY_COUNT; i++) 
			entries[i] = &train[rand() % train_n]; 
		Node *root = create_root(entries, 0, ENTRY_COUNT-1);
		Trees[pos] = root;
		assert(root != NULL);
	}
	return (void *)0;
}

void create_random_forest() {
	pthread_t tid[threadCount];
	//create N threads and each thread will create trees
	for (int i = 0; i < threadCount; ++i) {
		pthread_create(&tid[i], NULL, rand_tree_thread, (void *)0);
	}
	pthread_mutex_lock(&initialize);
	while(dead_threads != threadCount) 
		pthread_cond_wait(&initialize2, &initialize);
    
	pthread_mutex_unlock(&initialize);
	for (int i = 0; i < threadCount; ++i) 
		pthread_join(tid[i], NULL);
}

//testing step
volatile int q_dead_threads, q_jobCount;
int person_prediction[30000];

int evaluate_tree(Node* root, Data* entry) {
	if (root->dimension == ISLEAF) 
		return root->value; 
	if (root->threshold > entry->features[root->dimension]) 
		return evaluate_tree(root->left, entry);
	return evaluate_tree(root->right, entry);
}

int evaluate_all_trees(Data *entry) { 
	int score[2] = {};
	for (int i = 0; i < treeCount; ++i) //for each tree
		score[evaluate_tree(Trees[i], entry)]++;
	return score[0] > score[1] ? 0 : 1;
}

void* run_trees(void *arg) {
	while (1) {
		pthread_mutex_lock(&kepercayaan);
		if (q_jobCount == test_cnt) {
			pthread_mutex_lock(&kesabaran);
			q_dead_threads++;
			pthread_cond_signal(&persetan);
			pthread_mutex_unlock(&kepercayaan);
			pthread_mutex_unlock(&kesabaran);
			pthread_exit((void*)0); 
		}
		int pos = q_jobCount; //save this pos
		q_jobCount++; //we've taken this job
		pthread_mutex_unlock(&kepercayaan);
		//do the job
		Data *query = &test[pos];
		person_prediction[query->id] = evaluate_all_trees(query);
	}
	return (void *)0;

}

void parallelize_query() {
	pthread_t tid2[threadCount];
	for (int i = 0; i < threadCount; ++i) {
		pthread_create(&tid2[i], NULL, run_trees, (void *)0);
	}
	pthread_mutex_lock(&kesabaran);
	while(q_dead_threads != threadCount) {
		pthread_cond_wait(&persetan, &kesabaran);
	}
	pthread_mutex_unlock(&kesabaran);
	for (i = 0; i < threadCount; ++i) {
		pthread_join(tid2[i], NULL);		
	}
   // fprintf(stderr, "all parallelize threads joined\n");
}

int predictfunc(FILE *test_fp) {
	assert(test_fp); 
	int max = -1;
	test = (Data *)malloc(30000 * sizeof(Data));
	while(~fscanf(test_fp, "%d", &test[test_cnt].id)) {
		for (int i = 0; i < 33; i++) 
			fscanf(test_fp, "%f", &test[test_cnt].features[i]);
		if(test[test_cnt].id > max)	
			max = test[test_cnt].id;
		test_cnt++;
	}
	for (int i = 0; i < max; i++) 
		person_prediction[i] = -1;
	parallelize_query();
	return max;
}

void print_results();

int main(int argc, char *argv[]) {
	FILE *train_fp, *test_fp;
	init(argc, argv, &train_fp, &test_fp);
	read_train(train_fp);
	create_random_forest();
	free(train); 
	fclose(train_fp);
	print_results(argv[4], predictfunc(test_fp));
	return 0;
}

void init(int argc, char *argv[], FILE **train_fp, FILE **test_fp) {
	if (argc != 9) {
		puts("usage: ./hw4 -data [data_dir] -output submission.csv -tree [tree_number] -thread [thread_number]");
		exit(0);
	}
	srand(time(NULL));
	train = (Data *)malloc(30000*sizeof(Data));
	if (!train) err_sys("not enough memory for testing_data");
	treeCount = atoi(argv[6]);
	threadCount = atoi(argv[8]);
	char train_name[50], test_name[50]; 
	sprintf(train_name, "%s/training_data", argv[2]);
	sprintf(test_name, "%s/testing_data", argv[2]);
	FILE *train_file = fopen(train_name, "r");
	if (!train_file) err_sys("no such training data file");
	*train_fp = train_file;
	FILE *test_file = fopen(test_name, "r");
	if (!test_file) err_sys("no such testing data file");
	*test_fp = test_file;
}

void read_train(FILE *train_fp) {
	while(~fscanf(train_fp, "%d", &train[train_n].id)) { 
		for (int i = 0; i < 33; i++) 
			fscanf(train_fp, "%f", &train[train_n].features[i]);
		fscanf(train_fp, "%d", &train[train_n++].label);
	}
}

void print_trees() {
	for (int i = 0; i < treeCount; ++i) {
		puts("---tree start ---");
		traverse(Trees[i]);
		puts("---tree end   ---");
	}
}

void print_results(char ofile[], int max_id) {
	FILE *output = fopen(ofile, "w");
	assert(output != NULL);
	fprintf(output, "id,label\n");
	for (int i = 0; i < max_id; ++i) {
		if (person_prediction[i] != -1) fprintf(output, "%d,%d\n", i, person_prediction[i]);
	}
}


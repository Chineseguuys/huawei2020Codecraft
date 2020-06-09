#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define NUM_THREADS  2

//thread argument struct
typedef struct _thread_data {
	int tid;
	double studff;
} thread_data;

//thread function
void *thr_func(void *arg) {
	thread_data *data = (thread_data *)arg;

	printf("from thr_func, thread id:%d\n",data->tid);

	pthread_exit(NULL);
}

int main() {
	pthread_t thr[NUM_THREADS];
	int i,rc;

	//thread_data argument array
	thread_data thr_data[NUM_THREADS];

	//create threads
	for(i=0; i<NUM_THREADS; ++i) {
		thr_data[i].tid = i;
		if((rc = pthread_create(&thr[i],NULL,thr_func,&thr_data[i]))) {
			fprintf(stderr,"error:pthread_create,rc: %d\n",rc);
			return EXIT_FAILURE;
		}
	}

	//block untill all threads complete
	
	for(i=0; i<NUM_THREADS; ++i) {
		pthread_join(thr[i],NULL);
	}
	
	return 0;
}
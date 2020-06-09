#ifndef T_POOL
#define T_POOL

#include <pthread.h>
#include <ctype.h>
#include <iostream>

typedef struct tpool_work{
   void* (*work_routine)(void*); //function to be called
   void* args;                   //arguments 
   struct tpool_work* next;
}__attribute__((__aligned__(64))) tpool_work_t;


typedef struct tpool{
   size_t               shutdown;       //is tpool shutdown or not, 1 ---> yes; 0 ---> no
   size_t               maxnum_thread; // maximum of threads
   pthread_t            *thread_id;     // a array of threads
   long long int		thread_free;  // 当前的线程是否是空闲的
   tpool_work_t*        tpool_head;     // tpool_work queue
   pthread_cond_t       queue_ready;    // condition varaible
   pthread_mutex_t      queue_lock;     // queue lock
   //struct __attribute__((__aligned__)){ } aligned;
}__attribute__((__aligned__)) tpool_t;




// 用来指示某一个线程当前是否是空闲的状态
typedef struct thread_argument
{
	tpool_t* 	pool;
	int 		flags_index;
	//struct __attribute__((__aligned__(32))) { } aligned;	
}__attribute__((__aligned__(64))) thread_argument; 




/***************************************************
*@brief:
*       create thread pool
*@args:   
*       max_thread_num ---> maximum of threads
*       pool           ---> address of thread pool
*@return value: 
*       0       ---> create thread pool successfully
*       othres  ---> create thread pool failed
***************************************************/

int create_tpool(tpool_t** pool, thread_argument* args,size_t max_thread_num);

/***************************************************
*@brief:
*       destroy thread pool
*@args:
*        pool  --->  address of pool
***************************************************/
void destroy_tpool(tpool_t* pool);
 
/**************************************************
*@brief:
*       add tasks to thread pool
*@args:
*       pool     ---> thread pool
*       routine  ---> entry function of each thread
*       args     ---> arguments
*@return value:
*       0        ---> add ok
*       others   ---> add failed        
**************************************************/
int add_task_2_tpool(tpool_t* pool,void* (*routine)(void*),void* args);



#endif//tpool.h
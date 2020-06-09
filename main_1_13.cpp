#include <iostream>
#include <string>

#include <fstream>
#include <sstream>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <math.h>

//#include <algorithm>
#include <pthread.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

// thread_pool

typedef struct tpool_work{
   void* (*work_routine)(void*); //function to be called
   void* args;                   //arguments 
   struct tpool_work* next;
}__attribute__((__aligned__(64))) tpool_work_t;


typedef struct tpool{
   size_t               shutdown;       //is tpool shutdown or not, 1 ---> yes; 0 ---> no
   size_t               maxnum_thread; // maximum of threads
   pthread_t            *thread_id;     // a array of threads
   long long int        thread_free;  // 当前的线程是否是空闲的
   tpool_work_t*        tpool_head;     // tpool_work queue
   pthread_cond_t       queue_ready;    // condition varaible
   pthread_mutex_t      queue_lock;     // queue lock
   //struct __attribute__((__aligned__)){ } aligned;
}__attribute__((__aligned__)) tpool_t;




// 用来指示某一个线程当前是否是空闲的状态
typedef struct thread_argument
{
    tpool_t*    pool;
    int         flags_index;
    //struct __attribute__((__aligned__(32))) { } aligned;  
}__attribute__((__aligned__(64))) thread_argument; 


// thread_pool_functions
static void* work_routine(void* args)
{
    thread_argument* t_args = (thread_argument*)args;
    tpool_t* pool = t_args->pool;
    int flags_index = t_args->flags_index;
    tpool_work_t* work = NULL;

    while(1){
        pthread_mutex_lock(&pool->queue_lock);
        while(!pool->tpool_head && !pool->shutdown){ 
        // if there is no works and pool is not shutdown, 
        // it should be suspended for being awake
            //pool->thread_free[flags_index] = false;   // 当前的线程是空闲的
            pool->thread_free = pool->thread_free & (~(0x01LL << flags_index));
            // 把代表了当前的线程的位设置为 0
            pthread_cond_wait(&pool->queue_ready,&pool->queue_lock);
        }

        if(pool->shutdown){
            pthread_mutex_unlock(&pool->queue_lock);//pool shutdown,release the mutex and exit
            pthread_exit(NULL);
        }

        pool->thread_free = pool->thread_free | (0x01LL << flags_index);
        /* tweak a work*/
        work = pool->tpool_head;
        pool->tpool_head = (tpool_work_t*)pool->tpool_head->next;
        //pool->thread_free[flags_index] = true;  // 当前的线程正在忙碌
        // 把代表了当前线程的位设置为 1
        //std::cout << "w:" << pool->thread_free<< '\n';
        pthread_mutex_unlock(&pool->queue_lock);
        work->work_routine(work->args);     // 我们需要执行的函数

        free(work);
        //std::cout << "d1"<< '\n';
    }
return NULL;
}


int create_tpool(tpool_t** pool, thread_argument* args, size_t max_thread_num)
{
    (*pool) = (tpool_t*)malloc(sizeof(tpool_t));
    if(NULL == *pool){
        printf("in %s,malloc tpool_t failed!,errno = %d,explain:%s\n",__func__,errno,strerror(errno));
        exit(-1);
    }

    (*pool)->shutdown = 0;
    (*pool)->maxnum_thread = max_thread_num;
    (*pool)->thread_id = (pthread_t*)malloc(sizeof(pthread_t)*max_thread_num);
    (*pool)->thread_free = 0LL;

    if((*pool)->thread_id == NULL){
        printf("in %s,init thread id failed,errno = %d,explain:%s",__func__,errno,strerror(errno));
        exit(-1);
    }

    (*pool)->tpool_head = NULL;
    if(pthread_mutex_init(&((*pool)->queue_lock),NULL) != 0){
        printf("in %s,initial mutex failed,errno = %d,explain:%s",__func__,errno,strerror(errno));
        exit(-1);
    }
 
    if(pthread_cond_init(&((*pool)->queue_ready),NULL) != 0){
        printf("in %s,initial condition variable failed,errno = %d,explain:%s",__func__,errno,strerror(errno));
        exit(-1);
    }

    for(int i = 0; i < max_thread_num; i++){
        args[i].pool = *pool;
        args[i].flags_index = i;
        if(pthread_create(&((*pool)->thread_id[i]),NULL,work_routine,(void*)(&args[i])) != 0){
            printf("pthread_create failed!\n");
            exit(-1);
        }
    }
return 0;
}
 
void destroy_tpool(tpool_t* pool)
{
    tpool_work_t* tmp_work;
 
    if(pool->shutdown){
        return;
    }
    pool->shutdown = 1;
 
    pthread_mutex_lock(&pool->queue_lock);
    pthread_cond_broadcast(&pool->queue_ready);
    pthread_mutex_unlock(&pool->queue_lock);
 
    for(int i = 0; i < pool->maxnum_thread; i++){
        pthread_join(pool->thread_id[i],NULL);
    }
    free(pool->thread_id);
    while(pool->tpool_head){
        tmp_work = pool->tpool_head;
        pool->tpool_head = (tpool_work_t*)pool->tpool_head->next;
        free(tmp_work);
    }

    pthread_mutex_destroy(&pool->queue_lock);
    pthread_cond_destroy(&pool->queue_ready);
    //free(pool->thread_free);   // 后添加
    free(pool);
}



/**
 * 向线程池中添加一项任务
 */
int add_task_2_tpool(tpool_t* pool,void* (*routine)(void*),void* args)
{
    tpool_work_t* work,*member;
 
    if(!routine){
        printf("rontine is null!\n");
        return -1;
    }

    work = (tpool_work_t*)malloc(sizeof(tpool_work_t));
    if(!work){
        printf("in %s,malloc work error!,errno = %d,explain:%s\n",__func__,errno,strerror(errno));
        return -1;
    }

    work->work_routine = routine;
    work->args = args;
    work->next = NULL;
    pthread_mutex_lock(&pool->queue_lock);  // 进行数据的操作之前,一定要先获取锁
    member = pool->tpool_head;
    if(!member){
        pool->tpool_head = work;
    }
    else{
        while(member->next){
            member = (tpool_work_t*)member->next;
        }
        member->next = work;
    }

    //notify the pool that new task arrived!
    // 通知所有的线程,当前的队列中有新的任务需要进行处理
    pthread_cond_signal(&pool->queue_ready);
    pthread_mutex_unlock(&pool->queue_lock);
return 0;
}

/*****************
 * 上面的内容是多线程所需要的部分
 * 下面的部分尝试使用数组来代替 vector ,看看效率是否可以得到提高
******************/






// 全局变量
// 文件路径     
const char trainFile[] = "./data/train_data.txt";
const char testFile[] = "./data/test_data.txt";
const char predictFile[] = "./projects/student/result.txt";
const char answerFile[] = "./projects/student/answer.txt";
const char weightParamsFile[] = "modelweight.txt";

// 猜测可能的数据的纬度
static const int DIMEN_DATA = 1000;
static const int MAXS = 64*1024*1024;  // 最大可以存储 64M 的数据
static const int MAXL = 1000000;     // 猜测文件可能的最大行数  这里猜测 100万 行
static const int MAX_THREADS = 3;
static const int MAX_QUEUES = 64;
char train_buf[MAXS];           // 用于存储 训练数据的二进制文件
char test_buf[MAXS];            // 用于存储 测试数据的二进制文件
int Trainline_split[MAXL];   // 用于指示每一行的末尾的 偏移位置
int Testline_split[MAXL];

// 线程池
tpool_t* thread_pool;

// 全局的数据存储区域
struct features_labels
{
    double features[DIMEN_DATA];
    int label;
};

features_labels* TrainData_Set;
features_labels* TestData_Set;

struct store_args
{
    features_labels* store_box;
    int start_line;
    int end_line;
    int start_pos;
    int feature_nums;
}__attribute__((__aligned__(64)));


// thread 的 function 
void* store_train_data_block(void* __args)
{
    /**
        对比较大的文件,采用多线程分块进行读取
     */
    store_args* arg = (store_args*)(__args);
    features_labels* store_box = arg->store_box;
    int start_pos = arg->start_pos;
    double dataV;
    int row;
    int clo;
    for (row = arg->start_line; row < arg->end_line; ++row)
    {
        clo = 0;
        for (; clo < arg->feature_nums; ++clo)
        {
            if(train_buf[start_pos] == '-')
            {
                dataV = train_buf[start_pos+1] + 0.1*train_buf[start_pos + 3] + \
                    0.01 * train_buf[start_pos + 4] + 0.001*train_buf[start_pos + 5] - 53.328;
                dataV = -1*dataV;
                start_pos += 7;
            }
            else 
            {
                dataV = train_buf[start_pos] + 0.1*train_buf[start_pos + 2] + \
                    0.01*train_buf[start_pos + 3] + 0.001*train_buf[start_pos+4] - 53.328;
                start_pos += 6;
            }
            store_box[row].features[clo] = dataV;
        }
        store_box[row].label = (train_buf[start_pos] == '0' ? 0 : 1);
        start_pos += 2;
    }
    return NULL;
}

void* store_test_data_block(void* __args)
{
    /**
        对比较大的文件,采用多线程分块进行读取
     */
    store_args* arg = (store_args*)(__args);
    features_labels* store_box = arg->store_box;
    int start_pos = arg->start_pos;
    double dataV;
    int row;
    int clo;
    for (row = arg->start_line; row < arg->end_line; ++row)
    {
        clo = 0;
        for (; clo < arg->feature_nums; ++clo)
        {
            if(test_buf[start_pos] == '-')
            {
                dataV = test_buf[start_pos+1] + 0.1*test_buf[start_pos+3]+ \
                    0.01*test_buf[start_pos+4] + 0.001*test_buf[start_pos+5] - 53.328;
                dataV = -1*dataV;
                start_pos += 7;
            }
            else
            {
                dataV = test_buf[start_pos] + 0.1*test_buf[start_pos+2] + \
                    0.01*test_buf[start_pos+3] + 0.001*test_buf[start_pos +4] - 53.328;
                start_pos += 6;
            }
            store_box[row].features[clo] = dataV;
        }
    }
    return NULL;
}






int get_clomn_nums(const char* __buf)
{
    int count = 0;
    for(int i = 0; __buf[i] != '\n'; ++i)
    {
        if (__buf[i] == ',')
            ++count;
    }
    return count;
}

int get_line_nums(const char* __buf, int len, int* line_split)
{
    int count = 0;
    for (int i = 0; i < len; ++i)
    {
        if (__buf[i] == '\n')
        {
            line_split[count] = i;      // 第 count 行的行尾具体的位置, 下标从 0 开始
            ++count;
        }
    }
    return count;
}

void fread_anlysis(int& train_rowN, int& cloN, int& test_rowN, int& train_len, int& test_len)
{
    FILE* fp = fopen(trainFile, "rb");
    train_len = fread(train_buf, 1, MAXS, fp);
    train_rowN = get_line_nums(train_buf, train_len, Trainline_split);
    cloN = get_clomn_nums(train_buf);
    fclose(fp);
    fp = fopen(testFile, "rb");
    test_len = fread(test_buf, 1, MAXS, fp);
    test_rowN = get_line_nums(test_buf, test_len, Testline_split);
    fclose(fp);
}


#define _TEST_

#ifdef _TEST_
#include <time.h>
#endif




int main(int argc, char** argv)
{
    int train_rowN;
    int cloN;
    int test_rowN;
    int train_len;
    int test_len;
    int num_each_thread;
    int reminder;
    int i, j;
    clock_t start_t, end_t;

    // 创建线程
    thread_argument t_args[MAX_THREADS];
    store_args s_args[MAX_QUEUES];
    if (create_tpool(&thread_pool, t_args,  MAX_THREADS) != 0)
    {
        return -1;
    }

#ifdef _TEST_
    start_t = clock();
#endif

    fread_anlysis(train_rowN, cloN, test_rowN, train_len, test_len);
#ifdef _TEST_
    end_t = clock();
    std::cout << "file anysis using " << end_t - start_t << '\n';
#endif

#ifdef _TEST_
    start_t = clock();
#endif

    TrainData_Set = new features_labels[train_rowN];
    TestData_Set = new features_labels[test_rowN];

#ifdef _TEST_
    end_t = clock();
    std::cout << "new operator using " << end_t - start_t << '\n';
#endif

#ifdef _TEST_
    start_t = clock();
#endif

    num_each_thread = train_rowN / MAX_THREADS;
    reminder = train_rowN % MAX_THREADS;
    for (i = 0; i < MAX_THREADS; ++i)
    {
        s_args[i].store_box = TrainData_Set;
        s_args[i].feature_nums = cloN;
        s_args[i].start_line = num_each_thread * i;
        s_args[i].end_line = num_each_thread * (i + 1);
        s_args[i].start_pos = \
            (s_args[i].start_line == 0 ? 0 : Trainline_split[s_args[i].start_line - 1] + 1);

        add_task_2_tpool(thread_pool, &store_train_data_block, (void*)(&s_args[i]));
    }
    if (reminder)
    {
        s_args[i].store_box = TrainData_Set;
        s_args[i].feature_nums = cloN;
        s_args[i].start_line = num_each_thread * i;
        s_args[i].end_line = train_rowN;
        s_args[i].start_pos = \
            (s_args[i].start_line == 0 ? 0 : Trainline_split[s_args[i].start_line - 1] + 1);

        add_task_2_tpool(thread_pool, &store_train_data_block, (void*)(&s_args[i]));
        ++i;
    }

    num_each_thread =  test_rowN / MAX_THREADS;
    reminder = test_rowN % MAX_THREADS;
    for (j = 0; j < MAX_THREADS; ++j, ++i)
    {
        s_args[i].store_box = TestData_Set;
        s_args[i].feature_nums = cloN;
        s_args[i].start_line = num_each_thread * j;
        s_args[i].end_line = num_each_thread * (j + 1);
        s_args[i].start_pos = \
            (s_args[i].start_line == 0 ? 0 : Testline_split[s_args[i].start_line - 1] + 1);

        add_task_2_tpool(thread_pool, &store_test_data_block, (void*)(&s_args[i]));
    }
    if (reminder)
    {
        s_args[i].store_box = TestData_Set;
        s_args[i].feature_nums = cloN;
        s_args[i].start_line = num_each_thread * j;
        s_args[i].end_line = test_rowN;
        s_args[i].start_pos = \
            (s_args[i].start_line == 0 ? 0 : Testline_split[s_args[i].start_line - 1] + 1); 
        add_task_2_tpool(thread_pool, &store_test_data_block, (void*)(&s_args[i]));
    }

    while(true)
    {
        usleep(1000);
        pthread_mutex_lock(&thread_pool->queue_lock);
        if(thread_pool->thread_free == 0LL)
        {
            pthread_mutex_unlock(&thread_pool->queue_lock);
            break;
        }
        pthread_mutex_unlock(&thread_pool->queue_lock);
    }

#ifdef _TEST_
    end_t = clock();
    std::cout << "store data using time " << end_t - start_t << '\n';
#endif


    return 0;
}
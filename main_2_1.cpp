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

// 使用 mmap 需要的头文件
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



//#define _OFFLINE_
#define _ONLINE_
//#define _TEST_


#define CacheLine_Size 64

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
#ifdef _OFFLINE_
const char trainFile[] = "./data/train_data.txt";
const char testFile[] = "./data/test_data.txt";
const char predictFile[] = "./projects/student/result.txt";
const char answerFile[] = "./projects/student/answer.txt";
const char weightParamsFile[] = "modelweight.txt";

// 猜测可能的数据的纬度
static const int DIMEN_DATA = 1000;
static const int MAXS = 64*1024*1024;  // 最大可以存储 64M 的数据
static const int MAXL = 10000;     // 猜测文件可能的最大行数  这里猜测 1万 行
static const int MAX_THREADS = 3;
static const int MAX_QUEUES = 64;
char* train_buf;           // 用于存储 训练数据的二进制文件
char* test_buf;            // 用于存储 测试数据的二进制文件
int* Trainline_split;   // 用于指示每一行的末尾的 偏移位置
int* Testline_split;
int predictVec[MAXL];

#endif



#ifdef _ONLINE_
const char trainFile[] = "/data/train_data.txt";
const char testFile[] = "/data/test_data.txt";
const char predictFile[] = "/projects/student/result.txt";
const char answerFile[] = "/projects/student/answer.txt";
const char weightParamsFile[] = "modelweight.txt";

// 猜测可能的数据的纬度
static const int DIMEN_DATA = 5000;     // 猜测特征不会超过 5000维度
static const int MAXS = 512*1024*1024;  // 最大可以存储 512M 的数据
static const int MAXL = 500000;     // 猜测文件可能的最大行数  这里猜测 50万 行
static const int MAX_THREADS = 8;
static const int MAX_QUEUES = 64;
char* train_buf;           // 用于存储 训练数据的二进制文件
char* test_buf;            // 用于存储 测试数据的二进制文件
int* Trainline_split;   // 用于指示每一行的末尾的 偏移位置
int* Testline_split;
#endif


// 线程池
tpool_t* thread_pool;

// 全局的数据存储区域
struct features_labels
{
    double features[DIMEN_DATA];
    int label;
};

features_labels* TrainData_Set;    // 数据存储区域
features_labels* TestData_Set;

double weight_param[DIMEN_DATA] {0.0};      // 权重系数

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
    int& end_line = arg->end_line;
    double dataV;
    int row = arg->start_line;
    int clo = 0;
    int char_index = arg->start_pos;
    for (; row < end_line; ++row)
    {
        clo &= 0x00;
        for (; clo < arg->feature_nums; ++clo)
        {
            if(train_buf[char_index] == '-')
            {
                dataV = train_buf[++char_index] + 0.1*train_buf[char_index +=2] + \
                    0.01*train_buf[++char_index] + 0.001*train_buf[++char_index] - 53.328;
                dataV = -1*dataV;
                char_index += 2;
            }
            else
            {
                dataV = train_buf[char_index] + 0.1*train_buf[char_index += 2] + \
                    0.01*train_buf[++char_index] + 0.001*train_buf[++char_index] - 53.328;
                    char_index += 2;
            }
            store_box[row].features[clo] = dataV;
        }
        store_box[row].label = (train_buf[char_index] == '0' ? 0 : 1);

        char_index += 2;
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
    int& end_line = arg->end_line;
    int char_index = arg->start_pos;
    double dataV;
    int row = arg->start_line;
    int clo = 0;
    for (; row < end_line; ++row)
    {
        clo &= 0x00;
        for (; clo < arg->feature_nums; ++clo)
        {
            if(test_buf[char_index] == '-')
            {
                dataV = test_buf[++char_index] + 0.1*test_buf[char_index += 2]+ \
                    0.01*test_buf[++char_index] + 0.001*test_buf[++char_index] - 53.328;
                dataV = -1*dataV;
                char_index += 2;
            }
            else
            {
                dataV = test_buf[char_index] + 0.1*test_buf[char_index += 2] + \
                    0.01*test_buf[++char_index] + 0.001*test_buf[++char_index] - 53.328;
                char_index += 2;
            }
            store_box[row].features[clo] = dataV;
        }
    }
    return NULL;
}


inline double wxbCal(double* features, const int& feature_nums )
{
    double sum = 0.0;
    int items = CacheLine_Size / sizeof(double);
    int i,j;
    for (i = 0; i < feature_nums; i += items)
    {
        for(j = 0; j < items; ++j)
        {
            sum += weight_param[i + j] * features[i+j];
        }
        //std::cout << "("<< weight_param[i] << "*" << features[i] <<")  ";
        //sum += weight_param[i] * features[i];
    }
    //std::cout << sum << ", ";
    return sum;
}


inline double sigmoidCal(const double& mulsum)
{
    double expv = exp(-1 * mulsum);
    double expvInt = 1.0 / (1.0 + expv);
    return expvInt;
}


double gradientSlope(   features_labels* dataset, 
                        int index, 
                        const double* sigmodVec,
                        int& data_start,
                        int& data_end)
{
    double gsv = 0.0;
    int i,k;
    for(i = data_start, k = 0; i < data_end; ++i, ++k)
    {
        //std::cout <<"("<< dataset[i].label << "-" << sigmodVec[k] << "*"<<dataset[i].features[index] << ") ";
        gsv += (dataset[i].label - sigmodVec[k]) * dataset[i].features[index];
    }
    //std::cout << gsv << " ,";
    gsv = gsv / (double)(data_end - data_start);
    //std::cout << gsv << '\n';
    return gsv;
}


void train(const int& train_rowN, const int& feature_nums)
{
    int bloch_size = 40;
    double stepSize = 5.0;
    int times = train_rowN / bloch_size;
    int start, end, starting;
    double wxbVal, sigmodVal;
    double sigmoidVec[bloch_size];
    int i,j,k,iter;

    for(i = 0; i < times; ++i)
    {
        if( i != 0 && i%20 == 0)
            stepSize = stepSize *0.95;
        start = i * bloch_size;
        end = (i + 1) * bloch_size;

        for (j = start, k = 0; j < end; ++j, ++k)
        {
            wxbVal = wxbCal(TrainData_Set[j].features, feature_nums);
            sigmodVal = sigmoidCal(wxbVal);
            sigmoidVec[k] = sigmodVal;
        }
        for (j = 0; j < feature_nums; ++j)
        {
            weight_param[j] += stepSize * gradientSlope(TrainData_Set, j, sigmoidVec, start, end);
        }
    }

    starting = bloch_size / 2;
    for(i = 0; i < times; ++i)
    {
        if( i != 0 && i%20 == 0)
            stepSize = stepSize *0.5;
        start = i * bloch_size + starting;
        end = (i + 1) * bloch_size + starting;
        end = end > train_rowN ? train_rowN : end;

        for (j = start, k = 0; j < end; ++j, ++k)
        {
            wxbVal = wxbCal(TrainData_Set[j].features, feature_nums);
            sigmodVal = sigmoidCal(wxbVal);
            sigmoidVec[k] = sigmodVal;
        }
        for (int j = 0; j < feature_nums; ++j)
        {
            weight_param[j] += stepSize * gradientSlope(TrainData_Set, j, sigmoidVec, start, end);
        }
    }
}

void predict(const int& test_rowN, const int& feature_nums)
{
    double sigVal;
    int predictVal;
    char write_to_file[test_rowN * 2];
    FILE* write_file = fopen(predictFile, "wb");

    for (int j = 0; j < test_rowN; j++) {
        sigVal = sigmoidCal(wxbCal(TestData_Set[j].features, feature_nums));
        predictVal = sigVal >= 0.5 ? 1 : 0;   
        // 二分类问题的判决分割值，如果结果大于 0.5 ,那么认为是 1；如果结果小于 0.5 那么认为是 0
    #ifdef _OFFLINE_
        predictVec[j] = (predictVal);
    #endif
        write_to_file[2*j] = (predictVal == 0) ? '0' : '1';
        write_to_file[2*j + 1] = '\n';
    }

    fwrite(write_to_file, sizeof(char), test_rowN*2, write_file);
    fclose(write_file);
}


bool loadAnswerData(const char* awFile, std::vector<int> &awVec)
{
    std::ifstream infile(awFile);
    if (!infile) {
        //cout << "打开答案文件失败" << endl;
        exit(0);
    }

    while (infile) {
        std::string line;
        int aw;
        getline(infile, line);
        if (line.size() > 0) {
            std::stringstream sin(line);
            sin >> aw;
            awVec.push_back(aw);
        }
    }

    infile.close();
    return true;
}






int get_clomn_nums(const char* __buf)
{
    int count = 0;
    int i;
    for(i = 0; __buf[i] != '\n'; ++i)
    {
        count += (__buf[i] == ',' ? 1 : 0);
    }
    return count;
}

int get_train_line_nums(const char* __buf, int len, int cloN,int* line_split)
{
    int count = 0;
    int i;
    const int offset = 6*cloN +1;
    for (i = offset; i < len; ++i)
    {
        if (__buf[i] == '\n')
        {
            line_split[count] = i;      // 第 count 行的行尾具体的位置, 下标从 0 开始
            ++count;
            i += offset;
        }
    }
    return count;
}


int get_test_line_nums(const char* __buf, int len, int cloN, int* line_split)
{
    int count = 0;
    int i;
    const int offset = 6*cloN - 1;
    for(i = offset; i < len; ++i)
    {
        if(__buf[i] == '\n')
        {
            line_split[count] = i;
            ++count;
            i += offset;
        }
    }
    return count;
}



#ifdef _TEST_
#include <time.h>
#endif




int main(int argc, char** argv)
{
    const int read_pipelines = 7;
    int train_rowN;
    int cloN;
    int test_rowN;
    int train_len;
    int test_len;
    int num_each_thread, reminder;
    int i, j, fd_train, fd_test;
    struct stat st;
#ifdef _TEST_
    clock_t start_t, end_t;
#endif

#ifdef _TEST_
    start_t = clock();
#endif
    // 创建线程
    thread_argument t_args[MAX_THREADS];
    store_args s_args[MAX_QUEUES];

    if (create_tpool(&thread_pool, t_args,  MAX_THREADS) != 0)
    {
        return -1;
    }

#ifdef _TEST_
    end_t = clock();
    std::cout << "create thread pool  using time " << end_t - start_t << '\n';
#endif

#ifdef _TEST_
    start_t = clock();
#endif

    // 使用 mmap 进行读写
    fd_train = open(trainFile, O_RDONLY);
    fstat(fd_train, &st);
    train_len = st.st_size;
    train_buf = (char*)mmap(NULL, train_len, PROT_READ, MAP_PRIVATE, fd_train, 0);

    fd_test = open(testFile, O_RDONLY);
    fstat(fd_test, &st);
    test_len = st.st_size;
    test_buf = (char*)mmap(NULL, test_len, PROT_READ, MAP_PRIVATE, fd_test, 0);

    cloN = get_clomn_nums(train_buf);
    train_rowN = train_len / (6*cloN + 1);
    test_rowN = test_len / (6*cloN);

#ifdef _TEST_
    end_t = clock();
    std::cout << "file anysis using " << end_t - start_t << '\n';
#endif


#ifdef _TEST_
    start_t = clock();
#endif

    TrainData_Set = new features_labels[train_rowN];
    TestData_Set = new features_labels[test_rowN];
    Trainline_split = new int[train_rowN];
    Testline_split = new int[test_rowN];

#ifdef _TEST_
    end_t = clock();
    std::cout << "new operator using " << end_t - start_t << '\n';
#endif

#ifdef _TEST_
    start_t = clock();
#endif

#ifdef _TEST_
    start_t = clock();
#endif
    train_rowN = get_train_line_nums(train_buf, train_len, cloN, Trainline_split);
#ifdef _TEST_
    end_t = clock();
    std::cout << "get line nums using time " << end_t - start_t << '\n';
#endif


#ifdef _TEST_
    std::cout << "(train_rowN, test_rowN) = (" << train_rowN << ", " << test_rowN << ")\n"; 
#endif

#ifdef _TEST_
    start_t = clock();
#endif

    num_each_thread = train_rowN / read_pipelines;
    reminder = train_rowN % read_pipelines;
    for(i = 0; i < read_pipelines; ++i)
    {
        s_args[i].feature_nums = cloN;
        s_args[i].store_box = TrainData_Set;
        s_args[i].start_line = i * num_each_thread;
        s_args[i].end_line = (i+1) * num_each_thread;
        s_args[i].start_pos = (s_args[i].start_line == 0   \
            ? 0 : Trainline_split[s_args[i].start_line - 1] + 1);
        add_task_2_tpool(thread_pool, &store_train_data_block, (void*)(&s_args[i]));
    }

    if(reminder)
    {
        s_args[i].feature_nums = cloN;
        s_args[i].store_box = TrainData_Set;
        s_args[i].start_line = i * num_each_thread;
        s_args[i].end_line = train_rowN;
        s_args[i].start_pos = Trainline_split[s_args[i].start_line - 1] + 1;
        add_task_2_tpool(thread_pool, &store_train_data_block, (void*)(&s_args[i]));
    }

    test_rowN = get_test_line_nums(test_buf, test_len, cloN, Testline_split);

    // 等待线程执行完毕
    // 即读取数据完毕
    while(true)
    {
        usleep(100);
        pthread_mutex_lock(&thread_pool->queue_lock);
        if(thread_pool->thread_free == 0LL)
        {
            pthread_mutex_unlock(&thread_pool->queue_lock);
            break;
        }
        pthread_mutex_unlock(&thread_pool->queue_lock);
    }

#ifdef _TEST_
    //for(j = 0; j < 10; ++j)
    //    std::cout << TrainData_Set[train_rowN - 1].features[j] << " ";
    //std::cout << '\n';

    end_t = clock();
    std::cout << "load train buf to struct using time " << end_t - start_t << '\n';
#endif

#ifdef _TEST_
    start_t = clock();
#endif

    num_each_thread = test_rowN / read_pipelines;
    reminder = test_rowN % read_pipelines;
    for (i = 0; i < read_pipelines; ++i)
    {
        s_args[i].feature_nums = cloN;
        s_args[i].store_box = TestData_Set;
        s_args[i].start_line = i * num_each_thread;
        s_args[i].end_line = (i+1) * num_each_thread;
        s_args[i].start_pos = (s_args[i].start_line == 0   \
            ? 0 : Testline_split[s_args[i].start_line - 1] + 1);
        add_task_2_tpool(thread_pool, &store_test_data_block, (void*)(&s_args[i]));
    }

    if(reminder)
    {
        s_args[i].feature_nums = cloN;
        s_args[i].store_box = TestData_Set;
        s_args[i].start_line = i * num_each_thread;
        s_args[i].end_line = test_rowN;
        s_args[i].start_pos = Testline_split[s_args[i].start_line - 1] + 1;
        add_task_2_tpool(thread_pool, &store_test_data_block, (void*)(&s_args[i]));
    }

    train(train_rowN, cloN);

    // 等待线程执行完毕
    while(true)
    {
        usleep(100);
        pthread_mutex_lock(&thread_pool->queue_lock);
        if(thread_pool->thread_free == 0LL)
        {
            pthread_mutex_unlock(&thread_pool->queue_lock);
            break;
        }
        pthread_mutex_unlock(&thread_pool->queue_lock);
    } 

#ifdef _TEST_
    //for(j = 0; j < 10; ++j)
    //    std::cout << TestData_Set[test_rowN - 1].features[j] << " ";
    //std::cout << '\n';


    end_t = clock();
    std::cout << "train and read test buf to struct using time " <<end_t - start_t << '\n';
#endif

    predict(test_rowN, cloN);



#ifdef _TEST_
    // 测试部分
    std::vector<int> answerVec;
    int correctCount = 0;
    double accurate;
    loadAnswerData(answerFile, answerVec);
    //std::cout << "test data set size is " << logical.predictVec.size() << std::endl;
    correctCount = 0;
    for (int j = 0; j < test_rowN; j++) {
        if (j < answerVec.size()) {
            if (answerVec[j] == predictVec[j]) {
                correctCount++;
            }
        } else {
            std::cout << "answer size less than the real predicted value" << std::endl;
        }
    }

    accurate = ((double)correctCount) / answerVec.size();
    std::cout << "the prediction accuracy is " << accurate << std::endl;
#endif




#ifdef _TEST_
    start_t = clock();
#endif
    munmap(train_buf, train_len);
    munmap(test_buf, test_len);

// 销毁创建的线程
    destroy_tpool(thread_pool);

#ifdef _TEST_
    end_t = clock();
    std::cout << "munmap using time " << end_t - start_t << '\n';
#endif
    return 0;
}
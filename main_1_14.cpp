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









// 全局变量
// 文件路径
const char trainFile[] = "/data/train_data.txt";
const char testFile[] = "/data/test_data.txt";
const char predictFile[] = "/projects/student/result.txt";
const char answerFile[] = "/projects/student/answer.txt";
const char weightParamsFile[] = "modelweight.txt";

// 实在是无法确定线上的数据数据量究竟有多大
const int MAXS = 1024*1024*1024;  // 最大可以存储 64M 的数据
const int MAXL = 1000000;     // 猜测文件可能的最大行数  这里猜测 100万 行
char train_buf[MAXS];
char test_buf[MAXS];
int Trainline_split[MAXL];   // 用于指示每一行的末尾的 偏移位置
int Testline_split[MAXL];


const char sep[3] = {',', '\n', '\0'};
const int MAX_THREADS  = 16;
const int MAX_QUEUES = 64;  // 这个实际上没有用上


// 全局的线程池,可以在任何地方进行使用
tpool_t* thread_pool;

struct features_label
{
public:
    std::vector<double> features;
    int     label;
    features_label(std::vector<double>&& __f, int __l)
        : features(std::move(__f)), label(__l) {}

    features_label(features_label&& __fl)
        : features(std::move(__fl.features)),
        label(__fl.label)
    {}

    features_label()
    {}
}__attribute__((__aligned__(64)));


// LR  线性回归分析
struct LR
{
    std::vector<features_label> trainData_Set;
    std::vector<features_label> testData_Set;

    std::vector<int> predictVec;
    std::vector<double> param;      // 参数
    int feature_nums;
    int data_nums;

    void train();
    void predict();
    void storePredict(std::vector<int>& pre);
    void storeModel();
    LR(unsigned int _CLON, unsigned int _TRAIN_ROW, unsigned int _TEST_ROW);
    bool loadTrainData(char* __buf, const int& __len);
    bool loadTestData(char* __buf, const int& __len);
    double wxbCalc(const features_label& __data);
    double sigmoidCalc(const double wxb);
    double gradientSlope(const std::vector<features_label>& __data_set,
                        int index, const std::vector<double>& sigmoidVec,
                        int start,
                        int end);
    

    const double weight_initV = 0.5;
    double stepSize = 5;

    int batch_size = 40;

    const int maxIterTimes = 200;
    const double predictTrueThresh = 0.5;
    const int train_show_step = 10;
};


LR::LR(unsigned int _CLON, unsigned int _TRAIN_ROW, unsigned int _TEST_ROW)
    : trainData_Set(_TRAIN_ROW),
      testData_Set(_TEST_ROW),
      feature_nums(_CLON),
      data_nums(_TRAIN_ROW),
      param(_CLON, weight_initV),
      predictVec(_TEST_ROW)
{
    int i;
    for (i = 0; i < _TRAIN_ROW; ++i)
        trainData_Set[i].features.resize(_CLON);
    for (i = 0; i < _TEST_ROW; ++i)
        testData_Set[i].features.resize(_CLON);
}

bool LR::loadTrainData(char* __buf, const int& __len)
{
    int row = 0;
    int clo = 0;
    double dataV;
    char* p;
    p = strtok(__buf, sep);
    while(p)
    {
        dataV = atof(p);
        if (clo == this->feature_nums)
        {
            this->trainData_Set[row].label = (int)dataV;
            clo = 0;
            ++row;
        }
        else
        {
            this->trainData_Set[row].features[clo] = dataV;
            ++clo;
        }
        p = strtok(NULL, sep);
    }
}



bool LR::loadTestData(char* __buf, const int& __len)
{
    int row = 0;
    int clo = 0;
    double dataV;
    char* p;
    p = strtok(__buf, sep);
    while(p)
    {
        dataV = atof(p);
        this->testData_Set[row].features[clo] = dataV;
        ++clo;
        if (clo == this->feature_nums)
        {
            ++row;
            clo = 0;
        }
        p = strtok(NULL, sep);
    }
}


double LR::wxbCalc(const features_label& __data)
{
    double mulSum = 0.0L;
    int i;
    double wtv, feav;
    for (i = 0; i < param.size(); ++i) {
        wtv = param[i];
        feav = __data.features[i];
        mulSum += wtv * feav;
    }

    return mulSum;
}


double LR::sigmoidCalc(const double wxb)
{
    double expv = exp(-1 * wxb);
    double expvInv = 1 / (1 + expv);
    return expvInv;
}

double LR::gradientSlope(const std::vector<features_label> &dataSet, int index,
    const std::vector<double> &sigmoidVec,
    int data_start,
    int data_end)
{
    double gsV = 0.0L;
    int i;
    int k;
    double sigv, label;
    for (i = data_start, k = 0; i < data_end; ++i, ++k) {
        sigv = sigmoidVec[k];
        label = dataSet[i].label;
        gsV += (label - sigv) * (dataSet[i].features[index]);
    }

    gsV = gsV / (this->batch_size);
    return gsV;
}



void LR::train()
{
    int times = this->trainData_Set.size() / this->batch_size;
    int start;
    int end;
    double sigmoidVal;
    double wxbVal;
    int starting;
    std::vector<double> sigmoidVec(batch_size);
    for (int iter = 0; iter < 2; ++iter)
    {
        starting = iter * (this->batch_size / 2);
        for (int i = 0; i < times; ++i)
        {
            if (iter == 0 && i != 0 && i % 20 == 0)
            {
                this->stepSize = this->stepSize * 0.95;
            }
            if (iter == 1 && i != 0 && i % 10 == 0)
            {
                this->stepSize = this->stepSize * 0.5;
            }

            start = i * batch_size + starting;
            end = (i + 1) * batch_size + starting;
            end = end > trainData_Set.size() ? (end-starting) : end;

            for (int j = start, k = 0; j < end; ++j, ++k)
            {
                wxbVal = wxbCalc(trainData_Set[j]);
                sigmoidVal = sigmoidCalc(wxbVal);
                sigmoidVec[k] = sigmoidVal;
            }
            for (int j = 0; j < param.size(); ++j)
            {
                param[j] += stepSize * gradientSlope(trainData_Set, j, sigmoidVec, start, end);
            }
        }
    }
}


void LR::predict()
{
    double sigVal;
    int predictVal;
    char write_to_file[this->testData_Set.size() * 2];
    FILE* write_file = fopen(predictFile, "wb");
    for (int j = 0; j < testData_Set.size(); j++) {
        sigVal = sigmoidCalc(wxbCalc(testData_Set[j]));
        predictVal = sigVal >= predictTrueThresh ? 1 : 0;   
        // 二分类问题的判决分割值，如果结果大于 0.5 ,那么认为是 1；如果结果小于 0.5 那么认为是 0
        predictVec[j] = (predictVal);
        write_to_file[2*j] = (predictVal == 0) ? '0' : '1';
        write_to_file[2*j + 1] = '\n';
    }

    fwrite(write_to_file, sizeof(char), this->testData_Set.size()*2, write_file);
    fclose(write_file);
}

void LR::storePredict(std::vector<int> &predict)
{
    int i;

    std::ofstream fout(predictFile);
    if (!fout.is_open()) {
        //std::cout << "打开预测结果文件失败" << std::endl;
    }
    for (i = 0; i < predict.size(); i++) {
        fout << predict[i] << std::endl;
    }
    fout.close();
    return;
}




bool loadAnswerData(std::string awFile, std::vector<int> &awVec)
{
    std::ifstream infile(awFile.c_str());
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


struct thread_func_arg
{
    LR* lr;
    int start_pos;
    int start_line;
    int end_line;
}__attribute__((__aligned__(64)));


// thread 的 function 
void* store_train_data_block(void* __args)
{
    /**
        对比较大的文件,采用多线程分块进行读取
     */
    thread_func_arg* arg = (thread_func_arg*)(__args);
    LR* lr = arg->lr;
    int start_pos = arg->start_pos;
    double dataV;
    int row;
    int clo;
    //std::cout << "("<<arg->start_line << ", " << arg->end_line << ", "<< arg->start_pos<< ")\n";
    for (row = arg->start_line; row < arg->end_line; ++row)
    {
        clo = 0;
        for (; clo < lr->feature_nums; ++clo)
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
            lr->trainData_Set[row].features[clo] = dataV;
        }
        //dataV = atof(&train_buf[start_pos]);
        lr->trainData_Set[row].label = (train_buf[start_pos] == '0' ? 0 : 1);
        start_pos += 2;
        //while(train_buf[start_pos++] != '\n');
    }
    return NULL;
}

void* store_test_data_block(void* __args)
{
    /**
        对比较大的文件,采用多线程分块进行读取
     */
    thread_func_arg* arg = (thread_func_arg*)(__args);
    LR* lr = arg->lr;
    int start_pos = arg->start_pos;
    double dataV;
    char* p;
    int row;
    int clo;
    for (row = arg->start_line; row < arg->end_line; ++row)
    {
        clo = 0;
        for (; clo < lr->feature_nums; ++clo)
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
            lr->testData_Set[row].features[clo] = dataV;
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
    train_buf[train_len] = '\0';
    train_rowN = get_line_nums(train_buf, train_len, Trainline_split);
    cloN = get_clomn_nums(train_buf);
    fclose(fp);
    fp = fopen(testFile, "rb");
    test_len = fread(test_buf, 1, MAXS, fp);
    test_buf[test_len] = '\0';
    test_rowN = get_line_nums(test_buf, test_len, Testline_split);
    fclose(fp);
}

#define _USING_THREAD_
//#define _TEST_

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
#ifdef _TEST_
    clock_t start, finish;
    std::cout << "clock count for per second = " << CLOCKS_PER_SEC << '\n';
    start = clock();
#endif
    fread_anlysis(train_rowN, cloN, test_rowN, train_len, test_len);
#ifdef _TEST_
    finish = clock();
    std::cout << "file anlysis time = " << finish - start << '\n';
    std::cout << train_rowN << ", " << cloN << ", "<< test_rowN << ", " << train_len << ", "<<test_len << '\n';
    start = clock();
#endif
    LR logical(cloN, train_rowN, test_rowN);
#ifdef _TEST_
    finish = clock();
    std::cout << "class gen time = " << finish - start << '\n';
#endif

    // 创建线程
#ifdef _USING_THREAD_
#ifdef _TEST_
    start = clock();
#endif

    thread_argument t_args[MAX_THREADS];
    thread_func_arg f_args[MAX_QUEUES];
    if (create_tpool(&thread_pool, t_args,  MAX_THREADS) != 0)
    {
        return -1;
    }

#ifdef _TEST_
    finish = clock();
    std::cout << "thread gen time = " << finish - start << '\n';


    // 采用多线程将数据写入数据结构中
    // 分块读取的起始位置, 类的实例, 两个数据缓存区域 data_buf
    // 多线程的效果未必比单个线程的结果好
    start = clock();
#endif

    num_each_thread = train_rowN / MAX_THREADS;
    reminder = train_rowN % MAX_THREADS;
    for (i = 0; i < MAX_THREADS; ++i)
    {
        f_args[i].lr = &logical;
        f_args[i].start_line = num_each_thread * i;
        f_args[i].end_line = num_each_thread * (i + 1);
        f_args[i].start_pos = \
            (f_args[i].start_line == 0 ? 0 : Trainline_split[f_args[i].start_line - 1] + 1);

        add_task_2_tpool(thread_pool, &store_train_data_block, (void*)(&f_args[i]));
    }
    if (reminder)
    {
        f_args[i].lr = &logical;
        f_args[i].start_line = num_each_thread * i;
        f_args[i].end_line = train_rowN;
        f_args[i].start_pos = \
            (f_args[i].start_line == 0 ? 0 : Trainline_split[f_args[i].start_line - 1] + 1);

        add_task_2_tpool(thread_pool, &store_train_data_block, (void*)(&f_args[i]));
        ++i;
    }
//  先把训练的数据加载
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

// 在完成了加载训练数据之后
// 加载测试数据和训练的过程同步进行
    num_each_thread =  test_rowN / MAX_THREADS;
    reminder = test_rowN % MAX_THREADS;
    for (j = 0; j < MAX_THREADS; ++j, ++i)
    {
        f_args[i].lr = &logical;
        f_args[i].start_line = num_each_thread * j;
        f_args[i].end_line = num_each_thread * (j + 1);
        f_args[i].start_pos = \
            (f_args[i].start_line == 0 ? 0 : Testline_split[f_args[i].start_line - 1] + 1);

        add_task_2_tpool(thread_pool, &store_test_data_block, (void*)(&f_args[i]));
    }
    if (reminder)
    {
        f_args[i].lr = &logical;
        f_args[i].start_line = num_each_thread * j;
        f_args[i].end_line = test_rowN;
        f_args[i].start_pos = \
            (f_args[i].start_line == 0 ? 0 : Testline_split[f_args[i].start_line - 1] + 1); 
        add_task_2_tpool(thread_pool, &store_test_data_block, (void*)(&f_args[i]));
    }


    //训练的过程
    logical.train();

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

    //const std::vector<double>& vec = logical.trainData_Set[1].features;
    //std::for_each(vec.begin(), vec.end(), [](const double& __v) {std::cout << __v << " ";});
    //std::cout << "\n label = " << logical.trainData_Set[1].label << '\n';
#ifdef _TEST_
    finish = clock();
    std::cout << "loaddata + train using " << finish - start << '\n';
#endif



#endif

#ifndef _USING_THREAD_
    logical.loadTrainData(train_buf, train_len);
    logical.loadTestData(test_buf, test_len);
#endif




#ifdef _TEST_
    start = clock();
#endif
    logical.predict();

#ifdef _TEST_
    finish = clock();
    std::cout << "write file use time = " << (finish - start)<< '\n';
#endif










#ifdef _TEST_
    // 测试部分
    std::vector<int> answerVec;
    int correctCount = 0;
    double accurate;
    loadAnswerData(answerFile, answerVec);
    //std::cout << "test data set size is " << logical.predictVec.size() << std::endl;
    correctCount = 0;
    for (int j = 0; j < logical.predictVec.size(); j++) {
        if (j < answerVec.size()) {
            if (answerVec[j] == logical.predictVec[j]) {
                correctCount++;
            }
        } else {
            std::cout << "answer size less than the real predicted value" << std::endl;
        }
    }

    accurate = ((double)correctCount) / answerVec.size();
    std::cout << "the prediction accuracy is " << accurate << std::endl;
#endif

    destroy_tpool(thread_pool);
    return 0;
}
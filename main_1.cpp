#include <pthread.h>
#include <ctype.h>
#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <math.h>
#include <algorithm>

#include<unistd.h>


typedef struct tpool_work{
   void* (*work_routine)(void*); //function to be called
   void* args;                   //arguments 
   struct tpool_work* next;
   struct __attribute__((__aligned__)) { } aligned;
}tpool_work_t;


typedef struct tpool{
   size_t               shutdown;       //is tpool shutdown or not, 1 ---> yes; 0 ---> no
   size_t               maxnum_thread; // maximum of threads
   pthread_t            *thread_id;     // a array of threads
   long long int		thread_free;  // 当前的线程是否是空闲的
   tpool_work_t*        tpool_head;     // tpool_work queue
   pthread_cond_t       queue_ready;    // condition varaible
   pthread_mutex_t      queue_lock;     // queue lock
   struct __attribute__((__aligned__)){ } aligned;
}tpool_t;




// 用来指示某一个线程当前是否是空闲的状态
typedef struct thread_argument
{
	tpool_t* 	pool;
	int 		flags_index;
	struct __attribute__((__aligned__)) { } aligned;	
}thread_argument;



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
         pool->thread_free &= (~(0x01LL << flags_index));
         // 把代表了当前的线程的位设置为 0
         pthread_cond_wait(&pool->queue_ready,&pool->queue_lock);
      }

      if(pool->shutdown){
         pthread_mutex_unlock(&pool->queue_lock);//pool shutdown,release the mutex and exit
         pthread_exit(NULL);
      }

      /* tweak a work*/
      work = pool->tpool_head;
      pool->tpool_head = (tpool_work_t*)pool->tpool_head->next;
      //pool->thread_free[flags_index] = true;  // 当前的线程正在忙碌
      pool->thread_free |= (0x01LL << flags_index);
      // 把代表了当前线程的位设置为 1
      pthread_mutex_unlock(&pool->queue_lock);
 
      work->work_routine(work->args);     // 我们需要执行的函数

      free(work);
   }
return NULL;
}


int create_tpool(tpool_t** pool, thread_argument* args, size_t max_thread_num)
{
   (*pool) = (tpool_t*)malloc(sizeof(tpool_t));
   if(NULL == *pool){
      //printf("in %s,malloc tpool_t failed!,errno = %d,explain:%s\n",__func__,errno,strerror(errno));
      exit(-1);
   }

   (*pool)->shutdown = 0;
   (*pool)->maxnum_thread = max_thread_num;
   (*pool)->thread_id = (pthread_t*)malloc(sizeof(pthread_t)*max_thread_num);
   (*pool)->thread_free = 0LL;

   if((*pool)->thread_id == NULL){
      //printf("in %s,init thread id failed,errno = %d,explain:%s",__func__,errno,strerror(errno));
      exit(-1);
   }

   (*pool)->tpool_head = NULL;
   if(pthread_mutex_init(&((*pool)->queue_lock),NULL) != 0){
      //printf("in %s,initial mutex failed,errno = %d,explain:%s",__func__,errno,strerror(errno));
      exit(-1);
   }
 
   if(pthread_cond_init(&((*pool)->queue_ready),NULL) != 0){
      //printf("in %s,initial condition variable failed,errno = %d,explain:%s",__func__,errno,strerror(errno));
      exit(-1);
   }

   for(int i = 0; i < max_thread_num; i++){
      args[i].pool = *pool;
      args[i].flags_index = i;
      if(pthread_create(&((*pool)->thread_id[i]),NULL,work_routine,(void*)(&args[i])) != 0){
         //printf("pthread_create failed!\n");
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
      //printf("in %s,malloc work error!,errno = %d,explain:%s\n",__func__,errno,strerror(errno));
      return -1;
   }

   work->work_routine = routine;
   work->args = args;
   work->next = NULL;
   pthread_mutex_lock(&pool->queue_lock);
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


/**
 * 提前声明
 */
struct LR;


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
}__attribute__((__aligned__));

struct Weight_Params
{
    std::vector<double> wtSet;
};

struct arguments
{
    LR* _lr;
    std::vector<double>* vec;
    int start_ind;
    int end_ind;
}__attribute__((__aligned__));

// 文件路径
const char trainFile[] = "./data/train_data.txt";
const char testFile[] = "./data/test_data.txt";
const char predictFile[] = "./projects/student/result.txt";
const char answerFile[] = "./projects/student/answer.txt";
const char weightParamsFile[] = "modelweight.txt";

// 线程池
const int MAX_THREADS   = 3;
const int MAX_QUEUES    = 64;
tpool_t* thread_pool;



// some function 
inline double sigmoidCalculate(const double& __wxb)
{
    double expv = exp(-1 * __wxb);
    double expvv = 1 / (1 + expv);
    //std::cout << __wxb << ", " << expvv << '\n';
    return expvv;
}



// LR  线性回归分析
struct LR
{
    std::vector<features_label> trainData_Set;
    std::vector<features_label> testData_Set;
    std::vector<int> predictVec;
    Weight_Params   param;
    int feature_nums;
    int data_nums;

    void train();
    void predict();
    void storePredict(std::vector<int>& pre);
    void storeModel();
    LR();
    bool init();
    bool loadTrainData();
    bool loadTestData();
    void initParam();
    double wxbCalc(const features_label& __data);
    double sigmoidCalc(const double wxb);
    double lossCal();
    double gradientSlope(const std::vector<features_label>& __data_set,
                        int index, const std::vector<double>& sigmoidVec);
    

    double weight_initV = 0.5;
    double stepSize = 5;

    const int maxIterTimes = 200;
    const double predictTrueThresh = 0.5;
    const int train_show_step = 10;
};


void* thread_func1(void* __args)
{
    arguments* args = (arguments*)(__args);
    LR* lr = args->_lr;
    std::vector<double>* sigmoidVec = args->vec;

    //std::cout << "------for thread-------" << '\n';
    //std::cout <<"sigmoidVec size = "<< sigmoidVec->size() << '\n';

    double wxbVal;

    //std::cout << "thread_func1 running" << '\n';
    //std::cout << "(" << args->start_ind << ", " << args->end_ind << ")\n";
    for (int i = args->start_ind; i < args->end_ind; ++i)
    {
        wxbVal = lr->wxbCalc(lr->trainData_Set[i]);
        //std::cout << "wxbVal = " << wxbVal << '\n';
        wxbVal = sigmoidCalculate(wxbVal);
        //std::cout << "sigmoidVec = " << wxbVal << '\n';
        (args->vec)->at(i) = wxbVal;
    }
    return NULL;
}

void* thread_func2(void* __args)
{
    arguments* args = (arguments*)(__args);
    LR* lr = args->_lr;
    for (int i = args->start_ind; i < args->end_ind; ++i)
    {
        lr->param.wtSet[i] += lr->stepSize * \
            lr->gradientSlope(lr->trainData_Set, i, *args->vec);
    }
    return NULL;
}

void* thread_func3(void* __args)
{
    arguments* args = (arguments*)(__args);
    LR* lr = args->_lr;
    double sigVal;
    double predictVal;
    for (int i = args->start_ind; i < args->end_ind; ++i)
    {
        sigVal = sigmoidCalculate(lr->wxbCalc(lr->testData_Set[i]));
        predictVal = (sigVal >= (lr->predictTrueThresh) ? 1 : 0);
        lr->predictVec[i] = predictVal;
    }
    return NULL;
}

LR::LR()
{
    init();
}

bool LR::loadTrainData()
{
    //std::cout << "开始读取数据" << '\n';
    std::ifstream infile(trainFile);
    if(!infile) { std::cout << "打开文件失败" << '\n'; exit(0); }
    std::string line;

    char ch;
    double dataV;
    int i;
    int lebelV;
    while(infile)
    {
        std::getline(infile, line);
        if (line.size() > 0)
        {
            std::stringstream sin(line);
            std::vector<double> feature;
            i = 0;

            while(sin)
            {
                sin >> dataV;
                feature.push_back(dataV);
                sin >> ch;
                ++i;
            }
            /**
             * 一行的最后一个数据是 标签
             */
            lebelV = (int)feature.back();
            feature.pop_back();
            this->trainData_Set.push_back(features_label(std::move(feature), lebelV));
        }
    }
    infile.close();
    //std::cout << "成功的读取了训练的数据" << '\n';
    return true;
}

bool LR::loadTestData()
{
    //std::cout << "开始读取数据" << '\n';
    std::ifstream infile(testFile);
    if(!infile) { std::cout << "打开文件失败" << '\n'; exit(0); }
    std::string line;

    char ch;
    double dataV;
    int i;
    int lebelV;
    while(infile)
    {
        std::getline(infile, line);
        if (line.size() > 0)
        {
            std::stringstream sin(line);
            std::vector<double> feature;
            i = 0;

            while(sin)
            {
                sin >> dataV;
                feature.push_back(dataV);
                sin >> ch;
                ++i;
            }
            this->testData_Set.push_back(features_label(std::move(feature), 0));
        }
    }
    infile.close();
    //std::cout << "成功的读取了测试的数据" << '\n';
    return true;

}

void LR::initParam()
{
    for (int i =0; i < this->feature_nums; ++i)
        this->param.wtSet.push_back(this->weight_initV);
}


bool LR::init()
{
    trainData_Set.clear();
    loadTrainData();
    this->feature_nums = trainData_Set[0].features.size();
    this->data_nums = this->trainData_Set.size();
    param.wtSet.clear();
    this->initParam();
    return true;
}

/**
 * 计算 w * x
 */
double LR::wxbCalc(const features_label& __data)
{
    double mulSum = 0.0L;
    unsigned int i;
    for (i = 0; i < this->feature_nums; ++i)
    {
        mulSum += this->param.wtSet[i] * __data.features[i];
    }
    return mulSum;
}

inline double LR::sigmoidCalc(const double __wxb)
{
    double expv = exp(-1 * __wxb);
    double expvv = 1 / ( 1 + expv);
    return expvv;
}


double LR::gradientSlope(const std::vector<features_label>& __dataset, 
            int index, const std::vector<double>& sigmoidV)
{
    double gsV = 0.0L;
    int i;
    double sigv, label;
    for (i=0; i < __dataset.size(); ++i)
    {
        sigv = sigmoidV[i];
        label = __dataset[i].label;
        gsV += (label - sigv) * (__dataset[i].features[index]);
    }
    gsV = gsV / __dataset.size();
    return gsV;
}



void LR::train()
{
    std::vector<double> sigmoidVec(this->data_nums, 0.0);
    /**
     * 每一个线程需要处理的数据的个数
     */
    int i;
    int num_for_eachThread;
    int remind;
    arguments args[MAX_THREADS + 1];
    for (int j = 0; j < this->maxIterTimes; ++j)      // 暂时进行测试,只进行一个迭代过程
    {
        if (j != 0 && j % 10 == 0)
            this->stepSize = this->stepSize * 0.5;

        //std::cout << "iterator " << j << '\n';
        num_for_eachThread = this->data_nums / MAX_THREADS;
        remind = this->data_nums % MAX_THREADS;
        //std::cout << "num for each thread " << num_for_eachThread << '\n';
        //arguments args[MAX_THREADS + 1];
        //std::cout << "add task to thread queue " << '\n';
        for (i = 0; i < MAX_THREADS; ++i)
        {
            args[i].start_ind = num_for_eachThread * i;
            args[i].end_ind = num_for_eachThread * (i + 1);
            args[i].vec = &sigmoidVec;
            args[i]._lr = this;
            //std::cout <<args[i].start_ind << ", " << args[i].end_ind << '\n';
            add_task_2_tpool(thread_pool, &thread_func1, (void*)(&args[i]));
        }
        if (remind != 0)
        {
            /**
             * 主线程
             */
            double wxbVal;
            int start = num_for_eachThread * MAX_THREADS;
            int end = start + remind;

          //std::cout << "(" << start << ", " << end << '\n';

            for (i = start; i < end; ++i)
            {
                wxbVal = this->wxbCalc(this->trainData_Set[i]);
                wxbVal = this->sigmoidCalc(wxbVal);
                sigmoidVec[i] = wxbVal;
            }
        }

      //std::cout << "main thread pointer .." << '\n';
       while(true)
       {
            pthread_mutex_lock(&thread_pool->queue_lock);
            //std::cout <<"thread_free = "<< thread_pool->thread_free << '\n';
            if (thread_pool->thread_free == 0LL)
            {
                pthread_mutex_unlock(&thread_pool->queue_lock);
                break;
            }
            pthread_mutex_unlock(&thread_pool->queue_lock);
            usleep(10000);
       }
      //std::cout << "train part 1 have been done" << '\n';

       num_for_eachThread = this->feature_nums / MAX_THREADS;
       remind = this->feature_nums % MAX_THREADS;

       for (i = 0; i < MAX_THREADS; ++i)
        {
            args[i]._lr = this;
            args[i].vec = &sigmoidVec;
            args[i].start_ind = i * num_for_eachThread;
            args[i].end_ind = (i + 1) * num_for_eachThread;
            //std::cout << "test\n";
            add_task_2_tpool(thread_pool, &thread_func2, (void*)(&args[i]));
        }
        //std::cout << "have added all task to pool" << '\n';
        if (remind != 0)
        {
            int start = num_for_eachThread * MAX_THREADS;
            int end = start + remind;
            for (i = start; i < end; ++i)
            {
                param.wtSet[i] += stepSize * gradientSlope(trainData_Set, i, 
                            sigmoidVec);
            }
        }
        while(true)
        {
            pthread_mutex_lock(&thread_pool->queue_lock);
            //std::cout << "thread_free = " << thread_pool->thread_free << '\n';
            if (thread_pool->thread_free == 0LL)
            {
                pthread_mutex_unlock(&thread_pool->queue_lock);
                break;
            }
            pthread_mutex_unlock(&thread_pool->queue_lock);
            usleep(10000);
        }
    }

}

void LR::predict()
{
    double sigVal;
    double predictVal;
    this->loadTestData();
    this->predictVec.resize(this->testData_Set.size());
    int data_size = this->testData_Set.size();
    int num_for_eachThread = data_size / MAX_THREADS;
    int remind = data_size % MAX_THREADS;
    arguments args[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; ++i)
    {
        args[i]._lr = this;
        args[i].start_ind = num_for_eachThread * i;
        args[i].end_ind = num_for_eachThread * (i + 1);
        add_task_2_tpool(thread_pool, &thread_func3, (void*)(&args[i]));
    }
    if(remind)
    {
        int start = num_for_eachThread * MAX_THREADS;
        int end = start + remind;
        double sigVal;
        double predictVal;
        for (int i = start; i < end; ++i)
        {
            sigVal = sigmoidCalculate(wxbCalc(testData_Set[i]));
            predictVal = sigVal >= predictTrueThresh ? 1 : 0;
            predictVec[i] = predictVal;
        }
    }
    while(true)
    {
        pthread_mutex_lock(&thread_pool->queue_lock);
        if(thread_pool->thread_free == 0LL)
        {
            pthread_mutex_unlock(&thread_pool->queue_lock);
            break;
        }
        pthread_mutex_unlock(&thread_pool->queue_lock);
    }
    storePredict(predictVec);       // 将预测的结果保存到文件
}

/**
 * 将预测的结果存储到指定的路径
 */
void LR::storePredict(std::vector<int>& predict)
{
    std::string line;
    int i;
    std::ofstream fout(predictFile);
    if (!fout.is_open())
    {
        return;
    }
    for (i = 0; i < predict.size(); ++i)
        fout << predict[i] << std::endl;
    fout.close();
    return;
}

void LR::storeModel()
{
    std::string line;
    int i;

    std::ofstream fout(weightParamsFile);

    for(i = 0; i < this->feature_nums; ++i)
        fout << this->param.wtSet[i] << " ";
    fout.close();
    return;
}


//#define _TEST_

int main(int argc, char** argv)
{
    LR logist;
    
    //std::cout << "ready to train model" << '\n';
    #ifdef _TEST_
    std::cout << logist.feature_nums << '\n';
    std::cout << logist.data_nums << '\n';
    const features_label& res = logist.trainData_Set[logist.data_nums - 1];
    std::cout << res.label << '\n';
    std::for_each(  res.features.begin(),
                    res.features.end(),
                    [](const double& __d) { std::cout << __d << " ";}
    );
    std::cout << std::endl;
    #endif


    thread_argument t_args[MAX_THREADS];
    if (create_tpool(&thread_pool, t_args,  MAX_THREADS) != 0)
    {
        std::cout << "create pthread pool failed " << '\n';
        return -1;
    }

    //std::cout << "成功创建一个线程池" << '\n';

    logist.train();

    logist.predict();

    

    destroy_tpool(thread_pool);

    return 0;
}












#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <math.h>
#include <algorithm>

#include <stdio.h>
#include<unistd.h>

#include "tpool.h"


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



void loadAnswerData(const char* answerFile, std::vector<int>& vec_ans)
{
    std::ifstream infile(answerFile);
    if (!infile)
    {
        exit(0);
    }
    std::string line;
    int aw;
    while(infile)
    {
        getline(infile, line);
        if(line.size() >0)
        {
            std::stringstream sin(line);
            sin >> aw;
            vec_ans.push_back(aw);
        }
    }
    infile.close();
    return;
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
    const int maxIterTimes = 100;
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
    std::cout << "开始读取数据" << '\n';
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
    std::cout << "成功的读取了训练的数据" << '\n';
    return true;
}

bool LR::loadTestData()
{
    std::cout << "开始读取数据" << '\n';
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
    std::cout << "成功的读取了测试的数据" << '\n';
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
    for (int j = 0; j < this->maxIterTimes; ++j)		// 暂时进行测试,只进行一个迭代过程
    {
        if (j != 0 && j % 10 == 0)
            this->stepSize = this->stepSize * 0.5;

        std::cout << "iterator " << j << '\n';
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
            usleep(1000);
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
            usleep(1000);
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
    
    std::cout << "ready to train model" << '\n';
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

    std::cout << "成功创建一个线程池" << '\n';

    logist.train();

    logist.storeModel();

    logist.predict();
    /*
    std::for_each(  logist.param.wtSet.begin(),
                    logist.param.wtSet.end(),
                    [](double& __val) { std::cout << __val << " ";});
    std::cout << '\n';
    */

    std::vector<int> ans_biaozhen;
    loadAnswerData(answerFile, ans_biaozhen);
    int correctCount = 0;
    for (int i = 0; i < ans_biaozhen.size(); ++i)
    {
        if (ans_biaozhen[i] == logist.predictVec[i])
            ++correctCount;
    }
    double accurate = ((double)correctCount) / ans_biaozhen.size();
    std::cout << "the prediction accuracy is " << accurate << std::endl;

    destroy_tpool(thread_pool);

    return 0;
}


/**
 * 精度为 0.8285
 */
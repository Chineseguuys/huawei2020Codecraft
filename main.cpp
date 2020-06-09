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
};

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
};

// 文件路径
const char trainFile[] = "./data/train_data.txt";
const char testFile[] = "./data/test_data.txt";
const char predictFile[] = "./projects/student/result.txt";
const char answerFile[] = "./projects/student/answer.txt";

// 线程池
const int MAX_THREADS   = 3;
const int MAX_QUEUES    = 64;
tpool_t* thread_pool;




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
    

    double weight_initV = 1.0;
    const double stepSize = 0.3;
    const int maxIterTimes = 500;
    const double predictTrueThresh = 0.5;
    const int train_show_step = 10;
};


void* thread_func1(void* __args)
{
    arguments* args = (arguments*)(__args);
    LR* lr = args->_lr;
    double wxbVal;
    std::cout << "thread_func1 running" << '\n';
    std::cout << "(" << args->start_ind << ", " << args->end_ind << ")\n";
    for (int i = args->start_ind; i < args->end_ind; ++i)
    {
        wxbVal = lr->wxbCalc(lr->trainData_Set[i]);
        wxbVal = lr->sigmoidCalc(wxbVal);
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
    for (int j = 0; j < 1; ++j)		// 暂时进行测试,只进行一个迭代过程
    {
        num_for_eachThread = this->data_nums / MAX_THREADS;
        remind = this->data_nums % MAX_THREADS;
        std::cout << "num for each thread " << num_for_eachThread << '\n';
        //arguments args[MAX_THREADS + 1];
        std::cout << "add task to thread queue " << '\n';
        for (i = 0; i < MAX_THREADS; ++i)
        {
            args[i].start_ind = num_for_eachThread * i;
            args[i].end_ind = num_for_eachThread * (i + 1);
            args[i].vec = &sigmoidVec;
            args[i]._lr = this;
            std::cout <<args[i].start_ind << ", " << args[i].end_ind << '\n';
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

	    std::cout << "(" << start << ", " << end << '\n';

            for (i = start; i < end; ++i)
            {
                wxbVal = this->wxbCalc(this->trainData_Set[i]);
                wxbVal = this->sigmoidCalc(wxbVal);
                sigmoidVec[i] = wxbVal;
            }
            for (i = start; i < end; ++i)
		std::cout << sigmoidVec[i] << " ";
	    std::cout << '\n';
        }
	
	std::cout << "main thread pointer .." << '\n';

        while(true)
        {
            /**
             * 等待线程队列中的所有的任务执行完毕
             */
            if(thread_pool->tpool_head == NULL)
                break;
	    usleep(10000);
        }

	std::cout << "train part 1 have been done" << '\n';
	std::for_each(sigmoidVec.begin(), sigmoidVec.end(), 
		[](double& __val) { std::cout << __val << " ";});

        num_for_eachThread = this->feature_nums / MAX_THREADS;
        remind = this->feature_nums % MAX_THREADS;

        for (i = 0; i < MAX_THREADS; ++i)
        {
            args[i]._lr = this;
            args[i].vec = &sigmoidVec;
            args[i].start_ind = i * num_for_eachThread;
            args[i].end_ind = (i + 1) * num_for_eachThread;
            add_task_2_tpool(thread_pool, &thread_func2, &args[i]);
        }
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
            if(thread_pool->tpool_head == NULL)
                break;
            pthread_mutex_unlock(&thread_pool->queue_lock);
        }

    }

}
#define _TEST_

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

    if (create_tpool(&thread_pool, MAX_THREADS) != 0)
    {
        std::cout << "create pthread pool failed " << '\n';
        return -1;
    }

    std::cout << "成功创建一个线程池" << '\n';

    logist.train();

    std::for_each(  logist.param.wtSet.begin(),
                    logist.param.wtSet.end(),
                    [](double& __val) { std::cout << __val << " ";});
    std::cout << '\n';

    destroy_tpool(thread_pool);

    return 0;
}

#include <iostream>
#include <string>

#include <fstream>
#include <sstream>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <math.h>
#include "tpool.h"

#include <algorithm>

// 文件路径
const char trainFile[] = "./data/train_data.txt";
const char testFile[] = "./data/test_data.txt";
const char predictFile[] = "./projects/student/result.txt";
const char answerFile[] = "./projects/student/answer.txt";
const char weightParamsFile[] = "modelweight.txt";

// 实在是无法确定线上的数据数据量究竟有多大
const int MAXS = 64*1024*1024;  // 最大可以存储 64M 的数据
const int MAXL = 1000000;     // 猜测文件可能的最大行数  这里猜测 100万 行
char train_buf[MAXS];
char test_buf[MAXS];
int Trainline_split[MAXL];   // 用于指示每一行的末尾的 偏移位置
int Testline_split[MAXL];


const char sep[3] = {',', '\n', '\0'};
const int MAX_THREADS  = 3;
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
            dataV = atof(&train_buf[start_pos]);
            lr->trainData_Set[row].features[clo] = dataV;
            ++start_pos;
            while(train_buf[start_pos++] != ',');
        }
        dataV = atof(&train_buf[start_pos]);
        lr->trainData_Set[row].label = (int)dataV;
        ++start_pos;
        while(train_buf[start_pos++] != '\n');
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
        for (; clo < (lr->feature_nums - 1); ++clo)
        {
            dataV = atof(&test_buf[start_pos]);
            lr->testData_Set[row].features[clo] = dataV;
            ++start_pos;
            while(test_buf[start_pos++] != ',');
        }
        dataV = atof(&test_buf[start_pos]);
        lr->testData_Set[row].features[clo] = dataV;
        ++ start_pos;
        while(test_buf[start_pos++] != '\n');
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

#include <time.h>

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
    clock_t start, finish;
    std::cout << "clock count for per second = " << CLOCKS_PER_SEC << '\n';
    start = clock();
    fread_anlysis(train_rowN, cloN, test_rowN, train_len, test_len);
    finish = clock();
    std::cout << "file anlysis time = " << finish - start << '\n';
    std::cout << train_rowN << ", " << cloN << ", "<< test_rowN << ", " << train_len << ", "<<test_len << '\n';
    start = clock();
    LR logical(cloN, train_rowN, test_rowN);
    finish = clock();
    std::cout << "class gen time = " << finish - start << '\n';


    // 创建线程
#ifdef _USING_THREAD_

    start = clock();

    thread_argument t_args[MAX_THREADS];
    thread_func_arg f_args[MAX_QUEUES];
    if (create_tpool(&thread_pool, t_args,  MAX_THREADS) != 0)
    {
        return -1;
    }
    finish = clock();
    std::cout << "thread gen time = " << finish - start << '\n';

    // 采用多线程将数据写入数据结构中
    // 分块读取的起始位置, 类的实例, 两个数据缓存区域 data_buf
    // 多线程的效果未必比单个线程的结果好
    start = clock();
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

    //const std::vector<double>& vec = logical.trainData_Set[1].features;
    //std::for_each(vec.begin(), vec.end(), [](const double& __v) {std::cout << __v << " ";});
    //std::cout << "\n label = " << logical.trainData_Set[1].label << '\n';
#endif

#ifndef _USING_THREAD_
    logical.loadTrainData(train_buf, train_len);
    logical.loadTestData(test_buf, test_len);
#endif

    finish = clock();
    std::cout << "read file use time = " << (finish - start)<< '\n';

    start = clock();
    logical.train();
    finish = clock();
    std::cout << "Train use time = " << (finish - start)<< '\n';

    start = clock();
    logical.predict();
    finish = clock();
    std::cout << "write file use time = " << (finish - start)<< '\n';






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

    return 0;
}
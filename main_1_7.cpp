#include <iostream>
#include <string>

#include <fstream>
#include <sstream>
#include <string.h>
#include <vector>
#include <math.h>


// 文件路径
const char trainFile[] = "./data/train_data.txt";
const char testFile[] = "./data/test_data.txt";
const char predictFile[] = "./projects/student/result.txt";
const char answerFile[] = "./projects/student/answer.txt";
const char weightParamsFile[] = "modelweight.txt";

// 实在是无法确定线上的数据数据量究竟有多大
const int MAXS = 512*1024*1024;  // 最大可以存储 512M 的数据
char train_buf[MAXS];
char test_buf[MAXS];




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
    bool loadTrainData(const char* __buf, const int& __len);
    bool loadTestData(const char* __buf, const int& __len);
    double wxbCalc(const features_label& __data);
    double sigmoidCalc(const double wxb);
    //double lossCal();
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

bool LR::loadTrainData(const char* __buf, const int& __len)
{
    int row = 0;
    int clo = 0;
    int k = 0;
    double dataV;
    for (k = 0; k < __len;++k)
    {
        dataV = atof(&__buf[k++]);
        //std::cout << dataV << " ";
        if(clo == this->feature_nums)
            this->trainData_Set[row].label = (int)dataV;
        else 
            this->trainData_Set[row].features[clo] = dataV;
        ++clo;
        while (__buf[k] != ',' && __buf[k] != '\n')
            ++k;
        if (clo > this->feature_nums)
        {
            ++row;
            clo = 0;
        }
    } 
}



bool LR::loadTestData(const char* __buf, const int& __len)
{
    int row = 0;
    int clo = 0;
    int k = 0;
    double dataV;
    for (k = 0; k < __len; ++k)
    {
        dataV = atof(&__buf[k++]);
            this->testData_Set[row].features[clo] = dataV;
        ++clo;
        while (__buf[k] != ',' && __buf[k] != '\n')
            ++k;
        if (clo >= this->feature_nums)
        {
            ++row;
            clo = 0;
        }
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

    for (int j = 0; j < testData_Set.size(); j++) {
        sigVal = sigmoidCalc(wxbCalc(testData_Set[j]));
        predictVal = sigVal >= predictTrueThresh ? 1 : 0;   
        // 二分类问题的判决分割值，如果结果大于 0.5 ,那么认为是 1；如果结果小于 0.5 那么认为是 0
        predictVec[j] = (predictVal);
    }

    storePredict(predictVec);
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

int get_line_nums(const char* __buf, int len)
{
    int count = 0;
    for (int i = 0; i < len; ++i)
    {
        if (__buf[i] == '\n')
        {
            ++count;
        }
    }
    return count;
}

void fread_anlysis(int& train_rowN, int& cloN, int& test_rowN, int& train_len, int& test_len)
{
    FILE* fp = fopen(trainFile, "rb");
    train_len = fread(train_buf, 1, MAXS, fp);
    train_rowN = get_line_nums(train_buf, train_len);
    cloN = get_clomn_nums(train_buf);
    fclose(fp);
    fp = fopen(testFile, "rb");
    test_len = fread(test_buf, 1, MAXS, fp);
    test_rowN = get_line_nums(test_buf, test_len);
    fclose(fp);
}


int main(int argc, char** argv)
{
    int train_rowN;
    int cloN;
    int test_rowN;
    int train_len;
    int test_len;
    fread_anlysis(train_rowN, cloN, test_rowN, train_len, test_len);
    //std::cout << train_rowN << ", " << cloN << ", "<< test_rowN << ", " << train_len << ", "<<test_len << '\n';
    LR logical(cloN, train_rowN, test_rowN);

    logical.loadTrainData(train_buf, train_len);
    logical.loadTestData(test_buf, test_len);

    logical.train();
    logical.predict();

/*
    std::vector<int> answerVec;
    int correctCount = 0;
    double accurate;
    loadAnswerData(answerFile, answerVec);
    std::cout << "test data set size is " << logical.predictVec.size() << std::endl;
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
*/
    return 0;
}
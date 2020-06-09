#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <cmath>
#include <cstdlib>
using namespace std;


#ifndef TEST
#define TEST
#endif

/**
 * 这里面存储的是训练集的数据和其标签
 */
struct Data {
    vector<double> features;
    int label;
    Data(vector<double> f, int l) : features(f), label(l)
    {}
};

/**
 * 网络的权重参数
 */
struct Param {
    vector<double> wtSet;
};

/**
 * 线性回归算法
 */
class LR {
public:
    void train();
    void predict();
    int loadModel();
    int storeModel();
    LR(string trainFile, string testFile, string predictOutFile);

private:
    vector<Data> trainDataSet;
    vector<Data> testDataSet;
    vector<int> predictVec;
    Param param;
    string trainFile;
    string testFile;
    string predictOutFile;
    string weightParamFile = "modelweight.txt";

private:
    bool init();
    bool loadTrainData();
    bool loadTestData();
    int storePredict(vector<int> &predict);
    void initParam();
    double wxbCalc(const Data &data);
    double sigmoidCalc(const double wxb);
    double lossCal();
    double gradientSlope(const vector<Data> &dataSet, int index, const vector<double> &sigmoidVec,
        int data_start, int data_end);

private:
    int featuresNum;
    const double wtInitV = 0.5;
    /**
     * 一些机器学习的超参数
     */
    double stepSize = 5;
    /**
     * 看来学习的步长对学习的结果影响很大
     */
    const int batch_size = 40;
    const int maxIterTimes = 100;
    const double predictTrueThresh = 0.5;
    const int train_show_step = 10;
};

/**
 * 构造函数
 * 完成了训练数据的导入
 * 完成了权重值的初始化
 */
LR::LR(string trainF, string testF, string predictOutF)
{
    trainFile = trainF;
    testFile = testF;
    predictOutFile = predictOutF;
    featuresNum = 0;
    init();
}

/**
 * 从训练数据集中导入数据和标签
 */
bool LR::loadTrainData()
{
    ifstream infile(trainFile.c_str());
    string line;

    if (!infile) {
        cout << "打开训练文件失败" << endl;
        exit(0);
    }

    while (infile) {
        getline(infile, line);
        if (line.size() > featuresNum) {
            stringstream sin(line);
            char ch;
            double dataV;
            int i;
            vector<double> feature;
            i = 0;

            while (sin) {
                char c = sin.peek();
                if (int(c) != -1) {
                    sin >> dataV;
                    feature.push_back(dataV);
                    sin >> ch;
                    i++;
                } else {
                    cout << "训练文件数据格式不正确，出错行为" << (trainDataSet.size() + 1) << "行" << endl;
                    return false;
                }
            }
            int ftf;
            /**
             * 最后一个是标签 label
             */
            ftf = (int)feature.back();
            feature.pop_back();
            trainDataSet.push_back(Data(feature, ftf));     // 得到所有的数据集
        }
    }
    infile.close();
    std::cout << "成功读取了训练数据" << std::endl;
    return true;
}

void LR::initParam()
{
    /**
     * 使用初始值来初始化权重
     */
    int i;
    for (i = 0; i < featuresNum; i++) {
        param.wtSet.push_back(wtInitV);
    }
}

bool LR::init()
{
    trainDataSet.clear();
    bool status = loadTrainData();  // 将训练数据导入
    if (status != true) {
        return false;
    }
    featuresNum = trainDataSet[0].features.size();  // 获取特征的个数
    param.wtSet.clear();
    initParam();
    return true;
}


/**
 * 计算  w * x
 */
double LR::wxbCalc(const Data &data)
{
    double mulSum = 0.0L;
    int i;
    double wtv, feav;
    for (i = 0; i < param.wtSet.size(); i++) {
        wtv = param.wtSet[i];
        feav = data.features[i];
        mulSum += wtv * feav;
    }

    return mulSum;
}

inline double LR::sigmoidCalc(const double wxb)
{
    double expv = exp(-1 * wxb);
    double expvInv = 1 / (1 + expv);
    return expvInv;
}


/**
 * loss functin 实际上在计算过程中使用不到它
 * loss function = log(L(theta)) = 
 *  sum(yi * log(h(theta * xi)) + (1 - yi)*log(1 - h(theta * xi)))
 */
double LR::lossCal()
{
    double lossV = 0.0L;
    int i;

    for (i = 0; i < trainDataSet.size(); i++) {
        lossV -= trainDataSet[i].label * log(sigmoidCalc(wxbCalc(trainDataSet[i])));
        lossV -= (1 - trainDataSet[i].label) * log(1 - sigmoidCalc(wxbCalc(trainDataSet[i])));
    }
    lossV /= trainDataSet.size();
    return lossV;
}


/**
 * 计算某一个参数的梯度
 * loss function 是什么
 * loss function = log(L(theta)) = 
 *  sum(yi * log(h(theta * xi)) + (1 - yi)*log(1 - h(theta * xi)))
 * 详细的推导过程见  https://www.jianshu.com/p/dce9f1af7bc9
 * 推导的过程比较的复杂，但是结果的形式较为简单
 * ∂L(theta) / ∂wi = (yi - h(theta * xi))* xi
 */
double LR::gradientSlope(const vector<Data> &dataSet, int index,
    const vector<double> &sigmoidVec,
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
    int times = this->trainDataSet.size() / this->batch_size;
    int start;
    int end;
    double sigmoidVal;
    double wxbVal;
    int starting;
    vector<double> sigmoidVec(batch_size);
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
            end = end > trainDataSet.size() ? (end-starting) : end;

            for (int j = start, k = 0; j < end; ++j, ++k)
            {
                wxbVal = wxbCalc(trainDataSet[j]);
                sigmoidVal = sigmoidCalc(wxbVal);
                sigmoidVec[k] = sigmoidVal;
            }
            for (int j = 0; j < param.wtSet.size(); ++j)
            {
                param.wtSet[j] += stepSize * gradientSlope(trainDataSet, j, sigmoidVec, start, end);
            }
        }
    }
}

void LR::predict()
{
    double sigVal;
    int predictVal;

    loadTestData();
    for (int j = 0; j < testDataSet.size(); j++) {
        sigVal = sigmoidCalc(wxbCalc(testDataSet[j]));
        predictVal = sigVal >= predictTrueThresh ? 1 : 0;   
        // 二分类问题的判决分割值，如果结果大于 0.5 ,那么认为是 1；如果结果小于 0.5 那么认为是 0
        predictVec.push_back(predictVal);
    }

    storePredict(predictVec);
}

int LR::loadModel()
{
    string line;
    int i;
    vector<double> wtTmp;
    double dbt;

    ifstream fin(weightParamFile.c_str());
    if (!fin) {
        cout << "打开模型参数文件失败" << endl;
        exit(0);
    }

    getline(fin, line);
    stringstream sin(line);
    for (i = 0; i < featuresNum; i++) {
        char c = sin.peek();
        if (c == -1) {
            cout << "模型参数数量少于特征数量，退出" << endl;
            return -1;
        }
        sin >> dbt;
        wtTmp.push_back(dbt);
    }
    param.wtSet.swap(wtTmp);
    fin.close();
    return 0;
}

int LR::storeModel()
{
    string line;
    int i;

    ofstream fout(weightParamFile.c_str());
    if (!fout.is_open()) {
        cout << "打开模型参数文件失败" << endl;
    }
    if (param.wtSet.size() < featuresNum) {
        cout << "wtSet size is " << param.wtSet.size() << endl;
    }
    for (i = 0; i < featuresNum; i++) {
        fout << param.wtSet[i] << " ";  // 将权重值储存到文件当中，以空格进行分割
    }
    fout.close();
    return 0;
}


bool LR::loadTestData()
{
    ifstream infile(testFile.c_str());
    string lineTitle;

    if (!infile) {
        cout << "打开测试文件失败" << endl;
        exit(0);
    }

    while (infile) {
        vector<double> feature;
        string line;
        getline(infile, line);
        if (line.size() > featuresNum) {
            stringstream sin(line);
            double dataV;
            int i;
            char ch;
            i = 0;
            while (i < featuresNum && sin) {
                char c = sin.peek();
                if (int(c) != -1) {
                    sin >> dataV;
                    feature.push_back(dataV);
                    sin >> ch;
                    i++;
                } else {
                    cout << "测试文件数据格式不正确" << endl;
                    return false;
                }
            }
            testDataSet.push_back(Data(feature, 0));
        }
    }

    infile.close();
    return true;
}

bool loadAnswerData(string awFile, vector<int> &awVec)
{
    ifstream infile(awFile.c_str());
    if (!infile) {
        cout << "打开答案文件失败" << endl;
        exit(0);
    }

    while (infile) {
        string line;
        int aw;
        getline(infile, line);
        if (line.size() > 0) {
            stringstream sin(line);
            sin >> aw;
            awVec.push_back(aw);
        }
    }

    infile.close();
    return true;
}

int LR::storePredict(vector<int> &predict)
{
    string line;
    int i;

    ofstream fout(predictOutFile.c_str());
    if (!fout.is_open()) {
        cout << "打开预测结果文件失败" << endl;
    }
    for (i = 0; i < predict.size(); i++) {
        fout << predict[i] << endl;
    }
    fout.close();
    return 0;
}

int main(int argc, char *argv[])
{
    vector<int> answerVec;
    vector<int> predictVec;
    int correctCount;
    double accurate;
    string trainFile = "./data/train_data.txt";
    string testFile = "./data/test_data.txt";
    string predictFile = "./projects/student/result.txt";

    string answerFile = "./projects/student/answer.txt";

    LR logist(trainFile, testFile, predictFile);
/**
    //cout << "ready to train model" << endl;
    logist.train();

    //cout << "training ends, ready to store the model" << endl;
    logist.storeModel();

#ifdef TEST
    //cout << "ready to load answer data" << endl;
    loadAnswerData(answerFile, answerVec);
#endif

    //cout << "let's have a prediction test" << endl;
    logist.predict();

#ifdef TEST
    loadAnswerData(predictFile, predictVec);
    cout << "test data set size is " << predictVec.size() << endl;
    correctCount = 0;
    for (int j = 0; j < predictVec.size(); j++) {
        if (j < answerVec.size()) {
            if (answerVec[j] == predictVec[j]) {
                correctCount++;
            }
        } else {
            cout << "answer size less than the real predicted value" << endl;
        }
    }

    accurate = ((double)correctCount) / answerVec.size();
    cout << "the prediction accuracy is " << accurate << endl;
#endif
**/
    return 0;
}

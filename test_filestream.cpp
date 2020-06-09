#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

int main()
{
    std::string input_file = "./data/my_data.txt";
    std::ifstream infile(input_file.c_str());
    std::string line;

    std::getline(infile, line);

    std::stringstream sin(line);

    char ch;
    double datav;
    int i;
    std::vector<double> feature;
    i = 0;

    std::cout << int(',') << '\n';
    while(sin)
    {
        char c = sin.peek();
        sin >> datav;
        std::cout << datav << " ";
        sin >> ch;
    }
    infile.close();
    return 0;
}
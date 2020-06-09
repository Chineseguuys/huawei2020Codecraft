#include <iostream>

using namespace std;


class printer
{
public:
	virtual void printster() = 0;
};


class printerI : public printer
{
public:
	virtual void printster();

};


void printerI::printster()
{
	std::cout << "str" <<std::endl;
}


int main(int argc, char** argv)
{
	printerI* ptr = new printerI;
	delete ptr;
	return 0;
}
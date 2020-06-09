#include <stdio.h>
#include <stdlib.h>


int main(int argc, char** argv)
{
	char str[] = {'0','.','1','2','3','\0','0','.','3','4','\0'};
	double dataV;
	sprintf(str, "%.3f", dataV);
	printf("%f\n", dataV);

	return 0;
}
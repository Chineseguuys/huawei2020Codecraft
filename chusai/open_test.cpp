#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>



// 使用 mmap 需要的头文件
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


int main(int argc, char** argv)
{
	const char write_file_name[] = "a_file.txt";
	int fd = open(write_file_name, O_RDWR | O_CREAT, 0666);
	char buf[32];
	int n = sprintf(buf, "%d,", 123456);
	printf("%d\n", n);
	write(fd, buf, n);
	write(fd, "\n", 1);
	write(fd, buf, n);

	close(fd);
	return 0;
}
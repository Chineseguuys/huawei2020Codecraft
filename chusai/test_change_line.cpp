#include <iostream>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <unistd.h>


// 使用 mmap 需要的头文件
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>





int main(int argc, char** argv)
{
	const char file_name[] = "result.txt";
	struct stat st;

	int fd = open(file_name, O_RDONLY);
	fstat(fd, &st);
	int fd_len = st.st_size;
	char* file_content_buf =  (char*)mmap(NULL, fd_len, PROT_READ, MAP_PRIVATE, fd, 0);

	for (int index = 0; index < 100; ++index)
	{
		if (file_content_buf[index] == '\r')
			std::cout << index << " ";
	}
	std::cout << "\r\n";

	munmap(file_content_buf, fd_len);
	close(fd);
	return 0;
}	
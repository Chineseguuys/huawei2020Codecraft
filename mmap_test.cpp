#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>

int main()
{
	int fd;
	void* start;
	fd = open("./data/train_data.txt", O_RDONLY);
	struct stat st;
	int r = fstat(fd, &st);
	int len = st.st_size;
	printf("%s = %d\n","file len ", len );
	start = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);

	munmap(start, len);
	return 0;
}
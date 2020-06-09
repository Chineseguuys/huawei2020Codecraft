/**
 * 可以使用图来存储这些节点的信息
 * 然后采用深度遍历的方式获得这些环的情况
 * 注意这里的环，会出现环中环的情况。
 * 
 */


/**
 * 这个版本在正确性上已经有了保证
 * 后面的版本应当着重提高效率
 */


/**	main_2_1.cpp
	这里希望可以解决的是 vector 的反复的 realloc 的问题
 */


/** main_2_2.cpp
	当前的版本已经解决了 "vector 的反复 realloc 的问题"
	当前导致程序耗时的主要的因素就是 unordered_map<unsigned, graph_node*>::operator[] 耗时的问题
 */

/**
	circuit() 采用的是递归的方式进行处理的,递归的最大深度是固定的,也就是 7
	这里采用的递归,是否拖累了程序的运行过程
 */
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

#ifndef _VECTOR_
#include <vector>
#endif

using namespace std;


//#define _TEST_			// 用于测试使用
#define _TIME_ANA_
#define MAX_CIRCLE_LEN	7			// 环的长度的上限
#define MIN_CIRCLE_LEN  3			// 环的长度的下限
#define _OFF_LINE_
//#define _ON_LINE_



#ifdef _TIME_ANA_
#include <time.h>
#endif

enum visit_state 
{
	NOT_VISITED = 1, 	// 还没有被访问
	IS_VISITING = 2, 	// 正在被访问
	HAS_VISITED = 3     // 已经没有必要再进行访问
};



struct graph_node
{
	int ID;
	int index;
	visit_state state;
	graph_node(int __id) : ID(__id), index(0), state(NOT_VISITED) {}
	std::vector<graph_node*> next_node;		// 后继节点不再使用 ID 进行存储,而是直接采用指向每一个节点的指针
};




struct One_Cycle	// 用来记录每一个路径
{
	std::vector<int> cycle_path;
	int cycle_len;


	One_Cycle() : cycle_path(MAX_CIRCLE_LEN), cycle_len(0) {}

	One_Cycle(const One_Cycle& __right_item)
	{
		cycle_path = __right_item.cycle_path;
		cycle_len = __right_item.cycle_len;
	}

	One_Cycle(One_Cycle&& __right_item)
	{
		this->cycle_path = std::move(__right_item.cycle_path);
		this->cycle_len = __right_item.cycle_len;
	}

	bool operator <= (const One_Cycle& __right_value)
	{
		if(this->cycle_len < __right_value.cycle_len)
			return true;
		if (this->cycle_len > __right_value.cycle_len)
			return false;
		int index = 0;
		for (; index < cycle_len; ++index)
		{
			if (this->cycle_path[index] < __right_value.cycle_path[index])
				return true;
		}
		return false;
	}

	One_Cycle& operator = (One_Cycle&& __right_item)
	{
		this->cycle_path = std::move(__right_item.cycle_path);
		this->cycle_len = __right_item.cycle_len;
		return *this;
	}
};


void merge_elements(std::vector<One_Cycle>& array, std::vector<One_Cycle>& temp_array, int leftPos, int rightPos, int rightEnd);


/**
	这个排序的过程是有效的
	可以得到正确的排序的结果
 */
void merge_sort(std::vector<One_Cycle>& array, std::vector<One_Cycle>& temp_array, int left, int right)
{
	if(left < right)
	{
		int center = (left + right) / 2;
		merge_sort(array, temp_array, left, center);
		merge_sort(array, temp_array, center + 1, right);
		merge_elements(array, temp_array, left, center + 1, right);
	}
}


void merge_sort(std::vector<One_Cycle>& __all_paths)
// 对所有的路径进行排序
// 需要实现自定义的比较方法
{
	int path_nums = __all_paths.size();
	std::vector<One_Cycle> temp_array(path_nums);
	merge_sort(__all_paths, temp_array, 0, path_nums - 1);
}




void merge_elements(std::vector<One_Cycle>& array, std::vector<One_Cycle>& temp_array, int leftPos, int rightPos, int rightEnd)
{
	int leftEnd = rightPos - 1;
	int tempPos = leftPos;
	int numElements = rightEnd - leftPos + 1;

	while(leftPos <= leftEnd && rightPos <= rightEnd)
	{
		if(array[leftPos] <= array[rightPos])
			temp_array[tempPos++] = std::move(array[leftPos++]);
		else
			temp_array[tempPos++] = std::move(array[rightPos++]);
	}

	while(leftPos <= leftEnd)
		temp_array[tempPos++] = std::move(array[leftPos++]);
	while(rightPos <= rightEnd)
		temp_array[tempPos++] = std::move(array[rightPos++]);

	// 将 temp_array 中的元素拷贝回 array
	for (int i = 0; i < numElements; ++i, --rightEnd)
		array[rightEnd] = std::move(temp_array[rightEnd]);
}



/**
	使用图来存储这些路径信息
	还是使用邻接表来存储这些信息
 */

void create_graph(	char* __data_buf, 
					map<unsigned, graph_node*>& __id_nodePtr_refl, 
					unordered_map<unsigned, graph_node*>& __fast_id_nodePtr_refl,
					const int& __buf_len)
{
	int index  = 0;
	int from, to;
	graph_node* from_node, *to_node;


	for(index = 0; index < __buf_len;)
	{

		from = atoi(__data_buf + index);
		while(__data_buf[++index] != ',');
		++index;
		to = atoi(__data_buf + index);
		while(__data_buf[++index] != '\n');
		++index;

		if(__fast_id_nodePtr_refl.find(from) == __fast_id_nodePtr_refl.end())
		{
			from_node = new graph_node(from);
			__id_nodePtr_refl.insert({from, from_node});
			__fast_id_nodePtr_refl.insert({from, from_node});
		}
		else
		{
			from_node = __fast_id_nodePtr_refl[from];
		}

		if(__fast_id_nodePtr_refl.find(to) == __fast_id_nodePtr_refl.end())
		{
			to_node = new graph_node(to);
			__id_nodePtr_refl.insert({to, to_node});
			__fast_id_nodePtr_refl.insert({to, to_node});
		}

		else
		{
			to_node = __fast_id_nodePtr_refl[to];
		}

		from_node->next_node.push_back(to_node);
	}
}



/**
	递归程序转换为非递归程序
 */
void circuit(	graph_node* __root_node,
				graph_node* (&__node_stack)[7],
				int (&__node_iter_pos)[7],
				std::vector<One_Cycle>& __all_paths		
)
{
	int top = -1;		// top 指向当前的栈顶
	int root_ID = __root_node->ID;
	graph_node* top_node;
	__node_stack[++top] = __root_node;
	__root_node->state = IS_VISITING;
	__node_iter_pos[top] = 0;
	int index;

	LOOP:while(top != -1)
	{
		top_node = __node_stack[top];
		std::vector<graph_node*>& next_node = top_node->next_node;
		for (index = __node_iter_pos[top]; index < next_node.size(); ++ index)
		{
			graph_node* chile_node = next_node[index];

			if(chile_node->state == IS_VISITING && chile_node->ID == root_ID && top >= 2)
			{
				//std::cout << "find a cycle\n";
				One_Cycle temp;
				for(int index = 0; index <= top; ++index)
					temp.cycle_path[index] = __node_stack[index]->ID;
				temp.cycle_len = top + 1;
				__all_paths.push_back(std::move(temp));
				continue;
			}
			if(chile_node->state == NOT_VISITED && top < 6)
			{
				__node_iter_pos[top] = ++index;
				__node_stack[++top] = chile_node;
				__node_iter_pos[top] = 0;
				chile_node->state = IS_VISITING;
				goto LOOP;
			}
		}

		top_node->state = NOT_VISITED;
		--top;

	}
}






/**
 * 找到所有的环
 */
void find_circle(map<unsigned int, graph_node*>& __id_nodePtr_refl,
			unordered_map<unsigned, graph_node*>& __fast_id_nodePtr_refl)
{
	std::vector<One_Cycle> all_paths;
	map<unsigned int, graph_node*>::iterator iter;
	graph_node* node_stack[7];
	int node_iter_pos[7];

	for (iter = __id_nodePtr_refl.begin(); iter != __id_nodePtr_refl.end(); ++iter)
	{
		graph_node* node = iter->second;

	#ifdef _TEST_
		std::cout << "runing for node " << node->ID << '\n';
	#endif

		circuit(node, node_stack, node_iter_pos, all_paths);
		node->state = HAS_VISITED;
	}


	// 这里已经获取了所有的环
	// 环需要进行排序,首先,环的开始的元素应该是环中最小的元素
	// 环需要按照从短到长的顺序进行处理
	// 相同长度的环,需要按照环的最小的元素进行排序,


	// 我们尝试在这里进行一次排序,

#ifdef _TIME_ANA_
	clock_t start_t, end_t;
	start_t = clock();
#endif

	merge_sort(all_paths);

#ifdef _TIME_ANA_
	end_t = clock();
	std::cout << "sorting using time " << end_t - start_t << '\n';
#endif

	// 将结果写入到文件中
#ifdef _OFF_LINE_
	const char result_file[] = "result.txt";
#endif

#ifdef _ON_LINE_
	const char result_file[] = "/projects/student/result.txt";
#endif

#ifdef _TIME_ANA_
	start_t = clock();
#endif

	int fd = open(result_file, O_RDWR | O_CREAT, 0666);
	int path_len;
	int num_str_len;
	char buffer[32];
	num_str_len = sprintf(buffer, "%ld\r\n", all_paths.size());
	write(fd, buffer, num_str_len);
	for (One_Cycle& path : all_paths)
	{
		path_len = path.cycle_len - 1;
		for(int i = 0; i < path_len; ++i)
		{
			num_str_len = sprintf(buffer, "%d,", path.cycle_path[i]);
			write(fd, buffer, num_str_len);
		}
		num_str_len = sprintf(buffer, "%d\r\n", path.cycle_path[path_len]);
		write(fd, buffer, num_str_len);
	}

	close(fd);

#ifdef _TIME_ANA_
	end_t = clock();
	std::cout << "write result to file using time " << end_t - start_t << '\n';
#endif

	//std::cout << "Have cycle " << all_paths.size() << " in the graph " << '\n';
	//std::cout << "bucket nums  " << __fast_id_nodePtr_refl.bucket_count() << '\n';
	//std::cout << "max bucket nums " << __fast_id_nodePtr_refl.max_bucket_count() << '\n';
	//std::cout << "elements nums in the unordered map " <<  __fast_id_nodePtr_refl.size() << '\n';
}






int main(int argc, char** argv)
{

#ifdef _TIME_ANA_
	clock_t start_t, end_t;
	start_t = clock();
#endif

#ifdef _OFF_LINE_
	const char file_full_path[] = "./test_data_58284.txt";
#endif

#ifdef _ON_LINE_
	const char file_full_path[] = "/data/test_data.txt";
#endif


	char* test_data_buf;
	int fd, fd_len;
	struct stat st;
	map<unsigned int, graph_node*> id_nodePtr_refl;	// 用于存储 ID->graph_node_ptr 的键值对
	unordered_map<unsigned int, graph_node*> fast_id_nodePtr_refl;   // unorderd_map 的读取速度比 map 的速度要快

	if((fd = open(file_full_path, O_RDONLY)) < 0)
		perror("open file failed");

	fstat(fd, &st);
	fd_len = st.st_size;
	test_data_buf = (char*)mmap(NULL, fd_len, PROT_READ, MAP_PRIVATE, fd, 0);

	create_graph(test_data_buf, id_nodePtr_refl, fast_id_nodePtr_refl, fd_len);

#ifdef _TEST_
	std::cout << "successful create graph\n";
#endif

	find_circle(id_nodePtr_refl, fast_id_nodePtr_refl);

	munmap(test_data_buf, fd_len);
	close(fd);
#ifdef _TIME_ANA_
	end_t = clock();
	std::cout << "using time " << end_t - start_t << '\n';
#endif
	return 0;
}

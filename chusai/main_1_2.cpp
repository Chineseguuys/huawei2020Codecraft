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


#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>

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
	std::vector<int> next_node;
};

struct One_Cycle	// 用来记录每一个路径
{
	std::vector<int> cycle_path;
	int cycle_len;
};



/**
	使用图来存储这些路径信息
	还是使用邻接表来存储这些信息
 */

void create_graph(char* __data_buf, unordered_map<unsigned, graph_node*>& __id_nodePtr_refl, const int& __buf_len)
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

		if(__id_nodePtr_refl.find(from) == __id_nodePtr_refl.end())
		{
			from_node = new graph_node(from);
			__id_nodePtr_refl.insert({from, from_node});
		}
		else
		{
			from_node = __id_nodePtr_refl[from];
		}

		if(__id_nodePtr_refl.find(to) == __id_nodePtr_refl.end())
		{
			to_node = new graph_node(to);
			__id_nodePtr_refl.insert({to, to_node});
		}
		else
		{
			to_node = __id_nodePtr_refl[to];
		}

		from_node->next_node.push_back(to);
	}
}



void circuit(graph_node* __node, unordered_map<unsigned int, graph_node*>& __id_nodePtr_refl,
		std::vector<One_Cycle>& __all_paths,
		std::vector<int> __node_queue, const int& __head_id)
{
	if(__node->state == HAS_VISITED)
		return;

	if(__node->state == NOT_VISITED)
	{
		if(__node_queue.size() >= 7)
			return;
		__node->state = IS_VISITING;
		__node_queue.push_back(__node->ID);

	#ifdef _TEST_
		for (int& item : __node_queue)
			std::cout << item << "|";
		std::cout << '\n';
	#endif

		std::vector<int>& next_node = __node->next_node;
		for(int& child : next_node)
		{
		#ifdef _TEST_
			std::cout << "from " << __node->ID << " to " << child << '\n';
		#endif

			circuit(__id_nodePtr_refl[child], __id_nodePtr_refl, __all_paths, __node_queue, __head_id);

		#ifdef _TEST_
			std::cout << "return " << __node->ID << " from " << child << '\n';
		#endif
		}
	}
	else if(__node->state == IS_VISITING && __node->ID == __head_id)
	{
		if(__node_queue.size() >= 3)
		{
			One_Cycle temp;
			temp.cycle_path = __node_queue;
			temp.cycle_len = __node_queue.size();
			__all_paths.push_back(temp);
		}

	#ifdef _TEST_
		std::cout << "put a path" << '\n';
	#endif
		return;
	}
	else
		return;

	/**
		其他的情况都会递归返回
	 */

	__node->state = NOT_VISITED;
	__node_queue.pop_back();

#ifdef _TEST_
	for (int& item : __node_queue)
		std::cout << item << ".";
	std::cout << "\n";
#endif
}






/**
 * 找到所有的环
 */
void find_circle(unordered_map<unsigned int, graph_node*>& __id_nodePtr_refl)
{
	std::vector<One_Cycle> all_paths;
	std::vector<int> index_store;
	unordered_map<unsigned int, graph_node*>::iterator iter;

	for (iter = __id_nodePtr_refl.begin(); iter != __id_nodePtr_refl.end(); ++iter)
	{
		graph_node* node = iter->second;

	#ifdef _TEST_
		std::cout << "runing for node " << node->ID << '\n';
	#endif

		circuit(node, __id_nodePtr_refl, all_paths, index_store, node->ID);
		node->state = HAS_VISITED;
	}


	// 这里已经获取了所有的环
	// 环需要进行排序,首先,环的开始的元素应该是环中最小的元素
	// 环需要按照从短到长的顺序进行处理
	// 相同长度的环,需要按照环的最小的元素进行排序,

	for (One_Cycle& item : all_paths)
	{
		for (int& id : item.cycle_path)
			std::cout << id << " --> ";
		std::cout << '\n';
	}

	std::cout << "Have cycle " << all_paths.size() << " in the graph " << '\n';
}






int main(int argc, char** argv)
{
#ifdef _TIME_ANA_
	clock_t start_t, end_t;
	start_t = clock();
#endif

	const char file_full_path[] = "./test_data_26571.txt";
	char* test_data_buf;
	int fd, fd_len;
	struct stat st;
	unordered_map<unsigned int, graph_node*> id_nodePtr_refl;	// 用于存储 ID->graph_node_ptr 的键值对

	fd = open(file_full_path, O_RDONLY);
	fstat(fd, &st);
	fd_len = st.st_size;
	test_data_buf = (char*)mmap(NULL, fd_len, PROT_READ, MAP_PRIVATE, fd, 0);

	create_graph(test_data_buf, id_nodePtr_refl, fd_len);

#ifdef _TEST_
	std::cout << "successful create graph\n";
#endif

	find_circle(id_nodePtr_refl);

	munmap(test_data_buf, fd_len);

#ifdef _TIME_ANA_
	end_t = clock();
	std::cout << "using time " << end_t - start_t << '\n';
#endif
	return 0;
}

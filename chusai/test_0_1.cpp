/**
 * 可以使用图来存储这些节点的信息
 * 然后采用深度遍历的方式获得这些环的情况
 * 注意这里的环，会出现环中环的情况。
 * 
 */
#define __CESHI_VERSION__


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

using namespace std;

#include "graph.h"



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


void unblock(unordered_map<unsigned int, graph_node*>& __id_nodePtr_refl, int __id)
{
	graph_node* __node = __id_nodePtr_refl[__id];
	__node->blocked = false;
	std::cout << "unblock " << __node->ID << '\n';
	while(!__node->B.empty())
	{
		int temp_id = __node->B.back();
		__node->B.pop_back();
		if(__id_nodePtr_refl[temp_id]->blocked == true)
			unblock(__id_nodePtr_refl, temp_id);
	}
}

bool find_id(std::vector<int>& __next_node, int __id)
{
	for (auto item : __next_node)
		if(__id == item)
			return true;
	return false;
}


bool circuit(graph_node* __node, 
			std::vector<int>& __node_queue, 
			std::vector<One_Cycle>& __all_paths,
			unordered_map<unsigned int, graph_node*>& __id_nodePtr_refl, 
			const int& root_vert)
{
	std::cout << __node->ID << " : ";
	for(int child : __node->next_node)
	{
		std::cout << child << " ";
	}
	std::cout << '\n';

	bool find_cycle = false;
	__node_queue.push_back(__node->ID);
	__node->blocked = true;
	std::cout << "block " << __node->ID << '\n';
	for(int child : __node->next_node)
	{
		if(child == root_vert)
		{
			One_Cycle temp;
			temp.cycle_path = __node_queue;
			temp.cycle_len = __node_queue.size();
			__all_paths.push_back(temp);
			find_cycle = true;
		}
		else if(__id_nodePtr_refl[child]->blocked == false)
		{
			find_cycle = circuit(__id_nodePtr_refl[child], __node_queue, __all_paths, __id_nodePtr_refl, root_vert);
		}
	}
	if(find_cycle)
		unblock(__id_nodePtr_refl, __node->ID);
	else 
	{
		for (int child : __node->next_node)
		{
			if(find_id(__id_nodePtr_refl[child]->B, __node->ID) == false)
			{
				__id_nodePtr_refl[child]->B.push_back(__node->ID);
				std::cout << child << "push B " << __node->ID << '\n';
			}
		}
	}

	__node_queue.pop_back();
	return find_cycle;
}




void find_cycle(unordered_map<unsigned int, graph_node*>& __id_nodePtr_refl)
{
	std::vector<One_Cycle> all_paths;
	std::vector<int> node_queue;

	for(int i = 1; i <= 8; ++i)
	{
		std::cout << i << "become the main V" << '\n';
		circuit(__id_nodePtr_refl[i], node_queue, all_paths, __id_nodePtr_refl, i);
	}

	std::cout << "have path " << all_paths.size() << '\n';

	for(One_Cycle& item : all_paths)
	{
		for (auto iter : item.cycle_path)
			std::cout << iter << " ";
		std::cout << '\n';
	}
}



int main(int argc, char** argv)
{
	const char file_full_path[] = "./my_test_data.txt";
	char* test_data_buf;
	int fd, fd_len;
	struct stat st;
	unordered_map<unsigned int, graph_node*> id_nodePtr_refl;	// 用于存储 ID->graph_node_ptr 的键值对

	fd = open(file_full_path, O_RDONLY);
	fstat(fd, &st);
	fd_len = st.st_size;
	test_data_buf = (char*)mmap(NULL, fd_len, PROT_READ, MAP_PRIVATE, fd, 0);

	create_graph(test_data_buf, id_nodePtr_refl, fd_len);

	find_cycle(id_nodePtr_refl);

	munmap(test_data_buf, fd_len);
	return 0;
}

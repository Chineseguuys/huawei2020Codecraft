/**
 * 可以使用图来存储这些节点的信息
 * 然后采用深度遍历的方式获得这些环的情况
 * 注意这里的环，会出现环中环的情况。
 * 
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

using namespace std;

#include "graph.h"


#define _TEST_


#define MAX_CIRCLE_LEN	7
#define MIN_CIRCLE_LEN  3


struct One_Cycle	// 用来记录每一个路径
{
	int cycle_path[7];
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


void dfs_circle(graph_node* __node,
				graph_node* __parent_node, 
				std::vector<One_Cycle>& __all_cycle_paths, 
				std::vector<int>& __index_store,
				unordered_map<unsigned int, graph_node*>& __id_nodePtr_refl)
{
	if(__node->state == HAS_VISITED)
	{
		std::cout << __node->ID << " is HAS_VISITED" << '\n';
		__node->state = NOT_VISITED;
	}
	if(__node->state == IS_VISITING)
	{
		// 在遍历的过程中遇到了前面链中的节点
		int path_len = 1; 
		int store_len = __index_store.size();
		std::cout << "store_len = " << store_len <<" current_node ID = "<< __node->ID << '\n';
		for(auto item : __index_store)
			std::cout << item << ' ';
		std::cout << '\n';
		for(int i = store_len - 1; i >= 0 && __index_store[i] != __node->ID; --i,++path_len)
		{
			std::cout << __index_store[i] << " ";
		}
		std::cout << "\n";
		if(path_len < 3 || path_len > 7)
			return;
		One_Cycle temp;
		std::vector<int>::iterator iter;
		int i = path_len - 1;
		for(iter = __index_store.end() - 1; i >= 0; --i, --iter)
		{
			temp.cycle_path[i] = *iter;
		}
		temp.cycle_len = path_len;
		std::cout << "path_len = " << path_len << '\n';
		__all_cycle_paths.push_back(temp);
		return;
	}
	else if(__node->state == NOT_VISITED)
	{
		__node->state = IS_VISITING;
		std::cout << "node "<< __node->ID  << " has been seted IS_VISITING" << '\n';
		__index_store.push_back(__node->ID);
		std::vector<int>& next_node = __node->next_node;
		for(int child : next_node)
		{
			std::cout << __node->ID << " has child " << child << '\n';
			dfs_circle(__id_nodePtr_refl[child], __node, __all_cycle_paths, __index_store, __id_nodePtr_refl);
		}
		__node->state = HAS_VISITED;
		std::cout << "node " << __node->ID << " has been seted HAS_VISITED" << '\n';
		__index_store.pop_back();
	}
}




void find_circle(unordered_map<unsigned int, graph_node*>& __id_nodePtr_refl)
{
	std::vector<One_Cycle> all_paths;
	std::vector<int> index_store;
	unordered_map<unsigned int, graph_node*>::iterator iter;
	while(true)
	{
		for(iter = __id_nodePtr_refl.begin; iter != __id_nodePtr_refl.end(); ++iter)
		{
			if(iter->second->state == NOT_VISITED)
				break;
		}
		if(iter == __id_nodePtr_refl.end())
			break;
		dfs_circle(iter->second, nullptr, all_paths, index_store, __id_nodePtr_refl);
	}



#ifdef _TEST_
	std::cout << "have paths " << all_paths.size() << '\n';
	for(auto path_iter = all_paths.begin(); path_iter != all_paths.end(); ++path_iter)
	{
		std::cout << "len = " << path_iter->cycle_len<< "  ";
		for(int i = 0; i < path_iter->cycle_len; ++i)
			std::cout << path_iter->cycle_path[i] << " ";
		std::cout << "\n";
	}

#endif


}



int main(int argc, char** argv)
{
	const char file_full_path[] = "./short_test_data.txt";
	char* test_data_buf;
	int fd, fd_len;
	struct stat st;
	unordered_map<unsigned int, graph_node*> id_nodePtr_refl;	// 用于存储 ID->graph_node_ptr 的键值对

	fd = open(file_full_path, O_RDONLY);
	fstat(fd, &st);
	fd_len = st.st_size;
	test_data_buf = (char*)mmap(NULL, fd_len, PROT_READ, MAP_PRIVATE, fd, 0);

	create_graph(test_data_buf, id_nodePtr_refl, fd_len);

	find_circle(id_nodePtr_refl);
/*
	std::cout << "bucket_count = " <<id_nodePtr_refl.bucket_count() << "\n";
	for(auto iter = id_nodePtr_refl.begin(); iter != id_nodePtr_refl.end(); ++iter)
		std::cout << iter->first << '\n';
*/
	munmap(test_data_buf, fd_len);
	return 0;
}

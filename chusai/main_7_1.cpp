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

/**
	当前的版本完成了 最大连通分量的求解和 一些没有用的边的剪枝
	查找 环的算法还没有更新
	后面的版本,第一是尝试实现 johnson's algorithm ,其次是引入多线程的机制, 查看具体的效率
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
	HAS_VISITED = 3		// 已经访问结束,不需要再进行访问了
};

struct graph_node
{
	int ID;
	int index;
	visit_state state;
	graph_node(int __id) : ID(__id), index(0), state(NOT_VISITED) {}
	std::vector<graph_node*> childs;		// 后继节点不再使用 ID 进行存储,而是直接采用指向每一个节点的指针
	std::vector<graph_node*> inverse_childs;
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



void second_DFS(graph_node* __node, 
		unordered_map<unsigned, graph_node*>& has_visited,
		unordered_map<unsigned, graph_node*>& __a_SCC)
{
	has_visited.insert({__node->ID, __node});
	__a_SCC.insert({__node->ID, __node});
	for (graph_node* child_node : __node->inverse_childs)
	{
		if(has_visited.find(child_node->ID) != has_visited.end())
			continue;
		second_DFS(child_node, has_visited, __a_SCC);
	}
}



void first_DFS(graph_node* __node, unordered_map<unsigned, graph_node*>& __id_nodePtr_refl,
		unordered_map<unsigned, graph_node*>& has_visited,
		int* __node_stack,
		int& top)
{
	has_visited.insert({__node->ID, __node});
	__id_nodePtr_refl.erase(__node->ID);

	for(graph_node* child_node : __node->childs)
	{
		if(has_visited.find(child_node->ID) != has_visited.end())
			continue;
		first_DFS(child_node, __id_nodePtr_refl, has_visited, __node_stack, top);
	}

	__node_stack[++top] = __node->ID;	// 当一个节点访问完其所有的儿子节点之后,我们将其 push 进入我们的栈中
}





/**
	查找所有的强连通分量
	在得到了所有的强连通分量之后

	需要一个容器存储已经被访问的节点
	需要一个容器存储还没有被访问的节点
 */
void stronge_conn_comp(unordered_map<unsigned, graph_node*>& __id_nodePtr_refl,
		std::vector<unordered_map<unsigned, graph_node*> >& __all_SCCs)
{
	const int ELEMENTS_NUMS = __id_nodePtr_refl.size();
	int node_stack[ELEMENTS_NUMS];
	int top = -1;		// 初始化栈顶

	unordered_map<unsigned, graph_node*> has_visited;			// 存储已经访问过的节点
	has_visited.reserve(ELEMENTS_NUMS);

	// 进行第一次深度遍历,第一次深度遍历是对 正向图的遍历的过程;
	while(!__id_nodePtr_refl.empty())
	{
		graph_node* node = __id_nodePtr_refl.begin()->second;
		first_DFS(node, __id_nodePtr_refl, has_visited, node_stack, top);
	}

	// 下面对逆图进行一次深度遍历,在遍历的过程中就可以得到所有的强连通分量
	// 从 node_stack 的顶端开始进行 DFS 找到所有的强连通分量并进行保存
	__id_nodePtr_refl.swap(has_visited);
	unordered_map<unsigned, graph_node*> a_SCC;

	while(top != -1)
	{
		int node_ID = node_stack[top--];
		if(has_visited.find(node_ID) != has_visited.end())
			continue;
		graph_node* node = __id_nodePtr_refl[node_ID];

		second_DFS(node, has_visited, a_SCC);
		__all_SCCs.push_back(std::move(a_SCC));
	}

/*
	for (unordered_map<unsigned, graph_node*>& scc : __all_SCCs)
	{
		unordered_map<unsigned, graph_node*>::iterator iter = scc.begin();
		for (; iter != scc.end(); ++iter)
			std::cout << iter->second->ID<< "  ";
		std::cout << '\n';
	}
*/

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

		from_node->childs.push_back(to_node);
		to_node->inverse_childs.push_back(from_node);		// 同时构建一个反向的图,方便查找强连通分量
	}
}



/**
	考虑: 是否真的需要进行剪枝的操作
	我希望可以对我们的图进行  剪枝操作,删除不同的强连通分量之间的桥
 */
void cut_bridge_SSC(std::vector<unordered_map<unsigned, graph_node*> >& __all_SCCs)
{
	graph_node* node = nullptr;
	for (unordered_map<unsigned, graph_node*>& scc : __all_SCCs)
	{
		unordered_map<unsigned, graph_node*>::iterator iter = scc.begin();
		for (; iter != scc.end(); ++iter)
		{
			node = iter->second;
			for (graph_node*& child_node : node->childs)
				if (scc.find(child_node->ID) == scc.end())		// 儿子不在当前的强连通分量里面,那么就将其设置为 nullptr
				{
					//std::cout << "from "<< node->ID << " to " << child_node->ID << '\n';
					child_node = nullptr;
				}
		}
	}
}


void circuit(graph_node* __node,
		std::vector<One_Cycle>& __all_paths,
		std::vector<int>& __node_stack,
		const int& __head_node_id)	// 记录最小元素所在的下标的位置,方便我们在排序的时候进行排序
{
	__node_stack.push_back(__node->ID);
	__node->state = IS_VISITING;


	for(graph_node* child_node : __node->childs)
	{
		if (child_node == nullptr || child_node->state == HAS_VISITED)
		{
			continue;
		}
		if (child_node->state == IS_VISITING && child_node->ID == __head_node_id && __node_stack.size() >= MIN_CIRCLE_LEN)
		{
			// 我们找到了一个长度大于 3 的环
			One_Cycle temp;
			temp.cycle_len = __node_stack.size();
			for (int i = 0; i < temp.cycle_len; ++i)
				temp.cycle_path[i] = __node_stack[i];
			__all_paths.push_back(std::move(temp));
			continue;
		}
		if (child_node->state == NOT_VISITED && __node_stack.size() < MAX_CIRCLE_LEN)
		{
			circuit(child_node, __all_paths, __node_stack, __head_node_id);
		}
	}

	__node_stack.pop_back();
	__node->state = NOT_VISITED;
}



void get_circles(std::vector<unordered_map<unsigned, graph_node*> >& __all_SCCs)
{
	unordered_map<unsigned, graph_node*>::iterator iter;
	graph_node* node = nullptr;
	std::vector<One_Cycle> all_paths;		// 这里存储所有的 环
	std::vector<int> node_stack;
	int min_index = -1;
	int min_value = -1; 	// 所有的节点的 ID 都是非负值

	for (unordered_map<unsigned, graph_node*>& scc : __all_SCCs)
	{
		if (scc.size() < 3)			// 如果强连通分量中的元素个数小于 3 个,那么肯定是不可能副存在我们需要的环了
			continue;
		for (iter = scc.begin(); iter != scc.end(); ++iter)		// 在每一个强连通分量的内部求环
		{
			node = iter->second;
			circuit(node, all_paths, node_stack, node->ID);
			node->state = HAS_VISITED;
		}
	}

	std::cout << "we have cycles with number " << all_paths.size() << '\n';
}




int main(int argc, char** argv)
{

#ifdef _TIME_ANA_
	clock_t start_t, end_t;
	start_t = clock();
#endif

#ifdef _OFF_LINE_
	const char file_full_path[] = "./test_data_2755223.txt";
#endif

#ifdef _ON_LINE_
	const char file_full_path[] = "/data/test_data.txt";
#endif


	char* test_data_buf;
	int fd, fd_len;
	struct stat st;
	map<unsigned int, graph_node*> id_nodePtr_refl;	// 用于存储 ID->graph_node_ptr 的键值对
	unordered_map<unsigned int, graph_node*> fast_id_nodePtr_refl;   // unordered_map 的读取速度比 map 的速度要快
	std::vector<unordered_map<unsigned, graph_node*> > all_SCCs;

	if((fd = open(file_full_path, O_RDONLY)) < 0)
		perror("open file failed");

	fstat(fd, &st);
	fd_len = st.st_size;
	test_data_buf = (char*)mmap(NULL, fd_len, PROT_READ, MAP_PRIVATE, fd, 0);

#ifdef _TIME_ANA_
	start_t = clock();
#endif

	create_graph(test_data_buf, id_nodePtr_refl, fast_id_nodePtr_refl, fd_len);

#ifdef _TIME_ANA_
	end_t = clock();
	std::cout << "create graph using time " << end_t - start_t << '\n';
#endif


#ifdef _TIME_ANA_
	start_t = clock();
#endif

	stronge_conn_comp(fast_id_nodePtr_refl, all_SCCs);	// 找全部的强连通分量

#ifdef _TIME_ANA_
	end_t = clock();
	std::cout << "find strong conection comp using time " << end_t - start_t << '\n';
	std::cout << "have SCC " << all_SCCs.size() << '\n';
#endif

#ifdef _TIME_ANA_
	start_t = clock();
#endif

	cut_bridge_SSC(all_SCCs);

#ifdef _TIME_ANA_
	end_t = clock();
	std::cout << "cut bridge SSC using time " <<end_t - start_t << '\n';
#endif

#ifdef _TIME_ANA_
	start_t = clock();
#endif

	get_circles(all_SCCs);

#ifdef _TIME_ANA_
	end_t = clock();
	std::cout << "get circles using time " << end_t - start_t << '\n';
#endif

#ifdef _TEST_
	std::cout << "successful create graph\n";
#endif

	munmap(test_data_buf, fd_len);
	close(fd);


	return 0;
}

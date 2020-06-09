#ifndef _GRAPH_
#define _GRAPH_

#include <vector>

enum visit_state 
{
	NOT_VISITED = 1, 	// 还没有被访问
	IS_VISITING = 2, 	// 正在被访问
	HAS_VISITED = 3     // 已经没有必要再进行访问
};


#ifndef __CESHI_VERSION__

struct graph_node
{
	int ID;
	int index;
	visit_state state;
	graph_node(int __id) : ID(__id), index(0), state(NOT_VISITED) {}
	std::vector<int> next_node;
};

#else

struct graph_node
{
	int ID;
	int index;
	std::vector<int> next_node;
	std::vector<int> B;
	bool blocked;
	graph_node(int __id) : ID(__id), blocked(false) {}
};

#endif



#endif
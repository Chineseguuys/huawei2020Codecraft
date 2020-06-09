#include <iostream>
#include <vector>


using namespace std;




int main()
{
	vector<int> res {1,2,3,4,5};
	vector<int>::iterator iter;
	int i;
	for(i = 0, iter = res.end() - 1; iter >= res.begin();--iter)
		cout << *iter << '\n';
	return 0;
}
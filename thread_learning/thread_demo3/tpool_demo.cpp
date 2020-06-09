#include "tpool.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <vector>
#include <algorithm>

std::vector<int> vec(50);

void* fun(void* args)
{
   int thread = (int)args;
   printf("running the thread of %d\n",thread);
   vec[thread] = 12 * thread;
   return NULL;
}

int main(int argc, char* args[])
{
   tpool_t* pool = NULL;
   if(0 != create_tpool(&pool,5)){
        printf("create_tpool failed!\n");
        return -1;
   }

   for(int i = 0; i < 50; i++){
        add_task_2_tpool(pool,fun,(void*)i);
   }
   sleep(2);

   std::for_each(vec.begin(), vec.end(), 
            [](int& value)->void { printf("%d ", value);});

   destroy_tpool(pool);
return 0;
}

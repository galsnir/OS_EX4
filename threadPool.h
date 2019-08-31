#ifndef __THREAD_POOL__
#define __THREAD_POOL__
#include "pthread.h"
#include "osqueue.h"

typedef struct thread_pool
{
    int numOfThreads;
    pthread_t* threadsArray;
    OSQueue* tasksQueue;
    pthread_mutex_t Lock;
    pthread_cond_t update;
    int status;
}ThreadPool;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif

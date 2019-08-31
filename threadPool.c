#include "threadPool.h"
#include <fcntl.h>
#include "threadPool.h"
#include "stdlib.h"
#define STDERR_FD 2
#define SYS_CALL_FAILURE 10

typedef struct task
{
    void (*func)(void *params);
    void* params;
}Task;

void* runTask(void* args);

ThreadPool* tpCreate(int numOfThreads)
{
    ThreadPool* tp = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (tp == NULL)
    {
        return NULL;
    }

    tp->numOfThreads = numOfThreads;
    tp->threadsArray = (pthread_t*)malloc(sizeof(pthread_t) * tp->numOfThreads);
    if (tp->threadsArray == NULL) {
        return NULL;
    }

    tp->tasksQueue = osCreateQueue();
    tp->status = 0;

    if (pthread_mutex_init(&(tp->Lock), NULL) != 0 || pthread_cond_init(&(tp->update), NULL) != 0)
    {
        tpDestroy(tp, 0);
        return NULL;
    }

    int i;
    for (i = 0; i < tp->numOfThreads; i++)
    {
        pthread_create(&(tp->threadsArray[i]), NULL, runTask, (void *)tp);
    }

    return tp;
}

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks)
{
    if (threadPool == NULL)
    {
        return;
    }

    pthread_mutex_lock(&threadPool->Lock);
    // first time enter to tpDestory with valid thread pool
    if (threadPool->status == 0) {
        threadPool->status = 1;
        // make sure tpDestroy will ne called only once for thr thread pool
    }

    else
    {
        return;
    }
    pthread_mutex_unlock(&threadPool->Lock);


    if (shouldWaitForTasks == 0)
    {
        threadPool->status = 2;
    }

    pthread_mutex_lock(&(threadPool->Lock));

    /* Wake up all worker threads */
    if((pthread_cond_broadcast(&(threadPool->update)) != 0) ||
       (pthread_mutex_unlock(&(threadPool->Lock)) != 0))
    {
        exit(1);
    }

    int i;
    for (i = 0; i < threadPool->numOfThreads; i++) {
        pthread_join(threadPool->threadsArray[i], NULL);
    }

    threadPool->status = 2;

    //free memory
    while (!osIsQueueEmpty(threadPool->tasksQueue)) {
        Task* task = osDequeue(threadPool->tasksQueue);
        free(task);
    }

    osDestroyQueue(threadPool->tasksQueue);
    free(threadPool->threadsArray);
    pthread_mutex_destroy(&(threadPool->Lock));
    free(threadPool);
}

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param)
{
    if(threadPool == NULL || computeFunc == NULL || threadPool->status != 0)
    {
        return -1;
    }

    Task* task = (Task*)malloc(sizeof(Task));
    if (task == NULL)
    {
        return -1;
    }

    task->func = computeFunc;
    task->params = param;

    pthread_mutex_lock(&(threadPool->Lock));
    osEnqueue(threadPool->tasksQueue, (void *)task);

    // wake up thread that wait as long as the tasks queue is empty
    if(pthread_cond_signal(&(threadPool->update)) != 0)
    {
        exit(1);
    }

    pthread_mutex_unlock(&(threadPool->Lock));
    return 0;
}

void* runTask(void* args) {
    ThreadPool* tp = (ThreadPool*)args;
    struct os_queue* taskQueue = tp->tasksQueue;
    Task* task;
    while (tp->status < 2 && !osIsQueueEmpty(taskQueue))
    {
        /* Lock must be taken to wait on conditional variable */
        pthread_mutex_lock(&(tp->Lock));

        /* Wait on condition variable, check for spurious wakeups.
           When returning from pthread_cond_wait(), we own the lock. */
        if(osIsQueueEmpty(taskQueue) && (tp->status < 2))
        {
            pthread_cond_wait(&(tp->update), &(tp->Lock));
        }

        if (!(osIsQueueEmpty(taskQueue)))
        {
            // take task from the queue
            task = osDequeue(taskQueue);
            pthread_mutex_unlock(&(tp->Lock));
        }

        if (task != NULL)
        {
            // execute task
            task->func(task->params);
            free(task);
        }
    }
}

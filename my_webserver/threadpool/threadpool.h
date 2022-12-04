/*
 * @version: V1.0.0
 * @Author: LLF
 * @Date: 2022-11-21 16:51:46
 * @LastEditors: LLF
 * @LastEditTime: 2022-12-04 11:04:57
 * @company: Intelligent Robot Lab
 * @Mailbox: 1652228242@qq.com
 * @FilePath: /my_webserver/threadpool/threadpool.h
 * @Descripttion: 
 */
#ifndef THREADPOOL_
#define THREADPOOL_

#include"../CGImysql/sql_connection_pool.h"
#include"../lock/lock.h"
#include<cstdio>
#include<exception>
#include<list>
#include<pthread.h>

template<typedef T>
class threadpool
{
private:
    static void *worker();
    void run();

public:
    threadpool(/* args */);
    ~threadpool();
    bool append(T*request);
private:
    int m_thread_number;
    int m_max_request;
    pthread_t *m_threads;
    std::list<T *> m_workqueue;
    locker m_queueulocker;
    sem m_queuestat;
    bool m_stop;
    connection_pool *m_connpool;
};

template<typedef T>
threadpool::threadpool(connection_pool*connpool,int thread_number,int max_request):m_thread_number(thread_number),m_max_request(max_request),m_stop(false),m_threads(NULL),m_connpool(connpool){
    if(thread_number<=0||max__request<=0){
        throw std::execption();
    }
    //开辟堆空间存放线程组
    m_threads = new pthreads_t[m_thread_number];
    if(!m_threads){
        throw std::execption();
    }
    //循化创建子线程
    for (int i = 0; i < thread_number;++i){
        if(pthread_create(m_theads+i,NULL,worker,this)!=0){
            delete[] m_threads;
            throw std::execption();
        }
        if(pthread_detach(m_threads[i])){
            delete[] m_threads;
            throw std::execption();
        }
    }
}

template<typedef T>
threadpool::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}


template<typedef T>
bool threadpool::append(T*request){
    m_workqueue.lock();
    if(m_workqueue.size()>m_max_request){
        m_workqueue.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_workqueue.unlock();
    m_queuestat.post();
    return true;
}

template<typedef T>
static void*threadpool::worker(void*arg){
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template<typedef T>
void threadpool::run(){
    while(!=m_stop){
        m_workqueue.wait();
        m_workqueue.lock();
        if(m_workqueue.empty()){
            m_workqueue.unlock();
            return false;
        }
        T *request = m_workqueue.top();
        m_workqueue.pop_front();
        m_workqueue.unlock();
        if(!request){
            continue;
        }
        connectionRAII mysql(&request->mysql, m_connpool);
        request->process();
    }
}
#endif
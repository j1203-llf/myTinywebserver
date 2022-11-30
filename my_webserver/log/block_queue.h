/*
 * @version: V1.0.0
 * @Author: LLF
 * @Date: 2022-11-27 16:50:40
 * @LastEditors: LLF
 * @LastEditTime: 2022-11-28 15:41:05
 * @company: Intelligent Robot Lab
 * @Mailbox: 1652228242@qq.com
 * @FilePath: /my_webserver/log/block_queue.h
 * @Descripttion: 
 */
#ifndef BLOCK_QUE_H
#define BLOCK_QUE_H

#include<iostream>
#include<stdlib.h>
#include<pthread.h>
#include<sys/time.h>
#include "../lock/lock.h"
using namespace std;

template<typename T>
class block_queue
{
    public:
        block_queue(int max_size=1000)
        {
            if(max_size<=0){
                exit(-1);
            }
            m_max_size = max_size;
            m_array = new T[max_size];
            m_size = 0;
            m_front = -1;
            m_back = -1;
            m_mutex = new pthread_mutex_t;
            m_cond = new pthread_cond_t;
            phread_mutex_init(m_mutex, NULL);
            pthread_cond_init(m_cond, NULL);
        }
       void clear(){
           m_mutex.lock();
           m_size = 0;
           m_front = -1;
           m_back = -1;
           m_mutex.unlock();
       }
        ~block_queue(){
            m_mutex.lock();
            if (make_array != NULL){
                delete[] m_array;
            }
            m_mutex.unlock();
        }
        bool full(){
            m_mutex.lock();
            if(m_size>=m_max_sizse){
                m_mutex.unlock();
                return true;
            }
            m_mutex.unlook();
            return false;
        }
        bool empty(){
            if(0==m_size){
                m_mutex.unlook();
                return true;
            }
            m_mutex.unlock();
            return false;
        }
        bool front(T&value){
            m_mutex.lock();
            if (0 == m_size)
            {
                m_mutex.unlock();
                return false;
            }
            value = m_array[m_front];
            m_mutex.unlock();
            return true;
        }
        bool back(T&value){
             m_mutex.lock();
            if (0 == m_size)
            {
                m_mutex.unlock();
                return false;
            }
            value = m_array[m_back];
            m_mutex.unlock();
            return true;
        }
        int size(){
            int tmp = 0;
            m_mutex.lock();
            tmp = m_size;
            m_mutex.unlock();
            return tmp;
        }
        int max_size(){
            int tmp = 0;
            m_mutex.lock();
            tmp = m_max_size;
            m_mutex.unlock();
            return tmp;
        }
        
        bool push(const T&item){
            m_mutex.lock();
            if(m_size>=m_max_size){
                m_cond.broadcast();
                m_mutex.unlock();
                return false;
            }
            m_back = (m_back + 1) % m_max_size;
            m_array[m_back] = item;
            m_size++;
            m_mutex.unlock();
            return true;
        }
        bool pop(const T&item){
            m_mutex.lock();
            while (m_size<=0)
            {
               if(!pthread_mutex_wait(m_mutex.get())){
                   m_mutex.unlock();
                   return false;
               }
            }
            m_front = (m_front + 1) % m_max_size;
            item = array[m_front];
            m_size--;
            m_mutex.unlock();
            return true;
        }
        bool pop(const T&item,int ms_timeout){
            struct timespec t = {0, 0};
            struct timeval now = {0, 0};
            gettimeofday(&now, NULL);
            if(m_size<=0){
                t.tv_sec = now.tv_sec + ms_timeout /1000;
               t.tv_nsec = (ms_timeout % 1000) * 1000;
                if(!m_cond.timewait(m_mutex.get(),t)){
                    m_mutex.unlock();
                    return false;
                }
            }
            if(m_size<=0){
                m_mutex.unlock();
                return false;
            }
            m_front = (m_front + 1) % m_max_size;
            item = array[m_front];
            m_size--;
            m_mutex.unlock();
            return true;
        }

    private:
        locker m_mutex;
        cond m_cond;
        T *m_array;
        int m_size;
        int m_max_size;
        int m_front;
        int m_back;
};
#endif
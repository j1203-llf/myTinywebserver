/*
 * @version: V1.0.0
 * @Author: LLF
 * @Date: 2022-11-21 10:38:44
 * @LastEditors: LLF
 * @LastEditTime: 2022-11-22 14:21:17
 * @company: Intelligent Robot Lab
 * @Mailbox: 1652228242@qq.com
 * @FilePath: /my_webserver/lock/lock.h
 * @Descripttion: 
 */
#ifndef LOCK_H_
# define LOCK_H_

#include<exception>
#include<pthread.h>
#include<semaphore.h>
class locker
{
private:
    pthread_mutex_t m_mutex;

public:
    locker(){
        if(pthread_mutex_init(&m_mutex,NULL)!=0){
            throw std::exception();
        }
    }
    ~locker(){
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlook(){
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    pthread_mutex_t*get(){
        return &m_mutex;
    }
};
class sem{
   
    public:
        sem(){
            if(sem_init(&m_sem,0,0)!=0){
                throw std::exception();
            }
        }
        sem(int num){
            if(sem_init(&m_sem,0,num)!=0){
                throw std::exception();
            }
        }
        ~sem(){
            sem_destroy(&m_sem);
        }
        bool wait(){
            return sem_wait(&m_sem)==0;
        }
        bool post(){
            return sem_post(&m_sem)==0;
        }

    private:
        sem_t m_sem;
};
class cond
{
public:
    cond(){
        if(pthread_cond_init(&m_cond,NULL)!=0){
            throw std::exception();
        }
    }
    ~cond(){
        pthread_cond_destroy(&m_cond);
    }
    bool wait(pthread_mutex_t *m_mutex){
        return pthread_cond_wait(&m_cond, m_mutex)==0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t){
        return pthread_cond_timedwait(&m_cond, m_mutex,&t) == 0;
    }
    bool signal(){
        return pthread_cond_signal(&m_cond)==0;
    }
    bool broadcast(){
        return pthread_cond_broadcast(&m_cond)==0;
    }
    private:
        pthread_cond_t m_cond;
};
#endif
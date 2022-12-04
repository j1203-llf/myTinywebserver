/*
 * @version: V1.0.0
 * @Author: LLF
 * @Date: 2022-11-27 10:30:29
 * @LastEditors: LLF
 * @LastEditTime: 2022-12-01 14:14:40
 * @company: Intelligent Robot Lab
 * @Mailbox: 1652228242@qq.com
 * @FilePath: /my_webserver/timer/lst_timer.h
 * @Descripttion: 
 */
#ifndef LST_TIMER
#define LST_TIMER

#include <time.h>
#include"../log/log.h"

class util_timer;
struct client_data{
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};
//定时器类
class util_timer
{
    public:
        time_t expire;
        void (*cb_func)(client_data *);
        client_data *user_data;
        util_timer *prev;
        util_timer *next;

    public:
        util_timer() : prev(NULL), next(NULL) {}
    
};
//定时器容器类
class  sort_timer_lst{
    public:
        sort_timer_lst():head(NULL),tail(NULL){}
        ~sort_timer_lst(){
            util_timer *temp = head;
            while (temp)
            {
                head = head->next;
                delete temp;
                temp = head;
            }
        }
        //添加定时器到容器
        void add_timer(util_timer*timer){
            if(!timer){
                return;
            }
            if(!head){
                head = tail = timer;
                return;
            }
            if(timer->expire<head->expire){
                timer->next = head;
                head->prev = timer;
                head = timer;
                return;
            }
            add_timer(timer, head);
        }
        //调整定时器到容器
        void adjust_timer(util_timer*timer){
            if(!timer){
                return;
            }
            util_timer *tmp = timer->next;
            if(timer==head){
                head = head->next;
                timer->prev = NULL;
                timer->next = NULL;
                add_timer(timer, head);
            }
            if(!tmp||(timer->expire<tmp->expire)){
                return;
            }
            else{
                timer->prev->next = timer->next;
                timer->next->prev = timer->prev;
                add_timer(timer, timer->next);
            }
        }
        //从容器删除定时器
        void del_timer(util_timer*timer)
        {
            if(!timer)
            {
                return;
            }
            //链表中仅仅有一个定时器
            if((timer==head)&&(timer==tail))
            {
                delete timer;
                head = NULL;
                tail == NULL;
                return;
            }
            //删除头节点
            if(timer==head)
            {
                head = timer->next;
                head->prev = NULL;
                delete timer;
                return;
            }
        //删除尾节点
        if(timer==tail)
        {
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return;
        }
        //删除中间节点
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }  
    //定时任务处理函数
    void tick(){
        if(!head){
            return;
        }
        time_t cur = time(NULL);
        util_timer *tmp = head;
        while (tmp)
        {
          if(cur<tmp->expire){
              break;
          }
          tmp->cb_func(tmp->user_data);
          head = tmp->next;
          if(head){
              head->prev = NULL;
          }
          delete tmp;
          tmp = head;
        }
        }

    private:
    //重点函数
        void add_timer(util_timer*timer,util_timer*lst_head){
            util_timer *prev = lst_head;
            util_timer *tmp = prev->next;
            while (tmp)
            {   
                if(timer->expire<tmp->expire){
                    //先说清楚数据前，再说数据后
                    prev->next=timer;
                    timer->next=tmp;
                    tmp->prev = timer;
                    timer->prev = prev;
                    break;
                }
                prev = tmp;
                tmp->prev = prev;
            }
            if(!tmp){
                prev->next = timer;
                 tail->prev = prev;
                timer->next = NULL;
                tail = timer;
            }
        }

    private : 
        util_timer *head;
        util_timer *tail;
};
#endif

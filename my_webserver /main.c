/*
 * @version: V1.0.0
 * @Author: LLF
 * @Date: 2022-11-27 10:32:28
 * @LastEditors: LLF
 * @LastEditTime: 2022-12-04 11:02:05
 * @company: Intelligent Robot Lab
 * @Mailbox: 1652228242@qq.com
 * @FilePath: /my_webserver/main.c
 * @Descripttion: 
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "./lock/lock.h"
#include "./threadpool/threadpool.h"
#include "./timer/lst_timer.h"
#include "./http/http_conn.h"
#include "./log/log.h"
#include "./CGImysql/sql_connection_pool.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
#define TIMESLOT 5

#define SYNLOG
//#define ASYNLOG //异步写日志
#define listrnfdLT
//#define listenfdET //边缘触发非阻塞

extern addfd(int epollfd, int fd, bool one_shot);
extern remove(int epollfd, int fd);
extern setnonblocking(int fd);

//设置定时器相关数据
static int pipefd[2];
static sort_timer_lst timer_lst;
static int epollfd = 0;
//信号处理函数
void sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}
//设置信号函数
//仅通过管道发送信号值，不处理信号对应的逻辑，缩短异步执行时间，减少对主程序的影响。
void addsig(int sig,void(handler)(int),bool restart=true){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}
//定时处理任务，以实现不断触发SIGALRM信号
void timer_handler(){
    timer_lst.tick();
    alarm(TIMESLOT);
}
//回调函数，删除非活动连接在socket上的注册事件并关闭
void cb_func(client_data*user_data){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
    LOG_INFO("close fd:%d", user_data->sockfd);
    Log::get_instance()->flush();
}


int main(int argc,char*argv[]){
    #ifdef ASYNLOG
    Log::get_instance()->init("ServerLog", 2000, 800000, 8);
    #endif
    #ifdef  SYNLOG
    Log::get_instance()->init("ServerLog", 2000, 800000, 0);
    #endif
      if (argc <= 1)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    int port = atoi(argv[1]);
    //防止服务器退出
    addsig(SIGPIPE, SIG_IGN);
    //单理模式创建数据库连接池
    connection_pool *connPool = connection_pool::Getinstance();
    connPool->init("localhost", "debian-sys-maint", 1, "yourdb", 3306, 8);
    //创建线程池
    threadpool<http_conn> *pool = NULL;
    try
    {
        pool = new threadpool<http_conn>(connPool);
    }
    catch(...)
    {
        return 1;
    }
    http_conn *users = new http_conn(MAX_FD);
    assert(users);
    
    //初始化数据库读取表
    users->initmysql_result(conn_pool);
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    int flag = 1;
    //一种超时机制使其在一定时间后返回而不管是否有数据到来
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(listenfd, 5);
    assert(ret >=0);
    
    //创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd!= -1);

    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    //创建管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0], false);

    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);
    bool stop_server = false;

    client_data *users_timer = new client_data[MAX_FD];
   //超时标志
    bool timeout = false;
    alarm(TIMESLOT);

    while(!stop_server)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number<0&&errno!=EINTER){
            LOG_ERROR("%s", "epoll failure");
            break;
        }
    }
    for (int i = 0; i < number;i++){
        int sockfd = events[i].data.fd;
        //处理新到的客户连接
        if(sockfd==listenfd){
            struct sockaddr_in client_address;
            socklen_t client_addrlength = sizeof(client_address);
    #ifdef listenfdLT
            int connfd = accept(int listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if(connfd<0){
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                continue;
            }
            if(http_conn::m_user_count>=MAX_FD){
                show_error(connfd, "internal server busy");
                LOG_ERROR("%s", "internal server busy");
                continue;
            }
            users[connfd].init(connfd, client_address);
            //初始化client_data数据
            //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
            users_timer[connfd].address = client_address;
            users_timer[connfd].sockfd = connfd;
            util_timer *timer = new util_timer;
            timer->uaer_data = &users_timer[connfd];
            timer->cb_func = cb_func;
            timer_t cur = time(NULL);
            timer->expire = cur + 3 * TIMESLOT;
            users_timer[connfd].timer = timer;
            timer_lst.add_timer(timer);
    #endif

    #ifdef listenfdET
            whlie(1){
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                if (connfd < 0)
                    {
                        LOG_ERROR("%s:errno is:%d", "accept error", errno);
                        break;
                    }
                    if (http_conn::m_user_count >= MAX_FD)
                    {
                        show_error(connfd, "Internal server busy");
                        LOG_ERROR("%s", "Internal server busy");
                        break;
                    }
                    users[connfd].init(connfd, client_address);

                    //初始化client_data数据
                    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
                    users_timer[connfd].address = client_address;
                    users_timer[connfd].sockfd = connfd;
                    util_timer *timer = new util_timer;
                    timer->user_data = &users_timer[connfd];
                    timer->cb_func = cb_func;
                    time_t cur = time(NULL);
                    timer->expire = cur + 3 * TIMESLOT;
                    users_timer[connfd].timer = timer;
                    timer_lst.add_timer(timer);
                }
                continue;
#endif
            }
        else if(events[i].events&(EPOLLRDHUP|EPOLLHUP|EPOLLERR)){
            //服务器端关闭连接，移除对应定时器
            util_timer *timer = users_timer[sockfd].timer;
            timer->cb_func(&users_timer[sockfd]);
            
            if(timer){
                timer_lst.del_timer(timer);
            }
            //处理信号
            else if((sockfd==pipefd[0])&&(events[i].events&EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if(ret==-1){
                    continue;
                }
                else if(ret==0){
                    continue;
                }
                else{
                    for (int i = 0; i < ret;i++){
                        switch (signals[i]){
                            case SIGALRM:{
                                timeout = true;
                                break;
                            }
                            case SIGTERM:{
                                stop_server = true;
                            }
                        }
                    }
                }
            }
            //处理客户连接上接受到的数据
            else if(events[i].events&EPOLLIN){
                util_timer *timer = users_timer[sockfd].timer;
                if(users[sockfd].read_once()){
                    LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
                    Log::get_instance()->flush();
                    //读事件添加到请求队列
                    //对新定时器在链表位置进行调整
                    if(timer){
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        LOG_INFO("%s", "adjust timer once");
                        Log::get_instance()->flush();
                        timer_lst.adjust_timer(timer);  
                    }
                }
                else{
                    timer->cb_func(&users_timer[sockfd]);
                    if(timer){
                        timer_lst.del_timer(timer);
                    }
                }
            }
            else if(events[i].events&EPOLLOUT){
                util_timer *timer = users_timer[sockfd].timer;
                if(users[sockfd].write()){
                    LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
                    Log::get_intance()->flush();

                    //若有数据传输，操作同上
                    if(timer){
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        LOG_INFO("%s", "adjust time once");
                        Log::get_instance()->flush();
                        timer_lst.adjust_timer(timer);
                    }
                }
                else{
                    timer->cb_func(&users_timer[sockfd]);
                    id(timer){
                        timer_lst.del_timer(timer);
                    }
                }
            }
        }
        if(timeout){
            timer_handler();
            timeout = false;
        }
        }
        close(epollfd);
        close(listenfd);
        close(pipefd[0]);
        close(pipefd[1]);
        delete[] users;
        delete[] users_timer;
        delete pool;
        return 0;
}

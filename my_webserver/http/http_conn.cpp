/*
 * @version: V1.0.0
 * @Author: LLF
 * @Date: 2022-11-22 13:29:12
 * @LastEditors: LLF
 * @LastEditTime: 2022-11-22 19:13:33
 * @company: Intelligent Robot Lab
 * @Mailbox: 1652228242@qq.com
 * @FilePath: /my_webserver/http/http_conn.cpp
 * @Descripttion: 
 */
#include "http_conn.h"
#include "../log/log.h"
#include <map>
#include <mysql/mysql.h>
#include <fstream>

#define connfdLT 
#define connfdET

const char *ok_200_title = "ok";

const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";

const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";

const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";

const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

const char *doc_root = "/home/llf/centerya/te/study/TinyWebServer-raw_version";

map<string, string> users;
locker lock;

int settingnonblock(int fd){
    int falg = fcntl(connfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(connfd, F_SETFL, flag);
    return falg;
}

void addfd(int epoolfd,int fd,bool one_shot){
    struct epoll_event event;
    event.data.fd = fd;
    #ifdef connfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    #endif
    #ifdef listenfdET
    event.events = EPOLLIN | EPOLLRDHUP;
    #endif
    #ifdef connfdLT
    event.events = EPOLLIN | EPOLLLT | EPOLLRDHUP;
    #endif
    #ifdef listenfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
    #endif
    if(one_shot){
        event.events |= EPOLLONESHOT;
        epoll_ctrl(epoolfd, EPOLL_CTL_ADD, fd, &event);
        settingnonblock(fd);
    }
}

void removefd(int epollfd,int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
}
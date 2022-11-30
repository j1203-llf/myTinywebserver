/*
 * @version: V1.0.0
 * @Author: LLF
 * @Date: 2022-11-30 10:44:10
 * @LastEditors: LLF
 * @LastEditTime: 2022-11-30 16:00:44
 * @company: Intelligent Robot Lab
 * @Mailbox: 1652228242@qq.com
 * @FilePath: /my_webserver/CGImysql/sql_connection_pool.h
 * @Descripttion: 
 */
#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/lock.h"

using namespace std;

class connection_pool{
    public:
        MYSQL *Getconnection();
        bool ReleaseConnection(MYSQL *conn);
        int GetFreeConn();
        void DestoryPool();
        //单例模式
        static connection_pool *Getinstance();
        connection_pool();
        ~connection_pool();
        void init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int Maxconn);

    private:
        unsigned int MaxConn;
        unsigned int CurConn;//当前已使用连接数
        unsigned int FreeConn;//当前空闲连接数
    private:
        locker lock;
        list<MYSQL *> connList;//连接池
        sem reserve;
    private:
        string url;//主机地址
        int  Port;//数据i库的端口号
        string User;
        string PassWord;
        string Databasename;//使用数据库名
};
class connectionRAII{
    public:
        connectionRAII(MYSQL **con, connection_pool *conn_pool);
        ~connectionRAII();
    private:
        MYSQL *conRAII;
        connection_pool *poolRAII;
};
#endif
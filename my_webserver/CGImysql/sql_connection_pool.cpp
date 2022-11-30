/*
 * @version: V1.0.0
 * @Author: LLF
 * @Date: 2022-11-30 10:53:04
 * @LastEditors: LLF
 * @LastEditTime: 2022-11-30 16:31:11
 * @company: Intelligent Robot Lab
 * @Mailbox: 1652228242@qq.com
 * @FilePath: /my_webserver/CGImysql/sql_connection_pool.cpp
 * @Descripttion: 
 */
#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool(){
    this->CurConn = 0;
    this->FreeConn = 0;
}
connection_pool*connection_pool::Getinstance(){
    static connection_pool connPool;
    return &connPool;
}
void connection_pool::init(string url,string User,string Password,string DBName,int Port,unsigned int MaxConn){
    this->url = url;
    this->Port = Port;
    this->User = User;
    this->PassWord = PassWord;
    this->Databasename = DBName;

    lock.lock();
    for (int i = 0; i < MaxConn;i++){
        MYSQL *con = NULL;
        con = mysql_init(con);
        if(con==NULL)
            {
                cout << "Error:" << mysql_error(con);
                exit(1);
            }
            con = mysql_real_connect(con, url.c_str(), User.c_str(), Password.c_str(), DBName.c_str(), Port, NULL, 0);
        if(con==NULL){
            cout << "Error:" << mysql_error(con);
            exit(1);
        }
        connList.push_back(con);
        ++FreeConn;//？成功了该--吧，//这里是初始化，也就是创建多个连接数，自然是++
    }
    //将信号量初始化为最大连接次数？逻辑不对吧
    reserve = sem(FreeConn);
    this->MaxConn = FreeConn;
    lock.unlock();
}

//请求到达并返回一个可用连接
MYSQL*connection_pool::Getconnection(){
    MYSQL *con = NULL;
    if(0==connList.size())
        return NULL;
    reserve.wait();
    lock.lock();

    con = connList.front();
    connList.pop_front();

    --FreeConn;
    ++CurConn;

    lock.unlock();
    return con;
}

//释放当前连接,（并不会直接调用，而是使用RAII机制实现连接释放）
bool connection_pool::ReleaseConnection(MYSQL*con){
    if(NULL==con)
        return false;
    lock.lock();
    connList.push_back(con);
    ++FreeConn;
    --CurConn;
    lock.unlock();
    reserve.post();
    return true;
}

//销毁数据库连接池
void connection_pool::DestoryPool(){
    lock.lock();
    if(connList.size()>0){
        list<MYSQL *>:: iterator it;
        for (it = connList.begin(); it != connList.end();it++){
            MYSQL *con = *it;
            mysql_close(con);//通过释放con实现对it所指的对象进行释放
        }
        FreeConn = 0;
        CurConn = 0;
        lock.unlock();
        
    }
    lock.unlock();
}

//当前空闲连接数
int connection_pool::GetFreeConn(){
    return this->FreeConn;
}

connection_pool::~connection_pool(){
    DestoryPool();
}
//RAII机制释放数据库连接
connectionRAII::connectionRAII(MYSQL **SQL,connection_pool*conn_Pool){
    *SQL = conn_Pool->Getconnection();

    conRAII = *SQL;
    poolRAII = conn_Pool;
}
connectionRAII::~connectionRAII(){
    poolRAII->ReleaseConnection(conRAII);
}
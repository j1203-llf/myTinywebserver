/*
 * @version: V1.0.0
 * @Author: LLF
 * @Date: 2022-11-22 13:29:12
 * @LastEditors: LLF
 * @LastEditTime: 2022-12-01 11:13:27
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
locker m_lock;

void http_conn::initmysql_result(connection_pool *connPool)
{
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);//RAII机制，创建临时变量管理内存

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}
int settingnonblock(int fd){
    int flag = fcntl(fd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flag);
    return flag;
}

void addfd(int epollfd,int fd,bool one_shot){
    struct epoll_event event;
    event.data.fd = fd;
    #ifdef connfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    #endif
    #ifdef listenfdET
    event.events = EPOLLIN | EPOLLRDHUP;
    #endif
    #ifdef connfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
    #endif
    #ifdef listenfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
    #endif
    if(one_shot){
        event.events |= EPOLLONESHOT;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
        settingnonblock(fd);
    }
}

void removefd(int epollfd,int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
}

void modfd(int epollfd,int fd,int ev){
    epoll_event event;
    event.data.fd = fd;
#ifdef connfdET
event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = ev | EPOLLONESHOT | EPOLLRDBAND;
#endif

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::close_conn(bool real_close){
    if(real_close&&(m_sockfd!=-1)){
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void http_conn::init(int sockfd,const sockaddr_in &addr){
    m_sockfd = sockfd;
    m_address = addr;
    addfd(m_epollfd, sockfd, true);
    m_user_count++;
    init();
}

void http_conn::init(){
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}
//读取请求报文到read_buffer里面，直到对端无数据或关闭
bool http_conn::read_once(){
    if(m_read_idx>=READ_BUFFER_SIZE){
        return false;
    }
    int bytes_read = 0;
    while(true){
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if(bytes_read==-1){
            if(errno==EAGAIN||errno==EWOULDBLOCK)
                break;
            return false;
        }
        else if(bytes_read==0){
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}
//从状态机读取行
http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;
    for (;m_checked_idx<m_read_idx;m_checked_idx++){
        temp = m_read_buf[m_checked_idx];

        if(temp=='\r'){
            if((m_checked_idx+1)==m_read_idx)
                return LINE_OPEN;
            else if(m_read_buf[m_checked_idx+1]=='\n'){
                    m_read_buf[m_checked_idx++] = '\0';
                    m_read_buf[m_checked_idx++] = '\0';
                    return LINE_OK;
            }
                    return LINE_BAD;
        }

      else if(temp=='\n'){
        if(m_checked_idx>1&&m_read_buf[(m_checked_idx-1)]=='\r'){
                m_read_buf[m_checked_idx++] = '\0';
               m_read_buf[m_checked_idx++] = '\0';
               return LINE_OK;
        }
        return LINE_BAD;
      }
    }
    
    return LINE_OPEN;
}
//主状态机分析请求行
http_conn::HTTP_CODE http_conn::parse_request_line(char*text){
    m_url = strpbrk(text, "\t");
    if(!m_url)
        return BAD_REQUEST;
    *m_url++ = '\0';

    char *method = text;
    if(strcasecmp(method,"GET"))
        m_method = GET;
    if(strcasecmp(method,"POST")){
        m_method = POST;
        cgi = 1;
    }
    else {
        return BAD_REQUEST;
    }
    //防止还有t
    m_url += strspn(text, "\t");

    m_version = strpbrk(text, "\t");
    if(!m_version){
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(text, "\t");
    if(strcasecmp(m_version,"http/1.1")!=0)
        return BAD_REQUEST;

    if(strncasecmp(m_version,"http://",7)==0){
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if(strncasecmp(m_url,"https://",8)==0){
        m_url += 8;
        m_url = strchr(m_version, '/');
    }

    if(!m_url||m_url[0]!='/')
        return BAD_REQUEST;
    if(strlen(m_url)==1)
        strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}
//主状态机分析请求头
http_conn::HTTP_CODE http_conn::parse_headers(char*text){
    if(text[0]=='\0'){
        if(m_content_length!=0){
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if(strncasecmp(text,"connection:",11)==0){
        text += 11;
        text += strspn(text, "\t");
        if(strcasecmp(text,"keep-alive")==0){
            m_linger = true;
        }
    }
    else if(strncasecmp(text,"Content-length:",15)==0){
        text += 15;
        text += strspn(text, "\t");
        m_content_length = atol(text);
    }
    else if(strncasecmp(text,"Host:",5)==0){
        text += 5;
        text += strspn(text, "\t");
        m_host = text;
    }
    else{
        LOG_INFO("未知请求头：%s", text);
        Log::get_instance()->flush();
    }
    return NO_REQUEST;
}
//主状态机判断请求是否完整读入
http_conn::HTTP_CODE http_conn::parse_content(char*text){
    if(m_read_idx>=(m_content_length+m_checked_idx)){
        text[m_content_length] = '\0';
        m_string = text;
        return GET_REQUEST;

    }
    return NO_REQUEST;
}
//该函数是本文件的顶层函数
void http_conn::process(){
    HTTP_CODE read_ret = process_read();
    if(read_ret==NO_REQUEST){
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    bool write_ret = process_write(read_ret);
    if(!write_ret){
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}
//将主状态机封装，并读主状态机数据(http请求报文)
http_conn::HTTP_CODE http_conn::process_read(){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;
    while((m_check_state==CHECK_STATE_CONTENT&&line_status==LINE_OK)||(line_status=parse_line())==LINE_OK)
        {
            text = get_line();
            m_start_line = m_checked_idx;
            switch (m_check_state)
            {
            case CHECK_STATE_REQUESTLINE:{
                ret = parse_request_line(text);
                if(ret==BAD_REQUEST ){
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:{
                ret = parse_headers(text);
                if(ret==BAD_REQUEST)
                    return BAD_REQUEST;
                else if(ret==GET_REQUEST)
                    return do_request();
                break;
            }
            case CHECK_STATE_CONTENT:{
                ret = parse_content(text);
                if(ret==GET_REQUEST)
                    return do_request();
                line_status = LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
            }
        }
        return NO_REQUEST;
}
http_conn::HTTP_CODE http_conn::do_request(){
        strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
   
    const char *p = strrchr(m_url, '/');

    //处理cgi
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        //同步线程登录校验
        if (*(p + 1) == '3')
        {
         
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (users.find(name) == users.end())
            {

                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
      
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)//----------------这里是否该写成users[password] == password
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }

    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    
    return FILE_REQUEST;
}
void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
//把响应报文发到write_buffer里面所需的操作函数
//-----------------------------------------------------------------add_respose函数可以优化initializer_list标准库类型
//将响应报文发给浏览器端
bool http_conn::write(){
    int temp = 0;
    int newadd = 0;
    if(bytes_to_send==0){
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }
    while ((1))
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if(temp>0){
            bytes_have_send += temp;
            newadd = bytes_have_send - m_write_idx;
        }
        if(temp<=-1){
                if(errno==EAGAIN){
                    if(bytes_have_send>=m_iv[0].iov_len){
                        m_iv[0].iov_len = 0;
                        m_iv[1].iov_base = m_file_address + newadd;
                        m_iv[1].iov_len = bytes_to_send;
                    }
                    else{
                        m_iv[0].iov_base = m_write_buf + bytes_to_send;
                        m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
                    }
                    modfd(m_epollfd, m_sockfd, EPOLLOUT);
                    return true;
                }
                unmap();
                return false;
        }
        bytes_to_send -= temp;
        if(bytes_to_send<=0){
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);
            if(m_linger){
                init();
                return true;
            }
            else{
                return false;
            }
        }
    }
}

bool http_conn::add_respose(const char*format,...){
    if(m_write_idx>=WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if(len>=(WRITE_BUFFER_SIZE-1-m_write_idx)){
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    return true;
}
//添加状态行
bool http_conn::add_status_line(int status,const char *title){
    return add_respose("%s %d %s\r\n", "HTTP/1.1", status, title);
}
//添加消息头
bool http_conn::add_headers(int content_len)
{
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}
//添加消息报头，添加文本长度 连接状态，空行
bool http_conn::add_content_length(int content_len){
    return add_respose("content-length:%d\r\n", content_len);
}
//添加文本类型，这里就是html
bool http_conn::add_content_type(){
    add_respose("Content_Type:%s\r\n", "text/html");
}
//添加连接状态，通知浏览器端关闭or保持连接
bool http_conn::add_linger(){
    return add_respose("Connection:%s\r\n", (m_linger) ? "Keep-alive" : "Close");
}
//添加空行
bool http_conn::add_blank_line(){
    return add_respose("%s", "\r\n");
}
//添加文本内容
bool http_conn::add_content(const char*content){
    return add_respose("%s", content);
}
//把响应报文发到write_buffer里面
bool http_conn::process_write(HTTP_CODE ret){
    switch(ret){
        case INTERNAL_ERROR:{
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form))
                return false;
            break;
        }
        case BAD_REQUEST:{
            add_status_line(404, error_400_title);
            add_headers(strlen(error_400_form));
            if(add_content(error_400_form))
                return false;
            break;
        }
        case FORBIDDEN_REQUEST:{
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form))
                return false;
            break;
        }
        case FILE_REQUEST:{
            add_status_line(200, ok_200_title);
            if(m_file_stat.st_size!=0){
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;

                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }
            else{
                const char *ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if(!add_content(ok_string))
                    return false;
            }
        }
        default:
            return false;
        }
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv_count = 1;
        return true;
}

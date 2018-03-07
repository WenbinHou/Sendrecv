#pragma once

class environment;
class connection;
class listener;
class socket_environment;
class socket_connection;
class socket_listener;
class socket_listener;
#include <common/common.h>
#include <sys/eventfd.h>

#define INVALID_FD      ((int)-1)

struct fd_data
{
    enum fd_type
    {
        FDTYPE_UNKNOWN = 0,
        FDTYPE_SOCKET_NOTIFICATION_EVENT,
        FDTYPE_SOCKET_CONNECTION,
        FDTYPE_SOCKET_LISTENER,
    };

    fd_type type;
    int fd;
    void* owner;

public:
    fd_data() : type(FDTYPE_UNKNOWN), fd(INVALID_FD), owner(nullptr) { }
    fd_data(socket_environment* env, const int eventfd) 
        : type(FDTYPE_SOCKET_NOTIFICATION_EVENT), fd(eventfd), owner(env) { }
    fd_data(socket_connection* conn, const int connfd) 
        : type(FDTYPE_SOCKET_CONNECTION), fd(connfd), owner(conn) { }
    fd_data(socket_listener* lis, const int listenfd)
        : type(FDTYPE_SOCKET_LISTENER), fd(listenfd), owner(lis) { }
};


struct event_data
{
    enum event_owner_type
    {
        EVENTOWNER_ENVIRONMENT,
        EVENTOWNER_CONNECTION,
        EVENTOWNER_LISTENER,
        EVENTOWNER_MAX,
    };

    enum event_type
    {
        EVENTTYPE_UNKNOWN = 0,
        EVENTTYPE_ENVIRONMENT_DISPOSE,
        EVENTTYPE_CONNECTION_CLOSE,
        EVENTTYPE_CONNECTION_ASYNC_SEND,
        EVENTTYPE_CONNECTION_CONNECT_FAILED,
        EVENTTYPE_CONNECTION_RUNDOWN_RELEASE,
        EVENTTYPE_LISTENER_CLOSE,
        EVENTTYPE_LISTENER_RUNDOWN_RELEASE,
        EVENTTYPE_MAX,
    };

    event_type type;
    void* owner;
    event_owner_type owner_type;

    static event_data environment_dispose(environment* env) { return event_data{ EVENTTYPE_ENVIRONMENT_DISPOSE, env, EVENTOWNER_ENVIRONMENT }; }
    static event_data connection_close(connection* conn) { return event_data{ EVENTTYPE_CONNECTION_CLOSE, conn, EVENTOWNER_CONNECTION }; }
    static event_data connection_connect_failed(connection* conn) { return event_data{ EVENTTYPE_CONNECTION_CONNECT_FAILED, conn, EVENTOWNER_CONNECTION }; }
    static event_data connection_async_send(connection* conn) { return event_data{ EVENTTYPE_CONNECTION_ASYNC_SEND, conn, EVENTOWNER_CONNECTION }; }
    static event_data connection_rundown_release(connection* conn) { return event_data{ EVENTTYPE_CONNECTION_RUNDOWN_RELEASE, conn, EVENTOWNER_CONNECTION }; }
    static event_data listener_close(listener* listen) { return event_data{ EVENTTYPE_LISTENER_CLOSE, listen, EVENTOWNER_LISTENER }; }
    static event_data listener_rundown_release(listener* listen) { return event_data{ EVENTTYPE_LISTENER_RUNDOWN_RELEASE, listen, EVENTOWNER_LISTENER }; }
};



class fragment
{
private:
    /*const*/ char* _buffer;
    /*const*/ size_t _length;
    size_t _offset;

public:
    fragment(const void* buffer, const size_t length)
        : _buffer((char*)const_cast<void*>(buffer)), _length(length), _offset(0)
    {
        if (length) {
            ASSERT(buffer);
        }
    }
    fragment() : _buffer(nullptr), _length(0), _offset(0) { }

    const void* curr_buffer() const { return _buffer + _offset; }
    size_t curr_length() const { return _length - _offset; }

    const void* original_buffer() const { return _buffer; }
    size_t original_length() const { return _length; }

    void forward(size_t n)
    {
        ASSERT(n <= curr_length());
        _offset += n;
    }
};


enum head_type{
    HEAD_TYPE_INVAILD = 0,
    HEAD_TYPE_INIT,
    HEAD_TYPE_SEND,
    HEAD_TYPE_FINALIZE,
};

typedef struct datahead{
    enum head_type type;
    size_t content_size;
    int src;
    int dest;
public:
    datahead(){}
    datahead(enum head_type type, size_t size)
            :type(type), content_size(size){}
}datahead;

struct data_state{
    size_t      recvd_head_size;
    datahead    head_msg;
    size_t      total_content_size;
    size_t      recvd_content_size;
    char*       content;
public:
    data_state()
            :recvd_head_size(0), head_msg(), total_content_size(0), recvd_content_size(0),content(nullptr) {}
    void clear(){
        recvd_head_size = 0;
        memset(&head_msg, 0, sizeof(datahead));
        total_content_size = 0;
        recvd_content_size = 0;
        content = nullptr;
    }

};

typedef struct handler{
    size_t content_size;
    char*  content;
    int    src;
    int    dest;
    int    notify_fd;
    bool   is_finish;
public:
    handler():content_size(0), content(nullptr), src(-1), dest(-1), is_finish(false){
        notify_fd = CCALL(eventfd(0, EFD_CLOEXEC));
        IDEBUG("Create a handler with notify_fd = %d\n", notify_fd);
    }
    void set_handler(int s,  int d, size_t cs, char* c) {
        src = s; dest = d; content_size = cs; content = c;
        is_finish = false;
    }
}handler;
#include "environment.h"
#include "connection.h"
#include "listener.h"

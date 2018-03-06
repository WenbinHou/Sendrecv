#ifndef SENDRECV_UTILS_H
#define SENDRECV_UTILS_H

typedef  struct nodeinfo{
    char ip_addr[16];
    int  listen_port;
}nodeinfo;

enum head_type{
    HEAD_TYPE_INVAILD = 0,
    HEAD_TYPE_INIT,
    HEAD_TYPE_SEND,
};


typedef struct datahead{
    enum head_type type;
    size_t content_size;
public:
    datahead(enum head_type type, size_t size)
            :type(type), content_size(size){}
}datahead;


#endif

#ifndef SENDRECV_UTILS_H
#define SENDRECV_UTILS_H
#include <sys/types.h>
#include <string>
#include <string.h>

typedef  struct nodeinfo{
    char ip_addr[16];
    int  listen_port;
public:
    nodeinfo(){}
    nodeinfo(std::string ip, int port){
        strcpy(ip_addr, ip.c_str());
        listen_port = port;
    }
}nodeinfo;






#endif

#ifndef _RDMA_ENV_H
#define _RDMA_ENV_H
//#include <environment.h> 不需要此include
#include <thread>

class rdma_environment: public environment
{
    friend class rdma_connection;
    friend class rdma_listener;
public:
    rdma_environment();
    ~rdma_environment() override;
    rdma_listener   *create_rdma_listener(const char* bind_ip, const uint16_t port);
    rdma_connection *create_rdma_connection(const char* connect_ip, const uint16_t port);
private:
    void connection_loop();//用于rdma_event_channel
    void sendrecv_loop(); //用于ibv_comp_channel
private:
    struct ibv_device  **dev_list = nullptr; int num_devices;
    struct ibv_context *ctx = nullptr; //reference for rdma device
    struct ibv_pd      *pd  = nullptr;//for test:only one pd for one node
    struct ibv_cq      *cq  = nullptr;//事实上cq既可以放在rdma_connection中，和某一qp对应，又可以多个qp使用一个cq
                      //TODO:看如何是的一个连接使用一个cq
    struct ibv_comp_channel     *comp_channel;//同样一个comp_channel可以对应多个qp
    struct rdma_event_channel   *conn_ec;
    
    std::thread* _loop_thread_connection = nullptr;//用于建立连接的线程
    std::thread* _loop_thread_cq         = nullptr;//用于消息发送接收的线程
    //可能会需要eventfd

};


#endif

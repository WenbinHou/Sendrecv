#ifndef _RDMA_ENV_H
#define _RDMA_ENV_H
//#include <environment.h> 不需要此include
#include <thread>
#include <unordered_map>

class rdma_environment: public environment
{
    friend class rdma_connection;
    friend class rdma_listener;
public:
    rdma_environment();
    ~rdma_environment() override;
    void dispose() override;
    rdma_listener   *create_rdma_listener(const char* bind_ip, const uint16_t port);
    rdma_connection *create_rdma_connection(const char* connect_ip, const uint16_t port);
    //被动连接创建
    rdma_connection *create_rdma_connection_passive(struct rdma_cm_id *new_conn, struct rdma_cm_id *listen_id);
private:
    void connection_loop();//用于rdma_event_channel
    void sendrecv_loop(); //用于ibv_comp_channel

    void build_params(struct rdma_conn_param *params);
    void close_connection_loop();
private:
    //struct ibv_device  **dev_list = nullptr; int num_devices;
    //struct ibv_context *ctx = nullptr; //reference for rdma device
    int _efd_rdma_fd = INVALID_FD;//用于epoll处理每个rdma_connection中的comp_channel的fd，对应_loop_thread_cq
    //struct ibv_pd      *pd  = nullptr;//for test:only one pd for one node
    //struct ibv_cq      *cq  = nullptr;//现在一个rdma_connection对应一个cq,即一个qp对应一个cq
                      //TODO:看如何是的一个连接使用一个cq
    //struct ibv_comp_channel     *comp_channel; //现在comp_channel对应一个cq,位于rdma_connection对象中，将comp_channel设为非阻塞
    struct rdma_event_channel   *env_ec;//仅用于在rdma建立连接时使用，对应_loop_thread_connection
    
    std::thread* _loop_thread_connection = nullptr;//用于建立连接的线程
    std::thread* _loop_thread_cq         = nullptr;//用于消息发送接收的线程

    std::unordered_map<intptr_t, rdma_connection*> map_id_conn;//用于记录id和passive_connection之间的关系
    //可能会需要eventfd

};


#endif

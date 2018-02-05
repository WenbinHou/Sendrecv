#ifndef _RDMA_ENV_H
#define _RDMA_ENV_H
//#include <environment.h> 不需要此include
#include <thread>
#include <unordered_map>
#include <sys/epoll.h>
#include <sys/eventfd.h>

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
    void epoll_add(rdma_fd_data* fddata, const uint32_t events) const;
    void push_and_trigger_notification(const rdma_event_data& notification);
    void process_epoll_env_notificaton_event_rdmafd(const uint32_t events);
private:
    int _efd_rdma_fd = INVALID_FD;//用于epoll处理每个rdma_connection中的comp_channel的fd，对应_loop_thread_cq
    struct rdma_event_channel   *env_ec;//仅用于在rdma建立连接时使用，对应_loop_thread_connection
    
    std::thread* _loop_thread_connection = nullptr;//用于建立连接的线程
    std::thread* _loop_thread_cq         = nullptr;//用于消息发送接收的线程

    std::unordered_map<intptr_t, rdma_connection*> map_id_conn;//用于记录id和passive_connection之间的关系
    //eventfd在此处的作用同样是通知当前已经有数据准备好，可以发送
    int _notification_event_rdma_fd = INVALID_FD;
    rdma_fd_data _notification_event_rdma_fddata;
    tsqueue<rdma_event_data> _notification_rdma_queue;//此队列中暂时只处理发送的操作，不进行连接等操作的处理


};


#endif

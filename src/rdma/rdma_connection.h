#ifndef _RDMA_CONNECTION_H
#define _RDMA_CONNECTION_H
#include "rdma_header.h"

class rdma_connection : public connection
{
    // 添加rdma_context_environment
    friend class rdma_environment;
    friend class rdma_listener;
private:
    //主动建立连接
    rdma_connection(rdma_environment* env, const char* connect_ip, const uint16_t port);
    //通过listener建立连接
    rdma_connection(rdma_environment* env, const rdma_conn_fd connfd, const endpoint& remote_ep);
    void init(const bool isAccepted);
public:
    bool async_close() overide;
    //bool async_send() overide;
    bool async_connection() overide;
    //bool start_receive() overide;
    //bool async_send_many() //
    //endpoint remote_endpoint() const {};
    //endpoint local_endpoint() const {};
private:
    volatile bool _close_finish = false;
    rundown_protection _rundown;
    //std::atomic<connection_status> _status;
    //rdma_conn_fd              conn_fd  = {nullptr, nullptr};
    struct rdma_cm_id         *conn_id = nullptr;
    struct ibv_qp             *conn_qp = nullptr;
    //struct rdma_event_channel *conn_ec = nullptr; 此conn_ec 应该是env中的属性
    struct rdma_cm_event      *evnet   = nullptr;//不确定是否需要
    rdma_fd_data  conn_type;
    endpoint _remote_endpoint;
    endpoint _local_endpoint;

    //there should be a sendingqueue, because the peer_of_send should wait for the peer of recv have already prepared the recv_buffer
    //tsqueue<fragment> _sending_queue;

    //int _immediate_connect_error = 0;
    //
};

#endif

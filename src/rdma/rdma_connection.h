#ifndef _RDMA_CONNECTION_H
#define _RDMA_CONNECTION_H
//#include "rdma_header.h"
//#include <connection.h> 不需要include connection.h 在rdma_header中的sendrecv.h已经引入了

class rdma_connection : public connection
{
    friend class rdma_environment;
    friend class rdma_listener;
private:
    //主动建立连接
    rdma_connection(rdma_environment* env, const char* connect_ip, const uint16_t port);
    //通过listener建立连接,这个不再需要了
    //rdma_connection(rdma_environment* env, const struct rdma_cm_id *cm_id, const endpoint& remote_ep);
    //通过env建立连接
    rdma_connection(rdma_environment* env, struct rdma_cm_id *new_conn_id, struct rdma_cm_id* listen_id);
public:
    bool async_connect() override;
    bool async_close() override;
    
    bool async_send(const void* buffer, const size_t length) override;
    bool start_receive() override;
    bool async_send_many(const std::vector<fragment> frags) override; //
    endpoint remote_endpoint() const { return _remote_endpoint; };
    endpoint local_endpoint() const { return _local_endpoint; };
private:
    void build_conn_res();
    void build_qp_attr(struct ibv_qp_init_attr *qp_attr);
    void process_established();
    void update_local_endpoint();
    void update_remote_endpoint();
    void process_established_error();
    void close_rdma_conn();
private:
    volatile bool _close_finish = false;
    rundown_protection _rundown;
    std::atomic<connection_status> _status;
    //一个rdma_connection由五个结构体构成，id, pd，qp, cq, comp_channel
    struct rdma_cm_id         *conn_id = nullptr;
    struct ibv_context        *conn_ctx = nullptr;
    struct ibv_pd             *conn_pd = nullptr;
    struct ibv_qp             *conn_qp = nullptr;
    struct ibv_cq             *conn_cq = nullptr;
    struct ibv_comp_channel   *conn_comp_channel = nullptr;

    rdma_listener             *conn_lis = nullptr;//nulltpr表示主动连接，非NULL表示此连接时passive connection
    rdma_fd_data  conn_type;
    endpoint _remote_endpoint;
    endpoint _local_endpoint;

    //there should be a sendingqueue, because the peer_of_send should wait for the peer of recv have already prepared the recv_buffer
    tsqueue<fragment> _sending_queue;

    //int _immediate_connect_error = 0;
    //
};

#endif

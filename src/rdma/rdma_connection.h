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
    void register_rundown();
    void build_conn_res();
    void build_qp_attr(struct ibv_qp_init_attr *qp_attr);
    void process_established();
    void update_local_endpoint();
    void update_remote_endpoint();
    void process_established_error();
    void close_rdma_conn();
    void process_rdma_async_send();
    void try_to_send();
    void process_poll_cq(struct ibv_cq *ret_cq, struct ibv_wc *ret_wc_array, int num_cqe);
    void process_one_cqe(struct ibv_wc *wc);
    void post_send_ackctl_msg(message::msg_type type, uint64_t addr,
                              uint32_t rkey, uintptr_t send_ctx_addr, uint32_t recv_addr_index);
    void post_reuse_recv_ctl_msg(addr_mr* reuse_addr_mr);
    void post_new_recv_ctl_msg();
    void post_send_rdma_write(rdma_sge_list* pending_send_sge_list, uint64_t peer_addr,
                              uint32_t peer_rkey, uint32_t recv_addr_index);
    void post_send_req_msg(rdma_sge_list* sge_list, bool isDecrease, addr_mr* reuse_addr_mr);
    void dereg_recycle(addr_mr* addr_mr_pair);
private:
    volatile bool _close_finished = false;
    rundown_protection _rundown;
    std::atomic<connection_status> _status;
    //一个rdma_connection由五个结构体构成，id, pd，qp, cq, comp_channel
    struct rdma_cm_id         *conn_id = nullptr;
    struct ibv_context        *conn_ctx = nullptr;
    struct ibv_pd             *conn_pd = nullptr;
    struct ibv_qp             *conn_qp = nullptr;
    struct ibv_cq             *conn_cq = nullptr;
    struct ibv_comp_channel   *conn_comp_channel = nullptr;
    int    ack_num = 0;
    rdma_listener             *conn_lis = nullptr;//nulltpr表示主动连接，非NULL表示此连接时passive connection
    rdma_fd_data  conn_type;
    endpoint _remote_endpoint;
    endpoint _local_endpoint;

    //there should be a sendingqueue, because the peer_of_send should wait for the peer of recv have already prepared the recv_buffer
    tsqueue<rdma_sge_list*> _sending_queue;
    std::atomic_int          peer_rest_wr;
    std::atomic<bool>        peer_start_recv;
    //记录出去正在消耗的对端的wr外，剩下的wr(注：此wr仅仅表示用于接受控制信息的wr个数) 
    pool<message>           ctl_msg_pool;
    pool<addr_mr>           addr_mr_pool;//记录ctl_msg和addr_mr之间的关系
    pool<rdma_sge_list>     rdma_sge_pool;

    arraypool<recv_info> recvinfo_pool;
    //int _immediate_connect_error = 0;
    //
};

#endif

#ifndef SENDRECV_RDMA_CONN_P2P_H
#define SENDRECV_RDMA_CONN_P2P_H

#include "rdma_conn_p2p.h"
#include "rdma_resource.h"
#include <sendrecv.h>
#include <sys/eventfd.h>
#include "conn_system.h"

class rdma_conn_p2p {
    friend class conn_system;
private:
    int send_event_fd ;
    int recv_event_fd ;
private:
    unidirection_rdma_conn send_rdma_conn;
    exchange_qp_data send_direction_qp;
    unidirection_rdma_conn recv_rdma_conn;
    exchange_qp_data recv_direction_qp;

    void nofity_system(int event_fd);
    void create_qp_info(unidirection_rdma_conn &rdma_conn_info);
    void modify_qp_to_rtr(struct ibv_qp *qp, uint32_t remote_qpn, uint16_t dlid, int ib_port);
    void modify_qp_to_rts(struct ibv_qp *qp);
    int  test_post_n_recv(struct ibv_pd *pd, struct ibv_qp *qp, int n, size_t size);
    int  test_post_n_send(struct ibv_pd *pd, struct ibv_qp *qp, int n, size_t size);

public:
    rdma_conn_p2p(const rdma_conn_p2p&) = delete;
    rdma_conn_p2p(rdma_conn_p2p && ) = delete;
    rdma_conn_p2p & operator=(const rdma_conn_p2p&) = delete;

    rdma_conn_p2p();
    void test_send();//only test on pair<send_rdma_conn, send_direction_qp>
    int wait_poll_send();
    int wait_poll_recv();

    //bool isend();
    //bool irecv();
};


#endif //SENDRECV_RDMA_CONN_P2P_H

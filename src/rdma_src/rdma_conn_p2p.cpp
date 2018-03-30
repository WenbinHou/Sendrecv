#include "rdma_conn_p2p.h"
#include <vector>
#define RX_DEPTH 500
#define MAX_INLINE_LEN 256
#define MAX_SGE_LEN    10
rdma_conn_p2p::rdma_conn_p2p() {
    send_event_fd = CCALL(eventfd(0, EFD_CLOEXEC));
    recv_event_fd = CCALL(eventfd(0, EFD_CLOEXEC));
}

void rdma_conn_p2p::nofity_system(int event_fd)
{
    int64_t value = 1;
    CCALL(write(event_fd, &value, sizeof(value)));
}

void rdma_conn_p2p::create_qp_info(unidirection_rdma_conn &rdma_conn_info){
    //test whether there were ibv_device
    int num_devices;
    struct ibv_device **dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list) {
        ERROR("Failed to get IB devices list\n");
        ASSERT(0);
    }

    rdma_conn_info.rx_depth = RX_DEPTH;
    struct ibv_device *ibv_dev;
    for (int i = 0; i < num_devices; i++) {
        ibv_dev = dev_list[i];
        rdma_conn_info.context = ibv_open_device(ibv_dev);
        if(rdma_conn_info.context)
            break;
    }
    if(!(rdma_conn_info.context)) {
        ERROR("Cannot open any ibv_device.\n");
        ASSERT(0);
    }
    rdma_conn_info.ib_port = 1;//default is 1

    CCALL(ibv_query_port(rdma_conn_info.context, rdma_conn_info.ib_port, &(rdma_conn_info.portinfo)));
    rdma_conn_info.channel = ibv_create_comp_channel(rdma_conn_info.context);
    ASSERT(rdma_conn_info.channel);

    rdma_conn_info.pd = ibv_alloc_pd(rdma_conn_info.context);
    ASSERT(rdma_conn_info.pd);

    rdma_conn_info.cq = ibv_create_cq(rdma_conn_info.context, rdma_conn_info.rx_depth+1, this,
                                      rdma_conn_info.channel, 0);
    ASSERT(rdma_conn_info.cq);

    struct ibv_qp_init_attr init_attr;
    memset(&init_attr, 0, sizeof(init_attr));
    init_attr.send_cq = rdma_conn_info.cq;
    init_attr.recv_cq = rdma_conn_info.cq;
    init_attr.cap.max_send_wr  = rdma_conn_info.rx_depth;
    init_attr.cap.max_recv_wr  = rdma_conn_info.rx_depth;
    init_attr.cap.max_send_sge = MAX_SGE_LEN;
    init_attr.cap.max_recv_wr  = MAX_SGE_LEN;
    init_attr.cap.max_inline_data = MAX_INLINE_LEN;
    init_attr.qp_type = IBV_QPT_RC;
    //init_attr.sq_sig_all = 0;
    init_attr.sq_sig_all = 0;
    init_attr.qp_context = (void*)this;

    rdma_conn_info.qp = ibv_create_qp(rdma_conn_info.pd, &init_attr);
    ASSERT(rdma_conn_info.qp);


    struct ibv_qp_attr attr;
    attr.qp_state = IBV_QPS_INIT;
    attr.pkey_index = 0;
    attr.port_num   = rdma_conn_info.ib_port;
    attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

    if (ibv_modify_qp(rdma_conn_info.qp, &attr,
                      IBV_QP_STATE              |
                      IBV_QP_PKEY_INDEX         |
                      IBV_QP_PORT               |
                      IBV_QP_ACCESS_FLAGS)) {
        ERROR("Failed to modify QP to INIT\n");
        ASSERT(0);
    }
    ITRACE("Finish create the qp ~~~~~~~.\n");
}

void rdma_conn_p2p::modify_qp_to_rtr(struct ibv_qp *qp, uint32_t remote_qpn, uint16_t dlid, int ib_port){
    struct ibv_qp_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_1024;
    attr.dest_qp_num = remote_qpn;
    attr.rq_psn   = 0;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 0x12;
    attr.ah_attr.is_global = 0;
    attr.ah_attr.dlid = dlid;
    attr.ah_attr.sl = 0;
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.port_num = ib_port;
    int flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU |
                IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                IBV_QP_MIN_RNR_TIMER;
    CCALL(ibv_modify_qp(qp, &attr, flags));
}

void rdma_conn_p2p::modify_qp_to_rts(struct ibv_qp *qp) {
    struct ibv_qp_attr attr;
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 14;
    attr.retry_cnt = 7;
    attr.rnr_retry = 7;
    attr.sq_psn = 0;
    attr.max_rd_atomic = 1;
    flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
            IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
    CCALL(ibv_modify_qp(qp, &attr, flags));
}

int rdma_conn_p2p::test_post_n_recv(struct ibv_pd *pd, struct ibv_qp *qp, int n, size_t size)
{
    std::vector<char*> recv_region_list(n);
    std::vector<struct ibv_mr *> recv_mr_list(n);
    for(int i = 0;i < n;i++){
        recv_region_list[i] = (char*)malloc(size);
        recv_mr_list[i] = ibv_reg_mr(pd, recv_region_list[i], size, IBV_ACCESS_LOCAL_WRITE);
        ASSERT(recv_mr_list[i]);
    }
    int ret_post_num = 0;
    for(int i = 0;i < n;i++){
        struct ibv_recv_wr wr, *bad_wr = NULL;
        struct ibv_sge sge;

        wr.wr_id = 1;
        wr.next = NULL;
        wr.sg_list = &sge;
        wr.num_sge = 1;

        sge.addr = (uintptr_t)(recv_region_list[i]);
        sge.length = size;
        sge.lkey = recv_mr_list[i]->lkey;

        if(ibv_post_recv(qp, &wr, &bad_wr)) {
            break;
        }
        ret_post_num++;
    }
    return ret_post_num;
}

int rdma_conn_p2p::test_post_n_send(struct ibv_pd *pd, struct ibv_qp *qp, int n, size_t size)
{
    char* send_region = (char*)malloc(size);
    struct ibv_mr *send_mr = ibv_reg_mr(pd, send_region, size, IBV_ACCESS_LOCAL_WRITE);
    ASSERT(send_mr);
    int ret_post_num = 0;
    for(int i = 0;i < n;i++)
    {
        struct ibv_send_wr wr, *bad_wr = NULL;
        struct ibv_sge sge;
        memset(&wr, 0, sizeof(wr));
        wr.wr_id = 0;
        wr.opcode = IBV_WR_SEND;
        wr.sg_list = &sge;
        wr.num_sge = 1;
        if(i == n-1) wr.send_flags = IBV_SEND_SIGNALED;

        sge.addr = (uintptr_t)send_region;
        sge.length = size;
        sge.lkey = send_mr->lkey;

        if(ibv_post_send(qp, &wr, &bad_wr)){
            break;
        }
        ret_post_num++;
    }
    IDEBUG("have already send 100 post_send\n");
    return ret_post_num;
}

void rdma_conn_p2p::test_send() {
    test_post_n_recv(send_rdma_conn.pd, send_rdma_conn.qp, 1, 8388608);//
    modify_qp_to_rtr(send_rdma_conn.qp, send_direction_qp.qpn, send_direction_qp.lid, send_rdma_conn.ib_port);
    modify_qp_to_rts(send_rdma_conn.qp);
    test_post_n_send(send_rdma_conn.pd, send_rdma_conn.qp, 100, 8388608);
}

int rdma_conn_p2p::wait_poll_send(){
    int n, i;
    struct ibv_wc wc[100];
    struct ibv_cq *cq = send_rdma_conn.cq;
    int result = 0;
    do {
        n = ibv_poll_cq(cq, 100, wc);
        if(n < 0) {
            ERROR("something wrong.\n");
            ASSERT(0);
        }
        for(int i = 0;i < n;i++)
        {
            if (wc[i].status != IBV_WC_SUCCESS)
            {
                ERROR("%s\n",ibv_wc_status_str(wc[i].status));
                ASSERT(0);
            }
        }
        result += n;
        WARN("COMPLETE num = %d\n", result);
    }while (result < 100);
    return result;
}


int rdma_conn_p2p::wait_poll_recv(){
    int n, i;
    struct ibv_wc wc[100];
    struct ibv_cq *cq = recv_rdma_conn.cq;
    int result = 0;
    do {
        n = ibv_poll_cq(cq, 100, wc);
        if(n < 0) {
            ERROR("something wrong.\n");
            ASSERT(0);
        }
        for(int i = 0;i < n;i++)
        {
            if (wc[i].status != IBV_WC_SUCCESS)
            {
                ERROR("%s\n",ibv_wc_status_str(wc[i].status));
                ASSERT(0);
            }
        }
        result += n;
        WARN("COMPLETE RECV NUM = %d\n", result);
    }while (result < 100);
    return result;
}
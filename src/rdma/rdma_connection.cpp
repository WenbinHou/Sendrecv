#include "rdma_header.h"

rdma_connection::rdma_connection(rdma_environment *env, const char* connect_ip, const uint16_t port)
    :connection(env), _remote_endpoint(connect_ip, port), peer_rest_wr(MAX_RECV_WR)
{
    //创建rdma_cm_id,并注意关联id对应的类型是conn还是用于listen
    conn_type = rdma_fd_data(this);
    CCALL(rdma_create_id(env->env_ec, &conn_id, &conn_type, RDMA_PS_TCP));
    //当前状态是未连接
    _status.store(CONNECTION_NOT_CONNECTED); 

    int test_peer_rest_wr = peer_rest_wr.load();
    ASSERT(test_peer_rest_wr == MAX_RECV_WR);
   /* _rundown.register_callback([&](){
        //****************之后在考虑怎么写这个**********
        //如果发送队列中还有数据如何处理~~~~~~~
        //
        //
        if(OnClose) OnClose(this);
        ASSERT_RESULT(conn_id);
        if(conn_qp) rdma_destroy_qp(conn_id);//!!!!!!!!!!!!!!!!!!!!是否需要
        conn_qp = nullptr;
        CCALL(rdma_destroy_id(conn_id));
        conn_id = nullptr;
        _close_finish = true;
    });*/

}

rdma_connection::rdma_connection(rdma_environment* env, struct rdma_cm_id *new_conn_id, struct rdma_cm_id *listen_id)
    :connection(env), conn_id(new_conn_id), peer_rest_wr(MAX_RECV_WR)
{
    int test_peer_rest_wr = peer_rest_wr.load();
    ASSERT(test_peer_rest_wr == MAX_RECV_WR);
    //解析本地和对方地址以及端口
    update_local_endpoint();
    update_remote_endpoint();
    build_conn_res();
 
    conn_lis = (rdma_listener*)(((rdma_fd_data*)(listen_id->context))->owner);
}

void rdma_connection::close_rdma_conn()
{
    if(OnClose){
        OnClose(this);
    }
    rdma_destroy_qp(conn_id);    
    CCALL(ibv_destroy_cq(conn_cq));
    CCALL(ibv_destroy_comp_channel(conn_comp_channel));
    CCALL(ibv_dealloc_pd(conn_pd));
    DEBUG("Destroy part of resource pf rdma_connetion.\n");
}

void rdma_connection::build_qp_attr(struct ibv_qp_init_attr *qp_attr)
{
    memset(qp_attr, 0, sizeof(*qp_attr));
    qp_attr->send_cq = conn_cq;
    qp_attr->recv_cq = conn_cq;
    qp_attr->qp_type = IBV_QPT_RC;
    qp_attr->qp_context = (void*)this;
    qp_attr->sq_sig_all = 1;
    qp_attr->cap.max_inline_data = sizeof(message)+1;
    DEBUG("The size of message is %d\n",sizeof(message));
    qp_attr->cap.max_send_wr = 2 * MAX_RECV_WR + 1;
    qp_attr->cap.max_recv_wr = 2 * MAX_RECV_WR + 1;
    qp_attr->cap.max_send_sge = MAX_SGE_NUM;
    qp_attr->cap.max_recv_sge = MAX_SGE_NUM;
}

//创建connection的相关资源包括pd,comp_channel, cq, qp
void rdma_connection::build_conn_res()
{
    ack_num = 0;
    ASSERT(conn_id->verbs);conn_ctx = conn_id->verbs;
    conn_pd = ibv_alloc_pd(conn_ctx); ASSERT(conn_pd);
    conn_comp_channel = ibv_create_comp_channel(conn_ctx); ASSERT(conn_comp_channel);
    conn_cq = ibv_create_cq(conn_ctx, CQE_MIN_NUM, this, conn_comp_channel, 0);
    ASSERT(conn_cq);
    CCALL(ibv_req_notify_cq(conn_cq, 0));
    
    /*将comm_comp_channel设置为非阻塞，并放置于_environment中的_efd_rdma_fd*/
    struct ibv_qp_init_attr qp_attr;
    build_qp_attr(&qp_attr);
    CCALL(rdma_create_qp(conn_id, conn_pd, &qp_attr));
    ASSERT(conn_id->qp); conn_qp = conn_id->qp;
    TRACE("Build rdma connection resource finished.\n");
   
    //注册MAX_RECV_WR用于接受控制信息的资源
    /*在创建完资源之后，需要进行post_receive操作，将用于接受的消息注册金qp中*/    
    for(int i = 0;i < MAX_RECV_WR;i++){
        struct ibv_recv_wr wr, *bad_wr = NULL;
        struct ibv_sge sge;

        addr_mr* addr_mr_pair = addr_mr_pool.pop();
        //暂时每次pop的时候ibv_reg_mr，push的时候ibv_demsg_mr
        message* ctl_msg = ctl_msg_pool.pop();
        struct ibv_mr *ctl_msg_mr = ibv_reg_mr(conn_pd, ctl_msg, sizeof(message),
                IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
        ASSERT(ctl_msg_mr);
        addr_mr_pair->msg_addr = ctl_msg;
        addr_mr_pair->msg_mr   = ctl_msg_mr;
        
        sge.addr = (uintptr_t)ctl_msg;
        sge.length  = sizeof(message);
        sge.lkey    = ctl_msg_mr->lkey;

        wr.wr_id   = (uintptr_t)addr_mr_pair;
        wr.next    = nullptr;
        wr.sg_list = &sge;
        wr.num_sge = 1;

        CCALL(ibv_post_recv(conn_qp, &wr, &bad_wr)); 
    }
    DEBUG("A connection  post %d recv_wr into qp.\n", MAX_RECV_WR);

    //将conn_comp_channel加入到_efd_rdma_fd
    MAKE_NONBLOCK(conn_comp_channel->fd);
    CCALL(ibv_req_notify_cq(conn_cq, 0));
    //conn_type是否已经初始化
    if(conn_type.owner == nullptr){
        conn_type = rdma_fd_data(this, conn_comp_channel->fd);
        DEBUG("Passive connection create rdma_fd_data.\n");
    }
    else {
        conn_type.fd = conn_comp_channel->fd;
        DEBUG("Active connetion update rdma_fd_data.\n");
    }
    epoll_event event;
    event.events   = EPOLLIN;
    event.data.ptr = &conn_type;
    ASSERT(_environment);
    CCALL(epoll_ctl(((rdma_environment*)_environment)->_efd_rdma_fd, EPOLL_CTL_ADD, conn_comp_channel->fd, &event));
    DEBUG("A connection[passive/active] add conn_comp_channel->fd into _efd_rdma_fd.\n");
    
}

void rdma_connection::update_local_endpoint()
{
    struct sockaddr* local_sa = rdma_get_local_addr(conn_id);
    _local_endpoint.set_endpoint(local_sa);
}

void rdma_connection::update_remote_endpoint()
{
    struct sockaddr* remote_sa = rdma_get_peer_addr(conn_id);
    _remote_endpoint.set_endpoint(remote_sa);
}

void rdma_connection::process_established()
{
    //添加rundown相关函数
    connection_status original = _status.exchange(CONNECTION_CONNECTED);
    ASSERT(original == CONNECTION_CONNECTING);
    update_local_endpoint();
    if(OnConnect){
        OnConnect(this);
    }
    ASSERT(_status.load() == CONNECTION_CONNECTED);
    //此处应该有rundown.release, 因为connect已经完成了
}

void rdma_connection::process_established_error()
{
    connection_status original = _status.exchange(CONNECTION_CONNECT_FAILED);
    ASSERT(original == CONNECTION_CONNECTING);
    if(OnConnectError){
        OnConnectError(this, errno);
    }
    //需要资源的清理？
}

bool rdma_connection::async_connect()
{
    bool need_release;
    /*if(!_rundown.try_acquire(&need_release)){
        //关于如何触发失败操作，之后在写
        if(need_release) { 
            NOTICE("Some operation should be probably done when async_connect fail\n");
        } 
        return false;
    }*/
    connection_status expect = CONNECTION_NOT_CONNECTED;
    if (!_status.compare_exchange_strong(expect, CONNECTION_CONNECTING)) { 
        ERROR("Some unexpected happened when async_connect fail\n");
        return false;
    }
    ASSERT_RESULT(conn_id);
    //rdma_resolve_addr会将conn_id绑定到某一rdma_device
    CCALL(rdma_resolve_addr(conn_id, NULL, _remote_endpoint.data(), TIMEOUT_IN_MS));
    /*
     * 如果出现错误应该怎么处理，是否需要像触发某些操作
     */
    return true;
}

bool rdma_connection::async_close()
{
    //需要有rundown相关的操作
    //_rundown.shutdown() 
    rdma_disconnect(conn_id);
    return true;
}

bool rdma_connection::async_send(const void* buffer, const size_t length)
{
    ASSERT(buffer != nullptr); ASSERT(length > 0);
    //获取rundown
    size_t queue_size = _sending_queue.size();
    if(queue_size <= 0) {
        ((rdma_environment*)_environment)->
            push_and_trigger_notification(rdma_event_data::rdma_async_send(this));
    }
    //!!!!!!!!!!!在检查一遍
    size_t cur_len = length;
    const char* cur_send = (char*)buffer;
    int push_times= 0;
    while(cur_len > 0){
        push_times++;
        size_t reg_size;
        if(cur_len > MAX_SEND_LEN) reg_size = MAX_SEND_LEN;
        else reg_size = cur_len;
        //此pending_list需要放置到wr_id上，用于在完成操作后，ibv_dereg_mr
        rdma_sge_list *pending_list = new rdma_sge_list();
        //注册内存
        ASSERT(conn_pd);
        struct ibv_mr *mr = ibv_reg_mr(conn_pd, (void*)const_cast<char*>(cur_send), reg_size, IBV_ACCESS_LOCAL_WRITE);
        pending_list->num_sge++;
        pending_list->total_length += MAX_SEND_LEN;
        //struct ibv_sge = {}
        pending_list->sge_list.push_back((struct ibv_sge){(uintptr_t)cur_send, (uint32_t)reg_size, mr->lkey});
        pending_list->mr_list.push_back(mr);
        //pending_list将其放入_sending_queue中
        _sending_queue.push(pending_list);
        if(cur_len < MAX_SEND_LEN) break;
        else{
            cur_send += MAX_SEND_LEN;
            cur_len -= MAX_SEND_LEN;
        }
    }
    size_t new_size = _sending_queue.size();
    TRACE("push %d sending data into _sending_queue(now_size:%d)\n", push_times, new_size);
   
    return true;
}

void rdma_connection::process_rdma_async_send()
{
    int cur_peer_rest_wr = peer_rest_wr.load();
    DEBUG("cur_peer_rest_wr is %d.\n", cur_peer_rest_wr);
    //tsqueue<rdma_sge_list*> _sending_queue;
    while(peer_rest_wr.load()){
        rdma_sge_list *sge_list;
        bool issuc = _sending_queue.try_pop(&sge_list);
        if(!issuc) break;
        //发送请求
        addr_mr* addr_mr_pair = addr_mr_pool.pop();
        message *ctl_msg      = ctl_msg_pool.pop();
        struct ibv_mr *ctl_msg_mr = ibv_reg_mr(conn_pd, ctl_msg, sizeof(message),
                IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
        ASSERT(ctl_msg_mr);
        addr_mr_pair->msg_addr = ctl_msg;
        addr_mr_pair->msg_mr   = ctl_msg_mr;
        
        struct ibv_send_wr wr, *bad_wr = nullptr; struct ibv_sge sge;
        ctl_msg->type = message::MSG_REQ;
        ctl_msg->data.peeding_send_size = sge_list->total_length;
        ctl_msg->send_ctx_addr = (uintptr_t)sge_list;//记录要发送的数据的位置，想不到更好的方法了
        
        sge.addr = (uintptr_t)ctl_msg;
        sge.length = sizeof(*ctl_msg);
        sge.lkey = ctl_msg_mr->lkey;
        
        wr.wr_id   = (uintptr_t)addr_mr_pair;
        wr.next    = nullptr;
        wr.opcode  = IBV_WR_SEND;
        wr.sg_list = &sge;
        wr.send_flags = IBV_SEND_INLINE;
        
        CCALL(ibv_post_send(conn_qp, &wr, &bad_wr));
        int cur_pwr = --peer_rest_wr;
        DEBUG("Sending MSG_REQ to peer :request %lld size recv_buffer, send_ctx_addr is %lld (peer rest wr is >= %d).\n", 
                (long long)ctl_msg->data.peeding_send_size, 
                (long long)ctl_msg->send_ctx_addr, cur_pwr); 
    }
}

void rdma_connection::process_poll_cq(struct ibv_cq *ret_cq, struct ibv_wc *ret_wc_array, int num_cqe)
{
    for(int i = 0;i < num_cqe;i++){
        process_one_cqe(ret_wc_array+i);    
    }
}

void rdma_connection::process_one_cqe(struct ibv_wc *wc)
{
    if(wc->status != IBV_WC_SUCCESS){
        //判断是接受还是发送
        enum ibv_wc_opcode op = wc->opcode;
        switch (op){
            case IBV_WC_RECV:{
                message* recv_ctl_msg = (message*)(uintptr_t)wc->id;
                ASSERT(recv_ctl_msg->type == message::MSG_REQ);
                size_t recv_size = recv_ctl_msg->data.peeding_send_size;
                //分配接受的地址，并注册------------------------
                char* recv_buffer = (char*)malloc(recv_size);
                struct ibv_mr *recv_buffer_mr = ibv_reg_mr(conn_pd, recv_buffer, recv_size,
                        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
                //---------------------------------------------
                addr_mr* addr_mr_pair = addr_mr_pool.pop();
                message* send_ctl_msg      = ctl_msg_pool.pop();
                struct ibv_mr *send_ctl_msg_mr = ibv_reg_mr(conn_pd, send_ctl_msg, 
                        sizeof(message), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
                ASSERT(ctl_msg_mr);
                addr_mr_pair->msg_addr     = send_ctl_msg;
                addr_mr_pair->msg_mr       = send_ctl_msg_mr;

                send_ctl_msg->type         = message::MSG_ACK;
                send_ctl_msg->data.mr.addr = (uintptr_t)recv_buffer_mr->addr;
                send_ctl_msg->data.mr.rkey = recv_buffer_mr->rkey;
                send_ctl_msg->send_ctx_addr = recv_ctl_msg->send_ctx_addr;
                //准备发送，将ctl_msg发送到对端
                struct ibv_send_wr wr, *bad_wr = nullptr; struct ibv_sge sge;
                sge.addr   = (uintptr_t)send_ctl_msg;
                sge.length = sizeof(*send_ctl_msg);
                sge.lkey   = send_ctl_msg_mr->lkey;

                wr.wr_id = (uintptr_t)addr_mr_pair; wr.opcode = IBV_WR_SEND;
                wr.next  = nullptr; wr.sg_list = &sge; wr.num_sge = 1;
                wr.send_flags = IBV_SEND_INLINE;

                CCALL(ibv_post_send(conn_qp, &wr, &bad_wr));
                
                
               /*
                * message* ctl_msg = ctl_msg_pool.pop();
        struct ibv_mr *ctl_msg_mr = ibv_reg_mr(conn_pd, ctl_msg, sizeof(message),
                IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
        ASSERT(ctl_msg_mr);
        addr_mr_pair->msg_addr = ctl_msg;
        addr_mr_pair->msg_mr   = ctl_msg_mr;
        
        sge.addr = (uintptr_t)ctl_msg;
        sge.length  = sizeof(message);
        sge.lkey    = ctl_msg_mr->lkey;

        wr.wr_id   = (uintptr_t)addr_mr_pair;
        wr.next    = nullptr;
        wr.sg_list = &sge;
        wr.num_sge = 1;

        CCALL(ibv_post_recv(conn_qp, &wr, &bad_wr));
                * */
                
                DEBUG("");
                break;
            }
            case IBV_WC_RECV_RDMA_WITH_IMM:{
                break;
            }
            case IBV_WC_SEND:{
                break;
            }
            case IBV_WR_RDMA_WRITE_WITH_IMM:{
                break;
            }
            default:{
                FATAL("cannot handle ibv_wc_opcode %d.\n", op);
                ASSERT(0);break;
            }
        }
        else{

        }

    }
}

bool rdma_connection::start_receive()
{
    //
    return true;
}

bool rdma_connection::async_send_many(const std::vector<fragment> frags)
{
    //
    return true;
}

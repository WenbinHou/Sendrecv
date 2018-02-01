#include "rdma_header.h"

rdma_connection::rdma_connection(rdma_environment *env, const char* connect_ip, const uint16_t port)
    :connection(env), _remote_endpoint(connect_ip, port)
{
    //创建rdma_cm_id,并注意关联id对应的类型是conn还是用于listen
    conn_type = rdma_fd_data(this);
    CCALL(rdma_create_id(env->env_ec, &conn_id, &conn_type, RDMA_PS_TCP));
    //当前状态是未连接
    _status.store(CONNECTION_NOT_CONNECTED); 

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
    :connection(env), conn_id(new_conn_id)
{
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
    qp_attr->cap.max_send_wr = 5;
    qp_attr->cap.max_recv_wr = 5;
    qp_attr->cap.max_send_sge = 1;
    qp_attr->cap.max_recv_sge = 1;
}

//创建connection的相关资源包括pd,comp_channel, cq, qp
void rdma_connection::build_conn_res()
{
    ASSERT(conn_id->verbs);conn_ctx = conn_id->verbs;
    conn_pd = ibv_alloc_pd(conn_ctx); ASSERT(conn_pd);
    conn_comp_channel = ibv_create_comp_channel(conn_ctx); ASSERT(conn_comp_channel);
    conn_cq = ibv_create_cq(conn_ctx, CQE_MIN_NUM, this, conn_comp_channel, 0);
    CCALL(ibv_req_notify_cq(conn_cq, 0));
    
    /*将comm_comp_channel设置为非阻塞，并放置于_environment中的_efd_rdma_fd*/
    struct ibv_qp_init_attr qp_attr;
    build_qp_attr(&qp_attr);
    CCALL(rdma_create_qp(conn_id, conn_pd, &qp_attr));
    ASSERT(conn_id->qp); conn_qp = conn_id->qp;
    TRACE("Build rdma connection resource finished.\n");
   
    /*在创建完资源之后，需要进行post_receive操作，将用于接受的消息注册金qp中*/    
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
    //
    return true;
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

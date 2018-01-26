#include "rdma_header.h"
#define WAIT_ADDR_RESOLVE_TIME 500

rdma_connection::rdma_connection(rdma_environment *env, const char* connect_ip, const uint16_t port)
    :connection(env), _remote_endpoint(connect_ip, port)
{
    //创建rdma_cm_id,并注意关联id对应的类型是conn还是用于listen
    conn_type = rdma_fd_data(this);
    CCALL(rdma_create_id(env->conn_ec, &conn_id, &conn_type, RDMA_PS_TCP));
    //替换conn_id中的verbs参数，请务必一定可以这样做
    conn_id->verbs = env->ctx;//这句话好像没有意义
    //当前状态是未连接
    _status.store(CONNECTION_NOT_CONNECTED); 

    _rundown.register_callback([&](){
        /****************之后在考虑怎么写这个**********/
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
    });

}
//rdma_connection::rdma_connection(rdma_environment* env, const rdma_conn_fd connfd, const endpoint& remote_ep)

bool rdma_connection::async_connect()
{
    //rdma_resolve_addr会将conn_id绑定到某一rdma_device
    CCALL(rdma_resolve_addr(conn_id, NULL, _remote_endpoint.data(), WAIT_ADDR_RESOLVE_TIME));
    return true;
}

bool rdma_connection::async_close()
{
    //
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

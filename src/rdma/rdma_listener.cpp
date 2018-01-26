#include "rdma_header.h"
// rdma_fd_data
rdma_listener::rdma_listener(rdma_environment *env, const char* bind_ip, const uint16_t port):
listener(env), _bind_endpoint(bind_ip, port)
{
    listen_type = rdma_fd_data(this);
    CCALL(rdma_create_id(env->conn_ec, &listener, &listen_type, RDMA_PS_TCP));
    ASSERT(listener);
    //冒险操作
    listener->verbs = env->ctx;
    CCALL(rdma_bind_addr(listener, _bind_endpoint.data()));
    //以下写法的正确性
    _start_accept_required.store(false);

}

rdma_listen::start_accept()
{
}

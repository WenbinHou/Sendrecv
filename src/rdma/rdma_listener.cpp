#include "rdma_header.h"
// rdma_fd_data
rdma_listener::rdma_listener(rdma_environment *env, const char* bind_ip, const uint16_t port):
listener(env), _bind_endpoint(bind_ip, port)
{
    listen_type = rdma_fd_data(this);
    CCALL(rdma_create_id(env->conn_ec, &listener_rdma_id, &listen_type, RDMA_PS_TCP));
    ASSERT(listener_rdma_id);
    //冒险操作
    listener_rdma_id->verbs = env->ctx;
    CCALL(rdma_bind_addr(listener_rdma_id, _bind_endpoint.data()));
    //按照文彬的方式
    _start_accept_required.store(false);
    _rundown.register_callback([&](){
        if(OnClose){
            OnClose(this);
        }
        ASSERT_RESULT(listener_rdma_id);
        CCALL(rdma_destroy_id(listener_rdma_id));
        listener_rdma_id = nullptr;
        _close_finished = true;
    });
}

bool rdma_listener::start_accept()
{
    //
    return true;
}

bool rdma_listener::async_close()
{
    //
    return true;
}

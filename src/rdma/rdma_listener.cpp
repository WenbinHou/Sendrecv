#include "rdma_header.h"
// rdma_fd_data
rdma_listener::rdma_listener(rdma_environment *env, const char* bind_ip, const uint16_t port):
listener(env), _bind_endpoint(bind_ip, port)
{
    listen_type = rdma_fd_data(this);
    CCALL(rdma_create_id(env->env_ec, &listener_rdma_id, &listen_type, RDMA_PS_TCP));
    ASSERT(listener_rdma_id);
    CCALL(rdma_bind_addr(listener_rdma_id, _bind_endpoint.data()));
    //按照文彬的方式
    _start_accept_required.store(false);
   /* _rundown.register_callback([&](){
        if(OnClose){
            OnClose(this);
        }
        ASSERT_RESULT(listener_rdma_id);
        CCALL(rdma_destroy_id(listener_rdma_id));
        listener_rdma_id = nullptr;
        _close_finished = true;
    });*/
}
void rdma_listener::process_accept_success(rdma_connection* new_rdma_conn)
{
    //需要添加关于rundown相关操作
    ASSERT(new_rdma_conn);
    ASSERT(OnAccept);
    OnAccept(this, new_rdma_conn);
}
void rdma_listener::process_accept_fail()
{
    if(OnAcceptError){
        OnAcceptError(this, errno);
    }
}

bool rdma_listener::start_accept()
{
    ASSERT(OnAccept != nullptr);
    bool expect = false;
    if (!_start_accept_required.compare_exchange_strong(expect, true)) {
        return false;
    }
    //注意此处有rundown 获取操作
    /*bool need_release;
    if(!_rundown.try_acquire(&need_release)){
        if(need_release){
            //处理一些操作
        }
    }*/
    CCALL(rdma_listen(listener_rdma_id, INT_MAX));
    return true;
}

bool rdma_listener::async_close()
{
    //此处随意写了一下，之后改
    if(OnClose)
        OnClose(this);
    return true;
}

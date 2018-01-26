#ifndef _RDMA_LISTENER_H
#define _RDMA_LISTENER_H
#include "rdma_header"
class rdma_listener : public listener
{
    friend class rdma_environment;
    friend class rdma_connection;
public:
    bool start_accept() override;
    //bool async_close() override;
    
    endpoint bind_endpoint() const{return _bind_endpoint};
private:
    rdma_listener(rdma_environment *env, const char* bind_ip, const uint16_t port);
private:
    struct rdma_cm_id *listener;
    endpoint _bind_endpoint;
    rdma_fd_data listen_type;

    std::atomic_bool _start_accept_required;
    std::atomic_bool _close_required;

    volatile bool _close_finished = false;
    rundown_protection _rundown;

};

#endif 

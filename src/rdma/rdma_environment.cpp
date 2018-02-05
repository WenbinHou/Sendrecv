#include "rdma_header.h"
#include <net.h>
#include <sys/epoll.h>
#define CQ_SIZE 100

rdma_environment::rdma_environment()
{
    env_ec = rdma_create_event_channel();
    _efd_rdma_fd = CCALL(epoll_create1(EPOLL_CLOEXEC));
    _notification_event_rdma_fd = CCALL(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK));
    _notification_event_rdma_fddata = rdma_fd_data(this, _notification_event_rdma_fd);

    epoll_add(&_notification_event_rdma_fddata, EPOLLIN | EPOLLET);
    //开启两个线程来进行连接和收发处理
    _loop_thread_connection = new std::thread([this](){
        connection_loop();
        rdma_destroy_event_channel(env_ec);
        CCALL(close(_efd_rdma_fd));
        _efd_rdma_fd = INVALID_FD;
    }); 
    _loop_thread_cq = new std::thread([this](){
        sendrecv_loop();
        CCALL(close(_notification_event_rdma_fd));
        _notification_event_rdma_fd = INVALID_FD;
        CCALL(close(_efd_rdma_fd));
        _efd_rdma_fd = INVALID_FD;
    });

}

rdma_environment::~rdma_environment()
{
}

void rdma_environment::epoll_add(rdma_fd_data* fddata, const uint32_t events) const
{
    ASSERT(fddata); ASSERT_RESULT(fddata->fd);
    epoll_event event; event.events = events; event.data.ptr = fddata;
    CCALL(epoll_ctl(_efd_rdma_fd, EPOLL_CTL_ADD, fddata->fd, &event));
}

void rdma_environment::push_and_trigger_notification(const rdma_event_data& notification)
{
    const size_t new_size = _notification_rdma_queue.push(notification);
    TRACE("push a rdma_event_data into the _notification_rdma_queue(size:%d).\n", new_size);
    ASSERT_RESULT(_notification_event_rdma_fd);
    if(new_size == 1){
        uint64_t value = 1;
        CCALL(write(_notification_event_rdma_fd, &value, sizeof(value)));
    }
    else{
        DEBUG("skip write(_notification_event_rdma_fd): queued notification count = %lld\n",(long long)new_size);    
    }
}

void rdma_environment::dispose()
{
    _loop_thread_connection->join();
}

void rdma_environment::build_params(struct rdma_conn_param *params)
{
    memset(params, 0, sizeof(*params));
    params->retry_count = 10;
    params->rnr_retry_count = 10;
}

void rdma_environment::connection_loop()
{
    DEBUG("ENVIRONMENT start connection_loop.\n");
    struct rdma_cm_event *event = nullptr;
    struct rdma_conn_param cm_params;
    build_params(&cm_params);
    while(rdma_get_cm_event(env_ec, &event) == 0){
        struct rdma_cm_event event_copy;
        memcpy(&event_copy, event, sizeof(*event));
        rdma_ack_cm_event(event);

        switch(event_copy.event){
            case RDMA_CM_EVENT_ADDR_RESOLVED:{
                rdma_connection *conn = (rdma_connection*)(((rdma_fd_data*)(event_copy.id->context))->owner);
                conn->build_conn_res();
                CCALL(rdma_resolve_route(event_copy.id, TIMEOUT_IN_MS));
                break;                               
            }
            case RDMA_CM_EVENT_ROUTE_RESOLVED:{
                CCALL(rdma_connect(event_copy.id, &cm_params));
                TRACE("Finish rdma_cm_event_rout_resolved event.\n");
                break;                        
            }
            case RDMA_CM_EVENT_CONNECT_REQUEST:{
                rdma_connection *new_conn = create_rdma_connection_passive(event_copy.id, event_copy.listen_id);
                //if(((rdma_fd_data*)(event_copy.listen_id->context))->owner) printf("------------------\n");
                //event_copy.id->context = new_conn; 
                if(map_id_conn.find((intptr_t)(event_copy.id)) == map_id_conn.end()){
                    map_id_conn[(intptr_t)(event_copy.id)] = new_conn;
                }
                else{
                    FATAL("Cannot fix bug when handle rdma_cm_event_connect_request.\n");
                    ASSERT(0);
                }
                //记录id和
                CCALL(rdma_accept(event_copy.id, &cm_params));
                TRACE("Listener finish rdma_cm_event_connect_request event.\n");
                break;                        
            }                                   
            case RDMA_CM_EVENT_ESTABLISHED:{
                DEBUG("handle rdma_cm_event_established event.\n");
                if(map_id_conn.find((intptr_t)(event_copy.id)) == map_id_conn.end()){
                    rdma_connection *conn = (rdma_connection*)(((rdma_fd_data*)(event_copy.id->context))->owner);
                    conn->process_established();
                    NOTICE("%s to %s active connection established.\n",
                            conn->local_endpoint().to_string().c_str(), 
                            conn->remote_endpoint().to_string().c_str());
                }
                else{
                    rdma_connection *new_passive_conn = map_id_conn[(intptr_t)(event_copy.id)];
                    rdma_listener *listen = new_passive_conn->conn_lis;
                    listen->process_accept_success(new_passive_conn);
                    NOTICE("%s from %s passive connection established.\n",
                        new_passive_conn->local_endpoint().to_string().c_str(),
                        new_passive_conn->remote_endpoint().to_string().c_str());
                }
                //!!!!!!!!!!!!!!这之后需要将comp_channel和_efd_conn_关联!!!!!!!!!!!!!!!!!!
                break;                        
            }
            case RDMA_CM_EVENT_DISCONNECTED:{
                DEBUG("handle rdma_cm_event_disconnected event.\n");
                rdma_connection *conn = nullptr;
                if(map_id_conn.find((intptr_t)(event_copy.id)) == map_id_conn.end()){
                    conn = (rdma_connection*)(((rdma_fd_data*)(event_copy.id->context))->owner);
                }
                else{
                    conn = map_id_conn[(intptr_t)(event_copy.id)];
                    map_id_conn.erase((intptr_t)(event_copy.id));
                }
                conn->close_rdma_conn();
                delete conn;
                rdma_destroy_id(event_copy.id);
                break; 
            }
            case RDMA_CM_EVENT_ADDR_ERROR:
            case RDMA_CM_EVENT_ROUTE_ERROR:
            case RDMA_CM_EVENT_UNREACHABLE:
            case RDMA_CM_EVENT_REJECTED:{
                if(event_copy.id && event_copy.id->context){
                    rdma_connection *conn = (rdma_connection*)(((rdma_fd_data*)(event_copy.id->context))->owner);
                    conn->process_established_error();
                } 
                break;
            }
            case RDMA_CM_EVENT_CONNECT_ERROR:{
                if(event_copy.listen_id && event_copy.listen_id->context){
                    rdma_listener* lis = (rdma_listener*)(((rdma_fd_data*)(event_copy.listen_id->context))->owner);
                    lis->process_accept_fail();
                }
                else if(event_copy.id && event_copy.id->context){
                    rdma_connection *conn = (rdma_connection*)(((rdma_fd_data*)(event_copy.id->context))->owner);
                    conn->process_established_error();
                }
                else{
                    FATAL("BUG: Cannot handle error event %s because cannot find conn/listen object.\n", rdma_event_str(event_copy.event));
                    ASSERT(0);break;
                }
                break;
            }
            default:{
                FATAL("BUG: Cannot handle event:%s\n",rdma_event_str(event_copy.event));
                ASSERT(0);
                break;
            }
        }
    }
   return;
}

void rdma_environment::sendrecv_loop()
{
    DEBUG("ENVIRONMENT start sendrecv_loop.\n");
    const int EVENT_BUFFER_COUNT = 256;
    epoll_event* events_buffer = new epoll_event[EVENT_BUFFER_COUNT];
    struct ibv_wc ret_wc_array[CQE_MIN_NUM];
    while(true){
        const int readyCnt = epoll_wait(_efd_rdma_fd, events_buffer, 
                EVENT_BUFFER_COUNT, /*infinity*/-1);
        if(readyCnt<0){
            const int error = errno;
            if(error == EINTR) continue;
            ERROR("[rdma_environment] epoll_wait failed with %d (%s)\n", 
                error, strerror(error));
            break;
        }
        for(int i = 0;i < readyCnt;++i){
            const uint32_t curr_events = events_buffer[i].events;
            const rdma_fd_data* curr_rdmadata = (rdma_fd_data*)events_buffer[i].data.ptr;
            switch(curr_rdmadata->type){
                case rdma_fd_data::RDMATYPE_NOTIFICATION_EVENT:{
                    TRACE("trigger rdma_eventfd = %d\n", curr_rdmadata->fd);
                    ASSERT(this == curr_rdmadata->owner);
                    ASSERT(curr_rdmadata->fd == _notification_event_rdma_fd);
                    this->process_epoll_env_notificaton_event_rdmafd(curr_events);
                    break;                                            
                }
                case rdma_fd_data::RDMATYPE_ID_CONNECTION:{
                    //表示当前已经有完成任务发生了，可以通过ibv_get_cq_event获取
                    struct ibv_cq *ret_cq; void *ret_ctx; struct ibv_wc wc;
                    rdma_connection *conn = (rdma_connection*)curr_rdmadata->owner;
                    CCALL(ibv_get_cq_event(conn->conn_comp_channel, &ret_cq, &ret_ctx));
                    conn->ack_num++;
                    if(conn->ack_num == ACK_NUM_LIMIT){ 
                        ibv_ack_cq_events(ret_cq, conn->ack_num);
                        conn->ack_num = 0;
                    }
                    CCALL(ibv_req_notify_cq(ret_cq, 0));
                    int num_cqe;
                    while(num_cqe = ibv_poll_cq(ret_cq, CQE_MIN_NUM, ret_wc_array)){
                        conn->process_poll_cq(ret_cq, ret_wc_array, num_cqe);
                    }
                    break;
                }
                default:{
                    FATAL("BUG: Unknown rdma_fd_data: %d\n", (int)curr_rdmadata->type);
                    ASSERT(0);break;
                }
            }
        }
        //此处要加入一个判断是否需要停止loop的东西
        
    }
    return;
}

//目前要处理的内容只有发送操作
void rdma_environment::process_epoll_env_notificaton_event_rdmafd(const uint32_t events)
{
    uint64_t dummy; ASSERT(events & EPOLLIN);
    CCALL(read(_notification_event_rdma_fd, &dummy, sizeof(dummy)));
    //处理_notification_rdma_queue中的事物
    rdma_event_data evdata;
    while(_notification_rdma_queue.try_pop(&evdata)){
        switch(evdata.type){
            case rdma_event_data::RDMA_EVENTTYPE_ASYNC_SEND:{
                rdma_connection* conn = (rdma_connection*)evdata.owner;
                conn->process_rdma_async_send();
                
            }
        }
    }
}


rdma_connection* rdma_environment::create_rdma_connection(const char* connect_ip, const uint16_t port)
{
   return new rdma_connection(this, connect_ip, port); 
}

rdma_listener* rdma_environment::create_rdma_listener(const char* bind_ip, const uint16_t port)
{
    return new rdma_listener(this, bind_ip, port);
}

rdma_connection* rdma_environment::create_rdma_connection_passive(struct rdma_cm_id *new_conn, struct rdma_cm_id *listen_id)
{
    return new rdma_connection(this, new_conn, listen_id);
}


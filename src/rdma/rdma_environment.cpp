#include "rdma_header.h"
#include <sys/epoll.h>
#define CQ_SIZE 100

rdma_environment::rdma_environment()
{
    env_ec = rdma_create_event_channel();
    _efd_rdma_fd = CCALL(epoll_create1(EPOLL_CLOEXEC));
    //开启两个线程来进行连接和收发处理
    _loop_thread_connection = new std::thread([this](){
        connection_loop();
        rdma_destroy_event_channel(env_ec);
        CCALL(close(_efd_rdma_fd));
        _efd_rdma_fd = INVALID_FD;
    }); 
}

rdma_environment::~rdma_environment()
{
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
    struct rdma_cm_event *event = nullptr;
    struct rdma_conn_param cm_params;
    build_params(&cm_params);
    while(rdma_get_cm_event(env_ec, &event) == 0){
        struct rdma_cm_event event_copy;
        memcpy(&event_copy, event, sizeof(*event));
        rdma_ack_cm_event(event);

        switch(event_copy.event){
            case RDMA_CM_EVENT_ADDR_RESOLVED:{
                rdma_connection *conn = (rdma_connection*)(((rdma_fd_type*)(event_copy.id->context))->owner);
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
                //if(((rdma_fd_type*)(event_copy.listen_id->context))->owner) printf("------------------\n");
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
                    rdma_connection *conn = (rdma_connection*)(((rdma_fd_type*)(event_copy.id->context))->owner);
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
                    conn = (rdma_connection*)(((rdma_fd_type*)(event_copy.id->context))->owner);
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
                    rdma_connection *conn = (rdma_connection*)(((rdma_fd_type*)(event_copy.id->context))->owner);
                    conn->process_established_error();
                } 
                break;
            }
            case RDMA_CM_EVENT_CONNECT_ERROR:{
                if(event_copy.listen_id && event_copy.listen_id->context){
                    rdma_listener* lis = (rdma_listener*)(((rdma_fd_type*)(event_copy.listen_id->context))->owner);
                    lis->process_accept_fail();
                }
                else if(event_copy.id && event_copy.id->context){
                    rdma_connection *conn = (rdma_connection*)(((rdma_fd_type*)(event_copy.id->context))->owner);
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
    //**********
    return;
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


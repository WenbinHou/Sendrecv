#include "rdma_header.h"
#define CQ_SIZE 100

rdma_environment::rdma_environment()
{
    //创建ibv_context，此ibv_context 需要替换所有rdma_create_id创建的id中的verbs
    dev_list = ibv_get_device_list(&num_devices); 
    ASSERT(dev_list); ASSERT(num_devices > 0);
    ctx = ibv_open_device(dev_list[0]); ASSERT(ctx);
    DEBUG("RDMA_device %s have already open.\n", ibv_get_device_name(dev_list[0]));
    pd = ibv_alloc_pd(ctx); ASSERT(pd);
    comp_channel = ibv_create_comp_channel(ctx); ASSERT(comp_channel);
    cq = ibv_create_cq(ctx, CQ_SIZE, NULL, comp_channel, 0); ASSERT(cq);
    conn_ec = rdma_create_event_channel();
    //开启两个线程来进行连接和收发处理

    
}
rdma_environment::~rdma_environment()
{
    rdma_destroy_event_channel(conn_ec);
    CCALL(ibv_destroy_cq(cq));
    CCALL(ibv_destroy_comp_channel(comp_channel));
    CCALL(ibv_dealloc_pd(pd));
    CCALL(ibv_close_device(ctx));
    ibv_free_device_list(dev_list);
}

void rdma_environment::connection_loop()
{
   //*********** 
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



#include <rdma/rdma_cma.h>
#include "rdma_connection.h"
rdma_connection::rdma_connection(rdma_environment *env, const char* connect_ip, const uint16_t port)
    :connection(env), _remote_endpoint(remote_ip, port)
{
    //创建rdma_cm_id,并注意关联id对应的类型是conn还是用于listen
    conn_type = rdma_fd_data(this);
    CCALL(rdma_create_id(env->conn_ec, &conn_id, &conn_type, RDMA_PS_TCP));
    //替换conn_id中的verbs参数，请务必一定可以这样做
    conn_id->verbs = env->ctx;
}



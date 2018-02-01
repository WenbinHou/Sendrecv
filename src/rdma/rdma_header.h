#pragma once
#include <sendrecv.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

#define TIMEOUT_IN_MS 500
#define CQE_MIN_NUM 10
class rdma_connection;
class rdma_listener;
class rdma_environment;

//因为要通过channel获取到对应event中的rdma_cm_id的类型，所以需要rdma_fd_data这个结构体
typedef struct rdma_fd_data{
    enum rdma_type{
        RDMATYPE_UNKNOWN = 0,
        RDMATYPE_ID_CONNECTION,
        RDMATYPE_ID_LISTENER,
    };
    rdma_type type;
    void* owner;
public:
    rdma_fd_data():type(RDMATYPE_UNKNOWN),owner(nullptr) {}
    rdma_fd_data(rdma_connection * conn):type(RDMATYPE_ID_CONNECTION), owner(conn) {}
    rdma_fd_data(rdma_listener * listener):type(RDMATYPE_ID_LISTENER), owner(listener){ }
}rdma_fd_type;

//
#include "rdma_connection.h"
#include "rdma_listener.h"
#include "rdma_environment.h"

#pragma once
#include <sendrecv.h>
#include <net.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <vector>
#define TIMEOUT_IN_MS 500
#define MAX_SEND_LEN 2147483648
#define MAX_RECV_WR 20
#define CQE_MIN_NUM (MAX_RECV_WR*4+1) 
#define MAX_SGE_NUM 10
#define ACK_NUM_LIMIT 1 //暂时设置为1
class rdma_connection;
class rdma_listener;
class rdma_environment;

//因为要通过channel获取到对应event中的rdma_cm_id的类型，所以需要rdma_fd_data这个结构体
typedef struct rdma_fd_data{
    enum rdma_type{
        RDMATYPE_UNKNOWN = 0,
        RDMATYPE_NOTIFICATION_EVENT,
        RDMATYPE_ID_CONNECTION,
        RDMATYPE_ID_LISTENER,
    };
    rdma_type type;
    void* owner;
    int fd;
public:
    rdma_fd_data():type(RDMATYPE_UNKNOWN),owner(nullptr) {}
    rdma_fd_data(rdma_connection *conn)
        :type(RDMATYPE_ID_CONNECTION), owner(conn), fd(INVALID_FD) { }
    rdma_fd_data(rdma_connection * conn, const int conn_fd)
        : type(RDMATYPE_ID_CONNECTION), owner(conn), fd(conn_fd) {}
    rdma_fd_data(rdma_listener * listener, const int lis_fd)
        : type(RDMATYPE_ID_LISTENER), owner(listener), fd(lis_fd){ }
    rdma_fd_data(rdma_environment* rdma_env, const int eventfd)
        :type(RDMATYPE_NOTIFICATION_EVENT), fd(eventfd), owner(rdma_env){}
    rdma_fd_data(rdma_listener * listener)
        : type(RDMATYPE_ID_LISTENER), owner(listener), fd(INVALID_FD){ }

}rdma_fd_data;

typedef struct rdma_event_data
{
    enum rdma_event_type
    {
        RDMA_EVENTTPYE_UNKNOWN = 0,
        RDMA_EVENTTYPE_ASYNC_SEND,
        RDMA_EVENTTYPE_MAX,
    };
    rdma_event_type type;
    void* owner;
    static rdma_event_data rdma_async_send(connection* conn) {
        return rdma_event_data{RDMA_EVENTTYPE_ASYNC_SEND, conn};
    }
}rdma_event_data;
//一个rdma_sge_list表示要一起rdma write的数据
typedef struct rdma_sge_list
{
    std::vector<struct ibv_sge> sge_list;
    size_t num_sge;
    size_t total_length;//所有待发送的数据的大小（不能超过2G）
    std::vector<struct ibv_mr*> mr_list;
public:
    rdma_sge_list(): num_sge(0), total_length(0) {}
}rdma_sge_list;


//控制消息
typedef struct message{
    enum msg_type{
        MSG_INVALID = 0,
        MSG_REQ,
        MSG_ACK,
        MSG_MAX,
    };
    msg_type type;
    union{
        struct{
            uint64_t addr;
            uint32_t rkey;
        }mr;
        size_t peeding_send_size;
    }data;
    uintptr_t send_ctx_addr;
}message;

//记录addr和mr之间的关系
typedef struct addr_mr{
    message *msg_addr;
    ibv_mr  *msg_mr;
}addr_mr;

#include "rdma_connection.h"
#include "rdma_listener.h"
#include "rdma_environment.h"
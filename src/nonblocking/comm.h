#ifndef SENDRECV_COMM_H
#define SENDRECV_COMM_H

#define BASE_PORT 8800
#include <vector>
#include "utils.h"
#include "sendrecv.h"

class comm {
public:
    comm(const comm&) = delete;
    comm(comm && ) = delete;
    comm & operator=(const comm&) = delete;
    ~comm() = default;

private:
    int rank;
    int size;
    int my_listen_port;
    std::vector<nodeinfo> nodelist;
    std::vector<connection*> send_conn_list;
    std::vector<connection*> recv_conn_list;
    std::vector<bool>        is_recv_init_list;//means the respective recv_conn whether finished init(recv the rank)
    std::vector<bool>        is_send_init_list;//means the respectiive send conn whether finished init(send the rank)
    socket_environment env;
    socket_listener *lis;

    pool<datahead> datahead_pool;//used for get space for the head of send data
public:
    comm();
    bool init(int rank, int size, vector<nodeinfo> &nodelist);
    bool isend(int dest, const void *buf, size_t count, send_handler *req);
    bool irecv(int src,  void *buf, size_t count, recv_handler *req);
    bool wait(handler *req);

private:
    void set_send_connection_callback(int conn_rank, connection * conn);
    void set_recv_connection_callback(connection * conn);
};


#endif //SENDRECV_COMM_H

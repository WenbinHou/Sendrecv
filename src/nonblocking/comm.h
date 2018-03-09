#ifndef SENDRECV_COMM_H
#define SENDRECV_COMM_H

#define BASE_PORT 11111
#include <vector>
#include <mutex>
#include <condition_variable>
#include "utils.h"
#include "sendrecv.h"

class comm {
public:
    comm(const comm&) = delete;
    comm(comm && ) = delete;
    comm & operator=(const comm&) = delete;
    ~comm() {
        WARN("[Rank %d]ready to ~comm().\n", rank);
        env.dispose();
    }

private:
    int  rank;
    int  size;
    char my_listen_ip[16];//current only for ipv4
    int  my_listen_port;
    std::vector<nodeinfo> nodelist;
    std::vector<connection*> send_conn_list;
    std::vector<connection*> recv_conn_list;
    std::vector<bool>        is_send_init_list;//means the respectiive send conn whether finished init(send the rank)
    socket_environment env;
    socket_listener *lis;

    pool<datahead> datahead_pool;//used for get space for the head of send data

    int has_conn_num;
    std::mutex mtx;
    std::condition_variable cv;

    int close_conn_num;
public:
    comm();
    bool init(int rank, int size, std::vector<nodeinfo> &nodelist);
    bool isend(int dest, const void *buf, size_t count, handler *req);
    //handler or send_handler
    bool irecv(int src,  void *buf, size_t count, handler *req);
    //handler or recv_handler
    bool wait(handler *req);
    bool finalize();

    int get_size() {return size;}
    int get_rank() {return rank;}

private:
    void set_send_connection_callback(int conn_rank, connection * conn);
    void set_recv_connection_callback(connection * conn);
};


#endif
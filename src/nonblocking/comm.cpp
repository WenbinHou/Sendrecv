#include "comm.h"

comm::comm() {

}

bool comm::init(int rank, int size, vector <nodeinfo> &nodelist) {
    ASSERT(size);
    send_conn_list.resize(size); recv_conn_list.resize(size);
    is_send_init_list.resize(size); memset(&is_send_init_list[0], 0, sizeof(bool)*size);
    is_recv_init_list.resize(size); memset(&is_recv_init_list[0], 0, sizeof(bool)*size);

    lis = env.create_listener();
    //register the listen function;
    lis->OnAccept = [&](listener*,  connection* conn) {
        set_recv_connection_callback(conn);
    };

    lis->OnAcceptError = [&](listener*, const int error) {

    };

    lis->OnClose =[&](listener*) {

    };
    bool success = lis->start_accept();
    TEST_ASSERT(success);
    int i = 0;
    for(const nodeinfo& node : nodelist){
        socket_connection* conn = env.create_connection(node.ip_addr, node.listen_port);
        set_connection_callback(i, conn);
        send_conn_list.push_back(conn);
        success = conn->async_connect();
        TEST_ASSERT(success);
        ITRACE("[init]: connecting to rank %d ip(%s) port(%d)\n", i, node.ip_addr, node.listen_port);
        i++;
    }
}

void comm::set_send_connection_callback(int conn_rank, connection *conn) {
    conn->OnConnect = [conn_rank](connection* conn){
        conn->start_receive();//you can doesn't call this function
        datahead* head = datahead_pool.pop();
        //send the message contain the rank of comm

        head->type = HEAD_TYPE_INIT;
        head->content_size = rank;
        bool success = conn->async_send(head, sizeof(datahead));
        ASSERT(success);

        IDEBUG("[OnConnect] send_connection with rank %d ;nodeinfo:%s established. Sending INIT msg\n",
               conn_rank,((socket_connection*)conn)->remote_endpoint().to_string());
    };

    conn->OnConnectError = [&](connection *conn, const int error){
        usleep(10);

    };

    conn->OnClose = [&](connection* conn){

    };

    conn->OnHup = [&](connection*, const int error){

    };

    conn->OnReceive = [&](connection* conn, const void* buffer, const size_t length) {

    };

    conn->OnSend = [&](connection*, const void* buffer, const size_t length) {

    };

    conn->OnSendError = [&](connection*, const void* buffer, const size_t length, const size_t sent_length, const int error) {

    };
}

void comm::set_recv_connection_callback(connection * conn){

    conn->OnClose = [&](connection* conn){

    };

    conn->OnHup = [&](connection*, const int error){

    };

    conn->OnReceive = [&](connection* conn, const void* buffer, const size_t length) {

    };

    conn->OnSend = [&](connection*, const void* buffer, const size_t length) {

    };

    conn->OnSendError = [&](connection*, const void* buffer, const size_t length, const size_t sent_length, const int error) {

    };

}

bool comm::isend(int dest, const void *buf, size_t count, send_handler *req) {

    return true;
}

bool comm::irecv(int src, void *buf, size_t count, recv_handler *req) {

    return true;
}

bool comm::wait(handler *req) {
    return true;
}
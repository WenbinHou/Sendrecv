#include <sendrecv.h>
#include "comm.h"
#define _min(a,b)    (((a) < (b)) ? (a) : (b))

comm::comm() {

}

//this is a blocking function
bool comm::init(int rank, int size, std::vector<nodeinfo> &nlist) {
    this->nodelist.assign(nlist.begin(), nlist.end());
    ASSERT(size); ASSERT(size == this->nodelist.size());
    this->rank = rank; this->size = size;
    memcpy(my_listen_ip, nodelist[rank].ip_addr, 16);
    my_listen_port = nodelist[rank].listen_port;
    ITRACE("Start init rank %d in all %d process[ ip(%s), port(%d)].\n",
           this->rank, this->size, my_listen_ip, my_listen_port);
    send_conn_list.resize(size); recv_conn_list.resize(size);

    is_send_init_list.assign(size, false);
    has_conn_num   = 0;
    close_conn_num = 0;

    lis = env.create_listener(my_listen_ip, my_listen_port);
    //register the listen function;
    lis->OnAccept = [&](listener*,  connection* conn) {
        set_recv_connection_callback(conn);
        conn->start_receive();
    };

    lis->OnAcceptError = [&](listener*, const int error) {
        ERROR("[rank %d OnAcceptError]......\n", this->rank);
    };

    lis->OnClose =[&](listener*) {
        IDEBUG("you can deal with it on close.\n");
    };
    bool success = lis->start_accept();
    ASSERT(success);
    usleep(100);
    int i = 0;
    for(const nodeinfo& node : nodelist){
        socket_connection* send_conn = env.create_connection(node.ip_addr, node.listen_port);
        set_send_connection_callback(i, send_conn);
        success = send_conn->async_connect();
        ASSERT(success);
        TRACE("[rank %d]: try connecting to rank %d ip(%s) port(%d)\n", this->rank, i, node.ip_addr, node.listen_port);
        i++;
    }
    std::unique_lock<std::mutex> lck(mtx);
    while(has_conn_num != 2 * this->size)
        cv.wait(lck);
    lck.unlock();
    IDEBUG("[rank %d] Have finished the init [has_conn_num = %d]\n", this->rank, has_conn_num);
}

void comm::set_send_connection_callback(int peer_rank, connection *send_conn) {
    send_conn->OnConnect = [peer_rank, this](connection* conn){
        //conn->start_receive();//you can doesn't call this function
        send_conn_list[peer_rank] = conn;
        datahead* head = datahead_pool.pop();
        //send the message contain the rank of comm

        head->type = HEAD_TYPE_INIT;
        head->content_size = 0;
        head->src  = this->rank;
        head->dest = peer_rank;
        bool success = conn->async_send(head, sizeof(datahead));
        ASSERT(success);

        IDEBUG("[***********rank %d OnConnect] send_connection with rank %d ;nodeinfo:%s established. Sending INIT msg\n",
               this->rank, peer_rank,((socket_connection*)conn)->remote_endpoint().to_string().c_str());
    };



    send_conn->OnClose = [peer_rank, this](connection* conn){
        if(conn->get_conn_status() == CONNECTION_CONNECT_FAILED)
            IDEBUG("[rank %d] the failed connection close.\n", this->rank);
        else{
            this->close_conn_num++;
            if(close_conn_num == 2 * this->size){
                this->env.dispose();
            }
        }
    };

    send_conn->OnHup = [peer_rank, this](connection* conn, const int error){
        if (error == 0) {
            SUCC("[rank %d OnHup] Because rank %d is close normally...\n", this->rank, peer_rank);
        }
        else {
            ERROR("[rank %d OnHup] Because rank %d is close Abnormal...\n", this->rank, peer_rank);
        }
    };

    send_conn->OnSend = [peer_rank, this](connection* conn, const void* buffer, const size_t length) {
        if(is_send_init_list[peer_rank]){
            if(conn->is_sent_head){
                WARN("sizeof(*(char*)buffer  %lld\n", (long long) length);
                ASSERT(length == sizeof(datahead));
                datahead_pool.push((datahead*)const_cast<void*>(buffer));
                conn->is_sent_head = false;//means next onsend buffer is the real send content
            }
            else{
                handler *send_handler;
                bool success = conn->sending_data_queue.try_pop(&send_handler);
                ASSERT(success);
                ASSERT(send_handler);
                conn->is_sent_head = true;
                send_handler->is_finish = true;
                uint64_t value = 1;
                CCALL(write(send_handler->notify_fd, &value, sizeof(value)));

            }
        }
        else{
            is_send_init_list[peer_rank] = true;
            datahead_pool.push((datahead*)const_cast<void*>(buffer));
            ITRACE("[rank %d] Has Sent the HEAD_TYPE_INIT to rank:%d\n",
                   this->rank, (int)(((datahead*)buffer)->dest));
            this->has_conn_num++;
            if(has_conn_num == 2*this->size){
                std::unique_lock<std::mutex> lck(mtx);
                cv.notify_one();
            }
        }
    };

    send_conn->OnSendError = [peer_rank, this](connection* conn, const void* buffer, const size_t length, const size_t sent_length, const int error) {
        ERROR("[Rank %d] OnSendError: %d (%s). all %lld, sent %lld\n",
              this->rank, error, strerror(error), (long long)length, (long long)sent_length);
        ASSERT(0);
    };

    send_conn->OnConnectError = [peer_rank, this](connection *conn, const int error){
        IDEBUG("[rank %d connect to rank %d OnConnectError]: %d (%s) try again\n",
               this->rank, peer_rank, error, strerror(error));
        conn->async_close();
        socket_connection * newconn = env.create_connection(
                this->nodelist[peer_rank].ip_addr, this->nodelist[peer_rank].listen_port);
        newconn->OnConnectError = conn->OnConnectError;
        newconn->OnConnect = conn->OnConnect;
        newconn->OnHup = conn->OnHup;
        newconn->OnClose = conn->OnClose;
        newconn->OnSend = conn->OnSend;
        newconn->OnSendError = conn->OnSendError;
        ASSERT(newconn->async_connect());
    };
}

void comm::set_recv_connection_callback(connection * recv_conn){

    recv_conn->OnClose = [this](connection* conn){
        this->close_conn_num++;
        if(close_conn_num == 2 * this->size){
            this->env.dispose();
        }
    };

    recv_conn->OnHup = [this](connection*, const int error){
        if (error == 0) {
            SUCC("[rank %d OnHup] Because peer_rank is close normally...\n", this->rank);
        }
        else {
            ERROR("[rank %d OnHup] Because peer_rank is close Abnormal...\n", this->rank);
        }
    };

    recv_conn->OnReceive = [this](connection* conn, const void* buffer, const size_t length) {
        size_t left_len = length;
        char* cur_buf   = (char*) const_cast<void*>(buffer);
        while(left_len > 0){
            size_t &rhs = conn->cur_recv_data.recvd_head_size;//rhs == recvd_head_size
            if(rhs < sizeof(datahead)){
                if(left_len < sizeof(datahead) - rhs){
                    memcpy(&(conn->cur_recv_data.head_msg)+rhs, cur_buf, left_len);
                    rhs += left_len;
                    break;
                }
                else{
                    memcpy(&(conn->cur_recv_data.head_msg)+rhs, cur_buf, sizeof(datahead) - rhs);
                    cur_buf  += (sizeof(datahead) - rhs);
                    left_len -= (sizeof(datahead) - rhs);
                    rhs = sizeof(datahead);
                    //WARN("left_len:%d\n", (int)left_len);
                    if(conn->cur_recv_data.head_msg.type == HEAD_TYPE_SEND){
                        size_t cts = conn->cur_recv_data.head_msg.content_size;
                        conn->cur_recv_data.total_content_size = cts;
                        conn->cur_recv_data.content = (char*)malloc(cts);
                    }
                    else if(conn->cur_recv_data.head_msg.type == HEAD_TYPE_INIT){
                        int peer_rank = conn->cur_recv_data.head_msg.src;
                        ASSERT(conn->cur_recv_data.head_msg.content_size == 0);
                        ASSERT(conn->cur_recv_data.head_msg.dest == this->rank);
                        recv_conn_list[peer_rank] = conn;
                        ITRACE("[rank %d] Has Recvd the HEAD_TYPE_INIT from rank %d\n", this->rank, peer_rank);
                        conn->cur_recv_data.clear();
                        this->has_conn_num++;
                        if(has_conn_num == 2*this->size){
                            std::unique_lock<std::mutex> lck(mtx);
                            cv.notify_one();
                        }
                    }
                    continue;
                }
            }
            else{
                size_t &rcs = conn->cur_recv_data.recvd_content_size;
                size_t &tcs = conn->cur_recv_data.total_content_size;
                if(left_len < tcs - rcs){
                    memcpy(conn->cur_recv_data.content + rcs, cur_buf, left_len);
                    rcs += left_len;
                    break;
                }
                else{
                    memcpy(conn->cur_recv_data.content + rcs, cur_buf, tcs - rcs);
                    cur_buf  += (tcs - rcs);
                    left_len -= (tcs - rcs);
                    rcs = conn->cur_recv_data.total_content_size;
                    conn->recvd_data_queue.push(conn->cur_recv_data);
                    ITRACE("[rank %d] Has Recvd the %lld bytes data from rank %d\n",
                           this->rank, (long long)rcs, conn->cur_recv_data.head_msg.src);
                    conn->cur_recv_data.clear();
                    continue;
                }
            }
        }
    };
    //there won't be OnSend and OnSendError
}

bool comm::isend(int dest, const void *buf, size_t count, handler *req)
{
    req->set_handler(this->rank, dest, count, (char*)const_cast<void*>(buf), WAIT_ISEND);
    send_conn_list[dest]->sending_data_queue.push(req);
    //create the head of data
    datahead* head_ctx = datahead_pool.pop();
    head_ctx->content_size = count;
    head_ctx->src          = this->rank;
    head_ctx->dest         = dest;
    head_ctx->type         = HEAD_TYPE_SEND;
    std::vector<fragment> pending_send_ctx;
    pending_send_ctx.emplace_back(head_ctx, sizeof(datahead));
    pending_send_ctx.emplace_back(buf, count);
    send_conn_list[dest]->async_send_many(pending_send_ctx);
    return true;
}
// if a lot of recv
bool comm::irecv(int src, void *buf, size_t count, handler *req)
{
    req->set_handler(src, this->rank, count, (char*)buf, WAIT_IRECV);
    ASSERT(recv_conn_list[src]);

    connection* cur_recv_conn = recv_conn_list[src];
    cur_recv_conn->recving_data_queue.push(req);
    //There also need to decide the recving_data_queue.size and recvd_data_queue.size;
    return true;
}

bool comm::wait(handler *req) {
    if(req->type == WAIT_ISEND){
        uint64_t dummy;
        CCALL(read(req->notify_fd, &dummy, sizeof(dummy)));
        close(req->notify_fd);
        req->is_fd_open = false;
        ASSERT(req->is_finish);
        return true;
    }
    else if(req->type == WAIT_IRECV){
        int src = req->src;
        if(req->is_finish)
            return true;

        connection* cur_recv_conn = recv_conn_list[src];
        while(true){
            handler *tmp_req;
            bool success = cur_recv_conn->recving_data_queue.try_pop(&tmp_req);
            ASSERT(success);

            data_state tmp_recvd_data;
            while(!(cur_recv_conn->recvd_data_queue.try_pop(&tmp_recvd_data))){}
#ifndef NDEBUG
            ASSERT(tmp_recvd_data.total_content_size == tmp_recvd_data.recvd_content_size);
            ASSERT(tmp_recvd_data.total_content_size <= tmp_req->content_size);
            ASSERT(tmp_recvd_data.head_msg.dest == tmp_req->dest);
            ASSERT(tmp_recvd_data.head_msg.src  == tmp_req->src);
#endif
            memcpy(tmp_req->content, tmp_recvd_data.content, tmp_recvd_data.total_content_size);
            tmp_req->is_finish = true;
            if(tmp_req == req){
                return true;
            }
        }
    }
}

bool comm::finalize() {
    //async_close all the connection
    /*IDEBUG("[rank %d] ready to call finalize.\n", this->rank);
    for(int i = 0;i < this->size;++i){
        send_conn_list[i]->async_close();
        recv_conn_list[i]->async_close();
    }*/
}
#include "test.h"

#include <rdma/rdma_header.h>
#include <mutex>
#define LISTEN_PORT 8801
static size_t client_receive_bytes = 0;
static size_t server_receive_bytes = 0;

static size_t client_send_bytes = 0;
static size_t server_send_bytes = 0;

static bool is_client = false;
static char ip[16];
static size_t datasize;
static size_t size_MB;
static int times, senttimes = 0, recvtimes = 0;
static std::vector<fragment> frags;
static size_t echo_size;

static std::mutex client_close;
static std::mutex listener_close;

long long get_curtime(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000000 + tv.tv_usec;
}

static long long start_time, end_time;
void set_client()
{
    client_close.lock();
    rdma_environment env;

    rdma_connection * client_conn = env.create_rdma_connection(ip, LISTEN_PORT);
    client_conn->OnClose = [&](connection*) {
        SUCC("[Client] OnClose.\n");
        client_close.unlock();
    };
    client_conn->OnHup = [&](connection*, const int error) {
        if (error == 0) {
            SUCC("[Client] OnHup: %d (%s)\n", error, strerror(error));
        }
        else {
            ERROR("[Client] OnHup: %d (%s)\n", error, strerror(error));
        }
    };
    client_conn->OnConnect = [&](connection* conn) {
        SUCC("[Client] OnConnect\n");
        conn->start_receive();
        start_time = get_curtime();
        bool success = conn->async_send_many(frags);
        TEST_ASSERT(success);
    };
    client_conn->OnConnectError = [&](connection*, const int error) {
        ERROR("[Client] OnConnectError: %d (%s)\n", error, strerror(error));
        TEST_FAIL();
    };
    client_conn->OnReceive = [&](connection* conn, const void* buffer, const size_t length) {
        client_receive_bytes += length;

        if(client_receive_bytes == echo_size){
            recvtimes++;
            client_receive_bytes = 0;
            SUCC("[Client] receive %d round .\n", recvtimes);
            if(recvtimes < times){
                bool success = conn->async_send_many(frags);
                TEST_ASSERT(success);
            }
        }
        if(recvtimes == times){
            end_time = get_curtime();
            long long consume_time = end_time - start_time;
            NOTICE("[Client] consume time is %lld, speed %.lf  MBytes/sec\n",
                   (long long)consume_time, (double)(echo_size * 2 * times)/consume_time*1000000 );
            SUCC("[Client] have recv all the data ready to close.\n");
            bool success = conn->async_close();
            TEST_ASSERT(success);
        }

    };
    client_conn->OnSend = [&](connection* conn, const void* buffer, const size_t length) {
        client_send_bytes += length;
        SUCC("[Client] OnSend: %lld \n", (long long) length);
        if(client_send_bytes == echo_size) {
            senttimes++;
            SUCC("[*****Client*****] OnSend: %lld (%d round)\n", (long long) length, senttimes);
        }

    };
    client_conn->OnSendError = [&](connection*, const void* buffer, const size_t length, const size_t sent_length, const int error) {
        ERROR("[Client] OnSendError: %d (%s). all %lld, sent %lld\n", error, strerror(error), (long long)length, (long long)sent_length);
        TEST_FAIL();
    };
    bool success = client_conn->async_connect();
    ASSERT(success);
    client_close.lock();
    env.dispose();
}

void set_server()
{
    client_close.lock();
    listener_close.lock();
    rdma_environment env;
    rdma_listener* lis = env.create_rdma_listener("192.168.14.10", LISTEN_PORT);
    lis->OnAccept = [&](listener*, connection* server_conn) {
        SUCC("[ServerListener] OnAccept\n");

        server_conn->OnClose = [&](connection*) {
            SUCC("[ServerConnection] OnClose\n");
            client_close.unlock();
        };
        server_conn->OnHup = [&](connection*, const int error) {
            if (error == 0) {
                SUCC("[ServerConnection] OnHup: %d (%s)\n", error, strerror(error));
            }
            else {
                ERROR("[ServerConnection] OnHup: %d (%s)\n", error, strerror(error));
            }
        };

        server_conn->OnReceive = [&](connection* conn, const void* buffer, const size_t length) {

            server_receive_bytes += length;
            if(server_receive_bytes == echo_size){
                server_receive_bytes = 0;
                recvtimes++;
                SUCC("[ServerConnection] recv times %d ready to send data to client.\n", recvtimes);
                bool success = conn->async_send_many(frags);
                TEST_ASSERT(success);
            }
        };
        server_conn->OnSend = [&](connection* conn, const void* buffer, const size_t length) {
            server_send_bytes += length;
            SUCC("[ServerConnection] OnSend: %lld\n", (long long)length);
            if(server_send_bytes == echo_size){
                senttimes++;
                SUCC("[*****ServerConnection*****] OnSend: %lld (%d round)\n", (long long)length, senttimes);
            }
            if(senttimes == times) {
                NOTICE("[ServerConnection] ready to close.\n");
                bool success = conn->async_close();
                TEST_ASSERT(success);
            }
        };
        server_conn->OnSendError = [&](connection*, const void* buffer, const size_t length, const size_t sent_length, const int error) {
            ERROR("[ServerConnection] OnSendError: %d (%s). all %lld, sent %lld\n", error, strerror(error), (long long)length, (long long)sent_length);
        };

        server_conn->start_receive();
    };
    lis->OnAcceptError = [&](listener*, const int error) {
        ERROR("[ServerListener] OnAcceptError: %d (%s)\n", error, strerror(error));
        TEST_FAIL();
    };
    lis->OnClose = [&](listener*) {
        SUCC("[ServerListener] OnClose\n");
        listener_close.unlock();
    };
    bool success = lis->start_accept();
    TEST_ASSERT(success);

    client_close.lock();
    lis->async_close();
    listener_close.lock();

    env.dispose();
}



//client1 send mbytes data to server(client2) n times. and client2 send m bytes n times to client1
int main(int argc, char *argv[])
{
    if(argc < 2){
        SUCC("error parameter.\n");
        exit(0);
    }

    int op;
    while ((op = getopt(argc, argv, "c::n:t:")) != -1){
        switch(op){
            case 'c':
                if(optarg){
                    strncpy(ip, optarg, strlen(optarg));
                    is_client = true;
                }
                break;
            case 't':
                times = atoi(optarg);
                break;
            default:
                ERROR("parameter is error.\n");
                return 0;
        }
    }
    SUCC("isclient = %lld, ip = %s, send times = %lld.\n",
         (long long)is_client, ip,  (long long)times);

    char* tmp_buffer = (char*)malloc(1024);
    frags.push_back(fragment(tmp_buffer, 1024));


    for(int i = 1;i <= 3;i++){
        size_t malloc_size = 1024*1024*i;
        tmp_buffer = (char*)malloc(malloc_size);
        frags.push_back(fragment(tmp_buffer, malloc_size));
        echo_size += malloc_size;
    }

    if(is_client){
        NOTICE("Start client......\n");
        set_client();
    }
    else{
        NOTICE("Start server......\n");
        set_server();
    }
    return 0;
}



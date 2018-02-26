#include "test.h"
#include <rdma/rdma_header.h>
#include <mutex>
#include <thread>

#define LOCAL_HOST          ("192.168.168.254")
#define LOCAL_PORT          (8801)
#define THREAD_COUNT        (1)

static std::mutex client_close[THREAD_COUNT];
static std::mutex listener_close;
static std::mutex server_all_close;
static std::atomic_int server_alive_connections(THREAD_COUNT);
static std::atomic_int total_conns(0);

static void set_server_connection_callbacks(connection* server_conn)
{

    server_conn->OnClose = [&](connection*) {
        SUCC("[Passive Connection] OnClose (User User-Defined)\n"); 
        if(--server_alive_connections == 0){
            //server_all_close.unlock();
        }
    };
}
static void set_client_connection_callbacks(connection* client_conn, const int tid)
{
    client_conn->OnClose = [&](connection*) {
        SUCC("[Active Connection] OnClose (User User-Defined)\n");
        client_close[tid].unlock();
    };
    
    client_conn->OnConnect = [&](connection* conn) {
        SUCC("[Active Connection] OnConnect (User User-Defined)\n");
        int num = ++total_conns;
        DEBUG("!!!!!!clienti %d  ready to close.\n", num);
        conn->async_close();

    };
    client_conn->OnConnectError = [&](connection*, const int error) {
        ERROR("[Active Connection] OnConnectError: (User User-Defined) %d (%s)\n", error, strerror(error));
        TEST_FAIL();
    };
}

void test_multi_rdma_connection_onelisten()
{
    for(int tid = 0; tid < THREAD_COUNT; ++tid){
        client_close[tid].lock();
    }
    listener_close.lock();
    //server_all_close.lock();

    rdma_environment env;
    rdma_listener *lis = env.create_rdma_listener(LOCAL_HOST, LOCAL_PORT);
    lis->OnAccept = [&](listener*, connection* conn) {
        SUCC("[ServerListener] OnAccept\n");
        set_server_connection_callbacks(conn);
        SUCC("[ServerListener] established a passive rdma connection.\n");
    };
    lis->OnAcceptError = [&](listener*, const int error) {
        ERROR("[ServerListener] OnAcceptError: %d (%s)\n", error, strerror(error));
        TEST_FAIL();
    };
    lis->OnClose = [&](listener*) {
        SUCC("[ServerListener] OnClose\n");
        listener_close.unlock();
    };
    sleep(1);
    bool success = lis->start_accept();
    TEST_ASSERT(success);
    
    std::thread threads[THREAD_COUNT];
    for(int tid = 0;tid < THREAD_COUNT; ++tid){
        threads[tid] = std::thread([tid, &env](){
            rdma_connection *client = env.create_rdma_connection(LOCAL_HOST, LOCAL_PORT);
            set_client_connection_callbacks(client, tid);
            std::this_thread::sleep_for(std::chrono::milliseconds(tid * 4));
            const bool success = client->async_connect();
            TEST_ASSERT(success);
            client_close[tid].lock();
        });
    }
    for(int tid = 0; tid < THREAD_COUNT; ++tid){
        threads[tid].join();
    }
    lis->async_close();
    listener_close.lock();

    //server_all_close.lock();
    DEBUG("waiting for env close ~~~~~~.\n");
    //ASSERT(server_alive_connections == 0);
    env.dispose();

}

BEGIN_TESTS_DECLARATION(test_rdma_connections)
DECLARE_TEST(test_multi_rdma_connection_onelisten)
END_TESTS_DECLARATION

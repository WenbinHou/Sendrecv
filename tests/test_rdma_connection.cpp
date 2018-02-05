#include "test.h"
#include <rdma/rdma_header.h>

#define LOCAL_HOST          ("127.0.0.1")
#define LOCAL_PORT          (8801)
#define THREAD_COUNT        (32)


static void set_server_connection_callbacks(connection* server_conn)
{
    server_conn->OnClose = [&](connection*) {
        SUCC("[Passive Connection] OnClose (User User-Defined)\n"); 
    };
}
static void set_client_connection_callbacks(connection* client_conn)
{
    client_conn->OnClose = [&](connection*) {
        SUCC("[Active Connection] OnClose (User User-Defined)\n");
    };
    
    client_conn->OnConnect = [&](connection* conn) {
        SUCC("[Active Connection] OnConnect (User User-Defined)\n");
        DEBUG("client ready to close.\n");
        conn->async_close();
        //client_conn->async_close();

    };
    client_conn->OnConnectError = [&](connection*, const int error) {
        ERROR("[Active Connection] OnConnectError: (User User-Defined) %d (%s)\n", error, strerror(error));
        TEST_FAIL();
    };
}

void test_simple_rdma_connection()
{

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
    };
    bool lis_success = lis->start_accept();
    TEST_ASSERT(lis_success);
    
    rdma_connection *client = env.create_rdma_connection(LOCAL_HOST, LOCAL_PORT);
    set_client_connection_callbacks(client);
    const bool success = client->async_connect();
    TEST_ASSERT(success);

    env.dispose();

}

BEGIN_TESTS_DECLARATION(test_rdma_connection)
DECLARE_TEST(test_simple_rdma_connection)
END_TESTS_DECLARATION
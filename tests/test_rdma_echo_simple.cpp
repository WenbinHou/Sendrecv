#include "test.h"

#include <rdma/rdma_header.h>
#include <thread>
#include <vector>
#include <mutex>

#define LOCAL_HOST          ("192.168.168.254")
#define LOCAL_PORT          (8801)
#define ECHO_DATA_LENGTH    (1024 * 1024 * 8)  // 8MB

static char dummy_data[ECHO_DATA_LENGTH];

static size_t client_receive_bytes = 0;
static size_t server_send_bytes = 0;
static size_t server_receive_bytes = 0;
static std::mutex client_close;
static std::mutex listener_close;

static void set_server_connection_callbacks(connection *passive_conn)
{
    const unsigned long long TRADEMARK = 0xdeadbeef19960513;
    passive_conn->OnClose = [&](connection*) {
        SUCC("[PassiveConnection] OnClose\n");
    };
    passive_conn->OnHup = [&](connection*, const int error) {
        ERROR("[PassiveConnection] OnHup: %d (%s)\n", error, strerror(error));
    };
    passive_conn->OnSend = [&](connection* conn, const void* buffer, const size_t length) {

        server_send_bytes += length;
        SUCC("[PassiveConnection] OnSend: %lld (total: %lld)\n", (long long) length, (long long) server_send_bytes);
        if (server_send_bytes == ECHO_DATA_LENGTH) {
            INFO("[PassiveConnection] All echo back data sent.\n");
            conn->async_close();
        }
        void *org_buf = (char *) buffer - sizeof(TRADEMARK);
        TEST_ASSERT(*(decltype(TRADEMARK) *) org_buf == TRADEMARK);
        free(org_buf);
    };
    passive_conn->OnSendError = [&](connection*, const void* buffer, const size_t length, const size_t sent_length, const int error) {
        ERROR("[PassiveConnection] OnSendError: %d (%s). all %lld, sent %lld\n", error, strerror(error), (long long)length, (long long)sent_length);

        void* org_buf = (char*)buffer - sizeof(TRADEMARK);
        TEST_ASSERT(*(decltype(TRADEMARK)*)org_buf == TRADEMARK);
        free(org_buf);
    };
    passive_conn->OnReceive = [&](connection* conn, const void* buffer, const size_t length) {
        server_receive_bytes += length;
        SUCC("[PassiveConnection] OnReceive: %lld (total: %lld)\n", (long long)length, (long long)server_receive_bytes);
        if (server_receive_bytes == ECHO_DATA_LENGTH) {
            INFO("[PassiveConnection] All echo data received.\n");
        }
        void* tmp_buf = malloc(length + sizeof(TRADEMARK));
        TEST_ASSERT(tmp_buf != nullptr);
        *(std::remove_const<decltype(TRADEMARK)>::type*)tmp_buf = TRADEMARK;
        memcpy((char*)tmp_buf + sizeof(TRADEMARK), buffer, length);

        bool success = conn->async_send((char*)tmp_buf + sizeof(TRADEMARK), length);
        TEST_ASSERT(success);
    };
}

static void set_client_connection_callbacks(connection* active_conn)
{
    active_conn->OnConnect = [&](connection *conn){
        SUCC("[ActiveConnectiion] OnConnect\n");
        conn->start_receive();

        bool success = conn->async_send(dummy_data, ECHO_DATA_LENGTH);
        TEST_ASSERT(success);
    };
    active_conn->OnConnectError = [&](connection*, const int error) {
        ERROR("[ActiveConnection] OnConnectError: %d (%s)\n", error, strerror(error));
        TEST_FAIL();
    };
    active_conn->OnClose = [&](connection*) {
        SUCC("[ActiveConnection] OnClose\n");
        client_close.unlock();
    };

    active_conn->OnHup = [&](connection*, const int error) {
        ERROR("[ActiveConnection] OnHup: %d (%s)\n", error, strerror(error));
    };

    active_conn->OnSend =  [&](connection*, const void* buffer, const size_t length) {
        SUCC("[ActiveConnection] OnSend: %lld\n", (long long)length);
        ASSERT(length == ECHO_DATA_LENGTH);
    };

    active_conn->OnSendError = [&](connection*, const void* buffer, const size_t length, const size_t sent_length, const int error) {
        ERROR("[ActiveConnection] OnSendError: %d (%s). all %lld, sent %lld\n", error, strerror(error), (long long)length, (long long)sent_length);
        TEST_FAIL();
    };

    active_conn->OnReceive = [&](connection* conn, const void* buffer, const size_t length) {
        ASSERT(memcmp(buffer, dummy_data + client_receive_bytes, length) == 0);
        client_receive_bytes += length;
        SUCC("[ActiveConnection] OnReceive: %lld (total: %lld)\n", (long long)length, (long long)client_receive_bytes);
        if (client_receive_bytes == ECHO_DATA_LENGTH) {
            INFO("[ActiveConnection] All echo back data received. now client async_close()\n");
            conn->async_close();
        };
    };
}


void test_rdma_echo_simple()
{
    for (int i = 0; i < ECHO_DATA_LENGTH; ++i) {
        dummy_data[i] = (char)(unsigned char)i;
    }
    client_close.lock();
    listener_close.lock();

    rdma_environment env;
    rdma_listener *lis = env.create_rdma_listener(LOCAL_HOST, LOCAL_PORT);
    //register OnAccept OnAcceptError OnClose
    lis->OnAccept = [&](listener*, connection* conn){
        SUCC("[ServerListener] OnAccept\n");
        set_server_connection_callbacks(conn);
        conn->start_receive();
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
    rdma_connection *client = env.create_rdma_connection(LOCAL_HOST ,LOCAL_PORT);

    set_client_connection_callbacks(client);
    client->async_connect();
    success = client->async_connect();
    TEST_ASSERT(success);

    client_close.lock();
    lis->async_close();
    listener_close.lock();

    env.dispose();
}



BEGIN_TESTS_DECLARATION(test_rdma_echo_simple)
DECLARE_TEST(test_rdma_echo_simple)
END_TESTS_DECLARATION

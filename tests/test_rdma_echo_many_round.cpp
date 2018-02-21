#include "test.h"

#include <rdma/rdma_header.h>
#include <mutex>

#define LOCAL_HOST          ("127.0.0.1")
#define LOCAL_PORT          (8801)
#define ECHO_DATA_LENGTH    ((size_t)1024 * 1024 * 16)  // 16MB
#define ECHO_DATA_ROUND     ((size_t)16)
#define THREAD_COUNT        (32)

static char dummy_data[ECHO_DATA_LENGTH];

static size_t client_receive_bytes = 0;
static size_t client_send_bytes = 0;
static size_t server_send_bytes = 0;
static size_t server_receive_bytes = 0;
static std::mutex client_close;
static std::mutex listener_close;


static void set_server_connection_callbacks(connection* server_conn)
{
    const unsigned long long TRADEMARK = 0xdeedbeef19960513;
    server_conn->OnClose = [&](connection*) {
        SUCC("[PassiveConnection] OnClose\n");
    };
    server_conn->OnHup = [&](connection*, const int error) {
        ERROR("[ServerConnection] OnHup: %d (%s)\n", error, strerror(error));
    };
    server_conn->OnReceive = [&](connection* conn, const void* buffer, const size_t length) {

        server_receive_bytes += length;
        SUCC("[PassiveConnection] OnReceive: %lld (total: %lld)\n", (long long)length, (long long)server_receive_bytes);
        if (server_receive_bytes == ECHO_DATA_LENGTH * ECHO_DATA_ROUND) {
            INFO("[PassiveConnection] All echo data received.\n");
        }

        void* tmp_buf = malloc(length + sizeof(TRADEMARK));
        TEST_ASSERT(tmp_buf != nullptr);
        *(std::remove_const<decltype(TRADEMARK)>::type*)tmp_buf = TRADEMARK;
        memcpy((char*)tmp_buf + sizeof(TRADEMARK), buffer, length);

        bool success = conn->async_send((char*)tmp_buf + sizeof(TRADEMARK), length);
        TEST_ASSERT(success);
    };
    server_conn->OnSend = [&](connection* conn, const void* buffer, const size_t length) {

        server_send_bytes += length;
        SUCC("[PassiveConnection] OnSend: %lld (total: %lld)\n", (long long)length, (long long)server_send_bytes);
        if (server_send_bytes == ECHO_DATA_LENGTH * ECHO_DATA_ROUND) {
            INFO("[PassiveConnection] All echo back data sent.\n");
            conn->async_close();
        }

        void* org_buf = (char*)buffer - sizeof(TRADEMARK);
        TEST_ASSERT(*(decltype(TRADEMARK)*)org_buf == TRADEMARK);
        free(org_buf);
    };
    server_conn->OnSendError = [&](connection*, const void* buffer, const size_t length, const size_t sent_length, const int error) {
        ERROR("[PassiveConnection] OnSendError: %d (%s). all %lld, sent %lld\n", error, strerror(error), (long long)length, (long long)sent_length);

        void* org_buf = (char*)buffer - sizeof(TRADEMARK);
        TEST_ASSERT(*(decltype(TRADEMARK)*)org_buf == TRADEMARK);
        free(org_buf);
    };
}


static void set_client_connection_callbacks(connection* client_conn)
{
    client_conn->OnClose = [&](connection*) {
        SUCC("[ActiveConnectiion] OnClose\n");
        client_close.unlock();
    };
    client_conn->OnHup = [&](connection*, const int error) {
        ERROR("[ActiveConnectiion] OnHup: %d (%s)\n", error, strerror(error));
    };
    client_conn->OnConnect = [&](connection* conn) {
        SUCC("[ActiveConnectiion] OnConnect\n");
        conn->start_receive();
        bool success = conn->async_send(dummy_data, ECHO_DATA_LENGTH);
        TEST_ASSERT(success);
    };
    client_conn->OnConnectError = [&](connection*, const int error) {
        ERROR("[ActiveConnectiion] OnConnectError: %d (%s)\n", error, strerror(error));
        TEST_FAIL();
    };
    client_conn->OnReceive = [&](connection* conn, const void* buffer, const size_t length) {
        const size_t start = client_receive_bytes % ECHO_DATA_LENGTH;
        if (start + length <= ECHO_DATA_LENGTH) {
            ASSERT(memcmp(buffer, dummy_data + start, length) == 0);
        }
        else {
            // start + length > ECHO_DATA_LENGTH
            const size_t tmp = ECHO_DATA_LENGTH - start;
            ASSERT(memcmp(buffer, dummy_data + start, tmp) == 0);
            ASSERT(memcmp((char*)buffer + tmp, dummy_data + 0, length - tmp) == 0);
        }

        client_receive_bytes += length;
        SUCC("[ActiveConnectiion] OnReceive: %lld (total: %lld)\n", (long long)length, (long long)client_receive_bytes);
        if (client_receive_bytes == ECHO_DATA_LENGTH * ECHO_DATA_ROUND) {
            INFO("[ActiveConnectiion] All echo back data received. now client async_close()\n");
            conn->async_close();
        }
    };
    client_conn->OnSend = [&](connection* conn, const void* buffer, const size_t length) {
        ASSERT(length == ECHO_DATA_LENGTH);
        client_send_bytes += length;

        SUCC("[ActiveConnectiion] OnSend: %lld (%lld rounds)\n", (long long)length, (long long)client_send_bytes / ECHO_DATA_LENGTH);
        //fprintf(stderr, "[Client] OnSend: %lld (%lld rounds)\n", (long long)length, (long long)client_send_bytes / ECHO_DATA_LENGTH);

        if (client_send_bytes < ECHO_DATA_LENGTH * ECHO_DATA_ROUND) {
            const bool success = conn->async_send(dummy_data, ECHO_DATA_LENGTH);
            TEST_ASSERT(success);
        }
    };
    client_conn->OnSendError = [&](connection*, const void* buffer, const size_t length, const size_t sent_length, const int error) {
        ERROR("[ActiveConnectiion] OnSendError: %d (%s). all %lld, sent %lld\n", error, strerror(error), (long long)length, (long long)sent_length);
        TEST_FAIL();
    };
}

void test_rdma_echo_many_round()
{
    for (size_t i = 0; i < ECHO_DATA_LENGTH; ++i) {
        dummy_data[i] = (char)(unsigned char)i;
    }

    client_close.lock();
    listener_close.lock();

    rdma_environment env;
    rdma_listener* lis = env.create_rdma_listener(LOCAL_HOST, LOCAL_PORT);
    lis->OnAccept = [&](listener*, connection* conn) {
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

    rdma_connection* client = env.create_rdma_connection(LOCAL_HOST, LOCAL_PORT);
    set_client_connection_callbacks(client);
    success = client->async_connect();
    TEST_ASSERT(success);

    client_close.lock();

    lis->async_close();
    listener_close.lock();

    env.dispose();
}


BEGIN_TESTS_DECLARATION(test_rdma_echo_many_round)
                DECLARE_TEST(test_rdma_echo_many_round)
END_TESTS_DECLARATION



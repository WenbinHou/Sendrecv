#include <mutex>
#include "mpi_test.h"

socket_environment env;

socket_listener* lis;
size_t finish_count = 0;

socket_connection* conn;
size_t recv_length = 0;

const size_t DATSIZE = 1024 * 1024 * 32;  // 32MB
char data[DATSIZE];
pvlock lck(0);

void test()
{
    if (MY_RANK == 0) {
        lis = env.create_listener(PHONEBOOK_IB[MY_RANK], LISTEN_PORT);
        lis->OnAccept = [](listener*, connection* accepted_conn) {
            accepted_conn->OnReceive = [accepted_conn](connection* c, void* buffer, const size_t length) {
                ASSERT(accepted_conn == c);

                char* tmp = new char[length];
                memcpy(tmp, buffer, length);
                c->async_send(tmp, length);
            };
            accepted_conn->OnSend = [accepted_conn](connection* c, void* buffer, const size_t length) {
                ASSERT(accepted_conn == c);
                delete[] (char*)buffer;
            };
            accepted_conn->OnSendError = [accepted_conn](connection* c, void* buffer, const size_t length, const size_t sent_length, const int error) {
                ASSERT(accepted_conn == c);
                delete[] (char*)buffer;
                ERROR("[Server] OnSendError: %d (%s)\n", error, strerror(error));
                ASSERT(0);
            };
            accepted_conn->OnHup = [accepted_conn](connection* c, const int error) {
                ASSERT(accepted_conn == c);
                if (error == 0) {
                    SUCC("[Server] OnHup: %d (%s)\n", error, strerror(error));
                }
                else {
                    ERROR("[Server] OnHup: %d (%s)\n", error, strerror(error));
                    ASSERT(0);
                }
                c->async_close();
            };
            accepted_conn->OnClose = [accepted_conn](connection* c) {
                ASSERT(accepted_conn == c);
                ++finish_count;

                SUCC("[Server] Done! %d/%d\n", (int)finish_count, COMM_SIZE - 1);
                if (finish_count == COMM_SIZE - 1) {
                    lis->async_close();
                }
            };

            accepted_conn->start_receive();
        };

        lis->OnClose = [](listener*) {
            SUCC("[Server] Listener OnClose\n");
            lck.V();
        };

        lis->start_accept();
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (MY_RANK != 0) {
        memset(data, (char)MY_RANK, DATSIZE);

        conn = env.create_connection(PHONEBOOK_IB[0], LISTEN_PORT);

        conn->OnConnect = [](connection* c) {
            ASSERT(conn == c);
            c->start_receive();

            c->async_send(data, DATSIZE);
        };
        conn->OnConnectError = [](connection* c, const int error) {
            ASSERT(conn == c);
            ERROR("[Client:%d] OnConnectError: %d (%s)\n", MY_RANK, error, strerror(error));
            c->async_close();
        };
        conn->OnSendError = [](connection* c, void* buffer, const size_t length, const size_t sent_length, const int error) {
            ASSERT(conn == c);
            ERROR("[Client:%d] OnSendError: %d (%s)\n", MY_RANK, error, strerror(error));
            ASSERT(0);
            c->async_close();
        };
        conn->OnReceive = [](connection* c, void* buffer, const size_t length) {
            ASSERT(conn == c);

            for (size_t i = 0; i < length; ++i) {
                ASSERT(((char*)buffer)[i] == (char)MY_RANK);
            }

            recv_length += length;
            if (recv_length == DATSIZE) {
                SUCC("[Client:%d] Done!\n", MY_RANK);
                c->async_close();
            }
        };
        conn->OnClose = [](connection* c) {
            SUCC("[Client:%d] OnClose\n", MY_RANK);
            ASSERT(conn == c);
            ASSERT(recv_length == 0 || recv_length == DATSIZE);

            lck.V();
        };

        conn->async_connect();
    }

    lck.P();
}


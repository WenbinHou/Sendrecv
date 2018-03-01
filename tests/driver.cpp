
#define RUN_TEST(name)  extern void __do_test__##name(); __do_test__##name();

int main(int argc, char** argv)
{
    //RUN_TEST(test_lock);
    //RUN_TEST(test_rundown_protection);
    //RUN_TEST(test_socket_echo_simple);
    //RUN_TEST(test_socket_echo_many_round);
    //RUN_TEST(test_socket_echo_multi_thread);
    //RUN_TEST(test_socket_echo_multi_thread_uds);

    /*the test of rdma*/
    //RUN_TEST(test_rdma_connection);//success
    //RUN_TEST(test_rdma_connections);//success
    RUN_TEST(test_rdma_echo_simple);//success
    RUN_TEST(test_rdma_echo_many_round);//success
    RUN_TEST(test_rdma_echo_multi_thread);//success
    return 0;
}

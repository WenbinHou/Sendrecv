
#define RUN_TEST(name)  extern void __do_test__##name(); __do_test__##name();

int main(int argc, char** argv)
{
    //RUN_TEST(test_lock);
    //RUN_TEST(test_rundown_protection);
    //RUN_TEST(test_socket_echo_simple);
    //RUN_TEST(test_socket_echo_many_round);
    //RUN_TEST(test_socket_echo_multi_thread);
    //RUN_TEST(test_socket_echo_multi_thread_uds);
    
    RUN_TEST(test_rdma_connections);
   // RUN_TEST(test_rdma_connections);
    //RUN_TEST(test_rdma_multi_connection_thread);
    return 0;
}

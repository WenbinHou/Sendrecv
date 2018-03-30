#include <rdma_src/conn_system.h>
#include <thread>
#include <vector>

#define LOCAL_HOST          ("127.0.0.1")
#define PEER_HOST           ("127.0.0.1")
#define LOCAL_PORT          (8801)
#define PEER_PORT_BASE      (8801)

int main()
{
    int threads_num = 2;
    std::vector<std::thread> processes(threads_num);

    for(int i = 0;i < threads_num;i++){
        processes[i] = std::thread([i](){
            WARN("%s:%d ready to init with %s:%d.\n", LOCAL_HOST, LOCAL_PORT+i,
                 PEER_HOST, PEER_PORT_BASE + (i+1)%2);
            conn_system sys("127.0.0.1", LOCAL_PORT+i);
            rdma_conn_p2p *rdma_conn_object = sys.init(PEER_HOST, PEER_PORT_BASE + (i+1)%2);
            ASSERT(rdma_conn_object);
            WARN("[%s:%d] init finished.\n", LOCAL_HOST, LOCAL_PORT+i);
            if(i == 0){
                usleep(1000);
                rdma_conn_object->test_send();
                int n = rdma_conn_object->wait_poll_send();
                ASSERT(n == POST_TIMES);
            }
            else{
                int n = rdma_conn_object->wait_poll_recv();
                ASSERT(n == POST_TIMES);
            }
        });
    }
    for(auto& t: processes)
        t.join();
    return 0;
}



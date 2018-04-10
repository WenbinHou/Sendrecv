#include <rdma_src/conn_system.h>
#include <thread>
#include <vector>
#include <sys/sysinfo.h>

#define LOCAL_HOST          ("127.0.0.1")
#define PEER_HOST           ("127.0.0.1")
#define LOCAL_PORT          (8801)
#define PEER_PORT_BASE      (8801)
//#define DATA_LEN            (4*1024)
#define ITERS               10

/*
 * test case:
 * 1.isend one small msg: 16, 32, 64, 128, 512
 * 2.isend one big   msg: 1024, 1024*16, 1024*256, 1024*1024, 1024*1024*8
 */

int main(int argc, char *argv[])
{
    int threads_num = 2;
    std::vector<std::thread> processes(threads_num);

    for(int i = 0;i < threads_num;i++){
        processes[i] = std::thread([i](){
            cpu_set_t mask;
            CPU_ZERO(&mask);
            for (int ii = 0; ii < 14; ++ii)
                CPU_SET(ii, &mask), CPU_SET(ii + 28, &mask);
            CCALL(sched_setaffinity(0, sizeof(mask), &mask));
            WARN("%s:%d ready to init with %s:%d.\n", LOCAL_HOST, LOCAL_PORT+i,
                 PEER_HOST, PEER_PORT_BASE + (i+1)%2);
            conn_system sys("127.0.0.1", LOCAL_PORT+i);
            rdma_conn_p2p *rdma_conn_object = sys.init("127.0.0.1", PEER_PORT_BASE + (i+1)%2);
            ASSERT(rdma_conn_object);
            ITR_SPECIAL("%s:%d init finished.\n", LOCAL_HOST, LOCAL_PORT+i);

            if(i == 0)
            {
                ITR_SPECIAL("READY to send msg 500 times .......\n");
                rdma_conn_object->test_extreme_speed(500, 1024*1024, true);
            }
            else{
                ITR_SPECIAL("READY to recv msg 500 times .......\n");
                rdma_conn_object->poll_recv(500);
            }

        });
    }
    for(auto& t: processes)
        t.join();
    return 0;
}



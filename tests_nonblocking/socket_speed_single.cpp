
#include <nonblocking/comm.h>
#include <thread>
#include <vector>
#define TOTAL_RANK 2 //means there are TOTAL_RANK process
#define DATA_LENGTH    (1024 * 1024)

static long long get_curtime(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000000 + tv.tv_usec;
}

int main(){
    int total_rank = TOTAL_RANK;
    std::vector<nodeinfo> nodelist(total_rank);
    for(int i = 0;i < total_rank;++i){
        strcpy(nodelist[i].ip_addr ,"127.0.0.1");
        nodelist[i].listen_port = BASE_PORT + i;
        IDEBUG("ip_addr:%s listen_port:%d\n",
               nodelist[i].ip_addr, nodelist[i].listen_port);
    }

    std::vector<std::thread> process(total_rank);
    for(int rankid = 0;rankid < total_rank;++rankid){
        process[rankid] = std::thread([rankid, &nodelist](){
            int myrank = rankid,  allsize = nodelist.size();
            int peerrank = (allsize - myrank)/allsize;
            size_t send_bytes = 8 * 1024;
            comm comm_object;
            comm_object.init(myrank, allsize, nodelist);
            char *send_data = (char*)malloc(send_bytes);
            char *recv_data = (char*)malloc(send_bytes);
            for (int i = 0; i < send_bytes; ++i) {
                send_data[i] = (char)(unsigned char)i;
            }

            handler send_handler, recv_handler;
            long long start_time = get_curtime();
            for(int i = 0;i < 1000;++i){
                comm_object.isend(peerrank, send_data, send_bytes, &send_handler);
                comm_object.irecv(peerrank, recv_data, send_bytes, &recv_handler);
                comm_object.wait(&send_handler);
                comm_object.wait(&recv_handler);
                WARN("[RANK %d]  i = %d\n", comm_object.get_rank(), i);
                //ASSERT(memcmp(recv_data, send_data, send_bytes) == 0);
            }
            long long consume_time = get_curtime() - start_time;
            size_t total_bytes = (size_t)1000 * send_bytes* 2;
            double speed = (double)total_bytes/1024/1024/(consume_time/1000000);
            SUCC("[rank %d] tranfer_size:%lld consume_time:%.3lf speed:%.3lf\n",
                 comm_object.get_rank(), (long long)total_bytes, (double)consume_time/1000000, speed);
            comm_object.finalize();

        });
    }
    for(auto &p:process){
        p.join();
    }
    return 0;
}
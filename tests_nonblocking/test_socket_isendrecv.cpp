#include <nonblocking/comm.h>
#include <thread>
#include <vector>
#define TOTAL_RANK 4 //means there are TOTAL_RANK process
#define DATA_LENGTH    (1024 * 1024)

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
            char* send_data = (char*)malloc(DATA_LENGTH);
            char* recv_data = (char*)malloc(DATA_LENGTH * TOTAL_RANK);
            for (int i = 0; i < DATA_LENGTH; ++i) {
                send_data[i] = (char)(unsigned char)i;
            }
            //std::this_thread::sleep_for(std::chrono::milliseconds(rankid * 10));
            comm comm_object;

            comm_object.init(rankid, nodelist.size(), nodelist);

            handler send_handlers[TOTAL_RANK];
            handler recv_handlers[TOTAL_RANK];

            for(int des = 0;des < comm_object.get_size();++des){
                comm_object.isend(des, send_data, DATA_LENGTH, send_handlers + des);
                ITRACE("[Rank %d isend] to rank %d %lld btyes\n", comm_object.get_rank(), des, (long long)DATA_LENGTH);
            }
            for(int src = 0;src < comm_object.get_size();++src){
                comm_object.irecv(src, recv_data + src*DATA_LENGTH, DATA_LENGTH, recv_handlers + src);
            }
            for(int des = 0;des < comm_object.get_size();++des){
                comm_object.wait(send_handlers + des);
                ASSERT(send_handlers[des].content = send_data + des);
                ASSERT(send_handlers[des].is_finish);
                ASSERT(send_handlers[des].dest == des);
                ASSERT(send_handlers[des].src == comm_object.get_rank());
            }
            for(int src = 0;src < comm_object.get_size();++src){
                DEBUG("!!!!xxxx!!!![Rank %d] wait for rank %d finished.\n", comm_object.get_rank(), src);
                comm_object.wait(recv_handlers + src);
                ASSERT(memcmp(send_data, recv_data + src*DATA_LENGTH, DATA_LENGTH) == 0);
            }
            WARN("==========Rank %d almost finish task.\n", comm_object.get_rank());

            comm_object.finalize();

        });
    }
    for(auto &p:process){
        p.join();
    }
    return 0;
}
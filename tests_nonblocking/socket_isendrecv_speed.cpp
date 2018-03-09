#include <nonblocking/comm.h>
#include <iostream>
#include <sstream>
#include <fstream>
//current only for two node test
int myrank;
int allsize;
std::vector<nodeinfo> nodelist;
size_t send_bytes;

long long get_curtime(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000000 + tv.tv_usec;
}
void read_host_file(char* file)
{
    std::ifstream fin(file, std::ios::in);
    char line[1024]={0};
    while(fin.getline(line, sizeof(line)))
    {
        std::string lis_ip; int lis_port;
        std::stringstream word(line);
        word >> lis_ip;
        word >> lis_port;
        nodelist.emplace_back(lis_ip, lis_port);
    }
    fin.close();
    int i = 0;
    for(auto &node:nodelist){
        DEBUG("rank:%d ip_addr:%s port:%d\n", i, nodelist[i].ip_addr, nodelist[i].listen_port);
        i++;
    }
    allsize = nodelist.size();
}
// ./socket_isendrecv_speed -i 0 -l host_file (means the index i is belongs to the host_file)
int main(int argc, char* argv[])
{
    char nodelist_file_path[256];
    if(argc < 7){
        ERROR("error parameter only %d parameters.\n", argc);
        exit(0);
    }
    int op;
    while ((op = getopt(argc, argv, "i:f:n:")) != -1){
        switch(op){
            case 'i':
                myrank = atoi(optarg);
                break;
            case 'f':
                strcpy(nodelist_file_path, optarg);
                ITRACE("nodelist_file_path is [%s]\n", nodelist_file_path);
                read_host_file(nodelist_file_path);
                break;
            case 'n':
                send_bytes =  (size_t)1024 * atoi(optarg);
                ITRACE("Isend & irecv %lld bytes data.\n", (long long)send_bytes);
                break;
            default:
                ERROR("parameter is error.\n");
                return 0;
        }
    }
    int peerrank = (allsize - myrank)/allsize;
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

        //ASSERT(memcmp(recv_data, send_data, send_bytes) == 0);
    }
    long long consume_time = get_curtime() - start_time;
    size_t total_bytes = (size_t)1000 * send_bytes* 2;
    double speed = (double)total_bytes/1024/(consume_time/1000000);
    SUCC("[rank %d] tranfer_size:%lld consume_time:%.3lf speed:%.3lf\n",
         comm_object.get_rank(), (long long)total_bytes, (double)consume_time/1000000, speed);
    comm_object.finalize();
}

#include "mpi.h"
#include "stdio.h"

long long get_curtime(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000000 + tv.tv_usec;
}
int main (int argc, char *argv[])
{
    if(argc < 2)
    {
        printf("Too few parameter.\n");
        exit(0);
    }

    int myrank, allsize;
    MPI_Init (&argc, &argv);
    MPI_Comm_rank (MPI_COMM_WORLD, &myrank);
    MPI_Comm_size (MPI_COMM_WORLD, &allsize);
    int peerrank = (allsize - myrank)/allsize;

    char *send_data = (char*)malloc(send_bytes);
    char *recv_data = (char*)malloc(send_bytes);
    for (int i = 0; i < send_bytes; ++i) {
        send_data[i] = (char)(unsigned char)i;
    }

    int send_bytes = atoi(argv[1]);
    printf("[rank %d] send_bytes = %d, peerrank = %d\n", myrank, send_bytes, peerrank);
    MPI_Request send_handler, recv_handler;
    MPI_Status status;
    long long start_time = get_curtime();
    for(int i = 0;i < 100;++i){
        MPI_Isend(send_data, send_bytes, MPI_CHAR, peerrank, 123, MPI_COMM_WORLD, &send_handler);
        MPI_Irecv(recv_data, send_bytes, MPI_CHAR, peerrank, 123, MPI_COMM_WORLD, &recv_handler);
        MPI_Wait(&send_handler, &status);
        MPI_Wait(&recv_handler, &status);
    }
    long long consume_time = get_curtime() - start_time;
    size_t total_bytes = (size_t)100 * send_bytes* 2;
    double speed = (double)total_bytes/1024/1024/((double)consume_time/1000000);
    SUCC("[rank %d] tranfer_size:%lld consume_time:%.3lf speed:%.3lf\n",
         myrank, (long long)total_bytes, (double)consume_time/1000000, speed);

    MPI_Finalize();
    return  0;
}



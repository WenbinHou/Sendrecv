#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
double get_curtime(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec/1000000;
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

    size_t send_bytes = atoi(argv[1]);
    char *send_data = (char*)malloc(send_bytes);
    char *recv_data = (char*)malloc(send_bytes);
    for (int i = 0; i < send_bytes; ++i) {
        send_data[i] = (char)(unsigned char)i;
    }

    printf("[rank %d] send_bytes = %d, peerrank = %d\n", myrank, send_bytes, peerrank);
    MPI_Request send_handler, recv_handler;
    MPI_Status status;
    double start_time = get_curtime();
    for(int i = 0;i < 100;++i){
        MPI_Isend(send_data, send_bytes, MPI_CHAR, peerrank, 123, MPI_COMM_WORLD, &send_handler);
        MPI_Irecv(recv_data, send_bytes, MPI_CHAR, peerrank, 123, MPI_COMM_WORLD, &recv_handler);
        MPI_Wait(&send_handler, &status);
        MPI_Wait(&recv_handler, &status);
        //printf("[rank %d] iter %d.\n", myrank, i);
    }
    double consume_time = get_curtime() - start_time;
    size_t total_bytes = (size_t)100 * send_bytes* 2;
    double speed = (double)total_bytes/1024/1024/consume_time;
    printf("[rank %d] tranfer_size:%lld consume_time:%.6lf speed:%.2lfMBytes/sec %.6lf iters/sec\n",
         myrank, (long long)total_bytes, consume_time, speed, (double)100/consume_time);

    MPI_Finalize();
    return  0;
}



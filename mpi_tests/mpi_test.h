#pragma once

#include <mpi.h>
#include <common/common.h>
#include <sendrecv.h>
#include <unistd.h>

const int LISTEN_PORT = 40082;
char** PHONEBOOK_IP = nullptr;
char** PHONEBOOK_IB = nullptr;
int COMM_SIZE, MY_RANK;

int main(int argc, char** argv)
{
    CCALL(MPI_Init(&argc, &argv));

    CCALL(MPI_Comm_size(MPI_COMM_WORLD, &COMM_SIZE));
    CCALL(MPI_Comm_rank(MPI_COMM_WORLD, &MY_RANK));

    PHONEBOOK_IB = new char*[COMM_SIZE];
    PHONEBOOK_IP = new char*[COMM_SIZE];
    for (int i = 0; i < COMM_SIZE; ++i) {
        PHONEBOOK_IP[i] = new char[MPI_MAX_PROCESSOR_NAME];
        PHONEBOOK_IB[i] = new char[MPI_MAX_PROCESSOR_NAME];
    }

    char hostname[65];
    CCALL(gethostname(hostname, sizeof(hostname)));
    if (strcmp(hostname, "K210") == 0) {
        strcpy(PHONEBOOK_IP[MY_RANK], "192.168.4.10");
        strcpy(PHONEBOOK_IB[MY_RANK], "192.168.14.10");
    }
    else if (strcmp(hostname, "K228") == 0) {
        strcpy(PHONEBOOK_IP[MY_RANK], "192.168.4.28");
        strcpy(PHONEBOOK_IB[MY_RANK], "192.168.14.28");
    }
    else {
        ERROR("Unknown hostname: %s (must be K210 or K228)\n", hostname);
        ASSERT(0);
    }

    for (int i = 0; i < COMM_SIZE; ++i) {
        MPI_Bcast(PHONEBOOK_IB[i], MPI_MAX_PROCESSOR_NAME, MPI_CHAR, i, MPI_COMM_WORLD);
        MPI_Bcast(PHONEBOOK_IP[i], MPI_MAX_PROCESSOR_NAME, MPI_CHAR, i, MPI_COMM_WORLD);
    }

    if (MY_RANK == 0) {
        printf("Altogether %d processes:\n", COMM_SIZE);
        for (int i = 0; i < COMM_SIZE; ++i) {
            printf("[%d] IP: %s, IB: %s\n", i, PHONEBOOK_IP[i], PHONEBOOK_IB[i]);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Do the test now!
    extern void test();
    test();

    CCALL(MPI_Finalize());
    return 0;
}

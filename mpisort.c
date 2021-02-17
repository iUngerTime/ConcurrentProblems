//****************************************************
// MPI Parallel sort
//
// Author: Phil Howard
#ifndef _POSIC_C_SOURCE
#define _POSIX_C_SOURCE 200809
#endif

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include "usec.h"

static int g_data_size = 1000000;
static int g_print_values = 0;
static unsigned int g_seed = 0;
static unsigned int g_algo = 0;

int cmpfunc (const void * a, const void * b)
{
    return (*(int*)a - *(int*)b);
}

//***********************************************
// Take 2 arrays and merge them into destination
static void merge(int * dest, int * a, int aSize, int * b, int bSize)
{
    int aInd = 0;
    int bInd = 0;
    int n = 0;

    while(aInd < aSize && bInd < bSize)
    {
        if(a[aInd] <= b[bInd])
        {
            dest[n] = a[aInd];
            aInd++;
        }
        else
        {
            dest[n] = b[bInd];
            bInd++;
        }
        n++;
    }

    //copy remaining from A
    while(aInd < aSize)
    {
        dest[n] = a[aInd];
        aInd++;
        n++;
    }

    //copy remaining from B
    while(bInd < bSize)
    {
        dest[n] = b[bInd];
        bInd++;
        n++;
    }
}

static void hypersort(int *data, int size, int * localData, int localSize)
{
    //World size and rank
    int worldSize;
    int worldRank;
    int numParts;
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);

    //set the number of partitions per bucket
    numParts = worldSize - 1;

    //Scatter data to everyone
    //printf("Local Size: %d\n", localSize);
    MPI_Scatter(data, size/worldSize, MPI_INT, localData, localSize, MPI_INT, 
            0, MPI_COMM_WORLD);

    //local sort of our bucket
    qsort(localData, localSize, sizeof(int), cmpfunc);

    //pick representative elements (worldsize - 1)
    int * localReps = malloc(sizeof(int) * numParts);
    int increment = localSize / worldSize;
    for(int i = 0; i < numParts; i++)
    {
        localReps[i] = localData[(increment * (i + 1))];
        //printf("Rank: %d Local Rep = %d\n", worldRank, localReps[i]);
    }

    //all representative array
    int * allReps = NULL;
    int numAllReps = (numParts * worldSize);
    if(worldRank == 0)
        allReps = malloc(sizeof(int) * numAllReps);

    //Gather the info into processor 0
    MPI_Gather(localReps, numParts, MPI_INT, allReps, numParts, 
            MPI_INT, 0, MPI_COMM_WORLD); 

    //sort all the representatives
    if(worldRank == 0)
    {
        //sort
        qsort(allReps, numAllReps, sizeof(int), cmpfunc);

        //pick reps
        increment = numAllReps / worldSize;
        //printf("Increment for all reps: %d\n", increment);
        for(int i = 0; i < numParts; i++)
        {
            localReps[i] = allReps[(increment * (i + 1))];
            //printf("All Rep = %d\n", localReps[i]);
        }

        free(allReps);
    }

    //broadcast all reps
    MPI_Bcast(localReps, numParts, MPI_INT, 0, MPI_COMM_WORLD);

    //Everyone should have the same reps..
    /*for(int i = 0; i < numParts; i++)
    {
        printf("Reps sent by rank 0: %d\n", localReps[i]);
    }*/

    //local partitions array to store all partitions
    int ** partitions = malloc(sizeof(int *) * worldSize);
    int * partSize = malloc(sizeof(int) * worldSize);
    for(int i = 0; i < worldSize; i++)
    {
        partitions[i] = NULL;
        partSize[i] = 0;
    }
    int currRep = 0;
    partitions[currRep] = &localData[0];

    //Partition data into arrays
    for(int i = 0; i < localSize;)
    {
        //printf("Piece of data: %d\n", localData[i]);
        //printf("CurrRep: %d\n", currRep);
        //printf("local rep: %d\n", localReps[currRep]);
        if(currRep != worldSize - 1 && (localReps[currRep] < localData[i] || 
                    i == localSize - 1))
        {
            //go to next partition
            currRep++;

            //set the current spot of the local data to the partition
            partitions[currRep] = &localData[i];
            //printf("Curr representative value: %d\n", currRep);
        }
        else
        {
            i++; 
            partSize[currRep]++;
        }
    }

    /*printf("Printing partitions of rank %d\n", worldRank);
    for(int i = 0; i < worldSize; i++)
    {
        for(int j = 0; j < partSize[i]; j++)
        {
            printf("\tIn Part: %d is value: %d\n", i, *(partitions[i] + j));
        }
    }*/

    //everyone gets their partition
    MPI_Barrier(MPI_COMM_WORLD);

    //add self to the local partitions
    int * localPartitions = malloc(sizeof(int) * partSize[worldRank]);
    int localPartSize = partSize[worldRank];

    for(int i = 0; i < localPartSize; i++)
    {
        localPartitions[i] = *(partitions[worldRank] + i);
        //printf("  Added %d to local partition of rank %d\n",
        //        localPartitions[i], worldRank);
    }

    //Send data to processors
    for(int i = 0; i < worldSize; i++)
    {
        if(worldRank == i) //<---Self gathers its pieces
        {
            //gather partitions from all other processors
            for(int j = 0; j < worldSize; j++)
            {
                if(j != worldRank) //<--skip self
                {
                    //recieve size
                    int recvSize = -1;
                    MPI_Recv(&recvSize, 1, MPI_INT, j, 0, MPI_COMM_WORLD,
                            MPI_STATUS_IGNORE);

                    //create temp recvBuck
                    int * recvBucket = malloc(sizeof(int) * recvSize);
                    MPI_Recv(recvBucket, recvSize, MPI_INT, j, 0, MPI_COMM_WORLD,
                            MPI_STATUS_IGNORE);

                    /*printf("Rank %d recieved from Rank %d %d vals\n", 
                            worldRank, j, recvSize);
                    for(int x = 0; x < recvSize; x++)
                    {
                        printf("\t\tval recieved: %d\n", recvBucket[x]);
                    }*/

                    //create new array to store both
                    int * newData = malloc(sizeof(int) *
                            (recvSize + localPartSize));

                    //printf("Rank %d created new array of size %d\n", 
                    //        worldRank, (recvSize + localPartSize));

                    //merge the left and right into the new array
                    merge(newData, localPartitions, localPartSize,
                            recvBucket, recvSize);

                    //free recv and local partitions
                    free(localPartitions);
                    free(recvBucket);

                    /*for(int x = 0; x < recvSize + localPartSize; x++)
                    {
                        printf("\t\tmerged data val: %d\n", newData[x]);
                    }*/

                    //set the local partitions and size to new array
                    localPartitions = newData;
                    localPartSize += recvSize;
                }
            }
        }
        else
        {
            //printf("Rank %d sending to Rank %d\n", worldRank, i);
            //for(int j = 0; j < partSize[i]; j++)
            //  {
            //  printf("\tSending to %d val: %d\n", i, *(partitions[i] + j));
            //  }/
            //send size and partition
            MPI_Send(&partSize[i], 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(partitions[i], partSize[i], MPI_INT, i, 0, MPI_COMM_WORLD);
        }
    }

    //done with the partitions
    free(localReps);

    //final gather
    if(worldRank == 0)
    {
        int * finalData = malloc(sizeof(int) * size);
        int currDataInd = 0;

        //append local partitions of rank 0
        for(int i = 0; i < localPartSize; i++)
        {
            finalData[currDataInd + i] = localPartitions[i]; 
            //printf("\tPlaced %d at index %d\n", localPartitions[i], currDataInd + i);
        }
        currDataInd += localPartSize;

        //get data from all processors
        for(int i = 1; i < worldSize; i++)
        {
            //get the size
            int recvSize = -1;
            MPI_Recv(&recvSize, 1, MPI_INT, i, 0, MPI_COMM_WORLD,
                    MPI_STATUS_IGNORE);

            //printf("recieving %d values\n", recvSize);
            int * newBucket = malloc(sizeof(int) * recvSize);
            MPI_Recv(newBucket, recvSize,
                    MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            //append new bucket
            for(int i = 0; i < recvSize; i++)
            {
                finalData[currDataInd + i] = newBucket[i]; 
                //printf("\tPlaced %d at index %d\n", newBucket[i], currDataInd + i);
            }
            //printf("recv %d values\n", recvSize);
            //for(int i = 0; i < recvSize; i++)
            //{
            //    printf("\tindex: %d\tValue: %d\n", i,finalData[currDataInd + 1]);
            //}
            currDataInd += recvSize;
        }

        //set data = local data
        for(int i = 0; i < size; i++)
        {
            data[i] = finalData[i];
            //printf("Final data: %d\n", finalData[i]);
        }
    }
    else
    {
        //printf("Rank %d sending final to root\n", worldRank);
        //send size and partition
        //printf("sending %d values\n", localPartSize);
        MPI_Send(&localPartSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        MPI_Send(localPartitions, localPartSize, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }
}

static int raise_number(int power, int base)
{
    int value = 1;
    while(power != 0)
    {
        value = value * base;
        power--;
    }
    return value;
}

// Returns 0 if not a partner
// returns 1 if is a sender
// returns 2 if is a reciever
// returns 3 if is the final iter
// return -1 if no longer participant
static int SendOrRecv(int worldSize, int rank, int iter, int * partner)
{
    //computer power
    int jump = raise_number(iter, 2);
    //printf("jump: %d\n", jump);

    //end condition check
    if(jump >= worldSize)
        return 3;

    //find the core's designated job on this iter
    int currPart = 0;
    int recvr = 1;
    int prev = 0;
    while(currPart < worldSize)
    {
        if(currPart == rank)
        {
            //set the partner to the prev
            *partner = prev;

            //determine if sender (1), reciever (2), or noParter (0)
            if(recvr == 1 && (currPart + jump) >= worldSize)
                return 0; //no Partner

            if(recvr == 0)
            {
                *partner = prev;
                return 1; //is sender
            }

            if(recvr == 1) 
            {
                *partner = currPart + jump;
                return 2; //is reciever
            }
        }

        //Toggle if the next index is reciever or sender
        recvr = (recvr == 1 ? 0 : 1);

        //set the previous participating rank
        prev = currPart;

        //go to next core
        currPart += jump;
    }

    return -1;
}

//************************************
// Name: Merge sort
static void mergesort(int *data, int size, int * localData, int localSize)
{
    //World size and rank
    int worldSize;
    int worldRank;
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);

    //Scatter data first
    //printf("Local Size: %d\n", localSize);
    MPI_Scatter(data, size/worldSize, MPI_INT, localData, localSize, MPI_INT, 
            0, MPI_COMM_WORLD);

    //local sort
    qsort(localData, localSize, sizeof(int), cmpfunc);

    //merge until done
    int partner = -1;
    int iter = 0;
    int res = 0;
    while(res != 3)
    {
        //determine if sender or recieve
        res = SendOrRecv(worldSize, worldRank, iter, &partner);

        if(res == 0) //no partner
        {
            //printf("Rank: %d has no partner on iter: %d\n", worldRank, iter);
        }
        else if(res == 1) //sender
        {
            //printf("Rank: %d is a sender on iter: %d\n", worldRank, iter);
            //printf("Rank: %d is sending to rank: %d\n", worldRank, partner);

            //send an integer of local size first
            MPI_Send(&localSize, 1, MPI_INT, partner, 0, MPI_COMM_WORLD);
            MPI_Send(localData, localSize, MPI_INT, partner, 0, MPI_COMM_WORLD);

            //senders never particpate again
            res = 3;
        }
        else if(res == 2) //reciever
        {
            //printf("Rank: %d is a reciever on iter: %d\n", worldRank, iter);
            //printf("Rank: %d is recieving from rank: %d\n", worldRank, partner);
            //Get bucket size from partner (could be unequal)
            int recvSize = -1;
            MPI_Recv(&recvSize, 1, MPI_INT, partner, 0, MPI_COMM_WORLD,
                    MPI_STATUS_IGNORE);

            //create recvBucket
            int * recvBucket = malloc(sizeof(int) * recvSize);
            MPI_Recv(recvBucket, recvSize, MPI_INT, partner, 0, MPI_COMM_WORLD,
                    MPI_STATUS_IGNORE);

            //merge the two into one big array
            int * newBucket = malloc(sizeof(int) * (localSize + recvSize));
            merge(newBucket, localData, localSize, recvBucket, recvSize);

            //free local and recv
            //free(localData); //<---this causes crash (doesn't make sense)
            free(recvBucket);

            //set new local size and data
            localData = newBucket;
            localSize += recvSize;
        }
        else if(res == 3) //final
        {
            //only rank 0 gets here
            //printf("Rank: %d finishing on iter: %d\n", worldRank, iter);

            //set data = colated local data
            for(int i = 0; i < size; i++)
            {
                data[i] = localData[i];
            }
        }

        iter++;
    }
}

//***********************************************
static void initial_setup(int argc, char **argv)
{
    int opt;

    while ( (opt = getopt(argc, argv, "hs:p:S:r:") ) != -1)
    {
        switch (opt)
        {
            case 'h':
                printf("%s\n"
                        "-h print this help message and exit\n"
                        "-s <size of dataset\n"
                        "-p <print level>\n"
                        "-S <seed value>\n"
                        "-r algorithim\n"
                        ,argv[0]);
                exit(1);
            case 's':
                // size
                g_data_size = atoi(optarg);
                break;
            case 'p':
                // print values
                g_print_values = atoi(optarg);
                break;
            case 'S':
                // set the seed value
                g_seed = atoi(optarg);
                break;
            case 'r':
                //set the sort algo
                g_algo = atoi(optarg);
                break;
        }
    }

    // Fix the data size so it is an even multiple of the number of processors
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (g_data_size % world_size != 0)
    {
        g_data_size = (g_data_size/world_size + 1)*world_size;
        if (rank == 0) printf("Rounded up to %d\n", g_data_size);
    }

}

//***********************************************
static void print_data(int mask, char *msg, int *data, int size)
{
    if (g_print_values & mask)
    {
        int rank;

        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        for (int ii=0; ii<size; ii++)
        {
            printf("%s: %2d %8d: %12d\n", msg, rank, ii, data[ii]);
        }
    }
}

//***********************************************
void init_buffers(int **data, int **localData, int *local_size)
{
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    *local_size = g_data_size / world_size;
    *localData = (int *)malloc(sizeof(int) * (*local_size));
    if (*localData == NULL)
    {
        printf("Insufficient memory\n");
        exit(1);
    }

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Create the data
    if (rank == 0)
    {
        *data = (int *)malloc(sizeof(int) * g_data_size);
        if (*data == NULL)
        {
            printf("Insufficient memory\n");
            exit(1);
        }
        if (g_seed == 0) g_seed = (unsigned int)time(NULL);
        for (int ii=0; ii<g_data_size; ii++)
        {
            (*data)[ii] = rand_r(&g_seed);
        }

        print_data(0x0001, "Original data", *data, g_data_size);
    }
}

//*****************************************************
int main(int argc, char **argv)
{
    int *data;
    int *localData;
    int local_size;

    // Initialize the MPI environment
    MPI_Init(NULL, NULL);

    initial_setup(argc, argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    init_buffers(&data, &localData, &local_size);

    MPI_Barrier(MPI_COMM_WORLD);

    uint64_t start_time = usecs();

    if(g_algo == 0) hypersort(data, g_data_size, localData, local_size);
    else mergesort(data, g_data_size, localData, local_size);

    uint64_t end_time = usecs();

    if (rank == 0)
    {
        print_data(0x00FF, "final data", data, g_data_size);

        // validate data order
        int current = data[0];
        int error_count = 0;
        for (int ii=0; ii<g_data_size; ii++)
        {
            if (data[ii] < current) 
            {
                if (++error_count < 20)
                    printf("error: %d value %d\n", ii, data[ii]);
            }
            current = data[ii];
        }
        if (error_count > 0) printf("Found %d errors\n", error_count);
    }

    printf("Rank %d took %f seconds\n", rank, (end_time - start_time)/1000000.0);

    if (rank == 0) free(data);
    free(localData);

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Finalize();

    return 0;
}

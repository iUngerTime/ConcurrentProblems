//**************************************************
// Binary tree test program
//
// Author: Phil Howard

#define _POSIX_C_SOURCE 2
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <cstring>
#include <atomic>

#include "cbinary.h"
#include "usec.h"

#define LOCK_MODE_NONE          1
#define LOCK_MODE_COARSE        2
#define LOCK_MODE_READ_WRITE    3
#define LOCK_MODE_FINE          4

typedef struct
{
    int64_t     initial_size;
    int64_t     duration;
    int64_t     parallel_delay;
    int64_t     serial_delay;
    uint64_t    num_inserts;
    uint64_t    num_deletes;
    uint64_t    num_lookups;
    uint64_t    num_traversals;
    pthread_t   thread_id;
    tree_t      tree;
    int         index;
    int         test_type;
    int         inserts_per_thousand;
    int         deletes_per_thousand;
    int         lock_mode;
    int         num_threads;
    uint64_t    pad[64];
} thread_data_t;

// flag used to start/stop the worker threads
std::atomic_char g_running ;

//***********************************************
static void parse_args(int argc, char **argv, thread_data_t *args)
{
    int opt;

    memset(args, 0, sizeof(thread_data_t));

    // set defaults
    args->initial_size = 1000;
    args->inserts_per_thousand = 0;
    args->deletes_per_thousand = 0;
    args->lock_mode = LOCK_MODE_NONE;
    args->num_threads = 1;
    args->duration = 1000000;
    args->parallel_delay = 0;
    args->serial_delay = 0;

    while ( (opt = getopt(argc, argv, "hi:I:D:m:t:d:B:L:") ) != -1)
    {
        switch (opt)
        {
            case 'h':
                printf("%s\n"
                       "-h print this help message and exit\n"
                       "-i <start tree size>\n"
                       "-I <n> perform <n> inserts per 1000 operations\n"
                       "-D <n> perform <n> deletes per 1000 operations\n"
                       "-m <mode> set lock mode\n"
                       "    1 no locks\n"
                       "    2 Coarse grained locking\n"
                       "    3 Reader-Writer locking\n"
                       "    4 Fine grained locking\n"
                       "-t <nthreads> number of threads to run\n"
                       "-d <dur> duration of test in microseconds\n"
                       "-B <delay> delay between operations in microseconds\n"
                       "-L <delay> delay for holding lock on lookups\n"
                       , argv[0]);
                exit(1);
            case 'i':
                args->initial_size = atol(optarg);
                break;
            case 'I':
                args->inserts_per_thousand = atoi(optarg);
                break;
            case 'D':
                args->deletes_per_thousand = atoi(optarg);
                break;
            case 'm':
                args->lock_mode = atoi(optarg);
                break;
            case 't':
                args->num_threads = atoi(optarg);
                break;
            case 'd':
                args->duration = atol(optarg);
                break;
            case 'B':
                args->parallel_delay = atol(optarg);
                break;
            case 'L':
                args->serial_delay = atol(optarg);
                break;
        }
    }
}
//***********************************************
static void *Thread_Func(void *arg)
{
    thread_data_t *args = (thread_data_t*)arg;

    // init random number seed
    unsigned int seed = (unsigned int)nsecs();
    int option;
    int value;

    //printf("Thread %d seed %d\n", args->index, seed);
    args->num_inserts = 0;
    args->num_deletes = 0;
    args->num_lookups = 0;
    args->num_traversals = 0;

    // wait until told to start
    while (!g_running.load())
    {}

    // process until told to stop
    while (g_running.load())
    {
        usec_delay(args->serial_delay);
        
        option = rand_r(&seed) % 1000;
        if (option < args->inserts_per_thousand)
        {
            value = rand_r(&seed)%(2*args->initial_size);
            Tree_Insert(args->tree, value);
            args->num_inserts++;
        }
        else if (option < 
                (args->inserts_per_thousand + args->deletes_per_thousand))
        {
            value = rand_r(&seed)%(2*args->initial_size);
            Tree_Delete(args->tree, value);
            args->num_deletes++;
        }
        else
        {
            element_t elem;
            value = rand_r(&seed)%(2*args->initial_size);
            elem = Tree_Lookup(args->tree, value);
            args->num_lookups++;
            usec_delay(args->serial_delay);
            //printf("main element:%p\n", elem);
            Element_Release(elem);
        }
    }

    return NULL;
}

//***********************************************
// Count the nodes in a tree
// NOTE: g_node_count must be set to zero prior to the travers
int64_t g_node_count;
void Count_Nodes(element_t element)
{
    g_node_count++;
}
//***********************************************
int main(int argc, char **argv)
{
    tree_t tree;
    thread_data_t cmd_args;

    parse_args(argc, argv, &cmd_args);
    tree = Tree_Init(cmd_args.lock_mode);
    int value;

    unsigned int seed = (unsigned int)usecs();
    for (int ii=0; ii<cmd_args.initial_size; ii++)
    {
        value = rand_r(&seed)%(2*cmd_args.initial_size);
        Tree_Insert(tree, value);
    }

    thread_data_t *threads;
    threads = (thread_data_t*)malloc(cmd_args.num_threads*sizeof(thread_data_t));

    g_running.store(0);

    g_node_count = 0;
    Tree_Traverse(tree, Count_Nodes);

    printf("There were %ld nodes in the tree at the start of the run\n\n", g_node_count);

    cmd_args.tree = tree;
    for (int ii=0; ii<cmd_args.num_threads; ii++)
    {
        memcpy(&threads[ii], &cmd_args, sizeof(cmd_args));
        threads[ii].index = ii+1;
        pthread_create(&threads[ii].thread_id, NULL, Thread_Func, &threads[ii]);
    }

    // wait for threads to start
    usleep(300000);

    // Tell threads to start processing
    g_running.store(1);

    // let threads run for specified duration
    usleep(cmd_args.duration);
    
    // stop threads
    g_running.store(0);

    uint64_t num_inserts = 0;
    uint64_t num_deletes = 0;
    uint64_t num_lookups = 0;
    uint64_t num_traversals = 0;

    for (int ii=0; ii<cmd_args.num_threads; ii++)
    {
        pthread_join(threads[ii].thread_id, NULL);
        printf("Thread %3d stats: %10ld %10ld %10ld %10ld\n",
                ii+1, threads[ii].num_inserts, threads[ii].num_deletes,
                threads[ii].num_lookups, threads[ii].num_traversals);

        num_inserts += threads[ii].num_inserts;
        num_deletes += threads[ii].num_deletes;
        num_lookups += threads[ii].num_lookups;
        num_traversals += threads[ii].num_traversals;
    }

    printf("\n");
    printf("Thread Tot stats: %10ld %10ld %10ld %10ld\n",
            num_inserts, num_deletes, num_lookups, num_traversals); 

    free(threads);

    g_node_count = 0;
    Tree_Traverse(tree, Count_Nodes);

    printf("There were %ld nodes in the tree at the end of the run\n", g_node_count);

    Tree_Destroy(tree);

    return 0;
}


//*********************************************************
// Define a function to get a timestamp in microseconds
//
// Author: Phil Howard
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#include "usec.h"

//********************************************************
int64_t usecs()
{
    int64_t now_usecs;
    struct timeval now;

    gettimeofday(&now, NULL);

    now_usecs = now.tv_sec;
    now_usecs *= 1000000;
    now_usecs += now.tv_usec;

    return now_usecs;
}

//********************************************************
int64_t nsecs()
{
    int64_t now_nsecs;
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC_COARSE, &now);

    now_nsecs = now.tv_sec;
    now_nsecs *= 1000000000;
    now_nsecs += now.tv_nsec;

    return now_nsecs;
}

// busy wait for a specified number of nsecs
void usec_delay(int64_t usec_delay)
{
    if (usec_delay < 1) return;
    int64_t end = usecs() + usec_delay;
    while (end > usecs())
    {}
}

// busy wait for a specified number of nsecs
void nsec_delay(int64_t nsec_delay)
{
    if (nsec_delay < 2) return;
    int64_t end = nsecs() + nsec_delay;
    while (end > nsecs())
    {}
}

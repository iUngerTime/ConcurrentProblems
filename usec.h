#pragma once
//*********************************************************
// Declare a function to get a timestamp in microseconds
//
// Author: Phil Howard
#include <stdint.h>

// time in microseconds
// NOTE: It takes longer to compute the usecs time, so should use nsecs
int64_t usecs();

// time in nanoseconds
int64_t nsecs();

// busy wait for a specified number of usecs
void usec_delay(int64_t usecs);

// busy wait for a specified number of nsecs
void nsec_delay(int64_t nsecs);

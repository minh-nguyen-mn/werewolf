// Taken from CSC213's Worm Lab, courtesy of Prof. Charlie Curtsinger

#ifndef UTIL_H

#define UTIL_H


#include <stdint.h>

#include <stdlib.h>


// Sleep for a given number of milliseconds

void sleep_ms(size_t ms);


// Get the time in milliseconds since UNIX epoch

size_t time_ms();


#endif
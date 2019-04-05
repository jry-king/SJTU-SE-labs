/*
 *
 * Student Name: Jin Ruiyang
 * Student ID: 516030910408
 *
 * This file is a cache simulator written in C that takes a valgrind memory trace as input, simulates the hit/miss behavior of a cache memory on this trace, and outputs the total number of hits, misses and evictions.
 * The cache simulator uses LRU replacement policy, and it ignores the instruction access in input and doesn't contain actual data. Also, it assumes that memory accesses are aligned properly so that a single memory access never crosses block boundaries.
 *
 */
#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "math.h"

int main(int argc, char* argv[])
{
    // necessary variables
    int s = 0;              // number of set index bits
    int e = 0;              // number of lines per set
    int b = 0;              // number of block offset bits
    int hitCount = 0;       // number of hits
    int missCount = 0;      // number of misses
    int evictionCount = 0;  // number of evictions
    char access[100];       // a string used to store access information
    FILE* file;

    // parse the command
    if('s' != getopt(argc, argv, "s:E:b:t:"))
    {
        printf("wrong input\n");
        return 0;
    }
    s = atoi(optarg);
    if('E' != getopt(argc, argv, "s:E:b:t:"))
    {
        printf("wrong input\n");
        return 0;
    }
    e = atoi(optarg);
    if('b' != getopt(argc, argv, "s:E:b:t:"))
    {
        printf("wrong input\n");
        return 0;
    }
    b = atoi(optarg);
    if('t' != getopt(argc, argv, "s:E:b:t:"))
    {
        printf("wrong input\n");
        return 0;
    }
    if(NULL == (file = fopen(optarg, "r")))
    {
        printf("no such file or directory\n");
        return 0;
    }
    
    /* use an array of long long int to store cache lines in case the address is too long for an integer to store
     * also store the "history": how many operation of current set there is between current operation and last modification of current tag
     * to help decide which block to evict when there is an eviction base on LRU replacement policy
     * so the size is 2*(2^s)*e*sizeof(long long int) in total
     * the cache is made of contiguous sets, each consists of contiguous cache lines, which contains a tag and the "history" number
     */
    long long int* cache = (long long int*)calloc((int)pow(2,s), 2*e*sizeof(long long int));

    while(fgets(access, 100, file))
    {
        int miss = 1;           // this access hit or miss
        int evict = 1;          // if miss, need eviction or not
        int victim = 0;         // if evict, store the "history" of the victim
        
        //deal with access to data
        if('L' == access[1] || 'M' == access[1] || 'S' == access[1])
        {
            char addressString[10] = "";                // a string used to store address
            for(int i = 3; access[i] != ','; i++)
            {
                addressString[i-3] = access[i];
            }

            long long int address = strtoll(addressString, NULL, 16);               // the numeric value of the address(decimal)
            long long int index = (address/((int)(pow(2,b))))%((int)(pow(2,s)));    // the value of index bits
            long long int tag = address/(((int)(pow(2,b)))*((int)pow(2,s)));        // the value of tag bits

            // check hit or miss
            for(int j = index * 2 * e; j < (index+1) * 2 * e; j += 2)   // scan every tag in the set
            {
                if(tag == cache[j] && cache[j+1] != 0)                  // hit
                {
                    // change "history", scan every used "history" number in the set
                    for(int k = index * 2 * e + 1; (cache[k] != 0 && k < (index+1) * 2 * e + 1); k += 2)
                    {
                        cache[k] += 1;
                    }
                    cache[j+1] = 1;                                     // set the "history" of the accessed tag to 1

                    // while do not need miss or evict on hit, set the hitCount variable
                    miss = !miss;
                    evict = !evict;
                    hitCount++;                                         // the setting of counting variable is the same for load, store and modify
                    break;
                }
            }

            // miss, check eviction 
            if(miss)
            {
                missCount++;

                for(int l = index * 2 * e; l < (index+1) * 2 * e; l += 2)
                {
                    if(0 == cache[l] && 0 == cache[l+1])                    // there is an empty block
                    {
                        cache[l] = tag;                                     // put the cache line on
                        for(int m = index * 2 * e + 1; m <= l + 1; m += 2)  // change modify time
                        {
                            cache[m]++;
                        }
                        evict = !evict;                                     // do not need to evict in cold miss
                        break;
                    }
                    // record the "history" of potential victim in case eviction is needed
                    if(victim < cache[l+1])
                    {
                        victim = cache[l+1];
                    }
                }
            }


            // conflict miss, need eviction
            if(evict)
            {
                evictionCount++;
                for(int n = index * 2 * e + 1; n < (index+1) * 2 * e + 1; n += 2)   // find out the victim block and replace it with the new block
                {
                    if(cache[n] == victim)
                    {
                        cache[n] = 0;
                        cache[n-1] = tag;
                        break;
                    }
                }
                for(int p = index * 2 * e + 1; p < (index+1) * 2 * e + 1; p += 2)   // change "history"
                {
                    cache[p]++;
                }
            }

            // modification have one more hit on store operation no matter its load operation hit or missed
            if('M' == access[1])
            {
                hitCount++;
            }
        }
    }

    free(cache);
    fclose(file);
    printSummary(hitCount, missCount, evictionCount);
    return 0;
}

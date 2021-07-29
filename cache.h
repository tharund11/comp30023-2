/* Computer Systems Project 2
   Written by Tharun Dharmawickrema
*/

#include <inttypes.h>
#include <time.h>

#include "structure.h"

#ifndef INCLUDE_CACHE_H
#define INCLUDE_CACHE_H

// structure for holding one cache entry
typedef struct{
   int incache;
   int isexpired;
   time_t expirytime;
   header header;
   question question;
   resource_record answer;
   uint8_t *additional;
} cache_member;

// function prototypes
int check_cache(cache_member *cache, question *query, header *header, int client, int messagelen, uint8_t *len, FILE *fp);
void update_cache(cache_member *cache, question query, header header, resource_record answer, uint8_t *array, FILE *fp);

#endif
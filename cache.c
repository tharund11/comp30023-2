/* Computer Systems Project 2
   Written by Tharun Dharmawickrema
   
   Helper functions for dealing with cache.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cache.h"

// magic numbers
#define CACHE_SIZE 5
#define TIMESTAMP_LEN 28
#define MESSAGE_LENGTH_BYTESIZE 2

/* Function that checks if the given request has an answer in the cache, if so
   this reply is sent to client */
int
check_cache(cache_member *cache, question *query, header *header, int client, 
            int messagelen, uint8_t *len, FILE *fp){

    int i;
    time_t currenttime;
    
    // check if any of the current cache entries has expired
    for (i = 0; i < CACHE_SIZE; i++){
        if (cache[i].incache){
            time(&currenttime);
            if (difftime(currenttime, cache[i].expirytime) >= 0){
                cache[i].isexpired = 1;
            }
        }
    }
    
    for (i = 0; i < CACHE_SIZE; i++){
        
        if (cache[i].incache & !cache[i].isexpired){
            
            // check if entry matches request
            if (strcmp(cache[i].answer.domainname, query->domainname) == 0){
                
                // print to log
                char timestring[TIMESTAMP_LEN], expirytimestring[TIMESTAMP_LEN];
                struct tm *temp;
                time(&currenttime);

                temp = localtime(&currenttime);
                strftime(timestring, TIMESTAMP_LEN, "%FT%T%z", temp);
                temp = localtime(&cache[i].expirytime);
                strftime(expirytimestring, TIMESTAMP_LEN, "%FT%T%z", temp);

                fprintf(fp, "%s %s expires at %s\n", timestring, query->domainname, 
                        expirytimestring);
                fflush(fp);

                // create a new array to send this entry to client
                uint8_t array[messagelen + MESSAGE_LENGTH_BYTESIZE];
                uint8_t *pointer = array;

                // copy message length
                memcpy(pointer, len, MESSAGE_LENGTH_BYTESIZE);
                pointer += 2;

                // change entry id to the new request id
                cache[i].header.id = (cache[i].header.id & 0) | header->id;
                memcpy(pointer, &cache[i].header.id, 2);
                pointer += 2;

                // put bits together in two bytes, for easy copy
                uint8_t number = 0, num = 0;
                uint8_t row[2];
                number += (cache[i].header.qr << 7);
                number += (cache[i].header.opcode << 3);
                number += (cache[i].header.aa << 2);
                number += (cache[i].header.tc << 1);
                number += cache[i].header.rd;
                        
                num += (cache[i].header.ra << 7);
                num += (cache[i].header.z << 6);
                num += (cache[i].header.ad << 5);
                num += (cache[i].header.cd << 4);
                num += cache[i].header.rcode;
                
                row[0] = number;
                row[1] = num;
                memcpy(pointer, row, 2);
                pointer += 2;
                
                // copy rest of  header data
                memcpy(pointer, &cache[i].header.qdcount, 2);
                pointer += 2;
                // change answer count to 1
                cache[i].header.ancount = (cache[i].header.ancount & 0) + 1;
                memcpy(pointer, &cache[i].header.ancount, 2);
                pointer += 2;
                memcpy(pointer, &cache[i].header.nscount, 2);
                pointer += 2;
                memcpy(pointer, &cache[i].header.arcount, 2);
                pointer += 2;

                // copy question data
                memcpy(pointer, cache[i].question.qname, cache[i].question.sizeofqname);
                pointer += cache[i].question.sizeofqname;
                memcpy(pointer, &cache[i].question.qtype, 2);
                pointer += 2;
                memcpy(pointer, &cache[i].question.qclass, 2);
                pointer += 2;

                //copy answer data
                memcpy(pointer, cache[i].answer.name, cache[i].answer.sizeofname);
                pointer += cache[i].answer.sizeofname;
                memcpy(pointer, &cache[i].answer.type, 2);
                pointer += 2;
                memcpy(pointer, &cache[i].answer.class, 2);
                pointer += 2;

                // change ttl value
                time(&currenttime);
                cache[i].answer.ttl = cache[i].answer.ttl - difftime(currenttime, 
                                    cache[i].answer.timearrived);
                memcpy(pointer, &cache[i].answer.ttl, 4);
                pointer += 4;

                memcpy(pointer, &cache[i].answer.rdlength, 2);
                pointer += 2;
                memcpy(pointer, cache[i].answer.rdata, cache[i].answer.rdlength);
                pointer += cache[i].answer.rdlength;

                // copy any extra bytes if there
                if (cache[i].header.nscount > 0 || cache[i].header.arcount > 0){
                    memcpy(pointer, cache[i].additional, sizeof *cache[i].additional);
                }

                pointer = array;
                int n, totalsent;

                // attempt to send full message back to client in one write call
                n = write(client, array, messagelen + MESSAGE_LENGTH_BYTESIZE);
                if (n < 0) {
                    perror("write call (client connection)");
                    exit(EXIT_FAILURE);
                }
                pointer += n;
                
                //if full message not sent, keep writing until sent
                totalsent = n;
                while (totalsent != (messagelen + MESSAGE_LENGTH_BYTESIZE)){
                    n = write(client, pointer, (messagelen + MESSAGE_LENGTH_BYTESIZE) 
                            - totalsent);
                    if (n < 0){
                        perror("write call (client connection)");
                        exit(EXIT_FAILURE);
                    }
                    totalsent += n;
                    pointer += n;
                }
                return 1;
            }
        }
    }
    return 0;
}

/* Function that adds the given server answer to the correct
   cache position */
void
update_cache(cache_member *cache, question query, header header, 
             resource_record answer, uint8_t *array, FILE *fp){

    int i;
    struct tm *temp;
    time_t currenttime;
    char timestring[TIMESTAMP_LEN];
    
    // if there is an expired cache entry, put reply there
    for (i = 0; i < CACHE_SIZE; i++){
        if (cache[i].incache & cache[i].isexpired){
            
            // print to log
            time(&currenttime);
            temp = localtime(&currenttime);
            strftime(timestring, TIMESTAMP_LEN, "%FT%T%z", temp);
            fprintf(fp, "%s replacing %s by %s\n", timestring, 
                    cache[i].answer.domainname, answer.domainname);
            fflush(fp);

            cache[i].header = header;
            cache[i].question = query;
            cache[i].answer = answer;
            cache[i].incache = 1;
            cache[i].isexpired = 0;
            if (header.nscount > 0 || header.arcount > 0){
                cache[i].additional = array;
            }
            // calculate time at which entry will expire            
            cache[i].expirytime = answer.timearrived + (int)answer.ttl;
            return;
        }
    }

    // if no expired entries, check for empty entries, put reply there
    for (i = 0; i < CACHE_SIZE; i++){
        if (!cache[i].incache){
            cache[i].header = header;
            cache[i].question = query;
            cache[i].answer = answer;
            cache[i].incache = 1;
            cache[i].isexpired = 0;
            if (header.nscount > 0 || header.arcount > 0){
                cache[i].additional = array;
            }
            cache[i].expirytime = answer.timearrived + (int)answer.ttl;
            return;
        }
    }

    // if no expired or empty entries, find the index of the entry which is
    // closest to expiring, put reply there
    time_t current;
    time(&current);
    double result, closest = 10000;
    int closest_index;

    for (i = 0; i < CACHE_SIZE; i++){
        if (cache[i].incache){
            result = difftime(cache[i].expirytime, current);
            if (result <= closest){
                closest_index = i;
                closest = result;
            }
        }
    }

    // print to log
    time(&currenttime);
    temp = localtime(&currenttime);
    strftime(timestring, TIMESTAMP_LEN, "%FT%T%z", temp);
    fprintf(fp, "%s replacing %s by %s\n", timestring, 
            cache[closest_index].answer.domainname, answer.domainname);
    fflush(fp);

    // place reply at found index
    cache[closest_index].header = header;
    cache[closest_index].question = query;
    cache[closest_index].answer = answer;
    cache[closest_index].incache = 1;
    cache[closest_index].isexpired = 0;
    if (header.nscount > 0 || header.arcount > 0){
        cache[closest_index].additional = array;
    }
    cache[closest_index].expirytime = answer.timearrived + (int)answer.ttl;
    return;
}
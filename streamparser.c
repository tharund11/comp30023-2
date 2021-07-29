/* Computer Systems Project 2
   Written by Tharun Dharmawickrema
   
   Helper functions for parsing byte stream.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <arpa/inet.h>

#include "structure.h"

// magic numbers
#define END_OF_STRING '\0'

/* Function that reads data from query header and question into structures &
   returns a pointer to the position at which parsing stopped */
uint8_t 
*parse_query(uint8_t *read_array, header *query_header, question *query_question){

    int i;
    uint8_t *pointer = read_array;
   
    // store query id
    memcpy(&(query_header->id), pointer, sizeof(query_header->id));
    query_header->id = ntohs(query_header->id);
    pointer += 2;

    // do bit operations & store the bits
    uint8_t temp; 
    memcpy(&temp, pointer, sizeof temp);
    pointer += 1;
    query_header->qr = (temp >> 7) & 1;
    query_header->opcode = (temp >> 3) & 15;
    query_header->aa = (temp >> 2) & 1;
    query_header->tc = (temp >> 1) & 1;
    query_header->rd = temp & 1;

    memcpy(&temp, pointer, sizeof temp);
    pointer += 1;
    query_header->ra = (temp >> 7) & 1;
    query_header->z = (temp >> 6) & 1;
    query_header->ad = (temp >> 5) & 1;
    query_header->cd = (temp >> 4) & 1;
    query_header->rcode = temp & 15;

    memcpy(&(query_header->qdcount), pointer, sizeof query_header->qdcount);
    query_header->qdcount = ntohs(query_header->qdcount);
    pointer += 2;
    
    memcpy(&(query_header->ancount), pointer, sizeof query_header->ancount);
    query_header->ancount = ntohs(query_header->ancount);
    pointer += 2;
    
    memcpy(&(query_header->nscount), pointer, sizeof query_header->nscount);
    query_header->nscount = ntohs(query_header->nscount);
    pointer += 2;
        
    memcpy(&(query_header->arcount), pointer, sizeof query_header->arcount);
    query_header->arcount = ntohs(query_header->arcount);
    pointer += 2;
            
    uint8_t *temp_pointer = pointer;
    int count = 0;
    // find the position of the last character in the query name
    while (*temp_pointer != 0){
        temp_pointer++;
        count++;
    }
    count++;
    
    // store the query name
    query_question->qname = (uint8_t*)malloc(count * sizeof(uint8_t));
    assert(query_question->qname != NULL);
    for (i = 0; i < count; i++){
        memcpy(&query_question->qname[i], pointer, 1);
        pointer++;
    }
    query_question->sizeofqname = count;

    // convert query name into desired domain name format
    query_question->domainname = (char*)malloc((count - 1) * sizeof(char));
    int pos = 0, size;
    count = 0;

    // convert each int to ascii char & add '.'
    while (pos < (query_question->sizeofqname - 1)){
        if (pos != 0){
            query_question->domainname[count] = '.';
            count++;
        }
        size = query_question->qname[pos];
        pos++;
        for (i = 0; i < size; i++){
            query_question->domainname[count] = (char)query_question->qname[pos];
            count++;
            pos++;
        }
    }
    query_question->domainname[count] = END_OF_STRING;
        
    memcpy(&(query_question->qtype), pointer, sizeof query_question->qtype);
    query_question->qtype = ntohs(query_question->qtype);
    pointer += 2;
    
    memcpy(&(query_question->qclass), pointer, sizeof query_question->qclass);
    query_question->qclass = ntohs(query_question->qclass);
    pointer += 2;
    
    return pointer;
}

/*  Function that returns the total number of bytes
    that have already been saved into structures */
int getnototalsaved(uint8_t *pointer, uint8_t *array){
    uint8_t *current = array;
    int count = 0;

    while (current != pointer){
        current++;
        count++;
    }
    return count;
}

/*  Function that stores the rest of the bytes into an array */
void parse_additional(int size, uint8_t *pointer, uint8_t *array){
    memcpy(array, pointer, size);
    pointer += size;
}

/*  Function that reads data from answer, stores into structure &
    returns pointer to where the parsing stopped */
uint8_t 
*parse_answer(uint8_t *pointer, resource_record *answer, question *answer_question){
    
   uint16_t temp;
   int i;
     
   memcpy(&temp, pointer, sizeof temp); // store the qname
   temp = ntohs(temp); 
   
   // check if first two bits of qname are '11'
   if (((temp >> 14) & 3) == 3){
       // if qname equals "c00c", query name = question name
       if (((temp >> 2) & 3) == 3){
           answer->name = answer_question->qname;
           answer->sizeofname = answer_question->sizeofqname;
           answer->domainname = answer_question->domainname;
           pointer += 2;
        }
    }else{
        // if query name is different, store it
        uint8_t *temp_pointer = pointer;
        int count = 0;

        // find the position of the last character in the query name
        while (*temp_pointer != 0){
            temp_pointer++;
            count++;
        }
        count++;
        
        answer->name = (uint8_t*)malloc(count * sizeof(uint8_t));
        assert(answer->name != NULL);
        for (i = 0; i < count; i++){
            memcpy(&answer->name[i], pointer, 1);
            pointer++;
        }
        answer->sizeofname = count;

        // convert query name into desired domain name format
        answer->domainname = (char*)malloc((count - 1) * sizeof(char));
        int pos = 0, size;
        count = 0;

        // convert each int to ascii char & add '.'
        while (pos < (answer->sizeofname - 1)){
            if (pos != 0){
                answer->domainname[count] = '.';
                count++;
            }
            size = answer->name[pos];
            pos++;
            for (i = 0; i < size; i++){
                answer->domainname[count] = (char)answer->name[pos];
                count++;
                pos++;
            }
        }
        answer->domainname[count] = END_OF_STRING;
    }
    
    // store other fields
    memcpy(&(answer->type), pointer, sizeof answer->type);
    answer->type = ntohs(answer->type);
    pointer += 2;
    
    memcpy(&(answer->class), pointer, sizeof answer->class);
    answer->class = ntohs(answer->class);
    pointer += 2;

    memcpy(&(answer->ttl), pointer, sizeof answer->ttl);
    answer->ttl = ntohl(answer->ttl);
    pointer += 4;

    memcpy(&(answer->rdlength), pointer, sizeof answer->rdlength);
    answer->rdlength = ntohs(answer->rdlength);
    pointer += 2;

    // store rdata
    answer->rdata = (uint8_t*)malloc(answer->rdlength * sizeof(uint8_t));
    assert(answer->rdata != NULL);
    for (i = 0; i < answer->rdlength; i++){
        memcpy(&answer->rdata[i], pointer, 1);
        pointer++;
    }
    
    return pointer;
}
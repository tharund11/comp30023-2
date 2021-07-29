/* Computer Systems Project 2
   Written by Tharun Dharmawickrema
*/

#include <inttypes.h>
#include <time.h>

#ifndef INCLUDE_STRUCTURE_H
#define INCLUDE_STRUCTURE_H

// structure for holding the question
typedef struct{
   uint8_t *qname;
   int sizeofqname;
   char *domainname;
   uint16_t qtype;
   uint16_t qclass;
} question;

// structure for holding the header
typedef struct{
   uint16_t id;
   uint8_t qr;
   uint8_t opcode;
   uint8_t aa;
   uint8_t tc;
   uint8_t rd;
   uint8_t ra;
   uint8_t z;
   uint8_t ad;
   uint8_t cd;
   uint8_t rcode;
   uint16_t qdcount;
   uint16_t ancount;
   uint16_t nscount;
   uint16_t arcount;
} header;

// structure for holding a resource record
typedef struct{
   uint8_t *name;
   int sizeofname;
   char *domainname;
   uint16_t type;
   uint16_t class;
   uint32_t ttl;
   uint16_t rdlength;
   uint8_t *rdata;
   time_t timearrived;
} resource_record;

// function prototypes
uint8_t *parse_query(uint8_t *read_array, header *query_header, question *query_question);
int getnototalsaved(uint8_t *pointer, uint8_t *array);
void parse_additional(int size, uint8_t *pointer, uint8_t *array);
uint8_t *parse_answer(uint8_t *pointer, resource_record *answer, question *answer_question);

#endif
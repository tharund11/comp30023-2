/* Computer Systems Project 2
   Written by Tharun Dharmawickrema
   
   Program which creates a DNS server. Some of the code has been 
   attributed from the lecture slides, Practical 9 & 10.
*/
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <inttypes.h>
#include <time.h>

#include "structure.h"
#include "cache.h"

// Magic numbers
#define PORT "8053"
#define TIMESTAMP_LEN 28
#define CACHE_SIZE 5
#define MESSAGE_LENGTH_BYTESIZE 2
#define AAAA_REQUEST_INT 28
#define IPV6_BYTESIZE 16

#define CACHE

int main(int argc, char *argv[]) {
   
   int listenfd, connfd, n, i;
   struct addrinfo hints, *server_info;
   time_t rawtime;
   struct tm *currenttime;
   char timestring[TIMESTAMP_LEN];
   FILE *filepointer = fopen("dns_svr.log", "w");
	
   // initalising vars to create the server
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;       
	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_flags = AI_PASSIVE; 
	
	int status = getaddrinfo(NULL, PORT, &hints, &server_info);
	if (status != 0) {
		fprintf(stderr, "getaddrinfo (my server) call error: %s\n", 
            gai_strerror(status));
		exit(EXIT_FAILURE);
	}

   // creating socket to listen on
   listenfd = socket(server_info->ai_family, server_info->ai_socktype, 
                  server_info->ai_protocol);
   if (listenfd < 0){
      perror("socket (my server) connection call");
      exit(EXIT_FAILURE);
   }

   int enable = 1;
   if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))<0){
      perror("setsockopt call");
      exit(1);
   }
   if (bind(listenfd, server_info->ai_addr, server_info->ai_addrlen) < 0){
      perror("bind call");
      exit(EXIT_FAILURE);
   }
   freeaddrinfo(server_info);

   if (listen(listenfd, 10) < 0){  // listening for client
      perror("listen call");
      exit(EXIT_FAILURE);
   }
   
   // declaring vars to connect to upstream server
   struct addrinfo hints_server, *res, *rp;
   int serverfd;
   
   memset(&hints_server, 0, sizeof hints_server);
   hints_server.ai_family = AF_INET;       
   hints_server.ai_socktype = SOCK_STREAM; 

   // creating a cache structure and initalizing with empty members
   cache_member cache[CACHE_SIZE];
   for (i = 0; i < CACHE_SIZE; i++){
      cache_member new;
      new.incache = 0;
      new.isexpired = 0;
      cache[i] = new;
   }
   
   while (1){  // loop till ctrl+c pressed

      // receiving client connection
      struct sockaddr_storage client_address;
      socklen_t client_address_size = sizeof client_address;
      connfd = accept(listenfd, (struct sockaddr*) &client_address, 
                  &client_address_size);
      if (connfd < 0) {
         perror("accept call");
         exit(EXIT_FAILURE);
      }

      uint8_t len[MESSAGE_LENGTH_BYTESIZE];
      n = read(connfd, len, MESSAGE_LENGTH_BYTESIZE); // read length of message
      if (n < 0){
         perror("read call (client connection)");
         exit(EXIT_FAILURE);
      }

      uint16_t length; // convert length to an int
      memcpy(&length, len, sizeof length);
      int message_length = (int) ntohs(length);
      int totalread = 0;
      
      // attempt to read full message in one read call
      uint8_t read_array[message_length];
      uint8_t *array_pointer = read_array;
      n = read(connfd, array_pointer, message_length);
      if (n < 0){
         perror("read call (client connection");
         exit(EXIT_FAILURE);
      }
      array_pointer += n;
      
      // if full message not received, keep reading until received
      totalread += n;
      while (totalread != message_length){
         n = read(connfd, array_pointer, message_length - totalread);
         if (n < 0){
            perror("read call (client connection");
            exit(EXIT_FAILURE);
         }
         totalread += n;
         array_pointer += n;
      }
      
      // data structures to hold query info
      header query_header;
      question query_question;
      int notIP6 = 0;
      uint8_t *pointer = parse_query(read_array, &query_header, &query_question);
          
      // get current time and print to log
      time(&rawtime);
      currenttime = localtime(&rawtime);
      strftime(timestring, TIMESTAMP_LEN, "%FT%T%z", currenttime);
      fprintf(filepointer, "%s requested %s\n", timestring, query_question.domainname);
      fflush(filepointer);

      // if any extra records (authority/additional), parse to an array
      if (query_header.nscount > 0 || query_header.arcount > 0){
         int currentsaved = getnototalsaved(pointer, read_array);
         uint8_t additionalrecords[message_length - currentsaved];
         parse_additional(message_length - currentsaved, pointer, additionalrecords);
      }
      
      // check if request is AAAA
      if (query_question.qtype != AAAA_REQUEST_INT){
         notIP6 = 1;
      }

      if (!notIP6){
         // check if request is already in cache
         if (!check_cache(cache, &query_question, &query_header, connfd, 
                           message_length, len, filepointer)){
            
            // if not, connect to upstream server
            status = getaddrinfo(argv[1], argv[2], &hints_server, &res);
            if (status != 0) {
               fprintf(stderr, "getaddrinfo (upstream server) call error: %s\n", 
                     gai_strerror(status));
               exit(EXIT_FAILURE);
            }

            for (rp = res; rp != NULL; rp = rp->ai_next) {
               serverfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
               if (serverfd == -1)
                  continue;
               if (connect(serverfd, rp->ai_addr, rp->ai_addrlen) != -1)
                  break;
               close(serverfd);
            }
            if (rp == NULL) {
               fprintf(stderr, "failed to connect to upstream server\n");
               exit(EXIT_FAILURE);
            }
            freeaddrinfo(res);

            // send request to upstream server
            n = write(serverfd, len, MESSAGE_LENGTH_BYTESIZE);
            if (n < 0) {
               perror("write call (upstream server connection)");
               exit(EXIT_FAILURE);
            }

            // attempt to send full message in one write call
            uint8_t *send_pointer = read_array;
            int totalsent = 0;
            n = write(serverfd, send_pointer, message_length);
            if (n < 0) {
               perror("write call (upstream server connection)");
               exit(EXIT_FAILURE);
            }
            send_pointer += n;

            //if full message not sent, keep writing until sent
            totalsent += n;
            while (totalsent != message_length){
               n = read(serverfd, send_pointer, message_length - totalsent);
               if (n < 0){
                  perror("write call (upstream server connection)");
                  exit(EXIT_FAILURE);
               }
               totalsent += n;
               send_pointer += n;
            }

            uint8_t reply_len[2]; // read message length
            n = read(serverfd, reply_len, MESSAGE_LENGTH_BYTESIZE);
            if (n < 0){
               perror("read call (upstream server connection)");
               exit(EXIT_FAILURE);
            }

            uint16_t server_reply_length; // convert length to int
            memcpy(&server_reply_length, reply_len, sizeof server_reply_length);
            int server_message_length = (int) ntohs(server_reply_length);
            
            // attempt to read message in one read call
            uint8_t server_reply[server_message_length];
            uint8_t *reply_pointer = server_reply;
            n = read(serverfd, reply_pointer, server_message_length);
            if (n < 0) {
               perror("read call (upstream server connection)");
               exit(EXIT_FAILURE);
            }
            reply_pointer += n;

            // if full message not received, keep reading until received
            totalread = n;
            while (totalread != server_message_length){
               n = read(serverfd, reply_pointer, server_message_length - totalread);
               if (n < 0){
                  perror("read call");
                  exit(EXIT_FAILURE);
               }
               totalread += n;
               reply_pointer += n;
            }
            
            // data structures for reply info
            header answer_header;
            question answer_question;
            uint8_t *svr_pointer = parse_query(server_reply, &answer_header, 
                                 &answer_question);
            
            // if there any answers given, parse into array of answers
            resource_record answers[answer_header.ancount];
            for (i = 0; i < answer_header.ancount; i++){
               resource_record answer;
               svr_pointer = parse_answer(svr_pointer, &answer, &answer_question);
               answers[i] = answer;
            }

            // if any extra records (authority/additional), parse to an array
            int currentsaved = getnototalsaved(svr_pointer, server_reply);
            uint8_t additional_svrrecords[server_message_length - currentsaved];
            if (answer_header.nscount > 0 || answer_header.arcount > 0){
               parse_additional(server_message_length - currentsaved, svr_pointer, 
                           additional_svrrecords);
            }

            // add this reply to cache            
            update_cache(cache, answer_question, answer_header, answers[0], 
                        additional_svrrecords, filepointer);

            // print to log if appropriate
            if (answer_header.ancount > 0){
               time(&rawtime);
               answers[0].timearrived = rawtime;
               currenttime = localtime(&rawtime);
               
               if (answers[0].type == AAAA_REQUEST_INT){ 
                  // convert first answer ip address into string
                  uint8_t address[IPV6_BYTESIZE];
                  for (i = 0; i < IPV6_BYTESIZE; i++){
                     address[i] = answers[0].rdata[i];
                  }
                  char ip_address[INET6_ADDRSTRLEN];
                  inet_ntop(AF_INET6, address, ip_address, INET6_ADDRSTRLEN);

                  strftime(timestring, TIMESTAMP_LEN, "%FT%T%z", currenttime);
                  fprintf(filepointer, "%s %s is at %s\n", timestring, answers[0].domainname, 
                        ip_address);
                  fflush(filepointer);
               }
            }

            // send reply to client
            n = write(connfd, reply_len, MESSAGE_LENGTH_BYTESIZE);
            if (n < 0) {
               perror("write call (client connection)");
               exit(EXIT_FAILURE);
            }

            // attempt to send full message in one write call
            uint8_t *newpointer = server_reply;
            n = write(connfd, newpointer, server_message_length);
            if (n < 0) {
               perror("write call (client connection)");
               exit(EXIT_FAILURE);
            }
            newpointer += n;

            //if full message not sent, keep writing until sent
            totalsent = n;
            while (totalsent != server_message_length){
               n = read(serverfd, newpointer, server_message_length - totalsent);
               if (n < 0){
                  perror("write call (upstream server connection)");
                  exit(EXIT_FAILURE);
               }
               totalsent += n;
               newpointer += n;
            }
         }
           
      }else{
         // if not AAAA request
         time(&rawtime);
         currenttime = localtime(&rawtime);
         strftime(timestring, TIMESTAMP_LEN, "%FT%T%z", currenttime);
         fprintf(filepointer, "%s unimplemented request\n", timestring);
         fflush(filepointer);

         // create a new array for the modified reply to client
         uint8_t newarray[message_length + MESSAGE_LENGTH_BYTESIZE];
         uint8_t *clientreplypointer = newarray;
         uint8_t *temp_pointer = read_array;

         // copy message length
         memcpy(clientreplypointer, len, MESSAGE_LENGTH_BYTESIZE);
         clientreplypointer += MESSAGE_LENGTH_BYTESIZE;

         memcpy(clientreplypointer, temp_pointer, 2); // copy id
         clientreplypointer += 2;
         temp_pointer += 2;
                  
         // modify the QR, RA, RCODE bits
         uint8_t number = 0, num = 0;
         uint8_t row[2];
         number += 128;  // add 10000000
         number += (query_header.opcode << 3);
         number += (query_header.aa << 2);
         number += (query_header.tc << 1);
         number += query_header.rd;
                  
         num += 128;
         num += (query_header.z << 6);
         num += (query_header.ad << 5);
         num += (query_header.cd << 4);
         num += 4;
                 
         row[0] = number;
         row[1] = num;
         memcpy(clientreplypointer, row, 2);
         clientreplypointer += 2;
         temp_pointer += 2;
         
         // copy rest of message
         memcpy(clientreplypointer, temp_pointer, message_length - 4);
         clientreplypointer = newarray;

         // attempt to send full message back to client in one write call
         n = write(connfd, clientreplypointer, message_length 
                  + MESSAGE_LENGTH_BYTESIZE);
         if (n < 0) {
            perror("write call (client connection)");
            exit(EXIT_FAILURE);
         }
         clientreplypointer += n;
         
         //if full message not sent, keep writing until sent
         int totalsent = n;
         while (totalsent != (message_length + MESSAGE_LENGTH_BYTESIZE)){
            n = write(connfd, temp_pointer, (message_length + MESSAGE_LENGTH_BYTESIZE) 
                     - totalsent);
            if (n < 0){
               perror("write call (client connection)");
               exit(EXIT_FAILURE);
            }
            totalsent += n;
            temp_pointer += n;
         }
      }
      // close upstream server & client connections
      close(serverfd);
      close(connfd);
   }
   close(listenfd);
   fclose(filepointer);
   return 0;
}
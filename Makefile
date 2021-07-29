dns_svr: dns_svr.o streamparser.o cache.o structure.h cache.h
	gcc -Wall -o dns_svr dns_svr.o streamparser.o cache.o
dns_svr.o: dns_svr.c structure.h
	gcc -Wall -c dns_svr.c
streamparser.o: streamparser.c structure.h
	gcc -Wall -c streamparser.c
cache.o: cache.c structure.h cache.h
	gcc -Wall -c cache.c
clean:
	rm -f *.exe *.o *.log dns_svr
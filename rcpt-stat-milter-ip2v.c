#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


int main(int argc, char *argv[]) {
	if(argc<=1) {
		fprintf(stderr, "Usage: %s ipaddr-value\n", argv[0]);
		return -1;
	}
	struct in_addr addr;

	inet_aton(argv[1], &addr);

	printf("%i\n", addr.s_addr);
	return 0;
}

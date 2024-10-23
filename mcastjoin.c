#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory.h>
#include <arpa/inet.h>
#include <netinet/in.h>

void usage(char *argv0)
{
	fprintf(stderr, "usage: %s ip_address:port\n\n", argv0 );
	fprintf(stderr, "(example)\n");
	fprintf(stderr, "      %s 224.0.1.129:319       ... PTP\n", argv0);
	fprintf(stderr, "      %s 239.69.83.134:5004    ... AES67 RTP\n", argv0);
	fprintf(stderr, "      %s 239.69.83.133:5004    ... AES67 RTP\n", argv0);
	fprintf(stderr, "      %s 239.255.255.255:9875  ... AES67 SAP/SDP\n", argv0);
	fprintf(stderr, "      %s 224.0.0.251:5353      ... Multicast DNS\n", argv0);
}

int main(int argc, char *argv[]) {
	int sock, rc, ip[4];
	struct sockaddr_in addr;
	struct ip_mreq mreq;
	char ipaddr[256], ipaddr2[256];
	int port;

	char buf[2048];

	if (argc != 2) {
		usage(argv[0]);
		exit(-1);
	}
	rc = sscanf(argv[1], "%d.%d.%d.%d:%d", &ip[0], &ip[1], &ip[2], &ip[3], &port);
	if (rc != 5) {
		usage(argv[0]);
		exit(-2);
	}

	sprintf(ipaddr, "%0d.%0d.%0d.%0d", ip[0], ip[1], ip[2], ip[3]);

	printf("%s:%d\n", ipaddr, port);

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	//addr.sin_port = htons(319);		// PTP(319)
	//addr.sin_port = htons(5004);		// AES67 RTP(5004)
	//addr.sin_port = htons(9875);		// AES67 SAP/SDP(9875)
	//addr.sin_port = htons(5353);		// MDNS(5353)
	//addr.sin_port = htons(1234);		// recpt1(1234)
	addr.sin_addr.s_addr = INADDR_ANY;

	bind(sock, (struct sockaddr *)&addr, sizeof(addr));

	/* setsockoptは、bind以降で行う必要あり */
	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_interface.s_addr = INADDR_ANY;
	mreq.imr_multiaddr.s_addr = inet_addr(ipaddr);
	//mreq.imr_multiaddr.s_addr = inet_addr("224.0.1.129");    // PTP:319
	//mreq.imr_multiaddr.s_addr = inet_addr("239.69.83.134");    // AES67 RTP(5004)
	//mreq.imr_multiaddr.s_addr = inet_addr("239.69.83.133");    // AES67 RTP(5004)
	//mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.255");  // AES67 SAP/SDP(9875)
	//mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251");      // MDNS:5353
	//mreq.imr_multiaddr.s_addr = inet_addr("239.0.0.0");        // recpt1:1234

	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) != 0) {
		perror("setsockopt");
		return 1;
	}

	memset(buf, 0, sizeof(buf));
	recv(sock, buf, sizeof(buf), 0);

	while (1) {
		sleep (1);
	}

	printf("%s\n", buf);

	close(sock);

	return 0;
}

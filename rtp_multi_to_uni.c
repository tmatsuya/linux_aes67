#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <alsa/asoundlib.h>


int main(int argc, char **argv) {
	int sock1;
	struct sockaddr_in addr1;
	struct ip_mreq mreq;

	int sock2;
	struct sockaddr_in addr2;
	in_addr_t ipaddr2;

	int sock3;
	struct sockaddr_in addr3;
	in_addr_t ipaddr3;

	fd_set fds, readfds;
	char ip_addr1[256], ip_addr2[256], ip_addr3[256];
	int len, i, rc, port1, port2, port3, udp_len;
	char recv_buf[1500];
	int do_flag = 1;



	if (argc != 4) {
		fprintf( stdout, "usage: %s <recv multicast IP addr:port> <dst IP addr:port> <send multicast IP addr:port\n\n", argv[0] );
		fprintf( stdout, " example: %s 239.69.83.134:5004  1.2.3.4:5006  239.69.83.135:5004  ... AVIO-USB\n", argv[0]); 
		fprintf( stdout, "        : %s 239.1.3.140:5004    1.2.3.4:5006  239.1.3.141:5004    ... RAVENNA/AES67\n", argv[0]); 
		return -1;
	} else {
		int ip[4];
		rc = sscanf( argv[1], "%d.%d.%d.%d:%d", &ip[0], &ip[1], &ip[2], &ip[3] , &port1 );
		if (rc != 5) {
			fprintf( stderr, "Invalid parameter1\n" );
			return -1;
		}
		sprintf( ip_addr1, "%0d.%0d.%0d.%0d", ip[0], ip[1], ip[2], ip[3]);
		rc = sscanf( argv[2], "%d.%d.%d.%d:%d", &ip[0], &ip[1], &ip[2], &ip[3] , &port2 );
		if (rc != 5) {
			fprintf( stderr, "Invalid parameter2\n" );
			return -1;
		}
		sprintf( ip_addr2, "%0d.%0d.%0d.%0d", ip[0], ip[1], ip[2], ip[3]);
		rc = sscanf( argv[3], "%d.%d.%d.%d:%d", &ip[0], &ip[1], &ip[2], &ip[3] , &port3 );
		if (rc != 5) {
			fprintf( stderr, "Invalid parameter3\n" );
			return -1;
		}
		sprintf( ip_addr3, "%0d.%0d.%0d.%0d", ip[0], ip[1], ip[2], ip[3]);
		printf("ip1=%s port1=%d,  ", ip_addr1, port1);
		printf("ip2=%s port2=%d,  ", ip_addr2, port2);
		printf("ip3=%s port3=%d\n",  ip_addr3, port3);
	}



	// sock1: source packet
	sock1 = socket(AF_INET, SOCK_DGRAM, 0);

	addr1.sin_family = AF_INET;
	addr1.sin_port = htons(port1);
	addr1.sin_addr.s_addr = INADDR_ANY;

	bind(sock1, (struct sockaddr *)&addr1, sizeof(addr1));

	/* setsockoptは、bind以降で行う必要あり */
	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_interface.s_addr = INADDR_ANY;
	mreq.imr_multiaddr.s_addr = inet_addr(ip_addr1);

	if (setsockopt(sock1, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) != 0) {
		perror("setsockopt");
		return 1;
	}
	// sock3: destination packet	
	sock2 = socket(AF_INET, SOCK_DGRAM, 0);

	addr2.sin_family = AF_INET;
	addr2.sin_port = htons(port2);
	addr2.sin_addr.s_addr = inet_addr(ip_addr2);

	// sock3: destination packet	
	sock3 = socket(AF_INET, SOCK_DGRAM, 0);

	addr3.sin_family = AF_INET;
	addr3.sin_port = htons(port3);
	addr3.sin_addr.s_addr = inet_addr(ip_addr3);

	/* setsockoptは、bind以降で行う必要あり */
	//memset(&mreq, 0, sizeof(mreq));
	//ipaddr = inet_addr("127.0.0.1");
	//ipaddr = inet_addr("172.16.74.131");
	//if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, (char *)&ipaddr, sizeof(ipaddr)) != 0) {
	//	perror("setsockopt");
	//	return 1;
	//}


	FD_ZERO(&readfds);
	FD_SET(sock1, &readfds);
	FD_SET(sock2, &readfds);

	do_flag = 1;

	while (do_flag) {
		memcpy(&fds, &readfds, sizeof(fd_set));

		select(sock2+1, &fds, NULL, NULL, NULL);

		if (FD_ISSET(sock1, &fds)) {
			rc = recv(sock1, recv_buf, sizeof(recv_buf), 0);
//printf("recv(sock1)=%d\n",rc);
			if (rc < 0) {
				perror("recv");
				do_flag = 0;
			}
			rc = sendto(sock2, (void *)recv_buf, rc, 0, (struct sockaddr *)&addr2, sizeof(addr2));
			if (rc < 0) {
				perror("sendto");
				do_flag = 0;
			}
		}

	
		if (FD_ISSET(sock2, &fds)) {
			rc = recv(sock2, recv_buf, sizeof(recv_buf), 0);
//printf("recv(sock2)=%d\n",rc);
			if (rc < 0) {
				perror("recv");
				do_flag = 0;
			}
			rc = sendto(sock3, (void *)recv_buf, rc, 0, (struct sockaddr *)&addr3, sizeof(addr3));
			if (rc < 0) {
				perror("sendto");
				do_flag = 0;
			}
		}

	
	}

	close(sock1);
	close(sock2);
	close(sock3);

	return 0;
}


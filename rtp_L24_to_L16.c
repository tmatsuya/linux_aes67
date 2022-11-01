#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory.h>
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

	char ip_addr1[256], ip_addr2[256];
	int len, i, rc, port1, port2, udp_len, interval;
	char recv_buf[1500], send_buf[1500], *recv_p, *send_p;
	int block_count;
	unsigned short seq_no;
	int do_flag = 1;



	if (argc != 4) {
		fprintf( stdout, "usage: %s <src IP addr:port> <dst IP addr:port> <interval(ms)>\n\n", argv[0] );
		fprintf( stdout, " example: %s 239.69.83.133:5004  239.69.83.134:5004 5  ... AVIO-USB\n", argv[0]); 
		fprintf( stdout, "        : %s 239.1.3.139:5004    239.1.3.140:5004   5  ... RAVENNA/AES67\n", argv[0]); 
		return -1;
	} else {
		int ip[4];
		rc = sscanf( argv[1], "%d.%d.%d.%d:%d", &ip[0], &ip[1], &ip[2], &ip[3] , &port1 );
		if (rc != 5) {
			fprintf( stderr, "Invalid parameter\n" );
			return -1;
		}
		sprintf( ip_addr1, "%0d.%0d.%0d.%0d", ip[0], ip[1], ip[2], ip[3]);
		rc = sscanf( argv[2], "%d.%d.%d.%d:%d", &ip[0], &ip[1], &ip[2], &ip[3] , &port2 );
		if (rc != 5) {
			fprintf( stderr, "Invalid parameter\n" );
			return -1;
		}
		sprintf( ip_addr2, "%0d.%0d.%0d.%0d", ip[0], ip[1], ip[2], ip[3]);
		interval = atoi( argv[3] );
		printf("ip1=%s port1=%d,  ", ip_addr1, port1);
		printf("ip2=%s port2=%d, interval=%dms\n", ip_addr2, port2, interval);
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

	// sock2: destination packet	
	sock2 = socket(AF_INET, SOCK_DGRAM, 0);

	addr2.sin_family = AF_INET;
	addr2.sin_port = htons(port2);
	addr2.sin_addr.s_addr = inet_addr(ip_addr2);

	/* setsockoptは、bind以降で行う必要あり */
	//memset(&mreq, 0, sizeof(mreq));
	//ipaddr = inet_addr("127.0.0.1");
	//ipaddr = inet_addr("172.16.74.131");
	//if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, (char *)&ipaddr, sizeof(ipaddr)) != 0) {
	//	perror("setsockopt");
	//	return 1;
	//}


	block_count = 0;
	seq_no = 0;
	recv_p = recv_buf;
	send_p = send_buf;
	do_flag = 1;

	while (do_flag) {
		recv_p = recv_buf;
		rc = recv(sock1, recv_buf, sizeof(recv_buf), 0);
		if (rc < 0) {
			perror("recv");
			do_flag = 0;
		}
		if (block_count == 0) {
			send_p = send_buf;
                        *send_p++ = *recv_p++;
                        *send_p++ = *recv_p++;
                        *send_p++ = seq_no >> 8;
			*send_p++ = seq_no & 0xff;
			recv_p += 2;
			++seq_no;
			for (i=4; i<12; ++i)
				*send_p++ = *recv_p++;

			for (i=0; i<6*48; i+=3) {
				*send_p++ = *recv_p++;
				*send_p++ = *recv_p++;
				++recv_p;
			}
			++block_count;
		} else {
			recv_p += 12;	// skip RTP header
			for (i=0; i<288; i+=3) {
				*send_p++ = *recv_p++;
				*send_p++ = *recv_p++;
				++recv_p;
			}
			++block_count;
		}
		if (block_count == interval) {
			rc = sendto(sock2, (void *)send_buf, (send_p - send_buf), 0, (struct sockaddr *)&addr2, sizeof(addr2));
//write(1, &rc,sizeof(rc)); write(1, send_buf, rc);
			if (rc < 0) {
				perror("sendto");
				do_flag = 0;
			}
			send_p = send_buf;
			block_count = 0;
		}
	}

	close(sock1);
	close(sock2);

	return 0;
}


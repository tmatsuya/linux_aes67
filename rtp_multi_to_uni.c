#include <stdio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <alsa/asoundlib.h>


char ip_addr[256];
char if_name[16], if_addr[16];
int port;

/* 10個以上インターフェースがある場合は増やしてください */
#define MAX_IFR 10

void get_nic_ifname(char *if_name) {
    struct ifreq ifr[MAX_IFR];
    struct ifconf ifc;
    int fd;
    int nifs, i;

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    /* データを受け取る部分の長さ */
    ifc.ifc_len = sizeof(ifr);

    /* kernelからデータを受け取る部分を指定 */
    ifc.ifc_ifcu.ifcu_buf = (void *)ifr;

    ioctl(fd, SIOCGIFCONF, &ifc);

    /* kernelから帰ってきた数を計算 */
    nifs = ifc.ifc_len / sizeof(struct ifreq);

    /* 全てのインターフェース名を表示 */
    for (i=0; i<nifs; i++) {
//        printf("%s\n", ifr[i].ifr_name);
        if (strcmp(ifr[i].ifr_name, "lo")) {
            strcpy(if_name, ifr[i].ifr_name);
            break;
        }
    }

    close(fd);

    return;
}

void *manage() {
        int sock, len, sdp_len, i, rc;
        struct sockaddr_in addr;
        char send_buf[512], sdp_buf[512];

        sock = socket(AF_INET, SOCK_DGRAM, 0);

        addr.sin_family = AF_INET;
        addr.sin_port = htons(9875);                            // AES67 SAP/SDP(9875)
        addr.sin_addr.s_addr = inet_addr("239.255.255.255");    // AES67 SAP/SDP(9875)

        bind(sock, (struct sockaddr *)&addr, sizeof(addr));

        /* setsockoptは、bind以降で行う必要あり */
        //memset(&mreq, 0, sizeof(mreq));
        //mreq.imr_interface.s_addr = INADDR_ANY;
        //mreq.imr_multiaddr.s_addr = inet_addr(ip_addr);
        //mreq.imr_multiaddr.s_addr = inet_addr("224.0.1.129"); // PTP:319
        //mreq.imr_multiaddr.s_addr = inet_addr("239.69.83.133");       // AES67 AVIO-USB(5004)
        //mreq.imr_multiaddr.s_addr = inet_addr("239.1.3.139"); // AES67 Ravenna(5004)
        //mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251"); // MDNS:5353
        //mreq.imr_multiaddr.s_addr = inet_addr("239.255.0.1");   // recpt1:1234
        //if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) != 0) {
        //      perror("setsockopt");
        //      return 1;
        //}

        send_buf[0] = 0x20;     // Flags
        send_buf[1] = 0x00;     // Authenticattion Length
        send_buf[2] = 0x12;     // Message Identifier Hash
        send_buf[3] = 0x34;     // Message Identifier Hash
        strncpy( &send_buf[8], "application/sdp\0", 16);
        sprintf(sdp_buf, "v=0\r\no=- 1 0 IN IP4 %s\r\ns=Keio\r\nc=IN IP4 %s/32\r\nm=audio%d RTP/AVP 97\r\na=rtpmap: 97 L24/48000/2\r\n", if_addr, ip_addr, port);
        sdp_len = strlen( sdp_buf );
        strcpy( &send_buf[24], sdp_buf);

        while (1) {
                rc = sendto(sock, send_buf, 24 + sdp_len, 0, (struct sockaddr *)&addr, sizeof(addr));
//                sleep(30);
                sleep(5);
        }

        close(sock);

}


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
        struct ifreq ifr;

	char ip_addr1[256], ip_addr2[256], ip_addr3[256];
	int len, i, rc, port1, port2, port3, udp_len;
	char recv_buf[1500];
	int do_flag = 1;
        pthread_t thread_manage;



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


        // NIC I/F名を取得
        get_nic_ifname(if_name);

        /* IPv4のIPアドレスを取得したい */
        ifr.ifr_addr.sa_family = AF_INET;
        /* eth0のIPアドレスを取得したい */
        strcpy(ifr.ifr_name, if_name);
        ioctl(sock1, SIOCGIFADDR, &ifr);
        strcpy(if_addr, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
printf("I/F=%s\n", if_addr);

	strcpy(ip_addr, ip_addr3);
	port = port3;

        pthread_create(&thread_manage, NULL, (void *)manage, (void *)NULL);

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


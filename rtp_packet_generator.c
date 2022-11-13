#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <alsa/asoundlib.h>


char ip_addr[256];
char if_name[16], if_addr[16];
int port;

int sock;
int fd;

struct sockaddr_in addr;
int do_flag = 1;
struct sigaction act, oldact;
timer_t tid;
struct itimerspec itval;

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
	addr.sin_port = htons(9875);				// AES67 SAP/SDP(9875)
	addr.sin_addr.s_addr = inet_addr("239.255.255.255");	// AES67 SAP/SDP(9875)

	bind(sock, (struct sockaddr *)&addr, sizeof(addr));

	/* setsockoptは、bind以降で行う必要あり */
	//memset(&mreq, 0, sizeof(mreq));
	//mreq.imr_interface.s_addr = INADDR_ANY;
	//mreq.imr_multiaddr.s_addr = inet_addr(ip_addr);
	//mreq.imr_multiaddr.s_addr = inet_addr("224.0.1.129");	// PTP:319
	//mreq.imr_multiaddr.s_addr = inet_addr("239.69.83.133");	// AES67 AVIO-USB(5004)
	//mreq.imr_multiaddr.s_addr = inet_addr("239.1.3.139");	// AES67 Ravenna(5004)
	//mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251");	// MDNS:5353
	//mreq.imr_multiaddr.s_addr = inet_addr("239.255.0.1");	  // recpt1:1234
	//if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) != 0) {
	//	perror("setsockopt");
	//	return 1;
	//}

	send_buf[0] = 0x20;	// Flags
	send_buf[1] = 0x00;	// Authenticattion Length
	send_buf[2] = 0x12;	// Message Identifier Hash
	send_buf[3] = 0x34;	// Message Identifier Hash
	strncpy( &send_buf[8], "application/sdp\0", 16);
        sprintf(sdp_buf, "v=0\r\no=- 1 0 IN IP4 %s\r\ns=Keio\r\nc=IN IP4 %s/32\r\nm=audio%d RTP/AVP 97\r\na=rtpmap:97 L24/48000/2\r\n", if_addr, ip_addr, port);
	sdp_len = strlen( sdp_buf );
	strcpy( &send_buf[24], sdp_buf);

	while (1) {
		rc = sendto(sock, send_buf, 24 + sdp_len, 0, (struct sockaddr *)&addr, sizeof(addr));
		sleep(30);
	}

	close(sock);

}


void timer_handler(int signum)
{
	int udp_len, rc;
	char recv_buf[1500];

	rc = read(fd, &udp_len, sizeof(udp_len));
	if (rc < sizeof(udp_len)) {
		lseek(fd, 0, SEEK_SET);
		return;
	}

	rc = read(fd, recv_buf+sizeof(udp_len), udp_len);
	if (rc < udp_len) {
		lseek(fd, 0, SEEK_SET);
		return;
	}
	//printf("len=%d\n", udp_len);
	rc = sendto(sock, recv_buf + sizeof(udp_len), udp_len, 0, (struct sockaddr *)&addr, sizeof(addr));
	if (rc < 0) {
		perror("sendto");
		do_flag = 0;
	}
}


int main(int argc, char **argv) {
	char file_name[256];
	int len, i, rc, udp_len, interval;
	int seq_no, seq_no_diff;
	static int seq_no_before = -1;
	in_addr_t ipaddr;
        struct ifreq ifr;
	struct ip_mreq mreq;
	char recv_buf[1500];
	pthread_t thread_manage;

	if (argc != 4) {
		fprintf( stdout, "usage: %s <IP addr:port> <src raw RTP file> <interval(ms)>\n\n", argv[0] );
		fprintf( stdout, " example: %s 239.69.83.133:5004 AES67_L24_1ms.packet 1  ... AVIO-USB\n", argv[0]); 
		fprintf( stdout, "        : %s 239.1.3.139:5004   AES67_L24_1ms.packet 1  ... RAVENNA/AES67\n", argv[0]); 
		return -1;
	} else {
		strcpy(file_name, argv[2]);
		int ip[4];
		rc = sscanf( argv[1], "%d.%d.%d.%d:%d", &ip[0], &ip[1], &ip[2], &ip[3] , &port );
		if (rc != 5) {
			fprintf( stderr, "Invalid parameter\n" );
			return -1;
		}
		sprintf( ip_addr, "%0d.%0d.%0d.%0d", ip[0], ip[1], ip[2], ip[3]);
		interval=atoi(argv[3]);
		printf("ip=%s port=%d interval=%dms\n", ip_addr, port, interval);
	}


        // NIC I/F名を取得
        get_nic_ifname(if_name);


	fd = open(file_name, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip_addr);

       /* IPv4のIPアドレスを取得したい */
        ifr.ifr_addr.sa_family = AF_INET;
        /* eth0のIPアドレスを取得したい */
        strcpy(ifr.ifr_name, if_name);
        ioctl(sock, SIOCGIFADDR, &ifr);
        strcpy(if_addr, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
//printf("I/F=%s\n", if_addr);

	/* setsockoptは、bind以降で行う必要あり */
	//memset(&mreq, 0, sizeof(mreq));
	//ipaddr = inet_addr("127.0.0.1");
	//ipaddr = inet_addr("172.16.74.131");
	//if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, (char *)&ipaddr, sizeof(ipaddr)) != 0) {
	//	perror("setsockopt");
	//	return 1;
	//}

 
	memset(&act, 0, sizeof(struct sigaction));
	memset(&oldact, 0, sizeof(struct sigaction));
 
	// シグナルハンドラの登録
	act.sa_handler = timer_handler;
	act.sa_flags = SA_RESTART;
	if(sigaction(SIGALRM, &act, &oldact) < 0) {
		perror("sigaction()");
		return -1;
	}
 
	// タイマ割り込みを発生させる
	itval.it_value.tv_sec = 0;     // 最初の1回目は1ms後
	itval.it_value.tv_nsec = interval * 1000000;
	itval.it_interval.tv_sec = 0;  // 2回目以降は1ms間隔
	itval.it_interval.tv_nsec = interval * 1000000;

	// タイマの作成
	if(timer_create(CLOCK_REALTIME, NULL, &tid) < 0) {
	    perror("timer_create");
	    return -1;
	}
 
	//  タイマのセット
	if(timer_settime(tid, 0, &itval, NULL) < 0) {
		perror("timer_settime");
		return -1;
	}

	pthread_create(&thread_manage, NULL, (void *)manage, (void *)NULL);

	do_flag = 1;

	while (do_flag) {
		sleep(1);
	}



	close(sock);
	close(fd);


	return 0;
}





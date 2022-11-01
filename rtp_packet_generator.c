#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <alsa/asoundlib.h>


int sock, fd;
struct sockaddr_in addr;
int do_flag = 1;
struct sigaction act, oldact;
timer_t tid;
struct itimerspec itval;

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
	char ip_addr[256], file_name[256];
	int len, i, rc, port, udp_len, interval;
	int seq_no, seq_no_diff;
	static int seq_no_before = -1;
	in_addr_t ipaddr;
	struct ip_mreq mreq;
	char recv_buf[1500];

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

	fd = open(file_name, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip_addr);

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

	do_flag = 1;

	while (do_flag) {
		sleep(1);
	}



	close(sock);
	close(fd);


	return 0;
}

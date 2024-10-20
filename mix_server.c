#include <stdio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <alsa/asoundlib.h>
#include "rtdsc.h"

#define	CLIENT_MAX	(16)
#define	RECV_BUFFER_MAX	(10)
#define	SAMPLE_RATE	(48000)

struct _recv_buffer {
	int active;
	unsigned long long last_recv_time;
	short pcm_data[48*2*5];		// 16Bit * 48Khz * Stereo * 5ms
};

struct _client_info {
	unsigned short port;
	unsigned int   addr;
	int active;
	unsigned long long last_recv_time;
	struct _recv_buffer recv_buffer[RECV_BUFFER_MAX];
} client_info[CLIENT_MAX];
	

int	history_left [SAMPLE_RATE];
int	history_right[SAMPLE_RATE];
int	history_bottom;

int main(int argc, char **argv) {
	int sock1;
	struct sockaddr_in addr1;
	struct sockaddr_storage addr;
	int addrlen;
	struct ip_mreq mreq;
	char ip_addr1[256], ip_addr2[256], ip_addr3[256];
	int reclen, i, j, client_no, recv_buffer_no, rc, port1, port2, port3, udp_len;
	char recv_buf[1500];
	int do_flag = 1;
	unsigned int client_addr, client_port;
	int rtdsc_cycle_per_sec, time_per_sec;
	int pcm_byte_per_frame, pcm_msec;
	int rtp_payload_size;
	unsigned long long rdtsc_now;
	unsigned long long time_now, time_before;

	if (argc != 2) {
		fprintf( stdout, "usage: %s port\n\n", argv[0] );
		fprintf( stdout, " example: %s 5004\n", argv[0]); 
		return -1;
	} else {
		port1 = atoi(argv[1]);
	}

	// initialize RTDSC information
	rtdsc_cycle_per_sec = get_rtdsc_cycle_per_sec();
	//time_per_sec = rtdsc_cycle_per_sec / 5000;
	time_per_sec = rtdsc_cycle_per_sec;
	printf("RTDSC Clock=%dMHz (%d)\n", (rtdsc_cycle_per_sec / 1000000), rtdsc_cycle_per_sec);

	// Initialize Client buffer
	for (client_no=0; client_no<CLIENT_MAX; ++client_no) {
		client_info[client_no].port = 0;
		client_info[client_no].addr = 0;
		client_info[client_no].active = 0;
		for (recv_buffer_no=0; recv_buffer_no<RECV_BUFFER_MAX; ++recv_buffer_no) {
			client_info[client_no].recv_buffer[recv_buffer_no].active = 0;
		}
	}

	// Initialize sound history
	for (i=0; i<SAMPLE_RATE; ++i) {
		history_left [i] = 0;
		history_right[i] = 0;
	}
	history_bottom = 0;

	// sock1: source packet
	sock1 = socket(AF_INET, SOCK_DGRAM, 0);

	addr1.sin_family = AF_INET;
	addr1.sin_port = htons(port1);
	addr1.sin_addr.s_addr = INADDR_ANY;

	bind(sock1, (struct sockaddr *)&addr1, sizeof(addr1));

	// Set Non-Blocking Socket
	i = 1;
	ioctl(sock1, FIONBIO, &i);


	do_flag = 1;
	time_before = 0;

	while (do_flag) {
		unsigned char *ptr;
		int nokori;


		addrlen = sizeof(addr);
		rc = reclen = recvfrom(sock1, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&addr, &addrlen);
		rdtsc_now = rdtsc();
		time_now = (rdtsc_now / time_per_sec);

		if (rc < 0) {
			if (errno != EAGAIN) {
				perror("recv");
				do_flag = 0;
				break;
			}

			// transmit packet ?
			goto transmit;
		}

		// receive processing
		client_addr = htonl(*(unsigned int *)&addr.__ss_padding[2]);
		client_port = htons(*(unsigned short *)&addr.__ss_padding[0]);
		printf("%08X:%d\n", client_addr, client_port);

		switch (reclen) {
//			case 204:  pcm_byte_per_frame = 4; pcm_msec = 1; break; // L16 1ms
//			case 300:  pcm_byte_per_frame = 6; pcm_msec = 1; break; // L24 1ms
			case 972:  pcm_byte_per_frame = 4; pcm_msec = 5; break; // L16 5ms
//			case 1452: pcm_byte_per_frame = 6; pcm_msec = 5; break; // L24 5ms
//			default: printf("Invalid UDP len (%d)\n", reclen); goto transmit;
		}
		rtp_payload_size = reclen; //pcm_byte_per_frame * 48 * pcm_msec;

		// Initialize Client buffer
		for (client_no=0; client_no<CLIENT_MAX; ++client_no) {
			if (client_info[client_no].addr == client_addr && 
			    client_info[client_no].port == client_port &&
		        	client_info[client_no].active == 1) {
				// found client session
printf("Exist client session #%d\n", client_no);
				client_info[client_no].last_recv_time = rdtsc_now;
				for (recv_buffer_no=0; recv_buffer_no<RECV_BUFFER_MAX; ++recv_buffer_no) {
					if (client_info[client_no].recv_buffer[recv_buffer_no].active == 0) {
						// found empty recv buffer
						client_info[client_no].recv_buffer[recv_buffer_no].active = 1;
						client_info[client_no].recv_buffer[recv_buffer_no].last_recv_time = rdtsc_now;
						memcpy(client_info[client_no].recv_buffer[recv_buffer_no].pcm_data, recv_buf + 12,  rtp_payload_size - 12);
					}
				}
				goto transmit;
			}
		}

		// new client session
		for (client_no=0; client_no<CLIENT_MAX; ++client_no) {
			// found empty ssession
		        if (client_info[client_no].active == 0) {
printf("New client session #%d\n", client_no);
				client_info[client_no].active = 1;
				client_info[client_no].last_recv_time = rdtsc_now;
				client_info[client_no].addr = client_addr;
			   	client_info[client_no].port = client_port;
				// Inirialize recv buffer
				for (recv_buffer_no=0; recv_buffer_no<RECV_BUFFER_MAX; ++recv_buffer_no) {
					client_info[client_no].recv_buffer[recv_buffer_no].active = 0;
				}
				// store AES67 data to recv buffer #0
				client_info[client_no].recv_buffer[0].active = 1;
				client_info[client_no].recv_buffer[0].last_recv_time = rdtsc_now;
				memcpy(client_info[client_no].recv_buffer[0].pcm_data, recv_buf + 12,  rtp_payload_size - 12);
				goto transmit;
			}

		}

		rc = sendto(sock1, (void *)recv_buf, reclen, 0, (struct sockaddr *)&addr, sizeof(addr));


transmit:
		// Has 5 msec passed ?
		if ( time_now == time_before)
			continue;

		printf("time=%lld\n", time_now);

		time_before = time_now;
	}

	close(sock1);

	return 0;
}


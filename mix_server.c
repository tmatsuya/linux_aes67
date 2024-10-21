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
	unsigned char pcm_data[2*48*2*5];		// 16Bit * 48Khz * Stereo * 5ms
};

struct _client_info {
	struct sockaddr_storage sockaddr;
	unsigned short port;
	unsigned int   ipv4addr;
	int active;
	unsigned long long last_recv_time;
	struct _recv_buffer recv_buffer[RECV_BUFFER_MAX];
} client_info[CLIENT_MAX];
	

int	history_left [SAMPLE_RATE];
int	history_right[SAMPLE_RATE];
int	history_bottom;

int main(int argc, char **argv) {
	int sock1;
	struct sockaddr_in sockaddr1;
	struct sockaddr_storage sockaddr;
	int sockaddrlen;
	struct ip_mreq mreq;
	int reclen, i, j, client_no, recv_buffer_no, rc, port1, port2, port3, udp_len;
	char recv_buf[1500], send_buf[1500];
	int do_flag = 1;
	unsigned int client_ipv4addr, client_port;
	int rtdsc_cycle_per_sec, time_per_sec;
	int pcm_byte_per_frame, pcm_msec;
	int rtp_payload_size;
	unsigned long long rdtsc_now;
	unsigned long long time_now, time_before, time_valid_oldest;
	int recv_buffer_oldest_no;
	int seq_no;

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
		client_info[client_no].ipv4addr = 0;
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

	sockaddr1.sin_family = AF_INET;
	sockaddr1.sin_port = htons(port1);
	sockaddr1.sin_addr.s_addr = INADDR_ANY;

	bind(sock1, (struct sockaddr *)&sockaddr1, sizeof(sockaddr1));

	// Set Non-Blocking Socket
	i = 1;
	ioctl(sock1, FIONBIO, &i);


	do_flag = 1;
	time_before = 0;
	seq_no = 0;

	while (do_flag) {
		unsigned char *ptr;
		int nokori;


		sockaddrlen = sizeof(sockaddr);
		rc = reclen = recvfrom(sock1, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&sockaddr, &sockaddrlen);
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
		client_ipv4addr = htonl(*(unsigned int *)&sockaddr.__ss_padding[2]);
		client_port = htons(*(unsigned short *)&sockaddr.__ss_padding[0]);
		printf("%08X:%d\n", client_ipv4addr, client_port);

		switch (reclen) {
//			case 204:  pcm_byte_per_frame = 4; pcm_msec = 1; break; // L16 1ms
//			case 300:  pcm_byte_per_frame = 6; pcm_msec = 1; break; // L24 1ms
			case 972:  pcm_byte_per_frame = 4; pcm_msec = 5; break; // L16 5ms
//			case 1452: pcm_byte_per_frame = 6; pcm_msec = 5; break; // L24 5ms
			default: printf("Invalid UDP len (%d)\n", reclen); goto transmit;
		}
		rtp_payload_size = reclen; //pcm_byte_per_frame * 48 * pcm_msec;

		// exist client session ?
		for (client_no=0; client_no<CLIENT_MAX; ++client_no) {
			if (client_info[client_no].ipv4addr == client_ipv4addr && 
			    client_info[client_no].port == client_port &&
		        	client_info[client_no].active == 1) {
				// found client session
printf("Exist client session #%d\n", client_no);
				client_info[client_no].last_recv_time = rdtsc_now;
				for (recv_buffer_no=0; recv_buffer_no<RECV_BUFFER_MAX; ++recv_buffer_no) {
					if (client_info[client_no].recv_buffer[recv_buffer_no].active == 0) {
						// store AES67 data to empty recv buffer
						client_info[client_no].recv_buffer[recv_buffer_no].active = 1;
						client_info[client_no].recv_buffer[recv_buffer_no].last_recv_time = rdtsc_now;
						memcpy(client_info[client_no].recv_buffer[recv_buffer_no].pcm_data, recv_buf + 12,  rtp_payload_size - 12);
						goto transmit;
					}
				}
				// not found empty recv buffer
				goto transmit;
			}
		}

		// new client session
		for (client_no=0; client_no<CLIENT_MAX; ++client_no) {
			// found empty client ssession
		        if (client_info[client_no].active == 0) {
printf("New client session #%d\n", client_no);
				client_info[client_no].active = 1;
				client_info[client_no].sockaddr = sockaddr;
				client_info[client_no].last_recv_time = rdtsc_now;
				client_info[client_no].ipv4addr = client_ipv4addr;
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


transmit:
		// Has 5 msec passed ?
		if ( time_now == time_before)
			continue;

		// clear transmit AES67 data
		for (i=0; i<rtp_payload_size; ++i) {
			send_buf[i] = 0;
		}

		// 
		for (client_no=0; client_no<CLIENT_MAX; ++client_no) {
			// need processing ?
		        if (client_info[client_no].active == 1) {
				// session timeout ? (over (RECV_BUFFER_MAX * 20) (1sec) ?)
				if ((client_info[client_no].last_recv_time / time_per_sec) < (time_now - (RECV_BUFFER_MAX * 20))); {
					// disconnect session
					client_info[client_no].active = 0;
					continue;
				}

				// find valid oldest packet
				time_valid_oldest = 0xffffffffffffffffLL;
				recv_buffer_oldest_no = -1;
				for (recv_buffer_no=0; recv_buffer_no<RECV_BUFFER_MAX; ++recv_buffer_no) {
					// active packet ?
					if (client_info[client_no].recv_buffer[recv_buffer_no].active == 1) {
						// timeout packet ? (over RECV_BUFFER_MAX) (50ms))
						if ((client_info[client_no].recv_buffer[recv_buffer_no].last_recv_time / time_per_sec) < (time_now - (RECV_BUFFER_MAX * 1))); {
							// inactive packet
							client_info[client_no].recv_buffer[recv_buffer_no].active = 0;
							continue;
						}

						// found valid oldest packet
						if ((client_info[client_no].recv_buffer[recv_buffer_no].last_recv_time < time_valid_oldest)) {
							time_valid_oldest = client_info[client_no].recv_buffer[recv_buffer_no].last_recv_time;
							recv_buffer_oldest_no = recv_buffer_no;
						}
					}
				}

				// mixing process
				if (time_valid_oldest != 0xffffffffffffffffLL) {
					int mixed_left, mixed_right, left, right, nokori;
					unsigned char *ptr, *ptr2;
					for ( nokori = rtp_payload_size - 12, ptr = send_buf + 12, ptr2 = client_info[client_no].recv_buffer[recv_buffer_oldest_no].pcm_data; nokori > 0; nokori -= pcm_byte_per_frame, ptr+= pcm_byte_per_frame, ptr2+=pcm_byte_per_frame) {
						// get mixed data
						mixed_left  = (*(ptr+0) << 8) | *(ptr+1);
						mixed_right = (*(ptr+2) << 8) | *(ptr+3);
						// 16bit signed to 32bit signed
						if (mixed_left & 0x8000)
							mixed_left  |= 0xffff0000;
						if (mixed_right & 0x8000)
							mixed_right |= 0xffff0000;

						// get current client data
						left  = (*(ptr2+0) << 8) | *(ptr2+1);
						right = (*(ptr2+2) << 8) | *(ptr2+3);
						// 16bit signed to 32bit signed
						if (left & 0x8000)
							left  |= 0xffff0000;
						if (right & 0x8000)
							right |= 0xffff0000;

						// mixing
						mixed_left += left;
						mixed_right += right;

						// store mixing data to send buffer
						*(ptr+0) = mixed_left >> 8;
						*(ptr+1) = mixed_left & 0xff;
						*(ptr+2) = mixed_right >> 8;
						*(ptr+3) = mixed_right & 0xff;

					}
					// Done and inactie
					client_info[client_no].recv_buffer[recv_buffer_oldest_no].active = 0;

				}

			}
		}

		// RTP header
		send_buf[2] = (seq_no & 0xff00) >> 8;
		send_buf[3] = (seq_no & 0xff);

		for (client_no=0; client_no<CLIENT_MAX; ++client_no) {
			// need sending ?
		        if (client_info[client_no].active == 1) {
				// send AES67 packet
				rc = sendto(sock1, (void *)send_buf, rtp_payload_size, 0, (struct sockaddr *) &client_info[client_no].sockaddr, sizeof(sockaddr));
				if (rc < 0) {
					perror("sendto");
					do_flag = 0;
				}
			}
		}

//		printf("time=%lld\n", time_now);

		time_before = time_now;
	}

	close(sock1);

	return 0;
}


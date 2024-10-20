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

#define	SAMPLE_RATE	(48000)

int	history_left [SAMPLE_RATE];
int	history_right[SAMPLE_RATE];
int	history_bottom;

int main(int argc, char **argv) {
	int sock1;
	struct sockaddr_in addr1;
	struct sockaddr_storage addr;
	int addrlen;

	struct ip_mreq mreq;

	fd_set fds, readfds;
	char ip_addr1[256], ip_addr2[256], ip_addr3[256];
	int reclen, i, rc, port1, port2, port3, udp_len;
	char recv_buf[1500];
	int do_flag = 1;

	int pcm_byte_per_frame, pcm_msec;
	int rtp_payload_size;

	if (argc != 2) {
		fprintf( stdout, "usage: %s port\n\n", argv[0] );
		fprintf( stdout, " example: %s 5004\n", argv[0]); 
		return -1;
	} else {
		port1 = atoi(argv[1]);
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


	FD_ZERO(&readfds);
	FD_SET(sock1, &readfds);

	do_flag = 1;

	while (do_flag) {
		unsigned char *ptr;
		int nokori;
		memcpy(&fds, &readfds, sizeof(fd_set));

		select(sock1 + 1, &fds, NULL, NULL, NULL);


		if (FD_ISSET(sock1, &fds)) {
			addrlen = sizeof(addr);
			rc = reclen = recvfrom(sock1, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&addr, &addrlen);
//printf("recv()=%d\n", rc);
			if (rc < 0) {
				perror("recv");
				do_flag = 0;
				break;
			}

			switch (reclen) {
				case 204:  pcm_byte_per_frame = 4; pcm_msec = 1; break; // L16 1ms
				case 300:  pcm_byte_per_frame = 6; pcm_msec = 1; break; // L24 1ms
				case 972:  pcm_byte_per_frame = 4; pcm_msec = 5; break; // L16 5ms
				case 1452: pcm_byte_per_frame = 6; pcm_msec = 5; break; // L24 5ms
				default: printf("Invalid UDP len (%d)\n", reclen); continue;
			}
			rtp_payload_size = reclen; //pcm_byte_per_frame * 48 * pcm_msec;

			nokori = reclen;
			if (pcm_byte_per_frame == 4) {
				int left, right, p1, p2, p3, p4, p5, p6;
				for ( nokori = rtp_payload_size - 12, ptr = recv_buf + 12; nokori > 0; nokori -= pcm_byte_per_frame, ptr+= pcm_byte_per_frame) {
					p1 = history_bottom - ((SAMPLE_RATE/8) * 1);
					p2 = history_bottom - ((SAMPLE_RATE/8) * 2);
					p3 = history_bottom - ((SAMPLE_RATE/8) * 3);
					p4 = history_bottom - ((SAMPLE_RATE/8) * 4);
					p5 = history_bottom - ((SAMPLE_RATE/8) * 5);
					p6 = history_bottom - ((SAMPLE_RATE/8) * 6);
					if (p1 < 0) p1 += SAMPLE_RATE;
					if (p2 < 0) p2 += SAMPLE_RATE;
					if (p3 < 0) p3 += SAMPLE_RATE;
					if (p4 < 0) p4 += SAMPLE_RATE;
					if (p5 < 0) p5 += SAMPLE_RATE;
					if (p6 < 0) p6 += SAMPLE_RATE;
					left  = (*(ptr+0) << 8) | *(ptr+1);
					right = (*(ptr+2) << 8) | *(ptr+3);
					// 16bit signed to 32bit signed
					if (left & 0x8000)
						left  |= 0xffff0000;
					if (right & 0x8000)
						right |= 0xffff0000;
					history_left [ history_bottom ] = left;
					history_right[ history_bottom ] = right;
					left  = left + (history_left [p1]>>1) + (history_left [p2]>>2) + (history_left [p3]>>3) + (history_left [p4]>>4) + (history_left [p5]>>5) + (history_left [p6]>>6);
					right = right+ (history_right[p1]>>1) + (history_right[p2]>>2) + (history_right[p3]>>3) + (history_right[p4]>>4) + (history_right[p5]>>5) + (history_right[p6]>>6);
					*(ptr+0) = left >> 8;
					*(ptr+1) = left & 0xff;
					*(ptr+2) = right >> 8;
					*(ptr+3) = right & 0xff;
					if (++history_bottom >= SAMPLE_RATE)
						history_bottom = 0;
				}
			}

			if (pcm_byte_per_frame == 6) {
				int left, right, p1, p2, p3, p4, p5, p6;
				for ( nokori = rtp_payload_size - 12, ptr = recv_buf + 12; nokori > 0; nokori -= pcm_byte_per_frame, ptr+= pcm_byte_per_frame) {
					p1 = history_bottom - ((SAMPLE_RATE/8) * 1);
					p2 = history_bottom - ((SAMPLE_RATE/8) * 2);
					p3 = history_bottom - ((SAMPLE_RATE/8) * 3);
					p4 = history_bottom - ((SAMPLE_RATE/8) * 4);
					p5 = history_bottom - ((SAMPLE_RATE/8) * 5);
					p6 = history_bottom - ((SAMPLE_RATE/8) * 6);
					if (p1 < 0) p1 += SAMPLE_RATE;
					if (p2 < 0) p2 += SAMPLE_RATE;
					if (p3 < 0) p3 += SAMPLE_RATE;
					if (p4 < 0) p4 += SAMPLE_RATE;
					if (p5 < 0) p5 += SAMPLE_RATE;
					if (p6 < 0) p6 += SAMPLE_RATE;
					left  = (*(ptr+0) << 16) | (*(ptr+1) << 8) | *(ptr+2);
					right = (*(ptr+3) << 16) | (*(ptr+4) << 8) | *(ptr+5);
					// 24bit signed to 32bit signed
					if (left & 0x800000)
						left  |= 0xff000000;
					if (right & 0x800000)
						right |= 0xff000000;
					history_left [ history_bottom ] = left;
					history_right[ history_bottom ] = right;
					left  = left + (history_left [p1]>>1) + (history_left [p2]>>2) + (history_left [p3]>>3) + (history_left [p4]>>4) + (history_left [p5]>>5) + (history_left [p6]>>6);
					right = right+ (history_right[p1]>>1) + (history_right[p2]>>2) + (history_right[p3]>>3) + (history_right[p4]>>4) + (history_right[p5]>>5) + (history_right[p6]>>6);
					*(ptr+0) =  left  >> 16;
					*(ptr+1) = (left  >> 8) & 0xff;
					*(ptr+2) =  left  & 0xff;
					*(ptr+3) =  right >> 16;
					*(ptr+4) = (right >> 8) & 0xff;
					*(ptr+5) =  right & 0xff;
					if (++history_bottom >= SAMPLE_RATE)
						history_bottom = 0;
				}
			}

			rc = sendto(sock1, (void *)recv_buf, rc, 0, (struct sockaddr *)&addr, sizeof(addr));
//printf("sendto()=%d\n", rc);
			if (rc < 0) {
				perror("sendto");
				do_flag = 0;
			}
		}

	}

	close(sock1);

	return 0;
}


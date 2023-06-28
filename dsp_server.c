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
	struct sockaddr_storage addr;
	int addrlen;

	struct ip_mreq mreq;

	fd_set fds, readfds;
	char ip_addr1[256], ip_addr2[256], ip_addr3[256];
	int len, i, rc, port1, port2, port3, udp_len;
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
			rc = recvfrom(sock1, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&addr, &addrlen);
//printf("recv()=%d\n", rc);
			if (rc < 0) {
				perror("recv");
				do_flag = 0;
				break;
			}

			switch (rc) {
				case 204:  pcm_byte_per_frame = 4; pcm_msec = 1; break; // L16 1ms
				case 300:  pcm_byte_per_frame = 6; pcm_msec = 1; break; // L24 1ms
				case 972:  pcm_byte_per_frame = 4; pcm_msec = 5; break; // L16 5ms
				case 1452: pcm_byte_per_frame = 6; pcm_msec = 5; break; // L24 5ms
				default: printf("Invalid UDP len (%d)\n", len); continue;
			}
			rtp_payload_size = rc; //pcm_byte_per_frame * 48 * pcm_msec;

			nokori = rc;
			if (pcm_byte_per_frame == 4) {
				short left, right;
				for ( nokori = rtp_payload_size - 12, ptr = recv_buf + 12; nokori > 0; nokori -= pcm_byte_per_frame, ptr+= pcm_byte_per_frame) {
					left  = (*(ptr+0) << 8) | *(ptr+1);
					right = (*(ptr+2) << 8) | *(ptr+3);
					left  = left / 8;
					right = right / 8;
					*(ptr+0) = left >> 8;
					*(ptr+1) = left & 0xff;
					*(ptr+2) = right >> 8;
					*(ptr+3) = right & 0xff;
				}
			}

			if (pcm_byte_per_frame == 6) {
				int left, right;
				for ( nokori = rtp_payload_size - 12, ptr = recv_buf + 12; nokori > 0; nokori -= pcm_byte_per_frame, ptr+= pcm_byte_per_frame) {
					left  = (*(ptr+0) << 16) | (*(ptr+1) << 8) | *(ptr+2);
					right = (*(ptr+3) << 16) | (*(ptr+4) << 8) | *(ptr+5);
					//left  = left / 8;
					//right = right / 8;
					*(ptr+0) =  left  >> 16;
					*(ptr+1) = (left  >> 8) & 0xff;
					*(ptr+2) =  left  & 0xff;
					*(ptr+3) =  right >> 16;
					*(ptr+4) = (right >> 8) & 0xff;
					*(ptr+5) =  right & 0xff;
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


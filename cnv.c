#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>


int cnv_pcm_L24_to_L16() {
	int len;
	unsigned char buf[3], recv_buf[1500], send_buf[1500];
	while (1) {
		len = read(0, buf, 3);
		if (len != 3)
			break;
		putchar(buf[0]);
		putchar(buf[1]);
	}

}


// AES67 L24 1ms Raw UDP Packet to L16 5ms Raw UDP
int cnv_RTP_L24_to_L16() {
	char recv_buf[1500], send_buf[1500], *recv_p, *send_p;
	int block_count;
	int udp_len;
	int interval = 5;		// 5ms interval
	unsigned short seq_no;
	int i, rc, do_flag = 1;

	block_count = 0;
	seq_no = 0;
	recv_p = recv_buf;
	send_p = send_buf;
	do_flag = 1;

	while (do_flag) {
		// Read UDP Length
		rc = read(0, &udp_len, sizeof(udp_len));
		if (rc <= 0)
			break;
		// Read UDP Packet
		recv_p = recv_buf;
		rc = read(0, recv_buf, udp_len);
		if (rc <= 0)
			break;
		
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
			// Write New UDP Length
			udp_len = (send_p - send_buf);
			rc = write(1, &udp_len, sizeof(udp_len));
			// Write New UDP Packet
			rc = write(1, (void *)send_buf, udp_len);
//write(1, &rc,sizeof(rc)); write(1, send_buf, rc);
			if (rc < 0) {
				perror("write");
				do_flag = 0;
			}
			send_p = send_buf;
			block_count = 0;
		}
	}

	return 0;
}

int main(int argc, char **argv) {
//	cnv_pcm_L24_to_L16();	
	cnv_RTP_L24_to_L16();	// AES67 L24 1ms Raw UDP Packet to L16 5ms Raw UDP
}

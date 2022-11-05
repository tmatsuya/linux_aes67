#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <alsa/asoundlib.h>

#define PCM_BLOCK_MAX	1

int main(int argc, char **argv) {
	char ip_addr[256];
	int sock, len, old_len, i, rc, port;
	int seq_no, seq_no_diff;
	static int seq_no_before = -1;
	struct sockaddr_in addr;
	struct ip_mreq mreq;
	unsigned char recv_buf[1500], *pcm_buf;
	int cur_block = 0;
	snd_pcm_t *pcm;
	int pcm_msec;		// 1 or 5 msec interval
	int rtp_payload_size;	// 204 (L16 1ms) or 300 (L24 1ms) or 972 (L16 5ms) or 1452 (L24 5ms)
	int pcm_byte_per_frame;	// 6 (L24) or 4 (L16)
	char device[] = "default";				// default
	//char device[] = "sysdefault:CARD=vc4hdmi";		// pizero HDMI audio
	//char device[] = "plughw:CARD=DAC,DEV=0";	   		// pizero USB DAC
	//char device[] = "sysdefault:CARD=Headphones";		// pi4 headphone
	//char device[] = "sysdefault:CARD=AudioPCI";		// VM ubuntu
	snd_pcm_format_t format;
	// snd_pcm_readi/snd_pcm_writeiを使って読み書きする
	snd_pcm_access_t access = SND_PCM_ACCESS_RW_INTERLEAVED;
	// 再生周波数 48kHz
	unsigned int sampling_rate = 48000;
	// ステレオ
	static unsigned int channels = 2;
	// ALSAがサウンドカードの都合に合わせて勝手に再生周波数を変更することを許す
	unsigned int soft_resample = 1;
	// 100ミリ秒分ALSAのバッファに蓄える
	unsigned int latency = 100*1000;

	if (argc != 2) {
		fprintf( stdout, "usage: %s <IP addr:port>\n\n", argv[0] );
		fprintf( stdout, " example: %s 239.69.83.133:5004   ... AVIO-USB\n", argv[0]); 
		fprintf( stdout, "        : %s 239.1.3.139:5004     ... RAVENNA/AES67\n", argv[0]); 
		return -1;
	} else {
		int ip[4];
		rc = sscanf( argv[1], "%d.%d.%d.%d:%d", &ip[0], &ip[1], &ip[2], &ip[3] , &port );
		if (rc != 5) {
			fprintf( stderr, "Invalid parameter\n" );
			return -1;
		}
		sprintf( ip_addr, "%0d.%0d.%0d.%0d", ip[0], ip[1], ip[2], ip[3]);
//		printf("ip=%s port=%d\n", ip_addr, port);
	}


	if ( (pcm_buf = malloc(6*48*5*PCM_BLOCK_MAX)) == NULL ) {
		fprintf( stderr, "malloc failed\n" );
		return -1;
	}

	old_len = -1;

	while ( 1 ) {

		sock = socket(AF_INET, SOCK_DGRAM, 0);

		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		//addr.sin_port = htons(319);		// PTP(319)
		//addr.sin_port = htons(5004);		// AES67(5004)
		//addr.sin_port = htons(5353);		// MDNS(5353)
		//addr.sin_port = htons(1234);		// recpt1(1234)
		addr.sin_addr.s_addr = INADDR_ANY;

		bind(sock, (struct sockaddr *)&addr, sizeof(addr));

		/* setsockoptは、bind以降で行う必要あり */
		memset(&mreq, 0, sizeof(mreq));
		mreq.imr_interface.s_addr = INADDR_ANY;
		mreq.imr_multiaddr.s_addr = inet_addr(ip_addr);
		//mreq.imr_multiaddr.s_addr = inet_addr("224.0.1.129");	// PTP:319
		//mreq.imr_multiaddr.s_addr = inet_addr("239.69.83.133");	// AES67 AVIO-USB(5004)
		//mreq.imr_multiaddr.s_addr = inet_addr("239.1.3.139");	// AES67 Ravenna(5004)
		//mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251");	// MDNS:5353
		//mreq.imr_multiaddr.s_addr = inet_addr("239.255.0.1");	  // recpt1:1234

		if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) != 0) {
			perror("setsockopt");
			return 1;
		}

		len = recv(sock, recv_buf, sizeof(recv_buf), 0);
		if ( len != old_len ) {
			switch (len) {
				case 204:  pcm_byte_per_frame = 4; pcm_msec = 1;  // L16 1ms
					   format = SND_PCM_FORMAT_S16_BE; break;
				case 300:  pcm_byte_per_frame = 6; pcm_msec = 1;  // L24 1ms
					   format = SND_PCM_FORMAT_S24_3BE; break;
				case 972:  pcm_byte_per_frame = 4; pcm_msec = 5;  // L16 5ms
					   format = SND_PCM_FORMAT_S16_BE; break;
				case 1452: pcm_byte_per_frame = 6; pcm_msec = 5;  // L24 5ms
					   format = SND_PCM_FORMAT_S24_3BE; break;
			}
			rtp_payload_size = len; //pcm_byte_per_frame * 48 * pcm_msec;

			// 再生用PCMストリームを開く
			rc = snd_pcm_open( &pcm, device, SND_PCM_STREAM_PLAYBACK, 0 );
			if ( rc < 0 ) {
				fprintf( stderr, "pcm open failed:[%d]\n", rc );
				return -1;
			}

			// 再生周波数、フォーマット、バッファのサイズ等を指定する
			rc = snd_pcm_set_params( pcm, format, access, channels, sampling_rate, soft_resample, latency );

			memset(pcm_buf, 0, pcm_byte_per_frame*48*pcm_msec*PCM_BLOCK_MAX);
			memset(recv_buf, 0, sizeof(recv_buf));
			for (i=0; i<(latency/1000)*2; i+= pcm_msec) {
				rc = snd_pcm_writei ( pcm, ( const void* )pcm_buf, 48*pcm_msec*PCM_BLOCK_MAX );
				if( rc < 0 )
					rc = snd_pcm_recover( pcm, rc, 0 );
			}
			seq_no_before = -1;
			old_len = len;
			cur_block = 0;
		}

		while (1) {
			len = recv(sock, recv_buf, sizeof(recv_buf), 0);
//memse	t(recv_buf, 0, sizeof(recv_buf));
			if (len > 0) {
				if (len != old_len)
					break;
//write	(1, &len, sizeof(len)); write(1, recv_buf, len);
//write	(1, &recv_buf[12], len-12);
				seq_no =(recv_buf[2] << 8) | (recv_buf[3]);
				seq_no_diff = seq_no - seq_no_before;
				if (seq_no_diff > 65535)
					seq_no_diff -= 65536;
				if (seq_no_diff < 0)
					seq_no_diff += 65536;
				if (seq_no_diff != 1 && seq_no_before != -1 || len != rtp_payload_size) {
					fprintf(stderr, "seq_no=%d(%d), drop=%d, len=%d\n", seq_no, seq_no_before, (seq_no_diff-1), len);
				}
				seq_no_before = seq_no;
	
				// 書き出し
//for (	i=0;i<288;++i) printf(" %02X", recv_buf[12+i]); printf("\n");
				for (i=0; i<pcm_byte_per_frame*48*pcm_msec; ++i)
					pcm_buf[ cur_block*pcm_byte_per_frame*48*pcm_msec+i ] = recv_buf[ 12+i ];
				if (cur_block == (PCM_BLOCK_MAX-1) ) {
//rc=wr	ite(1, pcm_buf, pcm_byte_per_frame*48*PCM_BLOCK_MAX);
//fprin	tf(stderr,"%d\n",rc);
					rc = snd_pcm_writei ( pcm, ( const void* )pcm_buf, 48*pcm_msec*PCM_BLOCK_MAX );
					if( rc < 0 ) {
						// バッファアンダーラン
						rc = snd_pcm_recover( pcm, rc, 0 );
						if ( rc < 0 ) {
								fprintf( stderr, "Unable to recover this stream.\n" );
							snd_pcm_close( pcm );
							return -1;
						}
					}
					cur_block = 0;
				} else {
					++cur_block;
				}
			} else {
				break;
			}
		}
	
		shutdown(sock, 0);
		close(sock);
		snd_pcm_close( pcm );

	}

	return 0;
}

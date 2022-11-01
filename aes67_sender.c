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
	int sock, len, i, rc, port;
	int seq_no, seq_no_diff;
	static int seq_no_before = -1;
	struct sockaddr_in addr;
	struct ip_mreq mreq;
	unsigned char send_buf[1500], *pcm_buf;
	int cur_block = 0;
	snd_pcm_t *pcm;
	int pcm_msec;		// 1 or 5 msec interval
	int rtp_payload_size;	// 300 (L24 1ms) or 972 (L16 5ms)
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

	if (strstr( argv[0], "L16") > 0) {
		format = SND_PCM_FORMAT_S16_BE;
		pcm_byte_per_frame = 4;
		rtp_payload_size = 972;
		pcm_msec = 5;
	} else {
		format = SND_PCM_FORMAT_S24_3BE;
		pcm_byte_per_frame = 6;
		rtp_payload_size = 300;
		pcm_msec = 1;
	}

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


	if ( (pcm_buf = malloc(pcm_byte_per_frame*48*pcm_msec*PCM_BLOCK_MAX)) == NULL ) {
		fprintf( stderr, "malloc failed\n" );
		return -1;
	}

	// 録音用PCMストリームを開く
	rc = snd_pcm_open( &pcm, device, SND_PCM_STREAM_CAPTURE, 0 );
	if ( rc < 0 ) {
		fprintf( stderr, "pcm open failed:[%d]\n", rc );
		return -1;
	}

	// 再生周波数、フォーマット、バッファのサイズ等を指定する
	rc = snd_pcm_set_params( pcm, format, access, channels, sampling_rate, soft_resample, latency );

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip_addr);

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


	memset(pcm_buf, 0, pcm_byte_per_frame*48*pcm_msec*PCM_BLOCK_MAX);
	memset(send_buf, 0, sizeof(send_buf));
	seq_no = 0;

	while (1) {
		rc = snd_pcm_readi ( pcm, ( void * )pcm_buf, 48*pcm_msec*PCM_BLOCK_MAX );
		if( rc < 0 ) {
			// バッファアンダーラン
			rc = snd_pcm_recover( pcm, rc, 0 );
			if ( rc < 0 ) {
				fprintf( stderr, "Unable to recover this stream.\n" );
				snd_pcm_close( pcm );
				return -1;
			}
		}

		send_buf[2] = (seq_no & 0xff00) >> 8;
		send_buf[3] = (seq_no & 0xff);

		for (i=0; i<pcm_byte_per_frame*48*pcm_msec*PCM_BLOCK_MAX; ++i)
			send_buf[i+12]= pcm_buf[i];

		len = rtp_payload_size;
		rc = sendto(sock, send_buf, len, 0, (struct sockaddr *)&addr, sizeof(addr));
		if (rc == len) {
			++seq_no;
		} else
			break;
	}


	close(sock);
	snd_pcm_close( pcm );


	return 0;
}

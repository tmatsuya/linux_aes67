#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <alsa/asoundlib.h>

#define PCM_FRAME_MAX	48

int main(int argc, char **argv) {
	int i, rc, len;
	char *pcm_buf;
	snd_pcm_t *pcm;
	int pcm_msec;           // 1 or 5 msec interval
	int rtp_payload_size;   // 300 (L24 1ms) or 972 (L16 5ms)
	int pcm_byte_per_frame; // 6 (L24) or 4 (L16)
	char device[] = "default";
	//char device[] = "sysdefault:CARD=vc4hdmi";            // pizero HDMI audio
	//char device[] = "sysdefault:CARD=Headphones";           // pi4 headphone
	//char device[] = "sysdefault:CARD=AudioPCI";           // VM ubuntu
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
	unsigned int latency = 100 * 10000;

	if (strstr( argv[0], "16") > 0) {
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


	if ( (pcm_buf = malloc(pcm_byte_per_frame*PCM_FRAME_MAX)) == NULL ) {
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
	if ( rc < 0 ) {
		fprintf( stderr, "pcm set failed:[%d]\n", rc );
		snd_pcm_close( pcm );
		return -1;
	}

	while (1) {
		rc = snd_pcm_readi ( pcm, ( void * )pcm_buf, PCM_FRAME_MAX );
		if( rc == PCM_FRAME_MAX ) {
			rc = write(1, (void *)pcm_buf, PCM_FRAME_MAX*pcm_byte_per_frame);
		} else if (rc < 0) {
			rc = snd_pcm_recover( pcm, rc, 0);
			if (rc < 0)
				break;
       		} else
			break;
	}


	snd_pcm_close( pcm );

	return 0;
}

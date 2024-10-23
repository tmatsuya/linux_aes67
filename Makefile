PROJECT1=aes67_receiver
PROJECT2=play24
PROJECT3=rtp_packet_generator
PROJECT4=rtp_L24_to_L16
PROJECT5=record24
PROJECT6=aes67_sender
PROJECT7=rtp_multi_to_uni
PROJECT8=dsp_server
PROJECT9=mix_server
PROJECT10=mcastjoin

all: $(PROJECT1) $(PROJECT2) $(PROJECT3) $(PROJECT4) $(PROJECT5) $(PROJECT6) $(PROJECT7) $(PROJECT8) $(PROJECT9) $(PROJECT10)

clean:
	rm -f $(PROJECT1) $(PROJECT2) $(PROJECT3) $(PROJECT4) $(PROJECT5) $(PROJECT6) $(PROJECT7) $(PROJECT8) $(PROJECT9) $(PROJECT10) aes67_receiver_L16 aes67_sender_L16 rtp_L24_to_L24 play16 record16 *.o

$(PROJECT1): $(PROJECT1).c
	$(CC) -O -o $(PROJECT1) $(PROJECT1).c -lasound

$(PROJECT2): $(PROJECT2).c
	$(CC) -O -o $(PROJECT2) $(PROJECT2).c -lasound
	rm -f play16
	ln $(PROJECT2) play16

$(PROJECT3): $(PROJECT3).c
	$(CC) -O -o $(PROJECT3) $(PROJECT3).c -lrt -lpthread

$(PROJECT4): $(PROJECT4).c
	$(CC) -O -o $(PROJECT4) $(PROJECT4).c
	rm -f rtp_L24_to_L24
	ln $(PROJECT4) rtp_L24_to_L24

$(PROJECT5): $(PROJECT5).c
	$(CC) -O -o $(PROJECT5) $(PROJECT5).c -lasound
	rm -f record16
	ln $(PROJECT5) record16

$(PROJECT6): $(PROJECT6).c
	$(CC) -O -o $(PROJECT6) $(PROJECT6).c -lasound -lpthread
	rm -f $(PROJECT6)_L16
	ln $(PROJECT6) $(PROJECT6)_L16

$(PROJECT7): $(PROJECT7).c
	$(CC) -O -o $(PROJECT7) $(PROJECT7).c -lpthread

$(PROJECT8): $(PROJECT8).c
	$(CC) -O -o $(PROJECT8) $(PROJECT8).c

$(PROJECT9): $(PROJECT9).c
	$(CC) -O -o $(PROJECT9) $(PROJECT9).c

$(PROJECT10): $(PROJECT10).c
	$(CC) -O -o $(PROJECT10) $(PROJECT10).c

.PHONY: clean

PROJECT1=aes67_receiver
PROJECT2=play24
PROJECT3=rtp_packet_generator
PROJECT4=rtp_L24_to_L16
PROJECT5=record24
PROJECT6=aes67_sender

all: $(PROJECT1) $(PROJECT2) $(PROJECT3) $(PROJECT4) $(PROJECT5) $(PROJECT6) $(PROJECT7)

clean:
	rm -f $(PROJECT1) $(PROJECT2) $(PROJECT3) $(PROJECT4) $(PROJECT5) $(PROJECT6) $(PROJECT7) aes67_receiver_L16 aes67_sender_L16 rtp_L24_to_L24 play16 record16 *.o

$(PROJECT1): $(PROJECT1).c
	$(CC) -O -o $(PROJECT1) $(PROJECT1).c -lasound
	cp -p $(PROJECT1) $(PROJECT1)_L16

$(PROJECT2): $(PROJECT2).c
	$(CC) -O -o $(PROJECT2) $(PROJECT2).c -lasound
	cp -p $(PROJECT2) play16

$(PROJECT3): $(PROJECT3).c
	$(CC) -O -o $(PROJECT3) $(PROJECT3).c -lrt

$(PROJECT4): $(PROJECT4).c
	$(CC) -O -o $(PROJECT4) $(PROJECT4).c
	cp -p $(PROJECT4) rtp_L24_to_L24

$(PROJECT5): $(PROJECT5).c
	$(CC) -O -o $(PROJECT5) $(PROJECT5).c -lasound
	cp -p $(PROJECT5) record16

$(PROJECT6): $(PROJECT6).c
	$(CC) -O -o $(PROJECT6) $(PROJECT6).c -lasound
	cp -p $(PROJECT6) $(PROJECT6)_L16


.PHONY: clean

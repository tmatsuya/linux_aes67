# Linux用 AES67 Receiver & Sender

## 検証済み環境

 1-1. Raspberry Pi 4 / Zero W

 1-2.  Linux Ubuntu 20


## 必要ソフトウェア環境
	sudo apt-get install libasound2 libasound2-dev


## 制限事項(2022/11/1 18:30現在)

 3-1. 24Bit 1ms単位、16Bit 5ms単位のみ

 3-2 音声入出力対応

 3-3 SDP、PTP未対応


## コンパイルと実行方法

 4-1 make


## すぐ動かしたい人向け

以下をRaspi or Ubuntu上で実行すると標準AES67パケット(L24 1ms)が生成され、
それを受信してDefault ALSAデバイスから音が再生されます

```
 make
 ./rtp_packet_generator 239.69.83.133:5004 AES67_L24_1ms_sample.packet 1 &
 ./aes67_receiver 239.69.83.133:5004
```


以下をRaspi or Ubuntu上で実行するとマイコン向けAES67パケット(L16 5ms)が生成されます。
ESPS32上でAES67を再生待機すると音が再生されます

```
 make
 ./rtp_packet_generator 239.69.83.134:5004 AES67_L24_1ms_sample.packet 5 &
```


## ESP32用のAES67ゲートウェイ

  標準AES67 L24(1ms) 239.69.83.133:5004 を マイコン向け L16(5ms) 239.69.83.134:5004 へライブ変換する
 
```
 ./rtp_L24_to_L16 239.69.83.133:5004 239.69.83.134:5004 5
```


## Raspberry Pi Zero W/4のOTGの設定 (AES67のオーディオインターフェイスとして用いる使い方)

 5-1. /boot/config.txt に以下の行を追加
```
dtoverlay=dwc2
```

 5-2. /etc/module に以下の行を追加

```
dwc2
g_audio
```

 5-3. reboot する

 5-4. Raspberry Pi をホストに接続し、オーディオI/Fとして認識されることを確認する

 5-5. alsaの録音、再生デバイスとして認識されていることを確認する

```
aplay -L
arecord -L
```

 5-6. 出力デバイスとして利用する場合は aes67_sender??? を実行する

```
# ESP32用のAES67(L16/5ms)を239.69.83.134:5004で出力する
./aes67_sender_L16 239.69.83.134:5004
```


## プログラム一覧

```
  6-1.  aes67_receiver          AES67を受信しALSAデバイスで再生する(24bit 1ms)
  6-2.  aes67_receiver_L16      AES67を受信しALSAデバイスで再生する(16bit 5ms、マイコン用
)
  6-3.  aes67_sender   	        ALSAデバイスより録音されたものをAES67に送信する(24bit 1ms)
  6-4.  aes67_sender_L16        ALSAデバイスより録音されたものをAES67に送信する(16bit 5ms、マイコン用
)
  6-5.  rtp_packet_generator	ファイルに保存してあるAES67/RTP(RAWパケット)を受信し、ネットワーク上に再生する(AES67送出の実機がない時のdebug用)
  6-6.  rtp_L24_to_16		ネットワーク上の標準AES67(L24/1ms)パケットを受信し、マイコン用のAES67(L16/2ms)パケットに変換(ゲートウェイ)しリアルタイム送信する
  6-7.  play24			AES67/L24をALSAで再生する デバッグ用に作ったもの
  6-8.  record24                AES67/L24をALSAで録音する デバッグ用に作ったもの
```


## 使用例

```
  AVIO-USBで生成されたAES67を再生する
	./aes67_receiver 239.69.83.133:5004

  RAVENNA/AES67で生成されたAES67を再生する
	./aes67_receiver 239.1.3.139:5004

  ファイル(AES67_L24)より、AES67/RTPを1ms単位でマルチキャスト(239.69.83.133:5004)で送出する。ループ再生。標準AES67/RTPデバッグ用
	./rtp_packet_generator 239.69.83.133:5004 AES67_L24.packet 1

  ファイル(AES67_L16)より、AES67/RTPを5ms単位でマルチキャスト(239.69.83.134:5004)で送出する。ループ再生。マイコン向けAES67/RTPデバッグ用
	./rtp_packet_generator 239.69.83.134:5004 AES67_L16.packet 5

  標準AES67/RTP(L24 1msec間隔 239.69.83.133:5004)をマイコン向けAES67/RTP(L16 5msec間隔 239.69.83.134:5004)へ変換しリアルタイム送信する
	./rtp_L24_to_L16 239.69.83.133:5004 239.69.83.134:5004 5

  ALSAデバッグ用にAES67/RTPのペイロードのみデータより再生する
	./play24 < genkiwodashite.L24

  ALSAデバッグ用に録音データをAES67/RTPのペイロード形式で出力する
	./record24 > sample.L24
```


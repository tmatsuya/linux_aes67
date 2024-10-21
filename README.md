# Linux用 AES67 Receiver & Sender

## 検証済み環境

 1-1. Raspberry Pi 4/5/Zero W

 1-2.  Linux Ubuntu 20/22


## 必要ソフトウェア環境

 2-1. Ubuntu 20/22/Raspberry Pi 4/5/Zero W

```
	sudo apt-get install libasound2 libasound2-dev
```

 2-2. Ubuntu 24 ARM

```
    sudo apt-get install libgtk2.0-0t64 libgtk-3-0t64 libgbm-dev libnotify-dev libnss3 libxss1 libasound2t64 libxtst6 xauth xvfb libasound2-dev
```


## 制限事項(2024/10/21 現在)

 3-1. 16/24Bit 1/5ms単位対応

 3-2 ESP-32での音声入出力同時対応

 3-3 ESP-32での音声入力は16Bitのみ対応

 3-4 SDPは出力のみ対応、PTP未対応

 3-5 Ubuntu 24.04 ARMではPCM 16Bitのモード指定でエラーになる？


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


 AES67のマルチキャストを生成し、AES67レシーバで再生する

```
 ./demo_play
```

 DSPサーバを用いたディレイ・エコー処理した結果を再生する

```
 ./demo_dsp
```

 複数チャンネルのミキシング処理した結果を再生する(２チャンネル以降は手動で追加)

```
 ./demo_mix
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
  6-6.  rtp_L24_to_16		ネットワーク上の標準AES67(L24/1ms)パケットを受信し、マイコン用のAES67(L16/5ms)パケットに変換(ゲートウェイ)しリアルタイム送信する
  6-7.  rtp_L24_to_24		ネットワーク上の標準AES67(L24/1ms)パケットを受信し、マイコン用のAES67(L24/5ms)パケットに変換(ゲートウェイ)しリアルタイム送信する
  6-8.  play24			AES67/L24をALSAで再生する デバッグ用に作ったもの
  6-9.  record24                AES67/L24をALSAで録音する デバッグ用に作ったもの
  6-10. rtp_multi_to_uni        AES67マルチキャストをユニキャストとして受信、DSPサーバに処理してもらいその結果を別アドレスのマルチキャストで送信する
  6-11. dsp_server              音声信号処理を行うサーバ。AES67ユニキャストを受信し信号処理(ディレイ・エコー)をしたのちリアルタイムで返信する
  6-12. mix_server              音声信号処理を行うサーバ。複数のAES67ユニキャストを受信しミキシング信号処理をしたのちリアルタイムで返信する
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

  239.69.83.134:5004のAES67を 8.8.8.8:5004のDSPサーバにディレイ・エコー処理にしてもらい結果を 239.69.83.134:5005 に送信する
       ./rtp_multi_to_uni 239.69.83.134:5004 8.8.8.8:5004 239.69.83.134:5005

  DSPサーバをポート番号５００４で待機する
       ./dsp_server 5004

  ミキシングサーバをポート番号５００４で待機する
       ./mix_server 5004
```


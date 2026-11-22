# Sistem Penyiraman Tanaman Otomatis Berbasis IoT (Smart Irrigation)
Proyek UAS Mata Kuliah Internet of Things (FT468) — Program Studi Teknologi Informasi, Universitas Teuku Umar.

## Deskripsi Sistem
Sistem penyiraman tanaman otomatis berbasis arsitektur empat lapisan IoT menggunakan Arduino UNO R3 sebagai pengendali tepi (*edge controller*) dan NodeMCU ESP8266 sebagai *gateway* Wi-Fi yang menyajikan dashboard web untuk pemantauan serta kontrol jarak jauh melalui smartphone.

## Library & Sumber yang Digunakan
- `DHT sensor library` by Adafruit (pembacaan suhu dan kelembapan udara DHT11)
- `Servo` (library bawaan Arduino IDE untuk kontrol motor servo SG90)
- `SoftwareSerial` (library bawaan Arduino IDE untuk komunikasi serial UART UNO–NodeMCU)
- `ESP8266WiFi` & `ESP8266WebServer` (bawaan ESP8266 Arduino Core)

## Penggunaan Pin Perangkat Keras

### Arduino UNO R3
| Komponen | Pin Komponen | Pin Arduino UNO |
| :--- | :--- | :--- |
| Soil Moisture Sensor | A0 (Analog Out) | `A0` |
| Sensor Cahaya LDR | A0 (Analog Out) | `A1` |
| Sensor Suhu & Kelembapan DHT11 | DATA | `D4` |
| 1-Digit 7-Segment (5161BS) | Segmen A, B, C, D, E, F, G | `D3, D2, D12, D13, D8, D6, D5` |
| Modul Relay 1-Channel (Pompa) | IN | `D7` |
| Motor Servo SG90 | Signal (Oranye) | `D9` |
| NodeMCU ESP8266 (Serial UART) | D6 (TX) / D5 (RX) | `D10` (RX) / `D11` (TX) |

### NodeMCU LUA ESP8266
| Koneksi | Pin NodeMCU | Terhubung Ke |
| :--- | :--- | :--- |
| Serial RX | `D5` | `D11` (TX Arduino UNO) |
| Serial TX | `D6` | `D10` (RX Arduino UNO) |
| Common Ground | `GND` | `GND` Arduino UNO & Baterai 4xAA |

## Cara Menjalankan
1. Rakit seluruh komponen sesuai tabel penggunaan pin di atas dan pasang baterai 4xAA untuk catu daya pompa serta servo.
2. Salin berkas `secrets.example.h` menjadi `secrets.h`, lalu isi `WIFI_SSID` dan `WIFI_PASS` sesuai hotspot yang digunakan.
3. Unggah `arduino_uno.ino` ke board Arduino UNO (9600 baud).
4. Unggah `nodemcu_esp8266.ino` ke board NodeMCU 1.0 (ESP-12E Module).
5. Buka Serial Monitor NodeMCU untuk melihat alamat IP lokal, lalu akses alamat tersebut melalui browser HP.
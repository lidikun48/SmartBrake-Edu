# SmartBrake Edu

SmartBrake Edu adalah prototipe sistem telemetri pengereman berbasis ESP32 yang dikembangkan untuk membantu observasi praktik pengereman sepeda motor pada pelatihan *safety riding*. Sistem memperoleh data tekanan rem depan dan belakang dalam bentuk persentase relatif, membaca kecepatan roda depan dan belakang, menghitung distribusi penggunaan rem F:R, serta menampilkan dan merekam data melalui Web Dashboard pada jaringan Wi-Fi lokal.

Repository ini digunakan sebagai dokumentasi teknis source code dan desain perangkat keras SmartBrake Edu.

## Fitur Utama

- Pembacaan tekanan rem depan dan belakang dalam skala relatif 0–100%.
- Kalibrasi relatif menggunakan metode **Set Zero–Set Max**.
- Pembacaan kecepatan roda depan dan belakang menggunakan sensor *magnetic pickup*.
- Perhitungan distribusi penggunaan rem depan dan belakang (F:R).
- ESP32 sebagai *local access point* dan Web Server.
- Web Dashboard responsif untuk smartphone, tablet, dan laptop.
- Data Logger berbasis browser.
- Brake Test History dan Session Manager.
- Grafik kecepatan dan tekanan selama sesi.
- Brake Event Timeline dan Distribution History.
- Ekspor data hasil sesi.
- Penyimpanan konfigurasi perangkat menggunakan Preferences/NVS.
- Pembaruan firmware melalui OTA.

## Arsitektur Singkat

```text
Sensor Tekanan Depan & Belakang
            |
            v
       Pembacaan ADC
            |
            v
         Filter EMA
            |
            v
   Kalibrasi Relatif
     Set Zero–Set Max
            |
            v
 Persentase Tekanan 0–100%
            |
            +----------------------+
                                   |
Sensor Magnetic Pickup F/R        |
            |                      |
            v                      |
      Pembacaan Pulsa              |
            |                      |
            v                      |
 Perhitungan Kecepatan             |
            |                      |
            +----------+-----------+
                       |
                       v
                     ESP32
                       |
                       v
              Wi-Fi Lokal / HTTP
                       |
                       v
                 Web Dashboard
                       |
                       v
                  Data Logger
```

## Perangkat Keras Utama

- ESP32 DevKit V1
- 2 sensor tekanan hidrolik
- 2 sensor *magnetic pickup*
- PC817 pada antarmuka sensor kecepatan
- rangkaian pengondisi sinyal analog
- LED indikator sistem dan sensor
- sistem catu daya portabel

## Konfigurasi GPIO

| Fungsi | GPIO |
|---|---:|
| Sensor tekanan depan | 32 |
| Sensor tekanan belakang | 33 |
| Sensor kecepatan roda depan | 26 |
| Sensor kecepatan roda belakang | 27 |
| LED system | 2 |
| LED tekanan depan | 5 |
| LED tekanan belakang | 19 |
| LED kecepatan depan | 3 |
| LED kecepatan belakang | 22 |

## Struktur Repository

```text
SmartBrake-Edu/
├── README.md
├── platformio.ini
├── .gitignore
├── src/
│   └── main.cpp
├── docs/
│   ├── hardware/
│   │   └── README.md
│   ├── schematic/
│   │   ├── BrakePressure4.kicad_pro
│   │   ├── BrakePressure4.kicad_sch
│   │   ├── BrakePressure4.kicad_pcb
│   │   └── README.md
│   └── screenshots/
│       └── README.md
└── data/
    └── README.md
```

## Source Code

Firmware utama dan Web Dashboard berada pada:

```text
src/main.cpp
```

Web Dashboard disimpan di dalam firmware menggunakan `PROGMEM`, sehingga HTML, CSS, dan JavaScript berada pada file source yang sama dengan program ESP32.

## Pengembangan dengan PlatformIO

Project menggunakan framework Arduino pada PlatformIO untuk board ESP32 DevKit V1.

Build firmware:

```bash
pio run
```

Konfigurasi build dan metode upload terdapat pada `platformio.ini`.

### Upload USB

Untuk upload melalui kabel USB, gunakan `upload_protocol = esptool` pada `platformio.ini`.

### Upload OTA

Konfigurasi repository menyediakan pengaturan OTA menggunakan `espota` pada alamat lokal perangkat. Pastikan komputer telah terhubung ke jaringan SmartBrake Edu sebelum melakukan upload OTA.

## Konfigurasi Wi-Fi

Demi keamanan repository publik, password Access Point pada `src/main.cpp` menggunakan placeholder:

```cpp
const char* AP_PASS = "CHANGE_ME";
```

Ganti nilai tersebut dengan password yang akan digunakan pada perangkat sebelum melakukan build untuk penggunaan aktual. Jangan menyimpan password pribadi atau kredensial sensitif di repository publik.

## Desain KiCad

Berkas proyek KiCad tersedia pada folder [`docs/schematic`](docs/schematic):

- `BrakePressure4.kicad_sch`
- `BrakePressure4.kicad_pcb`
- `BrakePressure4.kicad_pro`

## Catatan Pengukuran

Nilai tekanan yang ditampilkan SmartBrake Edu merupakan **persentase tekanan relatif** berdasarkan kalibrasi Set Zero–Set Max. Sistem tidak dikalibrasi sebagai instrumen pengukuran tekanan absolut dalam satuan PSI, bar, atau MPa dan tidak mengukur gaya pengereman aktual pada bidang kontak ban dengan permukaan jalan.

Parameter kecepatan digunakan sebagai data pendukung untuk mengamati perubahan laju selama sesi dan tidak dinyatakan sebagai hasil pengukuran kecepatan terkalibrasi terhadap instrumen referensi.

## Konteks Akademik

SmartBrake Edu dikembangkan dalam Proyek Akhir Program Studi D4 Teknik Mesin Otomotif, Fakultas Vokasi, Universitas Negeri Yogyakarta.

**Pengembang:** Chiko Casillas  
**Tahun:** 2026

## Status Repository

- Firmware utama: tersedia.
- Web Dashboard: terintegrasi pada firmware.
- Skematik KiCad: tersedia.
- Layout PCB KiCad: tersedia.
- Konfigurasi PlatformIO: tersedia.
- Folder data dan dokumentasi tambahan: disiapkan untuk contoh data serta dokumentasi akademik yang dapat dipublikasikan.

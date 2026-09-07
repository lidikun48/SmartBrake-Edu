# SmartBrake Edu

SmartBrake Edu adalah sistem telemetri berbasis ESP32 untuk membantu evaluasi praktik pengereman sepeda motor pada pelatihan *safety riding*. Sistem mengintegrasikan pembacaan tekanan rem depan dan belakang secara relatif, kecepatan roda depan dan belakang, perhitungan distribusi penggunaan rem F:R, serta Web Dashboard dan Data Logger melalui jaringan Wi-Fi lokal.

## Tujuan

SmartBrake Edu dikembangkan sebagai perangkat bantu instruktur untuk memberikan data pendukung selama evaluasi praktik pengereman. Sistem tidak dimaksudkan untuk menggantikan penilaian visual instruktur, melainkan menambahkan informasi kuantitatif mengenai pola penggunaan rem dan perubahan kecepatan selama sesi pengujian.

## Fitur Utama

- Pembacaan tekanan rem depan dan belakang dalam skala relatif 0–100%.
- Kalibrasi relatif menggunakan metode **Set Zero–Set Max**.
- Pembacaan kecepatan roda depan dan belakang menggunakan sensor *magnetic pickup*.
- Perhitungan distribusi penggunaan rem depan dan belakang (F:R).
- Web Dashboard berbasis jaringan Wi-Fi lokal ESP32.
- Data Logger berbasis browser untuk merekam sesi pengujian.
- Grafik kecepatan dan tekanan selama sesi.
- Riwayat sesi dan ekspor data.
- Penyimpanan parameter perangkat menggunakan NVS/Preferences.
- Dukungan pembaruan firmware melalui OTA.

## Arsitektur Sistem

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
- Rangkaian pengondisi sinyal analog
- PC817 pada jalur sensor kecepatan
- LED indikator sistem dan sensor
- Sistem catu daya portabel

## Konfigurasi GPIO

| Fungsi | GPIO |
|---|---:|
| Sensor tekanan depan | 32 |
| Sensor tekanan belakang | 33 |
| Sensor kecepatan roda depan | 26 |
| Sensor kecepatan roda belakang | 27 |
| LED System | 2 |
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
│   └── README.md
├── docs/
│   ├── hardware/
│   │   └── README.md
│   ├── schematic/
│   │   └── README.md
│   └── screenshots/
│       └── README.md
└── data/
    └── README.md
```

Source firmware utama (`src/main.cpp`) akan ditambahkan dari versi final yang digunakan pada perangkat agar isi repository tetap sesuai dengan implementasi aktual.

## Pengembangan dengan PlatformIO

Project dikembangkan menggunakan framework Arduino pada PlatformIO untuk board ESP32 DevKit V1.

Build firmware:

```bash
pio run
```

Upload mengikuti environment dan metode upload yang didefinisikan pada `platformio.ini`.

## Catatan Pengukuran

Nilai tekanan pada SmartBrake Edu merupakan **persentase tekanan relatif** hasil kalibrasi Set Zero–Set Max. Sistem tidak dikalibrasi sebagai instrumen pengukuran tekanan absolut dalam satuan PSI, bar, atau MPa dan tidak mengukur gaya pengereman aktual pada bidang kontak ban dengan jalan.

Parameter kecepatan digunakan sebagai data pendukung selama evaluasi sesi pengereman dan bukan sebagai klaim pengukuran kecepatan terkalibrasi terhadap instrumen referensi.

## Konteks Pengembangan

Repository ini digunakan sebagai dokumentasi teknis pengembangan SmartBrake Edu untuk proyek akademik di bidang teknik otomotif dan evaluasi praktik *safety riding*.

## Status

Dokumentasi repository sedang disusun berdasarkan konfigurasi final perangkat yang digunakan pada pengujian lapangan.

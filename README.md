# El Konsolu Çalışması

LilyGO T-Display S3 ile Retro-Go tabanlı el konsolu ve microSD müzik çalar.

## Durum (2 Kasım 2026)

- Kartta eski sürüm yüklü. Serial logda depolama, oyun klasörleri ve launcher doğrulandı.
- Yeni yazılım hazır: Müzik uygulaması, microSD, PCM5102A DAC ve analog D-pad. Karta henüz yazılmadı.
- Ekran görüntüsü, tuşlar, Doom, SD ve ses donanımda henüz denenmedi.
- Eksik donanım: PCM5102A DAC modülü, microSD adaptör bağlantısı, 3,5 mm kulaklık.

## Klasörler

- `kablo-rehberi.html`: breadboard kablolama, müzik tuşları, pertinaks tasarımı. Yayınlanan sürüm: https://claude.ai/artifact/UCmoJYyV18ExkFTkgRT7tQ
- `retro-go/`: Retro-Go kaynak kodu, `t-display-s3` dalı. Değişiklikler commit edilmedi.
- `retro-go/muzik/`: müzik uygulaması. MP3 çözücü minimp3 (CC0).
- `roms-indirilen/sd/roms/`: oyunlar. Lisanslar `roms-indirilen/LISANSLAR.txt` içinde.
- `flash-cikti/`: karta yazılan imajlar (`base.bin`, `vfs.bin`).
- `yedekler/`: kartın eski yazılımının yedekleri.
- `referans/`: pin diyagramları ve kartın resmi şeması.
- `idf53.ps1`: ESP-IDF 5.3.6 ortamını açar.
- `karta-yaz.ps1`: yeni yazılımı ve oyunları karta yazar.
- `MUZIK-PLANI.md`: müzik donanımı ve pin kararları.
- `MALZEME-LISTESI.md`: alınacak malzemeler.
- `pertinaks-lehim-rehberi.html`: delik delik lehim adımları. Yayınlanan sürüm: https://claude.ai/artifact/VMFygoTegyb297KKMzmKbx
- `kutu/`: 3B yazıcı kutusu. `kutu.py` ölçülerden modeli üretir, STL dosyaları `kutu/stl` içinde. Önizleme: https://claude.ai/artifact/X227qUFdsHASdfbypUpouL

## Derleme

ESP-IDF kurulumu `C:\esp\v5.3.6` ve `C:\esp\idf53-tools` altında kalır. Retro-Go 5.3 ister, sistemdeki v6.0.2 ile derlenmez.

```powershell
. .\idf53.ps1
cd retro-go
python rg_tool.py --target t-display-s3 --fatsize 13107200 build-img launcher retro-core prboom-go muzik
```

## Karta yazma

Kart bağlıyken bu klasörde:

```powershell
.\karta-yaz.ps1
```

Port COM6 değilse `.\karta-yaz.ps1 -Port COM7` yaz.

## SD kart

FAT32 biçimli kart kullan. `roms-indirilen\sd` içindekileri kartın köküne kopyala, MP3 dosyalarını `roms\music` klasörüne koy. SD takılıyken sistem iç hafızayı kullanmaz.

## Geri dönme

Eski yazılım için `yedekler\tdisplay_app_backup_2026-11-01.bin` dosyasını `0x0` adresine yaz. Kart flash okumada kararsız, güvenilir okuma için `--no-stub read_flash` kullan. Yazma işlemleri sorunsuz.

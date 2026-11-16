# Malzeme Listesi

Projenin tamamında kullanılacak her şey: sende olanlar ve alınacaklar.
Eksik tüm parçaları (havya seti ve multimetre dahil) tek platform üzerinden doğrulanmış canlı linklerle almak için: [TRENDYOL-SIPARIS-LISTESI.md](TRENDYOL-SIPARIS-LISTESI.md)

## Sende olanlar

| Malzeme | Adet | Nerede kullanılıyor |
|---|---|---|
| LilyGO T-Display S3 | 1 | Konsolun kendisi |
| 3,7 V 1000 mAh Li-Po pil | 1 | Pille çalışma. Soketi yok, kablo gerekiyor |
| 6×6 tuş | 8 | D-pad, A, B, Start, Select |
| 10K direnç | 4 | D-pad direnç ağı |
| Breadboard | 1 | Deneme kurulumu |
| Dişi-erkek jumper kablo | 8 | Breadboard'da kart bağlantısı |
| Erkek-erkek jumper kablo | 10 | Breadboard'da tuş GND'leri. Pertinaksta kesilip tel olarak da kullanılır |
| microSD kart | 1 | Oyunlar ve şarkılar |

Elinde olup bu projede kullanılmayanlar: 10DOF sensör kartı, LED'ler, 330R dirençler.

## Belli değil, kontrol et

| Malzeme | Ne yapmalı |
|---|---|
| microSD → SD adaptörü | Kartınla geldiyse alma |
| Pil kablosu (1,25 mm JST-GH, 2 pin) | Kartın kutusuna bak. JST-PH 2.0 ve JST-SM takılmaz. Dükkanlarda bulamadım |
| 3,5 mm kablolu kulaklık | USB-C kulaklık bu kartta çalışmaz |
| Multimetre | Yoksa okul laboratuvarındakini kullan |
| 3B yazıcı ve PLA | Okul yazıcısını kullanacaksan sende |

## Alınacaklar

| Malzeme | Adet | Yaklaşık fiyat | Kullanım |
|---|---|---|---|
| PCM5102A DAC modülü, 3,5 mm jaklı | 1 | 132-355 TL | Müzik ve oyun sesi. [Direnc.net](https://www.direnc.net/pcm5102a-i2s-dac-dekoder-modul) 132 TL ama stokta yoktu, [ROBIZ](https://www.akakce.com/elektronik-devre-elemani/en-ucuz-robiz-pcm5102a-dac-modul-iis-ses-karti-muzik-uretici-dijital-analog-donusturucu-raspberry-pi-esp32-fiyati,1091322807.html) 355 TL stokta görünüyordu |
| 7×9 cm pertinaks | 1 | ~33 TL | [Robotistan](https://www.robotistan.com/7x9cm-double-sided-perfboard) |
| 12 pin dişi soket | 2 | ~10 TL | [Robotistan](https://www.robotistan.com/12-pin-female-header-en). Kart söküp takılabilir kalır |
| Küçük sürgülü anahtar | 1 | ~4 TL | [Direnc.net](https://www.direnc.net/3-pin-spdt-mini-on-off-switch-180-en). Pili açıp kapatır |
| M2×20 vida | 4 | fiyat bulamadım | Kutuyu kapatır |
| Çift taraflı bant | 1 | fiyat bulamadım | Pil ve modülleri arka kapağa sabitler |
| Havya seti | 1 | 400-420 TL | Sadece lehimi evde yapacaksan. [SolinpaTech ayarlı](https://www.akakce.com/lehim-makinesi-havya/solinpatech.html), [Tudor](https://www.akakce.com/lehim-makinesi-havya/tudor.html) |

İsteğe bağlı: makaron (ısıyla daralan boru), pil ve anahtar lehimlerini yalıtmak için. Yoksa izolasyon bandı da olur.

## Toplam

- Lehimi okul laboratuvarında yaparsan: yaklaşık 300-550 TL.
- Havya setini de alırsan: yaklaşık 700-1000 TL.
- Pil kablosu, vida ve bant fiyatları bulunamadı; bunlar için tahmini 100-150 TL dahil.

## Almana gerek olmayanlar

- 12×12 tuş ve kapak: tuşlar 3B yazıcıdan basılıyor.
- Buzzer: ses DAC'tan çıkıyor.
- Hazır 5 V microSD modülü: pille çalışırken güvenilir olmaz.

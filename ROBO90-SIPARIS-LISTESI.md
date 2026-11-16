# Robo90 Tek Sepet Sipariş Listesi

Bu liste, LilyGO T-Display S3 tabanlı El Konsolu ve MP3 Çalar projesinin donanım montajı, lehimleme ekipmanları (havya seti, multimetre) ve bağlantı kabloları için doğrudan Robo90 üzerindeki doğrulanmış ürün sayfalarını içerir.

---

## 1. Elektronik Devre Elemanları ve Modüller

| # | Malzeme | Amaç | Doğrudan Ürün Linki |
|---|---|---|---|
| 1 | **PCM5102A I2S DAC Dekoder Modül** | Oyun ve müzik ses çıkışı (3.5 mm kulaklık jaklı) | [Ürüne Git](https://www.robo90.com/pcm5102a-i2s-dac-dekoder-modul) |
| 2 | **7×9 cm Tek Yüzlü Delikli Pertinaks** | Devre elemanları ve tuşların lehimleneceği taban kart | [Ürüne Git](https://www.robo90.com/7x9-cm-tek-yuzlu-delikli-pertinaks-prototip-pcb-bakir-plaket-pertinaks-robo90-robo90-33116-27-B) |
| 3 | **40 Pin 2.54 mm Tek Sıra Dişi Header** | T-Display S3 kartı soketi için (kesilerek 2 adet 12'li yapılır) | [Ürüne Git](https://www.robo90.com/40-pin-254-mm-tek-sira-disi-header-siyah-konnektor-klemens-header-robo90-robo90-39897-40-B) |
| 4 | **Mini Sürgülü Switch (3 Pin / SPDT)** | Pil açma / kapama anahtarı (180 derece slide) | [Ürüne Git](https://www.robo90.com/mini-surgulu-on-off-switch-180-derece-slide-3-pin-buton-switch-anahtar-robo90-robo90-41689-89-B) |
| 5 | **Mikro SD Hafıza Kart Adaptör Kılıfı** | 3B baskı kasanın alt yarığına oturan ve arkasına tel lehimlenen kılıf | [Ürüne Git](https://www.robo90.com/mikro-sd-hafiza-kart-adaptoru-raspberry-pi-aksesuar-robo90-robo90-45226-30-B) |
| 5b | *(Yedek Alternatif)* **Micro SD Kart Modülü (SPI)** | Kılıf yerine hazır PCB slot tercih edilirse | [Ürüne Git](https://www.robo90.com/micro-sd-kart-okuyucu-modulu-arduino-uyumlu-donusturucu-robo90-robo90-41706-90-B) |
| 6 | **JST-GH 1.25 mm Konnektör Seti (Kablolu)** | Kartın üzerindeki pile giriş soketine takılacak 2 pin dişi kablo | [Ürüne Git](https://www.robo90.com/jst-gh-125mm-konnektor-seti-2-10-pin-kablolu-jst-kablo-robo90-robo90-56447-63-B) |
| 7 | **M2×20 mm YSB Vida (10 Adet)** | 3B baskı ön ve arka kapağı sabitlemek için civata | [Ürüne Git](https://www.robo90.com/m2x20mm-ysb-vida-galvaniz-10-adet-civata-somun-pul-robo90-robo90-45994-32-B) |
| 7b | **M2 Somun (10 Adet)** | M2 vidaların karşılığı | [Ürüne Git](https://www.robo90.com/m2-somun-galvaniz-10-adet-civata-somun-pul-robo90-robo90-45980-32-B) |

---

## 2. El Aletleri ve Ölçü Ekipmanları

| # | Ekipman | Amaç | Doğrudan Ürün Linki |
|---|---|---|---|
| 8 | **Lehimleme / Havya Kiti (40W)** | Kalem havya, lehim teli, lehim pastası, stand ve pompa içeren set | [Ürüne Git](https://www.robo90.com/lehimleme-havya-kiti-40w-havya-lehim-robo90-robo90-56500-23-B) |
| 9 | **DT-830D Dijital Multimetre (Sarı)** | Pil voltajı, polarite (+/-) ve kısa devre (buzzer) kontrolü | [Ürüne Git (Sarı)](https://www.robo90.com/dt-830d-dijital-multimetre-olcu-aleti-multimetre-robo90-robo90-40402-53-B) |
| 9b | **DT-830D Dijital Multimetre (Siyah)** | Siyah renk alternatifi | [Ürüne Git (Siyah)](https://www.robo90.com/dt-830d-dijital-multimetre-siyah-multimetre-robo90-robo90-44678-17-B) |

---

## 3. Montaj ve Bağlantı Öncesi Kritik Bilgiler

1. **Pil Kablosu ve Polarite:**
   * Pilin kendi çıplak kırmızı/siyah kabloları mevcuttur ancak T-Display S3 kartının üzerinde **JST 1.25 mm** erkek soket bulunmaktadır.
   * JST 1.25 mm kablonun dişi ucu karta takılır; diğer uçları sürgülü switch üzerinden pilin kablolarına lehimlenir.
   * **Önemli:** Pili sokete takmadan önce kartın arkasındaki **`+` ve `-`** işaretlerini multimetreyle doğrula. Ters voltaj ESP32-S3 çipine kalıcı hasar verir.

2. **MicroSD Adaptör Kılıfı:**
   * Kasanın (`kutu/kutu.py`) altındaki yarık 27 mm × 2.8 mm ölçüsünde standart SD adaptör kılıfına göre tasarlanmıştır.
   * Kılıfın arkasındaki bakır bacaklara doğrudan 6 tel lehimlenerek pertinaksa bağlanır (arada voltaj düşüren regülatör bulunmadığından pil zayıfladığında dahi 3.3V kararlı çalışır).

3. **Dişi Header Kesimi:**
   * 40 pinlik dişi header maket bıçağı veya yan keskiyle 12'şer pinlik iki parçaya bölünerek pertinaksa lehimlenir. Böylece LilyGO kartı doğrudan sokete takılıp çıkarılabilir kalır.

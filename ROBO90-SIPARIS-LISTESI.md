# Robo90 Tek Sepet Sipariş Listesi

Bu liste, LilyGO T-Display S3 tabanlı El Konsolu ve MP3 Çalar projesi için gereken tüm elektronik parçaları, el aletlerini (havya seti, multimetre) ve montaj elemanlarını **Robo90** üzerinden tek bir siparişte toplamak amacıyla hazırlanmıştır.

---

## 1. Elektronik Devre Elemanları ve Modüller

| # | Malzeme | Amaç | Robo90 Arama Terimi / Ürün Linki |
|---|---|---|---|
| 1 | **PCM5102A I2S DAC Dekoder Modül** | Oyun ve müzik ses çıkışı (3.5 mm kulaklık jaklı) | [PCM5102A I2S DAC Dekoder Modül](https://www.robo90.com/pcm5102a-i2s-dac-dekoder-modul) |
| 2 | **7×9 cm Delikli Pertinaks (Plaket)** | Devre bileşenlerinin ve tuşların lehimleneceği taban kart | [7x9 cm Tek Yüzlü Delikli Pertinaks](https://www.robo90.com/7x9-cm-tek-yuzlu-delikli-pertinaks) |
| 3 | **1×40 2.54 mm Tek Sıra Dişi Header** | T-Display S3 kartını takıp çıkarmak için (kesilerek 2 adet 12'li yapılır) | [1x40 Dişi Header 2.54mm](https://www.robo90.com/1x40-disi-header) |
| 4 | **Mini Sürgülü Switch (SPDT / 1P2T)** | Pil açma / kapama anahtarı | [Mini Sürgülü Switch SPDT](https://www.robo90.com/mini-surgulu-switch-spdt) |
| 5 | **Mikro SD Hafıza Kartı Adaptörü** | Kasadaki yuvaya tam oturan, doğrudan 3.3V bacaklarına tel lehimlenecek kılıf | [Mikro SD Hafıza Kart Adaptörü](https://www.robo90.com/mikro-sd-hafiza-kart-adaptoru-raspberry-pi-aksesuar-robo90-robo90-45226-30-B) |
| 6 | **Mikro JST 1.25 mm 2 Pin Dişi Kablo** | Kartın üstündeki pile giriş soketine takılacak kablo | [Mikro JST 1.25 2 Pin Dişi Kablo](https://www.robo90.com/mikro-jst-1.25-2-pin) |
| 7 | **M2×20 mm Silindir Başlı Civata / Vida (Somunlu, 4 Adet)** | 3B baskı ön ve arka kapağı birbirine sabitlemek için | [M2x20 mm Vida ve Somun](https://www.robo90.com/m2-vida) |

---

## 2. El Aletleri ve Ölçü Ekipmanları

| # | Ekipman | Amaç | Robo90 Arama Terimi / Ürün Linki |
|---|---|---|---|
| 8 | **Lehimleme / Havya Kiti** | Kalem havya, lehim teli, lehim pastası, pompa ve havya sehpası içeren temel set | [Lehimleme - Havya Kiti](https://www.robo90.com/lehimleme-havya-kiti) |
| 9 | **DT-830D Dijital Multimetre** | Pil voltajı, kutup doğrulaması (+/-) ve kısa devre (buzzer) kontrolü | [DT-830D Dijital Multimetre](https://www.robo90.com/dt-830d-dijital-multimetre) |

---

## 3. Montaj ve Bağlantı Öncesi Kritik Notlar

1. **Pil Kablosu ve Polarite:**
   * Pilin kendi kırmızı/siyah kabloları mevcuttur ancak T-Display S3 kartının üzerinde **JST 1.25 mm** erkek soket bulunur.
   * Robo90'dan alınan JST 1.25 mm dişi kablonun uçları, sürgülü anahtar üzerinden pilin kablolarına lehimlenecektir.
   * **Dikkat:** Pili sokete takmadan önce kartın arkasındaki **`+` ve `-`** işaretlerini multimetre ile test et. Ters polarite ESP32-S3 çipini kalıcı olarak bozar.

2. **MicroSD Adaptör Kılıfı:**
   * Kasanın (`kutu/kutu.py`) alt yarığı 27 mm × 2.8 mm ölçüsünde standart SD adaptör kılıfına göre modellenmiştir.
   * Kart kılıfının arkasındaki 6 pine ince jumper telleri lehimlenerek pertinaksa bağlanır (arada regülatör bulunmadığından pil zayıfladığında dahi 3.3V hattı kararlı çalışır).

3. **Dişi Header Kesimi:**
   * 40 pinlik dişi header maket bıçağı veya yan keski ile 12'şer pinlik iki parçaya bölünerek pertinaksa lehimlenir. Böylece LilyGO kartı lehimlenmeden doğrudan sokete takılır.

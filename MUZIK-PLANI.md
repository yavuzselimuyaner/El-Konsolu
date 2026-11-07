# Müzik Çalar Planı

Hedef: microSD'deki şarkıları ekrandan seçip kulaklıkla dinlemek.

## USB-C kulaklık bu kartta olmaz

Sebep kartın kendi devresi (`referans/T_Display_S3_sema.pdf`):

- Karttaki USB-C soketinin CC hatlarında direnç görünmüyor. Kulaklık kendini bağlanacak bir host olarak göremez.
- VBUS hattı yalnızca dışarıdan beslenir. Pil, VBUS'a diyotla ayrılmış durumda ve 5 V yükseltici yok. Pille çalışırken USB-C kulaklığa güç gitmez.
- USB soketinin veri hatları doğrudan ESP32-S3'e gidiyor, analog ses hattı yok. Pasif USB-C kulaklıklar da çalışmaz.
- ESP32-S3 sadece Bluetooth LE destekliyor. Bluetooth kulaklık da olmaz.

## Önerilen çözüm: PCM5102A DAC modülü

3,5 mm jaklı küçük bir I2S DAC modülü. Kulaklık bu jaka takılır. Oyun sesleri de aynı çıkıştan gelir, buzzer gerekmez. Modülün beslemesi 3,3 V pininden alınır. Lehim köprülerini modülün kendi notuna göre ayarla.

Eksik kalan tek şart 3,5 mm kablolu kulaklık. Kulaklık yalnızca USB-C ise, onu kullanma imkanı yok.

## Pin paylaşımı

Tuşlar, SD ve DAC toplam 15 hat ister, kartın 13 boş pini var. D-pad iki analog hatta iner, böylece iki pin açılır. Çapraz basış çalışmaya devam eder, Doom'da ileri giderken dönmek mümkün kalır.

- Yukarı ve Aşağı: IO1 (ADC). Yukarı doğrudan GND'ye, Aşağı 10K üzerinden GND'ye, IO1 10K ile 3V'a.
- Sol ve Sağ: IO2 (ADC). Aynı bağlantı.
- A: IO12. B: IO13. Start: IO16. Select: IO21.
- microSD: CS IO3, MOSI IO43, SCK IO44, MISO IO18.
- DAC: BCK IO17, LRCK IO10, DIN IO11.

## Yazılım

- `retro-go/muzik` uygulaması yazıldı, launcher'da Muzik sekmesi var.
- Şarkılar `roms/music` klasöründen okunur. Launcher şarkı seçtirir, uygulama o klasörü sırayla çalar.
- Uygulama içinde liste, durdurma, önceki/sonraki, ses, karışık çalma ve ekran kapatma var.
- Türkçe harfler ekranda düz harfle gösterilir, çünkü yazı tiplerinde yoklar.
- SD, DAC ve analog D-pad hedef config'e eklendi. SD yoksa sistem iç flash ile açılır.

## Sıra

- Yeni düzene göre tuşları bağla ve yazılımı karta yaz.
- DAC modülünü ve SD adaptörünü breadboard'a ekle, sesi dene.
- Hepsi çalışınca pertinaksa taşı.

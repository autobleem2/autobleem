# AutoBleem 2 Kullanıcı El Kitabı

AutoBleem 2, **PlayStation Classic** için bir oyun başlatıcısıdır ve sürüm 2'den itibaren
**Raspberry Pi**, **USB sürücüsünden önyükleme yapan PC** ve **Windows** için de kullanılabilir. PS1
oyunlarınızı kapak sanatıyla birlikte raf şeklinde gösterir, bundled PCSX emülatöründe başlatır ve
RetroArch yüklendiğinde diğer sistemlerin oyunlarını da oynatır. Bu el kitabı her platformda kurulum,
günlük kullanım ve gelen araçları kapsar.

> Her platform için indirmeler **https://autobleem.retromenele.pl/** adresindedir. Sayfa platforma
> göre düzenlenmiştir: her birinin *Install* paneli indirdikleriniz; aşağıdaki *Build inputs*
> yükleyicilerin kendileri indirdikleriniz.

## 1. Ne Alıyorsun

- **Başlatıcı** - kapak rafı, setler (PlayStation, RetroArch, Uygulamalar), oyun detayları, sistem
  menüsü, seçenekler, hafıza kartı ve devam noktası araçları. Her platformda aynı program.
- **İki PS1 Emülatörü** - `pcsx-abnxt`, mevcut olan (varsayılan) ve `pcsx-ab`, AutoBleem'in her zaman
  gönderdiği klasik. Seçeneklerden birini seçersin; ikisi de aynı ayarları ve hafıza kartlarını
  kullanırlar.
- **RetroArch** (her platformda isteğe bağlı) diğer sistemler için: NES, SNES, Mega Drive, Game Boy,
  arcade ve çok daha fazlası. AutoBleem, kopyaladığın ROM'lardan RetroArch listelerini oluşturur ve
  her oyunu doğru çekirdeğiyle başlatır.
- **Konsol Araçları** (yalnızca PlayStation Classic): WiFi, saat ve oyun kontrolcüsü eşleştirmesi için
  *PSC-Bios* ve AutoBleem kernel'ini yüklemek için *ABFlashKit*.
- **UpdateRoms** Windows için: bir konsol sürücüsünün RetroArch listelerini ve kapak resimlerini PC'de
  yeniler, çünkü konsol ağa bağlı değildir.

![Başlatıcı: kapak rafı, seçilen oyunun detayları, düğme ipuçları](../images/en/launcher.jpg)

<!-- pagebreak -->

## 2. Kurulum

### 2.1 PlayStation Classic

Windows PC'si, USB sürücüsü (USB 2.0, 8 GB veya daha fazla; yükleyici isterseniz biçimlendirir) ve
stok konsola ihtiyacın var. AutoBleem sürücüden hiçbir konsol değişikliği olmadan çalışır. Sürücü,
stok bir konsol için **FAT32** olmalıdır - kernel'i exFAT okuyamaz. Yalnızca AutoBleem kernel'i
yüklenmiş bir konsol (ABFlashKit, bölüm 6) exFAT sürücüden de başlatılır, bu FAT32'nin 4 GB sınırını
kaldırır.

1. **AutoBleemInstaller-<sürüm>.zip** dosyasını sitenin PlayStation Classic panelinden indir ve
   herhangi bir yere çıkart. `AutoBleemInstaller.exe` ve yüklediği AutoBleem paketini içerir.
2. Sürücüyü tak ve `AutoBleemInstaller.exe` başlat. Üstten sürücüyü seç. İstediğini işaretle:
   - **Sürücüyü Biçimlendir** - yalnızca yeni bir sürücü için (üzerindeki her şey silinir). Konsolda
     AutoBleem kernel'i yoksa FAT32'yi seç.
   - **Kapak Veritabanları** - PS1 kütüphanesinin kapak resmi ve detayları (varsayılan olarak işaretli;
     yaklaşık 300 MB).
   - **RetroArch** - RetroArch ve çekirdekleri, ekstra uygulamalar (Doom, Quake, Amiga, ...) ve
     libretro varlıkları, diğer sistemlerin oyunları için. Varsayılan olarak kapalı; yükleyiciyi
     yeniden çalıştırarak daha sonra eklenebilir.
   - **BIOS Dosyaları** - RetroArch çekirdeklerinin ihtiyaç duyduğu BIOS dosyaları (RetroArch gerekir).
   - **Örnek Oyunlar** - rafın boş olmaması için birkaç ücretsiz homebrew oyunu.
3. **Yükle** basın ve bekleyin. İlerleme çubukları ve günlük her adımı gösterir; sürücü sonunda `SONY`
   olarak adlandırılır ve `UpdateRoms` sürücüye konur (bölüm 5'e bakın).
4. Sürücüyü güvenle çıkart, konsolun **ikinci USB bağlantı noktasına** (sağ taraf, oyuncu 2) bağla ve
   konsolu aç. AutoBleem stok menü yerine başlar.

**Açma ve Kapatma.** Sürücü takılıyken konsol açılır, ışıkları birkaç saniye yanıp söner (AutoBleem
alınıyor) ve sonra ekranda hiçbir şey gösterilmeden beklemeye geçer - bu konsolun bir güncelleme
hazırlamasıdır, AutoBleem'in çalışması bu şekildedir. **Power** bir kez basın ve başlatıcı açılır.
Sistem menüsündeki *Power Off* veya konsolun Power düğmesi, konsolu **AutoBleem beklemeye** sokar:
sürücü önce kesilir, sonra ışık **kırmızı** olur - AutoBleem'in amaçlandığı gibi çalıştığının işareti -
ve sonraki Power basışı başlatıcıyı birkaç saniye içinde doğrudan geri getirir. **Işık kırmızıyken
sürücü çekilebilir** ve Windows onu kontrol etmesini istemeden PC'ye konabilir; Power basılmadan önce
geri koy. Konsolun gücünü kesme, bir sonraki sefer önyükleme beklemeye gider.

Bir sürücüyü **güncellemek** için, üzerine daha yeni bir yükleyici çalıştır: oyunlarınız, kayıtlarınız,
ayarlarınız ve RetroArch içeriği kalır; yalnızca AutoBleem'in kendi dosyaları değiştirilir. AutoBleem
1.0 veya AutoBleem-NG ile yapılan sürücü otomatik olarak yeni düzene getirilir.

> Stok konsol saat ve ağa sahip değildir: tarihler yalnızca AutoBleem kernel'i kurulduktan sonra
> gösterilir (bölüm 6) ve RetroArch oyunlarının kapak resmi PC'deki UpdateRoms'tan gelir (bölüm 5).

Oyunlar sürücünün `Games` klasörüne gider, oyun başına bir klasör - layout için bölüm 3.9'a bakın.

### 2.2 Raspberry Pi

AutoBleem, bir Pi'yi küçük bir konsola dönüştürür: masaüstü olmadan doğrudan başlatıcıya önyükler.
İki hazır görüntü sitede - 32-bit ve 64-bit - artı mevcut bir Raspberry Pi OS Lite için tar dosyası.

| Model | 32-bit Görüntü | 64-bit Görüntü | Notlar |
|---|---|---|---|
| Raspberry Pi 5 | evet | evet | |
| Raspberry Pi 4 Model B, Pi 400 | evet | evet | |
| Raspberry Pi 3 Model B / B+ / A+ | evet | evet | başlatıcı ve PS1 için iyi |
| Raspberry Pi Zero 2 W | evet | evet | 512 MB RAM: PS1 çalışır, daha ağır RetroArch çekirdekleri çalışmaz |
| Raspberry Pi 2 Model B | evet | yalnızca v1.2 | 3D için yavaş |
| Raspberry Pi 1, Zero, Zero W | hayır | hayır | ARMv6 - hiçbiri çalışmaz |

**32-bit görüntü PS1 oyunları için önerilir**: `pcsx-ab`'nin hızlı ARM yeniden derleyicisi yalnızca
32-bit'tir, bu nedenle 64-bit yapı PS1 oyunlarını daha yavaş çalıştırır. 64-bit görüntü daha geniş
RetroArch çekirdek setine sahiptir.

**Raspberry Pi Imager ile:**

1. Raspberry Pi Imager'ı yükle (raspberrypi.com/software). *Choose OS*'ta *Use custom*'ı seç ve
   indirdiğin `autobleem-<sürüm>-rpi-armhf.img.xz` (32-bit) veya `-arm64.img.xz` (64-bit) dosyasını
   seç - veya uygulamanın ayarlarına `https://autobleem.retromenele.pl/rpi-imager/os_list.json`
   repository URL'sini ekle ve listeden AutoBleem'i seç.
2. Imager'ın özelleştirme ekranını (dişli veya *Next*'ten sonraki soru) kullan **kullanıcı adı ve
   parolası, WiFi ağı ve ülkesini, SSH'ı etkinleştir**. AutoBleem ilk önyüklemede ağa ihtiyaç duyar.
3. Kartı yazın, Pi'ye ekran ve klavye veya kontrolcü bağlı olarak koyun ve açın.

**İlk önyükleme** 5 ila 25 dakika sürer ve ekranda neler yaptığını gösterir. Ağ olmadan bir ağ sorar
(WiFi listesi, parola), sonra RetroArch'ı kurması gerektiğini sorar (yanıt olmayan bir dakika evet
anlamına gelir), sistem bölümünü genişletir, kartta kalan kısımdan `AUTOBLEEM` veri bölümü oluşturur,
RetroArch ve çekirdeklerini, BIOS paketlerini ve örnek oyunları kurar ve başlatıcıya yeniden başlatır.

Yanıtlar kartın önyükleme bölümündeki **`autobleem.txt`**'de önceden verilebilir (ilk önyüklemeden
önce herhangi bir PC'de düzenlenebilir):

| Anahtar | Varsayılan | Anlam |
|---|---|---|
| `root_gib` | 8 | Sistem bölümünün GiB cinsinden boyutu; geri kalanı oyun bölümü olur. |
| `hdmi_mode` | 1920x1080@60 | Tüm önyükleme için ekran modu (eski bir TV için `1280x720@60`). |
| `retroarch` | (sorulur) | `evet` / `hayır` - RetroArch ve diğer sistemler veya yalnızca PS1. |
| `thumbnails` | hiçbiri | `boxarts` çevrimdışı kapaklar için tüm kapak setini yansıtır (~9000 dosya). |
| `bios`, `downloads`, `samples` | evet | BIOS paketlerini, her indirmeyi veya örnek oyunları atlamak için `hayır`'ı ayarla. |

**Mevcut bir Raspberry Pi OS Lite**'de (Bookworm veya Trixie): `autobleem-rpi.tar.gz`'yi (veya arm64'ü)
Pi'ye kopyala, çıkart ve `sudo bash install.sh` çalıştır. Aynı soruları sorar, sonraki önyüklemede
root'u küçülterek (`--shrink-root <GiB>`) veri bölümü oluşturur ve başlatıcıyı ilk konsolda koyar.

Kurulumdan sonra kartın **`AUTOBLEEM` bölümü** (exFAT) doldurduğun şeydir: kartı çıkart ve herhangi
bir PC'de aç veya ağ üzerinden kopyala (SSH açık). PS1 oyunları için `Games/`, diğer sistemler için
`RetroArch/roms/<sistem>/`, PS1 BIOS için `System/Bios/` (bölüm 3.10), temalar için `Themes/`.

### 2.3 PC USB Sürücü

USB'den önyükleme yapabilen herhangi bir PC için aynı cihaz - 32-bit sistem, eski makineler de
çalışır:

1. Sitedeki PC panelinden `autobleem-<sürüm>-pcusb-i386.img.xz` indir ve Raspberry Pi Imager (*Use
   custom*), balenaEtcher veya Rufus (DD modu) ile 8 GB veya daha fazla sürücüye yaz.
2. PC'yi sürücüden önyükle (PC'niz önyükleme menüsü anahtarı - F12, F8, Esc...). Hem BIOS hem de
   UEFI önyüklemesi çalışır; **Secure Boot kapalı** olmalı.
3. İlk önyükleme Pi'ninktir: ağ kablosu yoksa ağ sorusu, RetroArch sorusu, sonra kurulum - ağ kablosu
   varsa yaklaşık sekiz dakika - ve başlatıcıya yeniden başlatma.

Sürücünün daha sonra oyunlarınız için `AUTOBLEEM` bölümü vardır, çalışan bir PC'de sürücüyü taktığında
Windows 10'da (1903 ve daha yeni) ikinci sürücü olarak görülür. `autobleem.txt` ilk bölümde, Pi'deki
aynı anahtarlarla (hiçbiri `hdmi_mode` yok - PC ekranın yerel modunu kullanır).

### 2.4 Windows

Windows programı olarak AutoBleem: tam ekran, emülatörler ve RetroArch programlar olarak başlatılır.

1. **AutoBleemSetup-<sürüm>.exe** indir ve çalıştır. Yönetici hakları olmadan kullanıcı başına yüklenir:
   program `%LOCALAPPDATA%\Programs\AutoBleem`'in altında, veriler (oyunlar, ayarlar, temalar,
   RetroArch) seçtiğin klasörde - varsayılan `Documents\AutoBleem`.
2. Bileşenleri işaretle - kapak veritabanları, RetroArch (resmi Windows yapısı ve çekirdekleri),
   BIOS dosyaları, örnek oyunlar - ve kurulum yardımcısının indirilmesine izin ver.
3. Start Menüsü veya Masaüstünden AutoBleem başlat. PC'de klavye gamepad olarak çalışır (bölüm 3.2).

Üzerine daha yeni bir kurulum çalıştırmak programı günceller ve veri klasörünü tutar. Başlatıcı ayrıca
günde bir kez siteyi kontrol eder ve yenisi varsa güncelleme sunar (bölüm 3.11).

<!-- pagebreak -->

## 3. AutoBleem'i Kullanma

### 3.1 Başlatıcı

Başlatıcı rafa açılır: mevcut setin kapakları, orta sırada seçilen kapak, yanında detayları - yayıncı,
yıl, seri, bölge, oyuncular, son ne zaman oynanması - ve oynat düğmesi. Alttaki çubuk düğmelerin ne
yaptığını listeler. Oyun klasöründün taraması her başlangıçta arka planda çalışır; çalışırken sağ üst
köşedeki bir balon ilerlemeyi gösterir ve yeni oyunlar raf üzerinde bulundukça görünür.

![Set Seçici: üç sekme ve mevcut sekmenin grupları numara sayımlarıyla](../images/en/set-picker.jpg)

### 3.2 Kontroller

| Düğme | Raf'ta |
|---|---|
| Sol / Sağ | Önceki / sonraki oyun. Tutma üzerinde kaydırma. |
| L1 / R1 | Başlıkların önceki / sonraki ilk harfine atla. |
| Cross | Seçilen oyunu başlat (PS1 oyunu PS1 emülatöründe; RetroArch oyunu çekirdeğinde; Uygulama okumadan sonra). |
| Square | Seçilen PS1 oyununu RetroArch'ta başlat. |
| Triangle | Düğme kılavuzu. |
| Start | Mevcut setten rastgele oyun. |
| Select | Set Seçici: PlayStation / RetroArch / Uygulamalar sekmeleri (L1 / R1), sekmenin grupları (Yukarı / Aşağı, L2 / R2 sayfa), Cross seçer. |
| Aşağı | Oyunun altındaki simge satırını aç (Ayarlar, Oyun, Hafıza Kartı, Devam). Yukarı kapatır. |
| L2 + R2 | Sistem menüsü (bölüm 3.4). |

**Klavye ile** (kontrolcüsü olmayan PC veya konsolda USB klavye, Pi veya PC sürücüsü) tuşlar yerini
alırlar: **Ok tuşları** = d-pad, **Enter** = Cross, **Esc veya Backspace** = Circle, **Tab** = Triangle,
**Space** = Square, **F1 / F2** = Select / Start, **Page Up / Page Down** = L1 / R1, **Home / End** =
L2 / R2, **F10** = sistem menüsü. Geliştirme makinesinde Esc programı kapatır ve Space Start'tır.

Her liste ve menüde: Yukarı / Aşağı taşınır, **L2 / R2 sayfaları çevirir**, L1 / R1 ilk / son satıra
atlar, **Cross seçer, Circle geri gider**. Ayarları olan bir ekran, Circle ile ayrıldığında bunları
kaydeder.

![Seçilen oyunun altındaki simge satırı](../images/en/launcher-icons.jpg)

### 3.3 Setler

**Select** set seçiciyi açar. PlayStation sekmesi *Tüm Oyunlar*, *Dahili Oyunlar*'ı listeler (PlayStation
Classic'te konsolun yerleşik yirmi tane), `Games/` altında yaptığın her klasör (alt klasördeki oyun o
gruba aittir), *Favori Oyunlar*, *Oyun Geçmişi* ve, herhangi bir oyun biri olarak işaretlendiğinde,
*Işıklı Silah Oyunları*. RetroArch sekmesi oyun içeren sistem başına bir grup artı RetroArch'ın Favorileri
ve Geçmişini listeler. Uygulamalar sekmesi uygulamaları türe göre gruplandırır: *Tüm uygulamalar*, sonra
*Oyunlar*, *Emülatörler*, *Araçlar*, *Medya* ve *Diğer* (kategori her uygulamanın `app.ini` dosyasında
ayarlanır). Her satır kaç öğe tuttuğunu gösterir; biri olmayan grup, Ayarları yalnızca gösteren boş rafa
açılır.

### 3.4 Hızlı Menü

Başlatıcıda **Yukarı** veya simge satırında **dişli simgesi** (Ayarlar / Oyun / Hafıza Kartı / Devam
nerede): karruseldan ulaştığın eylemler için hızlı menü. Kısa liste: *Oyunları Yeniden Tara* (şimdi
tarama başlat), *Store* (eklenti indirmek için AutoBleem Store), *Ağ ve Kontrolcüler* (yalnızca
yüklü eklenti `network` girişini sağladığında - konsolda PSC-Bios, Pi ve PC sürücüsü: WiFi, Bluetooth
eşleştirme, gamepad eşleme sihirbazı - bölüm 6'ya bakın; o eklenti devre dışı bırakıldığında "Eklentiler'den
etkinleştir" ile gri renkte - Cross Eklentiler listesini açar), ve *Sistem menüsü...*
(aşağıdaki tam menü). Yukarı / Aşağı taşır (sarılır), Cross seçer, Circle geri gider. Burada benzersiz
hiçbir şey yok - her öğe sistem menüsünde de var.

### 3.5 Sistem Menüsü

**L2 + R2** (birlikte, herhangi bir sırada) raf üzerinde sistem menüsünü açar. Menü bölümlere ayrılmıştır:

| Bölüm | Öğe | Ne Yapar |
|---|---|---|
| (üst) | Oyunları Yeniden Tara | Yeni, değişmiş veya kaldırılan oyunları şimdi ara (tarama klasörü de kendi başına izler). |
| | Eklentiler | Sürücüdeki eklentiler - AutoBleem Store ve diğerleri (bölüm 3.12). |
| **Kütüphane** | Oyun Yöneticisi | Oyun klasörleriyle PS1 oyunları bir liste olarak: oyun sil, kapakları temizle. Tarama çalışırken devre dışı. |
| | Hafıza Kartları | Hafıza kartı setlerin (bölüm 3.7). |
| | Tarama İşlemcileri | Her taramanın ilk çalıştırdığı programlar - sırası, açık veya kapalı (bölüm 3.13). Tarama çalışırken devre dışı. |
| **Sistem** | Seçenekler | AutoBleem ayarları (bölüm 3.6). |
| | Ağ ve Kontrolcüler | Yalnızca yüklü eklenti `network` girişini sağladığında (`extension.ini`'de `Provides=network` - konsolda PSC-Bios, Pi ve PC sürücüsü) - WiFi, Bluetooth kontrolcüsü eşleştirme, DualShock 3 kurulumu ve gamepad eşleme sihirbazı - bölüm 6'ya bakın. O eklenti yüklü ama devre dışı bırakılmışsa, öğe "Eklentiler'den etkinleştir" notu ile gri kalır - Cross Eklentiler listesini açar. |
| | Donanım Bilgisi | Makine gerçekleri: sistem, CPU, depolama, ağ arayüzleri, saat dilimi, ekran, kontrolcüler ve eşlemeleri. AutoBleem kernel'i olan konsolda bu **PSC-Bios** açar (bölüm 6); diğer makinelerde bu bilgi sayfasını gösterir. |
| | Yazılım Güncellemesi | (Raspberry Pi ve PC) Daha yeni AutoBleem veya RetroArch için siteyi şimdi kontrol et. |
| | Hakkında | Krediler ve lisans. |
| **Ayrıl** | RetroArch | Başlatıcıyı RetroArch'ın kendi menüsü için bırak. RetroArch'ı kapatma geri gelir. |
| | Kapat | Onaydan sonra: konsolda AutoBleem bekleme - sürücü kesildi, ışık kırmızı, Power başlatıcıyı geri getirir (bölüm 2.1); Pi veya PC'de makine kapatılır. |

![Sistem Menüsü](../images/en/system-menu.jpg)

### 3.6 Seçenekler

Ayarlar gruplara ayrılmıştır; Yukarı / Aşağı gruplar arasında taşınır, Sol / Sağ değeri değiştirir,
Circle ayrılır ve kaydeder. Her değişiklik hemen uygulanır.

| Grup / Ayar | Ne Yapar |
|---|---|
| **Arayüz**: AutoBleem Teması | Görünüm. Temalar `Themes/`'te yaşar; oraya atılan tema zip, sonraki ziyarette açılır. AutoBleem'in gönderdiği temalar her güncellemede yenilenir - birini özelleştirmek için önce yeni ad altında kopyala. |
| Kapak Stili | PS1 kapakları etrafında çizilen mücevher kutusu çerçevesi. |
| Dil | Başlatıcı dili, hemen uygulanır (17 dil). |
| Temadan Font Kullan / Font | Klasik ekranlar yazı tipi: temainki veya `resources/fonts`, `RetroArch/fonts` veya temadan herhangi bir `.ttf`/`.otf`. |
| Zaman Aşımını Göster (zaman aşımı yok için 0): | "Gösteriliyor: ..." bildirimi ne kadar süre kalır, saniye cinsinden (0 = sonsuza kadar). |
| **Ses**: Müzik, Arka Plan Müziği | Hangi şarkının başlatıcının altında çaldığı (temainki veya `resources/music`'ten dosya) ve çalınıp çalınmadığı. |
| **Emülasyon**: PS1 Emülatörü | `pcsx-abnxt` (varsayılan: AutoBleem eklemeleriyle mevcut PCSX-ReARMed) veya `pcsx-ab` (klasik). Biri tarafından kaydedilen devam noktası diğerinde devam eder, oyun BIOS dosyası olmadan çalışmadığı sürece. |
| Geniş Ekran | Her oyun için PS1 emülatörü resim şekli. |
| Tüm PSX Oyunlarını RA ile Oyna | Her PS1 oyunu RetroArch'ın PS1 çekirdeğinde başlar. |
| RA Yapılandırmasını Güncelle | AutoBleem, orada oyun başlattığında ayarlarını RetroArch'ın yapılandırmasına yazar. |
| **Kütüphane**: Dahili Oyunları Göster | Konsol oyunlarını PlayStation listelerinde (yalnızca PlayStation Classic). |
| Kapak Resmini Çevrimiçi Getir | Tarama, libretro sunucularından eksik kapakları getirir (Raspberry Pi, PC, Windows). |
| **Güncellemeler** | (Raspberry Pi, PC, Windows) `kararlı`, `en son` (ön sürümler de) veya `kapalı`. |

![Seçenekler, gruplarda](../images/en/options.jpg)

### 3.7 Oyunun Ayarları

Oyun seçiliyken, **Aşağı** satırını açar: **Ayarlar** (yukarıdaki seçenekler), **Oyun** (oyunun kendi
ayarları), **Hafıza Kartı** (kartı) ve **Devam** (kaydedilen noktaları). Cross imlecin altındakini açar.

**Oyun editörü** oyun detaylarını sağda ve ayarlarını solda, üç grupta gösterir:

- **Oyun**: *Favori* (Favori Oyunlar grubunda), *Işıklı Silah Oyunu* (bir ışıklı silah oyunu - Işıklı
  Silah grubuna katılır ve her zaman RetroArch'ta çalışır, PS1 çekirdeği GunCon'a sahiptir), *RA ile
  Oyna* (bu oyun RetroArch'ta çalışır), *Veriyi Kilitle* (tarayıcı oyunun başlığını, serisini ve disk
  listesini belirlediğin gibi bırakır).
- **Video**: yüksek çözünürlük, tarama çizgileri ve seviyesi, kare atlama, GPU eklentisi ve *Filtre* -
  resim nasıl ölçeklendirilir: Kapalı (düz pikseller), Doğrusal (yumuşatılmış) veya Sharp (parlak pikseller
  titremezsiz; yalnızca `pcsx-abnxt` - klasik `pcsx-ab` ve RetroArch bunu Kapalı olarak gösterir).
- **Emülatör**: SpeedHack, CPU saati, SPU enterpolasyonu, önyükleme logosu (kapalı BIOS kabuğunu atlar -
  özel logosu önyüklemeyi kıran homebrew disk için) ve `pcsx-abnxt` ile *Yumuşatma* filtresi ve *Sony
  düzeltmeleri* açma/kapatma.

Triangle oyunu yeniden adlandırır, Square hafıza kartını değiştirir, Start yeni kart paylaşır. Circle
kaydeder ve ayrılır.

**Emülatörde Kaydedilen Ayarlar.** Emülatörün kendi menüsü *Bu oyun için ayarları kaydet* bulunur. Bir
oyunun orada kaydedilen ayarları olduktan sonra, oyun bunlarla oynanır ve oyun editörü Video ve Emülatör
satırlarını gri gösterir, *Emülatörde Kaydedilen* başlığı altında bu değerlerle. Oyun editörü ayarlarına
geri dönmek için **Ayarların Kilidini Aç**'ı seç ve onayla: bu emülatörün kaydettiği ayarları siler ve
satırlar yeniden değiştirilebilir. Her iki emülatör, `pcsx-ab` ve `pcsx-abnxt`, aynı kaydedilen ayarları
okur ve yazar.

![Oyun Editörü](../images/en/game-editor.jpg)

### 3.7 Hafıza Kartları ve Devam Noktaları

Her PS1 oyunun varsayılan olarak kendi hafıza kartı vardır (`Games/!SaveStates/<oyun klasörü>/`'nde
kaydedilen noktalarıyla tutulur). Sistem menüsündeki **Hafıza Kartları** **paylaşılan setleri**
yönetir - birden fazla oyun kullanan bir kart, `Games/!MemCards/`'te tutulur: bir tane oluştur (Square,
ekran klavyesi ile), yeniden adlandır (Cross), sil (Triangle). Oyun editöründeki *Hafıza Kartını
Değiştir* veya Hafıza Kartı simgesinden set konur.

**Hafıza kartı editörü** (Hafıza Kartı simgesi) oyunun kartını ve ikinci kartını yan yana, her
kaydedilmiş ikon ve başlıkla gösterir: kaydedilmiş olan ikisi arasında kopyala (Square), sil
(Triangle), kart defragment (Select). Start sağdaki kartı başka bir set için değiştirir.

![Hafıza Kartı Editörü](../images/en/memory-card-editor.jpg)

**Devam Noktaları**: PS1 oyununu konsolun Reset düğmesi (veya Pi veya PC'de emülatörün menüsü) ile
çıktığında, AutoBleem nerede olduğunun kaydedilmiş bir durumunu tutar ve **Devam** simgesinin altında
sunar - dört slot, her biri anın resmiyle. Cross slottan devam eder, Triangle siler. Devam noktası olan
oyun Devam simgesinde küçük resim gösterir.

### 3.8 Oyunları, RetroArch'ı ve Uygulamaları Başlatma

**Cross** seçilen oyunu başlatır. PS1 oyunu seçilen PS1 emülatöründe (bölüm 3.5) tam ekranda çalışır,
çıkana kadar - konsolda ön **Reset** düğmesi (başlatıcıya devam noktasıyla) veya **Power** (konsol
kapanır) ile; Pi veya PC'de emülatörün menüsü (pad'ta Select + Start veya klavyede Esc) ile.
**Square** PS1 oyununu RetroArch'ta başlatır.

**RetroArch** oyunu RetroArch'ta başlatıcının sistemine seçtiği çekirdeğiyle başlar; menüsünden
*Kapanışı Kapat* veya *RetroArch'ı Kapat* başlatıcıya geri gelir. Sistem menüsündeki RetroArch öğesi
RetroArch'ın kendi menüsünü (XMB) hiçbir şey yüklenmediyken açar, ayarları ve kendi içerik listeleri için.

**Uygulama** (Uygulamalar seti: konsol araçları ve konsolda RetroArch paketinin getirdiği ekstra
uygulamalar - Doom, Quake, Amiga, ...) ilk olarak bilgisini gösterir; Cross başlatır, Circle geri
gelir.

![Başlamadan Önce Uygulamanın Bilgisi](../images/en/app-start.jpg)

### 3.9 Oyun Ekleme

**PS1 Oyunları** `Games` klasörüne gider, **oyun başına bir klasör**, oyundan sonra adlandırılır:

```
Games/
  Crash Bandicoot/            Crash Bandicoot.cue + Crash Bandicoot.bin
  Final Fantasy VII/          Final Fantasy VII (Disc 1).chd, (Disc 2).chd, (Disc 3).chd
  Platformers/                bir klasörün oyunları: seç seçicideki kendi grubu
    Klonoa/                   Klonoa.pbp
```

- Biçimler: `.cue` + `.bin` (veya `.img`), `.pbp`, `.chd` (zstd de), `.ecm` (tarama tarafından çözülür),
  `.iso`. Sıkıştırılmış oyun da çalışır: **Unzip** işlemcisi taramadan önce açar (bölüm 3.13).
- Multi-disk oyunu her disk içinde bir klasördür; `Game (Disc 1)`, `Game (Disc 2)` ... adı verilen klasörler
  tarama tarafından bir `Game` klasörüne birleştirilir.
- Doğrudan `Games/`'e atılan oyunlar (gevşek dosyalar) tarama tarafından klasörlere sıralanır.
- **Kapak** oyunun görüntüsü yanında PNG, adı gibi. Biri olmadan, kapak resmi kapak veritabanlarından gelir
  veya - RetroArch yüklüyse - libretro küçük resimleri setinden; Pi, PC veya Windows'ta eksik olan çevrimiçi
  getirme (Seçenekler → *Kapak Resmini Çevrimiçi Getir*).
- Tarama her disk serisini okur ve başlık, yayıncı, yıl, oyuncular ve bölgeyi RetroArch PlayStation
  veritabanından veya kapak veritabanlarından alır. Oyun editöründe herhangi bir şeyi değiştur ve
  *Veriyi Kilitle*'yi işaretle tutmak için.

**Diğer Sistemler** `RetroArch/roms/` altına gider, **sistem başına bir klasör, RetroArch veritabanları
gibi adlandırılır** (klasör sizin için yapılır): `Nintendo - Nintendo Entertainment System`, `Nintendo
- Super Nintendo Entertainment System`, `Sega - Mega Drive - Genesis`, `Nintendo - Game Boy Advance`,
`FBNeo - Arcade Games` (veya `Arcade`), ... ROM'lar sıkıştırılmış kalabilir. Pi, PC veya Windows'ta tarama
bunları kendisi okur ve RetroArch listelerini yazar; konsol sürücüsünde, PC'de **UpdateRoms**'u çalıştır
(bölüm 5).

**Uygulamalar** `Apps/<ad>/`'ın altına `app.ini` (ad, simge, ne çalıştıracağı) ve `run.sh` ile gider.

**Temalar** `Themes/<ad>/`'ın altına gider (`theme.json` ve görüntüler) - veya tema zip'ini `Themes/`'e
at.

### 3.10 PS1 BIOS

**PlayStation Classic**'te emülatör konsolun kendi BIOS'unu kullanır. **Raspberry Pi, PC ve Windows**'ta
kendi PS1 BIOS'unu `System/Bios/`'a koy: `romw.bin` (US/European SCPH-5501/5502) ve `romJP.bin`
(Japonca SCPH-5500). Yükleyiciler bunları RetroArch BIOS paketlerinden doldurur, kendi dosyaların
yoksa. Bunlar olmadan emülatör yerleşik HLE BIOS'ta çalışır, birçok oyun hoşlanır ve bazıları hoşlanmaz.

### 3.11 Güncellemeler

- **Raspberry Pi, PC sürücüsü, Windows**: başlatıcı start'ta ve günde bir kez siteyi kontrol eder (Seçenekler
  → *Güncellemeler* kanalı; Sistem menüsündeki *Yazılım Güncellemesi* şimdi kontrol eder). Daha yeni
  AutoBleem veya RetroArch olduğunda sorar: *Şimdi Güncelle* her şeyi indir ve ilk önyükleme ilerleme
  ekranıyla yükleyiciyi yeniden çalıştır; *Yarın Hatırlat* ve *Bu Sürümü Atla* diğer cevaplar. Oyunlarınız
  ve ayarlarınız kalır; başlatıcı güncelleme sonra bir kez yeniden tarar.
- **PlayStation Classic**: daha yeni `AutoBleemInstaller.exe` sürücü üzerinde çalıştır (bölüm 2.1).

### 3.12 Eklentiler ve AutoBleem Store

**Eklentiler** kendi ekranlarını başlatıcıya ekler. Sürücüde `Extensions/<ad>/`'da yaşarlar (Raspberry Pi
veri bölümü, Windows veri klasörü); kurmak için zip'i oraya aç. **L2 + R2 → Eklentiler** listeler: Cross
çalıştırır, Triangle kapatır veya yeniden açar. Ağ gereken eklenti ağ olmadan başlatılmaz ve başlatıcıyı
durdurmuş olan kapatılır - liste bunu söyler.

![Eklentiler Listesi](../images/en/extensions.jpg)

**AutoBleem Store** ilk eklentidir: bir basışla AutoBleem'in çalıştığı her sistemde kurmak için Uygulamalar
ve Oyunlar (PlayStation Classic AutoBleem kernel'i WiFi gerekir). Dört sekme, L1 / R1 arası:

- **Uygulamalar** ve **Oyunlar**: kaynakların ne sunduğu, her resim, sürüm, boyut ve kaynak favicon ile.
  Yüklü öğeler gri. Cross kurar (veya günceller veya başarısızlıktan sonra yeniden dener), Triangle Store'un
  kurduğunu kaldırır. L2 / R2 veya Sol / Sağ sayfaları çevir, **Select** bir kez bir kaynağı göster, **Start**
  başlıkları ara. Öğe resimleri önbellek ve yükleme başarısız olursa yeniden denenir.
- **İndirmeler**: indirilen, bekleyen, başarısız veya yüklü olan. İlerleme çubuğu sabit günceller. İndirmeler
  Store'dan çıkıktan sonra arka planda devam eder; oyun başlatma veya kapama yalnızca duraklat ve durdurulmuş
  indirme neresi bıraktıysa devam eder. Yüklü oyun sonraki taramadan sonra rafa görünür, Store'un resmi
  kapakta. 2 GB üzerinde indirmeler tüm platformlarda çalışır, 32-bit yapılar da.
- **Kaynaklar**: listeler nereden gelir - AutoBleem'in kendi kataloğu, `System/Extensions/store/sources/`'a
  atılan TSV listesi ve **Kaynak URL'si Ekle** ile eklediğin adresler. Her kaynak liste simge gösterir. Cross
  eklediğin yeniden adlandırır, adresi değiştirir, `http://` ve `https://` arasında geçişler veya kaldırır.

![Store'un Uygulamalar Sekmesi](../images/en/store-apps.jpg)

![Kaynak Menüsü](../images/en/store-source-menu.jpg)

AutoBleem kataloğunun sunduğu indir sitesinde de listelenir, `https://autobleem.retromenele.pl/store/`.
**Eklediğin kaynakların ne içerdiğinden siz sorumlusunuz.**

**Ağ üzerinde Kendi Oyunlarınız**: `abstored`, Store'un LAN sunucusu, PS1 oyunları klasörünü aynı ağ
üzerinde Store'a sunuyor. Herhangi bir Linux makinesinde çalışır - Raspberry Pi, ev sunucusu - ve yalnızca
klasörü okur. `abstored <oyun klasörü>` ile başlat, tarayıcıda `http://<bu makine>:8124/` aç ne sunduğunu
ve bulduğu sorunları görmek için ve kaynak olarak `http://<bu makine>:8124/store.tsv` ekle. Linux ve
Windows için hazır programlar Store sayfasında, **LAN Sunucusu** sekmesinde; hizmet olarak kurulumu
`INSTALL-linux.md` (`ext_store/server/` kaynakta). **LAN Share** (bölüm 5.2) PC'deki oyun ve diskler
böyle sunucuya koyar.

### 3.13 Tarama İşlemcileri

**Tarama İşlemcileri** her taramanın oyunlarını okumadan önce çalıştırdığı küçük programlar. Biri AutoBleem'in
okumaması bir biçimi biri olur - sıkıştırılmış oyun örneğin - veya oyun verisini değiştirir, çeviri yama gibi.
Sürücüde `System/Processors/<ad>/`'da yaşarlar (Raspberry Pi veri bölümü, Windows veri klasörü); kurmak için
klasörü orada aç. Sonraki tarama çalıştırır.

- **Unzip AutoBleem ile gelir**: tarama okumadan `Games/`'te sıkıştırılmış PS1 oyunlarını açar ve sıkıştırılmış
  ROM'ları onda bir (arcade setleri sıkıştırılmış kalır). AutoBleem güncelleme bunu günceller ve kapatıldıysa
  kapalı bırakır.
- Zaten oyunla ilgilenen işlemci oyun değişene kadar yeniden çalıştırılmaz.
- İşlemci çalışırken sağ üst köşedeki balon ne yaptığını gösterir; uyarı veya başarısızlık altındaki satırda
  görünür. `processors.log` günlük klasöründe detaylar.
- Oyun veya RetroArch başlatma dosyaları değiştiren işlemciyi durdurur; sonraki tarama işini bitir.

**L2 + R2 → Tarama İşlemcileri** çalıştığı sırada gösterir, PS1 oyunları için bir sekme ve ROM'lar için biri
(L1 / R1). **Square** işlemci kaldırır ve Yukarı / Aşağı taşır - sıra önemli: açan unzip yamasından önce gelmelidir.
**Cross** kapatır veya açar, **Triangle** sonraki taramada her oyuna yeniden bakma, **Circle** geri gelir ve
herhangi bir şeyi değiştirdiysen tarama başlat. Başka makine için yapılan işlemci listede kalır, gri.

![Tarama İşlemcileri](../images/en/processors.jpg)

Kendi yazma: Unzip sayfası, `https://github.com/autobleem2/proc_unzip`, işlemcinin ne yapması gerektiğini
anlatır ve `tools/proc_check.py` AutoBleem kaynakta paylaşmadan önce birini kontrol eder.

<!-- pagebreak -->

## 4. Ekranlar

### 4.1 Oyun Yöneticisi

Klasörleriyle PS1 oyunları bir liste ve seçilen kapağı ve detayları. Cross oyun editörünü açar, **Square
oyunu siler** (klasör ve ikinci sorudan sonra kaydedilen noktalar), Triangle her kapak PNG'yi oyunların
yanında siler (tarama veritabanlarından almaya devam eder), L2 / R2 sayfalar. Sürücünün boş alanı sağ üst
köşede. Oyun Yöneticisi tarama çalışana kadar bekler.

![Oyun Yöneticisi](../images/en/game-manager.jpg)

### 4.2 Donanım Bilgisi

Makine gerçekleri - sistem, donanım, depolama boş alanla, ağ adresleri, ekran ve ses sürücüleri, bağlı
kontrolcüler - her saniye yeniden okunan. AutoBleem kernel'i olan konsolda bu **PSC-Bios** açar (bölüm 6).

![Donanım Bilgisi](../images/en/hardware-info.jpg)

### 4.3 Düğme Kılavuzu

Raf'taki Triangle: her ekranın her düğmesi bir sayfada. USB klavyesi bağlıyken veya kullanılmışsa, Klavye
sütunu pad düğmelerinin yanına tuşları gösterir.

![Düğme Kılavuzu](../images/en/button-guide.jpg)

### 4.4 Ekranda Klavye

Yazılan her yerde - hafıza kartı seti, oyun başlığı, WiFi şifresi, kaynak adresi - aynı klavye, telefon gibi
yerleştirilmiş: harfler, sembol sayfası (`/ \ : ? & = % @ #` ve adres ya da parola ihtiyaç duyduğu geri kalanı)
ve iki aksan harfi sayfası, Shift, sayfa tuşu, Space, Backspace ve Onayla alt sırada. Yönler taşır, Cross
yazar, Triangle siler, Square boşluk, **L1** Shift (büyük harf için iki kez), **R1** sonraki sayfa, **L2 /
R2** imleci taşır, Start onaylar, Circle iptal eder. USB klavyesi her yerde yazar: Enter onaylar, Esc iptal
eder.

![Ekranda Klavye](../images/en/keyboard.jpg)

<!-- pagebreak -->

## 5. PC'de

### 5.1 UpdateRoms - konsol sürücü yenileme

PlayStation Classic ağı yok, bu nedenle sürücüsünün RetroArch listeleri ve kapak resmi PC'de yapılır:
**UpdateRoms** PC'de başlatıcı taraması Pi'de yapar, PC ağı ve konsol yolları ile, konsol açılır ve her
şeyi yerinde bulur.

1. ROM'larını sürücüye `RetroArch/roms/<sistem>/` altına kopyala (bölüm 3.9). Klasör adları RetroArch
   veritabanı adları olmalı; yükleyici yaygın olanları yapar.
2. **`UpdateRoms\UpdateRoms.exe` sürücüden başlat** (yükleyici oraya koydu). Oturduğu sürücüyü bulur, aşama
   satırı, ilerleme çubuğu ve günlük gösterir ve:
   - sürücü hiçbiri olmadığında RetroArch veritabanı paketini indir ve her ROM'ı tanımla - veritabanı
     bildiği oyun kendi adını alır;
   - sistem başına bir oynatma listesi `RetroArch/bin/playlists/`'e yazı konsol yolları ile, RetroArch
     kendisi oraya eklediğini tutarak;
   - hiçbiri olmayan her ROM'un kapak resmini libretro küçük resim sunucularından `RetroArch/bin/thumbnails/`'e
     getirir.
3. Sürücüyü güvenle çıkart ve konsola geri koy. Set seçicinin RetroArch sekmesi oyun içeren her sistemi listeler.

ROM klasörleri her değişiklikten sonra çalıştır; hiçbiri değişmedi klasör atlanır, böylece yeniden çalıştırma
hızlıdır. Günlük `System/Logs/updateroms.log`. Kart okuyucusundaki Raspberry Pi kartı aynı şekilde
yenilenebilir (`UpdateRoms.exe <sürücü> --target rpi`), Pi ağ sahipse kendisi yapar.

### 5.2 LAN Share - oyun ve diskler ağ sunucusunda

**LAN Share** (`LanShare.exe`, Store sayfasında **LAN Sunucusu** sekmesinde) PS1 oyunlarını ev ağ
Store sunucusuna - Raspberry Pi, NAS veya başka PC'de `abstored` - ve PS1 diskini PC'nin CD/DVD
sürücüsünden okur. Konsolda, Pi veya PC'de Store oradan kurar. Kurulacak hiçbir şey yok; ayarlar
`%LOCALAPPDATA%\AutoBleem LAN Share/`'da tutulur.

![LAN Share Penceresi](../images/en/lanshare.jpg)

1. **Sunucu**: adresini gir (`http://<adresi>:<port>`, Store sahip) ve **Bağlan** bas. Oyunları ve bulduğu
   sorunları solda listeler. Oraya oyun koymak için birini ver:
   - **Paylaş** - sunucunun oyun klasörü ağ üzerinde paylaştığı gibi (Samba), örneğin `\\raspberrypi\games`:
     LAN Share oraya kopyalar ve sunucuyu taramaya çağırır. Sunucu kendisi salt okunur kalır.
   - **Simbol** - sunucu `--allow-uploads` ile başlatıldığında: simbol (sunucu başlangıçta ve
     `<state>/upload-token`'da tutundu). LAN Share HTTP üzerinden yükler ve durdurulmuş yükleme neresi
     bıraktıysa devam eder.
2. **Bu PC'deki Oyunlar**: oyun klasörü seç (klasör başına oyun), oyunları işaretle ve **İşaretli Oyunları
   Yayınla** bas. **Sunucuda** sunucunun oyun daha yoksa söyler (seri ile, yoksa başlık ile); böyle oyun
   asla iki kez gönderilmez. **Sunucuda Olmayan Tikle** kalanı işaretler.
3. **Disk**: PS1 diskini sürücüye koy ve **Disk Oku ve Yayınla** bas. Disk tamamında `.bin` + `.cue`
   (ve LibCrypt oyunu için `.sbi`, sürücü subchannel verirse), başlıktan adlandırılmış, bilinen iyi
   dump'a karşı kontrol (veritabanları seçilmişse) ve yayınlandi. Birden fazla diskli oyun için
   **Oyun Birden Fazla Disk'tedir** işaretle: LAN Share her sonraki diski ister ve bir oyun olarak
   birlikte yayınlar.
4. **Sunucudan Sil...** seçilen oyunları sunucudan alır. Hiçbir şey silinmez: her sunucunun oyunlarının
   yanında `.removed` klasörüne taşınır ve geri taşıma geri koyar.

**Veritabanları** - AutoBleem kapak klasörü (`coversU/P/J.db`) ve RetroArch'ın `Sony - PlayStation.rdb`
- başlıkları ve okunan disk kontrolünü ver; ikisi de isteğe bağlı. **Bu PC'deki Oyunları Store ile Paylaş**
(varsayılan kapalı) bu PC'de klasörü Store'a doğrudan sunuyor. İlk kez Windows güvenlik duvarı hakkında sorar:
sadece özel ağları izin ver.

<!-- pagebreak -->

## 6. Konsol Araçları (PlayStation Classic)

PlayStation Classic sürücüsü için iki araç. İkisi de başlatıcının teması ve diliyle çizer ve iki de pad tarafından
yönetilir - ve gamepad sihirbazında konsolun ön düğmeleri tarafından. **PSC-Bios** konsolun paketiyle gelen bir
eklentidir: Sistem menüsünün *Donanım Bilgisi* bunu açar ve Eklentiler listesindedir. **ABFlashKit** Apps
setindeki bir Uygulamadır.

### 6.1 PSC-Bios

Konsol paketiyle gelen eklenti, Raspberry Pi ve PC sürücüsünde de mevcuttur. Sistem Menüsünün *Ağ ve Kontrolcüler*
öğesinden (veya Eklentiler listesinden) açılır. O eklenti yüklü ama kapalıysa Hızlı menüde ve Sistem menüsünde
*Ağ ve Kontrolcüler* öğesi "Eklentiler'den açın" notu ile gridir - Cross orada Eklentiler listesini açar.

Açılış ekranı makine gerçeklerini gösterir: zaman, saat dilimi, WiFi/Ethernet/Bluetooth ağ adaptörleri adresleriyle
ve bağlı her kontrolcü bir düğme eşleştirmesi olup olmadığı. Ağ ve Bluetooth kısımları konsolda AutoBleem kernel'i
(bölüm 6.2) veya Raspberry Pi / PC sürücüsünde sistem araçları gerekir; gamepad sihirbazı herhangi bir sistemde
çalışır.

![PSC-Bios: Ağ ve Kontrolcüler Merkezi](../images/en/pscbios-main.jpg)

- **Select - WiFi Ağı** (kernel veya NetworkManager): ağ adı (yazı veya taramadan seçim), parola, sürücü modu,
  ve *Uygula / Ağı Yeniden Başlat*. Saat dilimi de buradan ayarlanır. Konsolun IP adresi bağlandıktan sonra
  gösterilir.
- **Square - Bluetooth Kontrolcüleri**: Bluetooth gamepad taraması (DualShock 4, etc.), eşle veya sil için.
- **L1 - DualShock 3 Eşleştirmesi**: ilk DualShock 3 için USB bağlantısı, kernel sixaxis eklentisi ile.
- **R1 - Kontrolcü Eşlemesi**: eşleme sihirbazı (aşağı).
- **Triangle - Hakkında**, **Circle - başlatıcıya geri**.

**Gamepad sihirbazı** bağlı pad'ı ham gösterir - her eksen, düğme ve şapka sayılar olarak ve DualShock resmi
basarken aydınlanan şekilde. Test altındaki pad güvenilir olmadığından, sihirbaz konsolun **ön düğmeleri**
tarafından yönetilir: **RESET** sonraki pad'a geçer, **OPEN** eşlemeyi başlatır (sonra her soru cevapla - resimde
aydınlanan düğmeyi bas veya OPEN pad'ın böyle düğmesi yoksa), **POWER** iptal eder veya bırakır. Pad'ta Circle'ı
2 saniye tutma sihirbazı bırakır (bar doldurur ve alt ipucu "2 s Tut: Çık" der). Pad'ın henüz eşlemesi yokken,
herhangi bir düğmeyi 2 saniye tutma bunu yapar ("Herhangi bir Düğmeyi 2 s Tut: Çık"). Kısa basış normal eşlenir.
Klavyede Esc / Space / Enter POWER / RESET / OPEN için durur. Sonunda yeni eşleme test için eklenir ve OPEN
seçtiğin ad altında kaydeder; başlatıcı bundan sonra yükler.

![PSC-Bios: Kontrolcü Eşleme Sihirbazı](../images/en/pscbios-wizard.jpg)

### 6.2 ABFlashKit - AutoBleem Kernel'i

AutoBleem kernel'i konsolun Linux kernel'inin isteğe bağlı değiştirilmesi: çalışan bir saat, USB WiFi ve Bluetooth
dongle'ları (PSC-Bios ve Bluetooth pad'lar için) ve emülatörün devam noktaları için kullandığı ön düğme desteğini
getir. ABFlashKit kurar, konsolun yedeklemesini yapıp yapabilir konsolunu Sony'nin kendi kurtarması üzerinden
geri koyar.

> **Bu araç konsolun flash belleğine yazı.**  Kesilen flash - akım kesildi, sürücü çekildi - konsolu başlayamaz
> yapabilir ve özel kernel kurulumu garantiyi iptal eder. Konsolu güçlü ve sürücüyü içinde tutun kendisi yeniden
> başlatana kadar. ABFlashKit bu uyarıda açılır; *Anladım* devam eder, *Çık* bırakır.

![ABFlashKit Menüsü](../images/en/abflashkit-menu.jpg)

- **Kernel'i Kur**: konsolun bölüm yedeklemesini sürücüde (`LBOOT.EPB`) henüz yoksa yapar, kontrol eder ve kernel
  görüntü, kernel ve AutoBleem sistem dosyalarını yazar ve yeniden başlatır. *Hepsi tamam - ekran siyah
  olduğunda güç kablosunu çıkar*: konsolun gücünü çek ve geri tak.
- **Tam Yedekleme**: dört bölüm `LBOOT.EPB`'ye, daha sonra geri getirme için (önceki yedekleme sorudan sonra
  üzerine yazılır).
- **Kurtarma Modu**: yedekleme stok birini kontrol eder, kurtarma bayrağı ayarlar ve Sony kurtarmasına yeniden
  başlatır, sürücüde `LBOOT.EPB`'den konsolu geri getir - stok firmware'e geri yol.

Her adımın altında ilerleme çubuğu adım ne kadar ileri gösterir. Araç başka özel firmware çalıştıran (BleemSync,
Project Eris) konsolu kurulumunu reddeder: önce stok'a geri getir.

<!-- pagebreak -->

## 7. Bir Şey Yanlış Giderse

- **Günlükler**: AutoBleem günlükleri bellekte tutar, bu nedenle sürücü her zaman yazılmaz - sadece yanlış olduğunda
  `System/Logs/` sürücü, kart veya veri klasörüne: başlatıcı, PS1 oyunu veya RetroArch çökmesi `System/Logs/crash-<n>/`'e
  kaydedilir (son üç tutulur) ve başlatıcı geri geldiğinde bir kez söyler. Her günlüğü tutmak için *Seçenekler -> Tanılama
  -> Günlükleri USB bellekte tut*'u aç (sonraki başlangıçtan) veya PC'de `System/Logs/keep` boş dosya oluştur. Pi veya PC'de
  *Donanım Bilgisi* günlüklerin nerede olduğunu gösterir ve Square `System/Logs/saved-<n>/`'e kaydeder. Dosyalar:
  `autobleem.log` (başlatıcı), `launch.log` ve `pcsx.log` (PS1 oyunu başlatması ve emülatör çıkışı), `retroarch.log` ve
  - her zaman sürücüde - `update.log` (çevrimiçi güncelleme) ve `updateroms.log` (UpdateRoms).
- **Oyun Raf'ta Değil**: klasör yerleşimini kontrol et (oyun başına klasör, bölüm 3.9 görüntü biçimleri). *Oyun Yöneticisi*
  tarama reddettikleri oyunları *Eklenmedi* işaretleri ile listeler, nedeni ile; böyle klasör Square siler. Sistem menüsündeki
  Yeniden Tara çalıştırma çalıştırır.
- **Kapak Yok**: kapak veritabanları yüklenmedi (yükleyiciyi tekrar çalıştır işaretli olarak) veya RetroArch oyunları konsolda
  UpdateRoms PC'de çalıştırılmadı.
- **Kontrolcü Hiçbir Şey Yapmıyor veya Düğmeleri Karışık**: PSC-Bios gamepad sihirbazı (bir konsol) eşler; Pi veya PC'de
  Donanım Bilgisi sayfası SDL'nin gördüğü listeler.
- **Konsol Oyundan Sonra Siyah Ekran**: AutoBleem kendisi pencereyi yeniden inşa eder (üç kez kadar); siyah kalırsa, güç
  düğmesini tutun ve konsolu tekrar açın.
- **Raspberry Pi**: `Alt+F2` ikinci konsolda giriş istemi verir; SSH ilk önyüklemeden açık. `sudo journalctl -u autobleem`
  başlatıcı hizmetini gösterir; `sudo systemctl restart autobleem` yeniden başlatır. Bitemeyen ilk başlatış (ağ yok)
  sonraki başlatışta yeniden dener.
- **Windows**: `Esc` başlatıcıyı bırakır; veri klasörü kuruluşta seçilen (varsayılan `Documents\AutoBleem`), günlükler
  `System\Logs`'ta.

AutoBleem özgür yazılım (GNU GPL v3 veya daha yeni), garanti yok. Destek ve haberler: About ekranında bağlı Discord
sunucusu ve https://autobleem.retromenele.pl/.

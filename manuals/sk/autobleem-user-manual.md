# Používateľská príručka AutoBleem 2

AutoBleem 2 je spúšťač hier pre **PlayStation Classic** - a od verzie 2 tiež pre
**Raspberry Pi**, **PC spúšťaný z USB kľúča** a **Windows**. Zobrazuje vaše PS1 hry ako
policu s obalmi a detailmi hier, spúšťa ich v zväzenom emulátor PCSX a s nainštalovaným
RetroArch tiež hrá hry ďalších systémov. Táto príručka sa zaoberá inštaláciou na každú
platformu, každodenným používaním a nástrojmi, ktoré s AutoBleem prichádzajú.

> Stiahnutie pre každú platformu sú na **https://autobleem.retromenele.pl/**. Stránka je
> organizovaná podľa platformy: panel *Install* každej z nich je to, čo si stiahnete;
> sekcia *Build inputs* pod ňou je to, čo si samotní inštalátori stahujú.

## 1. Čo dostanete

- **Spúšťač** - kolotoč s obalmi, sady (PlayStation, RetroArch, aplikácie), detaily hier,
  systémová ponuka, možnosti, nástroje pre pamäťové karty a body obnovenia. Rovnaký program
  na všetkých platformách.
- **Dva emulátory PS1** - `pcsx-abnxt`, ten aktuálny (predvolený), a `pcsx-ab`, klasický
  emulátor balíka AutoBleem. Vyberiete si jeden v možnostiach; oba používajú rovnaké
  nastavenia a pamäťové karty.
- **RetroArch** (voliteľný na každej platforme) pre ostatné systémy: NES, SNES, Mega Drive,
  Game Boy, arkádu a mnoho ďalších. AutoBleem si vytváral seznamy RetroArch z ROM, ktoré si
  kopírujete, a spúšťa každú hru so správnym jadrom.
- **Nástroje konzoly** (iba PlayStation Classic): *PSC-Bios* pre WiFi, hodiny a mapovanie
  herného ovládača a *ABFlashKit* na inštaláciu jadra AutoBleem.
- **UpdateRoms** pre Windows: obnovuje zoznamy RetroArch a obaly hier na USB kľúči konzoly na
  PC, pretože konzola sama nemá sieť.

![Spúšťač: Polica s obalmi, detaily vybranej hry, nápoveda tlačidiel](../images/en/launcher.jpg)

<!-- pagebreak -->

## 2. Inštalácia

### 2.1 PlayStation Classic

Potrebujete PC s Windows, USB kľúč (USB 2.0, 8 GB alebo viac; inštalátor ho na požiadanie
naformátuje) a konzolu. AutoBleem beží z kľúča bez zmien konzoly. Kľúč musí byť **FAT32**
pre konzolu bez jadra AutoBleem - jej jadro nevie čítať exFAT. Len konzola s nainštalovaným
jadrom AutoBleem (ABFlashKit, kapitola 6) tiež bootuje z exFAT kľúča, čím sa prekonáva
limit FAT32 4 GB.

1. Stiahnite si **AutoBleemInstaller-<verzia>.zip** z panela PlayStation Classic na webe a
   rozbaľte ho kdekoľvek. Obsahuje `AutoBleemInstaller.exe` a balík AutoBleem, ktorý
   inštaluje.
2. Zapojte kľúč a spustite `AutoBleemInstaller.exe`. Vyberte kľúč hore. Zaškrtnite, čo
   chcete:
   - **Format the stick** - iba pre nový kľúč (všetko na ňom sa zmaže). Vyberte FAT32,
     pokiaľ konzola nemá jadro AutoBleem.
   - **Cover databases** - obaly a detaily knižnice PS1 (zaškrtnuté predvolene; približne
     300 MB).
   - **RetroArch** - RetroArch s jadrami, ďalšími aplikáciami (Doom, Quake, Amiga, ...) a
     assets libretro pre hry ďalších systémov. Predvolene vypnuté; možno ich pridať neskôr
     spustením inštalátora znova.
   - **BIOS files** - BIOS súbory, ktoré jadrá RetroArch potrebujú (vyžaduje RetroArch).
   - **Sample games** - niekoľko bezplatných homebrew hier, aby polica nebola prázdna.
3. Stlačte **Install** a čakajte. Pruhy priebehu a záznam ukazujú každý krok; kľúč je na
   konci pomenovaný `SONY` a je na neho umiestnený `UpdateRoms` (pozri kapitola 5).
4. Bezpečne vyberte kľúč, zapojte ho do **druhého USB portu** konzoly (pravého, hráč 2) a
   zapnite konzolu. AutoBleem sa spustí namiesto štandardného menu.

**Zapínanie a vypínanie.** S kľúčom vo vnútri sa konzola spustí, niekoľko sekúnd bliká
(AutoBleem sa vyzvedáva) a potom ide do režimu pohotovosti pred tým, ako sa čokoľvek
zobrazí - to je vlastný spôsob konzoly, ako sa pripraviť na aktualizáciu, ako sa AutoBleem
spúšťa. Stlačte **Power** raz a spúšťač sa objaví. *Power Off* v systémovej ponuke alebo
tlačítko Power na konzole vloží konzolu do **pohotovosti AutoBleem**: kľúč sa najskôr
odpojí, potom sa svetlo zmení na **červené** - znak, že AutoBleem funguje správne - a
ďalšie stlačenie Power vrátí spúšťač späť v niekoľkých sekundách. **Keď je svetlo červené,
môžete kľúč vybrať** a dať do PC bez toho, aby Windows požiadalo jeho kontrolu; vrátite ho
späť pred stlačením Power. Odpojenie napájania konzoly prejde bootovacou pohotovosťou
znova nabudúce.

Ak chcete **aktualizovať** kľúč, spustite novší inštalátor cez neho: vaše hry, uložená hra,
nastavenia a obsah RetroArch zostanú; iba súbory AutoBleem samého sa nahrádzajú. Kľúč
vytvorený s AutoBleem 1.0 alebo AutoBleem-NG sa automaticky prenáša do nového usporiadania.

> Konzola bez jadra AutoBleem nemá hodiny ani sieť: dátumy sa zobrazujú iba po inštalácii
> jadra AutoBleem (kapitola 6) a obaly hier RetroArch pochádzajú z UpdateRoms na PC
> (kapitola 5).

Hry sa umiestnili do priečinka `Games` kľúča - jeden priečinok na hru - pozri oddiel 3.9
pre usporiadanie.

### 2.2 Raspberry Pi

AutoBleem zmení Pi na malú konzolu: bootuje sa priamo do spúšťača bez desktopového
prostredia. Dva hotové obrázky sú na webe - 32bitový a 64bitový - plus tarball pre
existujúci Raspberry Pi OS Lite.

| Model | 32bitový obrázok | 64bitový obrázok | Poznámky |
|---|---|---|---|
| Raspberry Pi 5 | áno | áno | |
| Raspberry Pi 4 Model B, Pi 400 | áno | áno | |
| Raspberry Pi 3 Model B / B+ / A+ | áno | áno | v poriadku pre spúšťač a PS1 |
| Raspberry Pi Zero 2 W | áno | áno | 512 MB pamäte: PS1 beží, ťažšie jadrá RetroArch nie |
| Raspberry Pi 2 Model B | áno | iba v1.2 | pomalé pre čokoľvek 3D |
| Raspberry Pi 1, Zero, Zero W | nie | nie | ARMv6 - žiadny obrázok nebeží |

**32bitový obrázok je odporúčaný** pre PS1 hry: `pcsx-ab` má rýchly ARM recompiler iba
pre 32bitový, takže 64bitový build spúšťa PS1 hry pomalšie. 64bitový obrázok má väčšiu
sadu jadier RetroArch.

**S Raspberry Pi Imager:**

1. Nainštalujte Raspberry Pi Imager (raspberrypi.com/software). V *Choose OS* vyberte
   *Use custom* a stiahnutý `autobleem-<verzia>-rpi-armhf.img.xz` (32bitový) alebo
   `-arm64.img.xz` (64bitový) - alebo pridajte URL úložiska
   `https://autobleem.retromenele.pl/rpi-imager/os_list.json` v nastaveniach aplikácie a
   vyberte AutoBleem zo zoznamu.
2. Pomocou Imager obrazovky prispôsobenia (ozubené koleso alebo otázka po *Next*) nastavte
   **meno používateľa a heslo, WiFi sieť a krajinu a zapnite SSH**. AutoBleem potrebuje
   sieť pri prvom štarte.
3. Napíšte kartu, vložte ju do Pi s obrazovkou a klávesnicou alebo herným ovládačom
   pripojeným a zapnite ju.

**Prvý boot** trvá 5 až 25 minút a na obrazovke ukazuje, čo robí. Bez siete sa pýta na
jednu (zoznam WiFi, heslo), potom sa pýta, či nainštalovať RetroArch (minúta bez odpovede
znamená áno), zväčšuje systémový oddiel, vytváral oddiel `AUTOBLEEM` zo zvyšku karty,
inštaluje RetroArch a jeho jadrá, balíčky BIOS a vzorové hry a restartuje do spúšťača.

Odpovede je možné zadať dopredu v **`autobleem.txt`** na bootovacieho oddiele karty
(upraviteľný na ľubovoľnom PC pred prvým spustením):

| Kľúč | Predvolený | Význam |
|---|---|---|
| `root_gib` | 8 | Veľkosť systémového oddiele v GiB; zvyšok sa stane oddielom hier. |
| `hdmi_mode` | 1920x1080@60 | Režim obrazovky pre celý boot (`1280x720@60` pre staršiu TV). |
| `retroarch` | (asked) | `yes` / `no` - RetroArch a ďalšie systémy alebo iba PS1. |
| `thumbnails` | none | `boxarts` zrkadlí celú sadu obalov pre offline pokrytie (~9000 súborov). |
| `bios`, `downloads`, `samples` | yes | Nastavte na `no` pre preskočenie balíčkov BIOS, všetkých stiahnutí alebo vzorových hier. |

**Na existujúcom Raspberry Pi OS Lite** (Bookworm alebo Trixie): skopírujte
`autobleem-rpi.tar.gz` (alebo 64bitový) na Pi, rozbaľte a spustite `sudo bash install.sh`.
Pýta sa na rovnaké otázky, vytváral oddiel dát zmenšením koreňa pri ďalšom štarte
(`--shrink-root <GiB>`) a umiestnite spúšťač na prvú konzolu.

Po inštalácii je **`AUTOBLEEM` oddiel** karty (exFAT) to, čo si vyplníte: vyberte kartu
a otvorte si ju na ľubovoľnom PC alebo si ju skopírujte cez sieť (SSH je zapnuté).
`Games/` pre PS1 hry, `RetroArch/roms/<systém>/` pre ostatné systémy, `System/Bios/` pre
PS1 BIOS (oddiel 3.10), `Themes/` pre motívy.

### 2.3 USB kľúč PC

Rovnaké zariadenie pre ľubovoľný PC, ktorý bootuje z USB - 32bitový systém, takže staršie
stroje tiež fungujú:

1. Stiahnite si `autobleem-<verzia>-pcusb-i386.img.xz` z panela PC a napíšte ho na kľúč 8
   GB alebo väčší s Raspberry Pi Imager (*Use custom*), balenaEtcher alebo Rufus (režim DD).
2. Bootujte PC z kľúča (klávesa menu bootovacieho PC - F12, F8, Esc...). Funguje ako BIOS,
   tak UEFI boot; **Secure Boot musí byť vypnutý**.
3. Prvý boot je rovnaký ako na Pi: otázka na sieť, pokiaľ nie je kábel, otázka na RetroArch,
   potom inštalácia - približne osem minút s drôtenou sieťou - a reštart do spúšťača.

Kľúč potom má oddiel `AUTOBLEEM` pre vaše hry, viditeľný na Windows 10 (1903 a novšie)
ako druhá jednotka, keď zapojíte kľúč do bežiaceho PC. `autobleem.txt` je na prvom oddiele
s rovnakými kľúčmi ako na Pi (žiadny `hdmi_mode` - PC používa nativný režim obrazovky).

### 2.4 Windows

AutoBleem ako program pre Windows: na celú obrazovku, emulátory a RetroArch spustené ako
programy.

1. Stiahnite si **AutoBleemSetup-<verzia>.exe** a spustite ho. Inštaluje sa na používateľa bez
   práv správcu: program pod `%LOCALAPPDATA%\Programs\AutoBleem`, dáta (hry, nastavenia,
   motívy, RetroArch) do priečinka, ktorý si vyberiete - predvolene `Documents\AutoBleem`.
2. Zaškrtnite komponenty - databázy obalov, RetroArch (oficiálny Windows build a jeho jadrá),
   BIOS súbory, vzorové hry - a nechajte pomocníka inštalácie ich stiahnuť.
3. Spustite AutoBleem z ponuky Štart alebo z plochy. Na PC funguje klávesnica ako herný
   ovládač (oddiel 3.2).

Spustenie novšej inštalácie ho aktualizuje a uchováva priečinok dát. Spúšťač tiež jedenkrát
denne kontroluje web a ponúka aktualizáciu, keď je (oddiel 3.11).

<!-- pagebreak -->

## 3. Používanie AutoBleem

### 3.1 Spúšťač

Spúšťač sa otvorí na polici: obaly aktuálnej sady, vybraný v strede, jej detaily vedľa -
vydavateľ, rok, seriálne číslo, oblasť, hráči, kedy bola naposledy hraná - a tlačítko play.
Lišta dole uvádza, čo tlačítka robia. Skenovanie priečinka hier beží na pozadí pri každom
štarte; počas jeho behu sa bublinám vpravo hore zobrazuje jeho priebehu a nové hry sa
objavujú na polici, ako sú nájdené.

![Výber sady: tri karty a skupiny aktuálnej sady s počtom hier](../images/en/set-picker.jpg)

### 3.2 Ovládanie

| Tlačítko | Na polici |
|---|---|
| Vľavo / Vpravo | Predchádzajúca / nasledujúca hra. Držaním sa posúva. |
| L1 / R1 | Preskok na predchádzajúce / nasledujúce prvé písmeno názvu. |
| Cross | Spusti vybranú hru (PS1 hru v emulátor PS1; hru RetroArch v jej jadre; aplikáciu po jej čítaní). |
| Square | Spusti vybranú PS1 hru v RetroArch namiesto toho. |
| Triangle | Sprievodca tlačidlami. |
| Start | Náhodná hra z aktuálnej sady. |
| Select | Výber sady: karty PlayStation / RetroArch / Aplikácie (L1 / R1), skupiny karty (Hore / Dole, L2 / R2 strana), Cross vyberie. |
| Dole | Otvorte rad ikon pod hrou (Nastavenia, Hra, Pamäťová karta, Obnovenie). Hore ju zavrie. |
| L2 + R2 | Systémová ponuka (oddiel 3.4). |

**S klávesnicou** (PC bez herného ovládača alebo USB klávesnica na konzole, Pi alebo PC
kľúči) klávesy stojí: **Šípky** = d-pad, **Enter** = Cross, **Esc alebo Backspace** =
Circle, **Tab** = Triangle, **Space** = Square, **F1 / F2** = Select / Start, **Page Up /
Page Down** = L1 / R1, **Home / End** = L2 / R2, **F10** = systémová ponuka. Na vývojárskom
počítači Esc zavrie program a Space je Start.

V každom zozname a ponuke: Hore / Dole sa pohybujú, **L2 / R2 otáčajú stránky**, L1 / R1
skáčú na prvý / posledný riadok, **Cross vyberie, Circle späť**. Obrazovka s nastaveniami
ich uloží, keď ju opustíte pomocou Circle.

![Rad ikon pod vybranou hrou](../images/en/launcher-icons.jpg)

### 3.3 Sady

**Select** otvorí výber sady. Karta PlayStation uvádza *All Games*, *Internal Games*
(dvadsať zabudovaných hier konzoly na PlayStation Classic), každý priečinok, ktorý ste
vytvorili pod `Games/` (hra v podpriečinku patrí do danej skupiny), *Favorite Games*, *Game
History* a keď je nejaká hra označená ako taká, *Lightgun Games*. Karta RetroArch uvádza
jednu skupinu na systém, ktorý má hry, plus vlastné Favorites a History RetroArch. Karta
Aplikácie skupinuje aplikácie podľa typu: *All apps*, potom *Games*, *Emulators*, *Tools*,
*Media* a *Other* (kategória sa nastavuje v súbore `app.ini` každej aplikácie). Každý riadok
ukazuje, koľko položiek obsahuje; skupina bez čokoľvek sa otvorí na prázdnej polici s radom
ikon ukazujúcim iba Nastavenia.

### 3.4 Rýchla ponuka

**Hore** v spúšťači alebo **ikona ozubeného kolesa** v rade ikon (kde sú Nastavenia / Hra /
Pamäťová karta / Obnovenie): rýchla ponuka pre činnosti, ktoré chcete z kolotoča. Krátky
zoznam: *Re-Scan Games* (spustí skenovanie teraz), *Store* (AutoBleem Store na stiahnutie
rozšírení), *Network & Controllers* (iba kde nainštalované rozšírenie poskytuje položku
`network` - PSC-Bios na konzole, Pi a PC kľúči: WiFi, párovanie Bluetooth, sprievodca
mapovaním herného ovládača - pozri oddiel 6; zašednuté s "zapnite v Rozšíreniach", keď je
rozšírenie vypnuté - Cross otvorí zoznam Extensions) a *System menu...* (úplná ponuka nižšie).
Hore / Dole sa pohybujú (zabalené), Cross vyberie, Circle späť. Tu nie je nič jedinečné -
každá položka je tiež v systémovej ponuke.

### 3.5 Systémová ponuka

**L2 + R2** (spolu, v ľubovoľnom poradí) otvorí systémovú ponuku cez policu. Ponuka je
rozdelená do sekcií:

| Sekcia | Položka | Čo robí |
|---|---|---|
| (hore) | Re-Scan Games | Vyhľadá nové, zmenené alebo odstránené hry teraz (skenovanie tiež sleduje priečinok samo). |
| | Extensions | Rozšírenia na kľúči - AutoBleem Store a ďalšie (oddiel 3.12). |
| **Library** | Game Manager | PS1 hry ako zoznam s ich priečinkami: odstránite hru, vyčistite obaly. Zakázané počas skenovania. |
| | Memory Cards | Vaše sady pamäťových kariet (oddiel 3.7). |
| | Scanner processors | Programy, ktoré každé skenovanie spustí ako prvé - ich poradie, zapnuté alebo vypnuté (oddiel 3.13). Zakázané počas skenovania. |
| **System** | Options | Nastavenia AutoBleem (oddiel 3.6). |
| | Network & Controllers | Iba kde nainštalované rozšírenie poskytuje položku `network` (`Provides=network` v `extension.ini` - PSC-Bios na konzole, Pi a PC kľúči) - WiFi, párovanie Bluetooth ovládača, nastavenie DualShock 3 a sprievodca mapovaním herného ovládača - pozri kapitola 6. Keď je rozšírenie nainštalované, ale vypnuté, táto položka zostáva zašednutá s poznámkou "zapnite v Rozšíreniach" - Cross tam otvorí zoznam Extensions. |
| | Hardware Information | Fakty o počítači: systém, CPU, úložisko, sieťové rozhrania, časové pásmo, displej, herné ovládače a ich mapovanie. Na konzole s jadrom AutoBleem to otvorí PSC-Bios (kapitola 6); na iných počítačoch sa zobrazí táto informačná stránka. |
| | Software Update | (Raspberry Pi a PC) Kontrola webu pre novší AutoBleem alebo RetroArch teraz. |
| | About | Kredity a licencia. |
| **Leave** | RetroArch | Opusti spúšťač pre vlastnú ponuku RetroArch. Zatvorenie RetroArch sa vrátí. |
| | Power Off | Po potvrdení: na konzole pohotovosť AutoBleem - kľúč odpojený, svetlo červené, Power vrátí spúšťač späť (oddiel 2.1); na Pi alebo PC sa počítač vypne. |

![Systémová ponuka](../images/en/system-menu.jpg)

### 3.6 Možnosti

Nastavenia sú v skupinách; Hore / Dole sa pohybujú medzi nimi, Vľavo / Vpravo zmení
hodnotu, Circle opusti a uloží. Každá zmena sa aplikuje okamžite.

| Skupina / nastavenie | Čo robí |
|---|---|
| **Interface**: AutoBleem Theme | Vzhľad. Motívy sú v `Themes/`; zip motívu tam vhodený sa rozbalí pri ďalšej návšteve. Motívy, ktoré AutoBleem dodáva, sa obnovujú s každou aktualizáciou - na prispôsobenie si ich najskôr skopírujte pod novým menom. |
| Cover Style | Rám v priehľade obalu nakresleného okolo obalov PS1. |
| Language | Jazyk spúšťača, aplikovaný okamžite (17 jazykov). |
| Use Font from Theme / Font | Písmo klasických obrazoviek: z motívu alebo ľubovoľného `.ttf`/`.otf` z `resources/fonts`, `RetroArch/fonts` alebo priečinka motívu. |
| Showing Timeout | Ako dlho zostane oznámenie "Showing: ..." viditeľné, v sekundách (0 = navždy). |
| **Sound**: Music, Background Music | Ktorá skladba hrá pod spúšťačom (z motívu alebo súbor z `resources/music`) a či sa hrá nejaká. |
| **Emulation**: PS1 Emulator | `pcsx-abnxt` (predvolený: aktuálny PCSX-ReARMed s rozšíreniami AutoBleem) alebo `pcsx-ab` (klasický). Bod obnovenia uložený v jednom pokračuje v druhom, pokiaľ hra nebeží bez BIOS súboru. |
| Widescreen | Tvar obrázku emulátora PS1 pre každú hru. |
| Play all PSX games with RA | Každá PS1 hra sa spustí v jadre PS1 RetroArch. |
| Update RA Config | AutoBleem zapisuje nastavenia do konfigurácie RetroArch, keď tam spustí hru. |
| **Library**: Show Internal Games | Zabudované hry konzoly v zoznamoch PlayStation (iba PlayStation Classic). |
| Fetch box art online | Skenovanie sťahuje chýbajúce obaly zo serverov libretro (Raspberry Pi, PC, Windows). |
| **Updates** | (Raspberry Pi, PC, Windows) `stable`, `latest` (vrátane predvydaní) alebo `off`. |

![Možnosti v skupinách](../images/en/options.jpg)

### 3.7 Nastavenia hry

S vybranou hrou, **Dole** otvorí jej rad ikon: **Settings** (vyššie uvedené možnosti),
**Game** (vlastné nastavenia hry), **Memory Card** (jej pamäťová karta) a **Resume** (jej
body obnovenia). Cross otvorí ten pod kurzorom.

**Editor hier** ukazuje detaily hry vpravo a jej nastavenia vľavo v troch skupinách:

- **Game**: *Favorite* (v skupiny Obľúbené hry), *Lightgun Game* (hra so svetelnou
  pištoľou - vstúpi do skupiny Lightgun a vždy sa spustí v RetroArch, ktorého jadro PS1
  má GunCon), *Play using RA* (táto hra beží v RetroArch), *Lock data* (skener ponechá
  názov, seriálne číslo a zoznam diskov hry tak, ako ste ich nastavili).
- **Video**: vysoké rozlíšenie, obrazové riadky a ich úroveň, preskočenie snímok, plugin
  GPU a *Filter* - ako sa obrázok škáluje: Off (prosté pixely), Linear (vyhladené) alebo
  Sharp (ostré pixely bez blikania; iba `pcsx-abnxt` - klasický `pcsx-ab` a RetroArch ho
  zobrazujú ako Off).
- **Emulator**: SpeedHack, frekvencia CPU, interpolácia SPU, logo pri spustení (vypnuté
  preskočí shell BIOS - pre homebrew disk, ktorého vlastné logo rozbitý boot), a s
  `pcsx-abnxt` filtr *Smoothing* a prepínač *Sony hacks*.

Triangle premenuje hru, Square zmení jej pamäťovú kartu, Start zdieľa novú kartu. Circle
uloží a opustí.

**Nastavenia uložené v emulátor.** Vlastná ponuka emulátora má *Save settings for this
game*. Keď hra má tam uložené nastavenia, sú to tie, ktoré hrá, a editor hier ukazuje
svoje riadky Video a Emulator zašednuté s týmito hodnotami pod nadpisom *Saved in the
emulator*. Aby ste sa vrátili k nastaveniam editora hier, vyberte **Unlock the settings**
a potvrďte: to zmaže nastavenia, ktoré emulátor uložil, a riadky je možné znova meniť.
Obidva emulátory, `pcsx-ab` a `pcsx-abnxt`, čítajú a zapisujú rovnaké uložené nastavenia.

![Editor hier](../images/en/game-editor.jpg)

### 3.7 Pamäťové karty a body obnovenia

Každá PS1 hra má predvolene svoju pamäťovú kartu (uchovávané s jej bodami obnovenia v
`Games/!SaveStates/<priečinok hier>/`). **Memory Cards** v systémovej ponuke spravuje
**zdieľané sady** - kartu, ktorú používa viac hier, uchovávané v `Games/!MemCards/`:
vytvorte jednu (Square, s klávesnicou na obrazovke), premenujte (Cross), odstránite
(Triangle). Hra sa umiestnila do sady s *Change memory card* v jej editore alebo z jej
ikony Memory Card.

**Editor pamäťovej karty** (ikona Memory Card) ukazuje kartu hry a druhú kartu vedľa seba,
s každou ikonou uloženej hry a nadpisom: skopírujte ulozenu hru medzi týmito dvoma
(Square), odstránite jednu (Triangle), defragmentujte kartu (Select). Start vymeňuje kartu
vpravo za inú sadu.

![Editor pamäťovej karty](../images/en/memory-card-editor.jpg)

**Body obnovenia**: keď opustíte PS1 hru tlačítkom Reset konzoly (alebo ponukou
emulátora na Pi alebo PC), AutoBleem uloží bod obnovenia, kde ste boli a ponúka ho pod
ikonou **Resume** - štyri pozície, každá s obrázkom okamžiku. Cross pokračuje z pozície,
Triangle ju odstráni. Hra s bodom obnovenia ukazuje malý obrázok na ikone Resume.

### 3.8 Spúšťanie hier, RetroArch a aplikácií

**Cross** spustí vybranú hru. PS1 hra beží vo vybranom emulátore PS1 (oddiel 3.5), na
celú obrazovku, dokiaľ ju neopustíte - na konzole predným tlačítkom **Reset** (späť do
spúšťača s bodom obnovenia) alebo **Power** (konzola sa vypne); na Pi alebo PC cez ponuku
emulátora počas hry (Select + Start na hernom ovládači alebo Esc na klávesnici). **Square**
spustí PS1 hru v RetroArch namiesto toho.

Hra **RetroArch** sa spustí v RetroArch s jadrom, ktoré si spúšťač vybral pre svoj
systém; *Close Content* alebo *Quit RetroArch* v jeho ponuke sa vrátia do spúšťača.
Položka RetroArch v systémovej ponuke otvorí vlastnú ponuku RetroArch (XMB) bez
nič nahrané, pre jej nastavenia a vlastné zoznamy obsahu.

Aplikácia (**Apps** sada: nástroje konzoly a na konzole dodatočné aplikácie, ktoré balík
RetroArch prináša - Doom, Quake, Amiga, ...) najskôr zobrazí svoje čítanie; Cross ju
spustí, Circle sa vrátí.

![Čítanie aplikácie pred jej spustením](../images/en/app-start.jpg)

### 3.9 Pridávanie hier

**PS1 hry** idú do priečinka `Games`, **jeden priečinok na hru**, pomenovaný po hre:

```
Games/
  Crash Bandicoot/            Crash Bandicoot.cue + Crash Bandicoot.bin
  Final Fantasy VII/          Final Fantasy VII (Disc 1).chd, (Disc 2).chd, (Disc 3).chd
  Platformers/                priečinok hier: skupiny vo výbere sady
    Klonoa/                   Klonoa.pbp
```

- Formáty: `.cue` + `.bin` (alebo `.img`), `.pbp`, `.chd` (tiež zstd), `.ecm` (dekódovaný
  skenovaním), `.iso`. Zipped hra tiež funguje: procesor **Unzip** ho rozbalí pred
  skenovaním (oddiel 3.13).
- Hra s viacerými diskami je jeden priečinok so všetkými diskami v ňom; priečinky s názvom
  `Game (Disc 1)`, `Game (Disc 2)` ... sa skenovaním spoja do jedného priečinka `Game`.
- Hry vhodené priamo do `Games/` (voľné súbory) sú skenovaním zoradeným do priečinkov.
- **Obal** je PNG vedľa obrázku hry, pomenovaný ako on. Bez jedného obalu pochádza z
  databáz obalov alebo - s nainštalovaným RetroArch - z thumbnailuv libretro; na Pi, PC
  alebo Windows chýbajúci je stahovaný online (Options → *Fetch box art online*).
- Skenovanie číta seriálne číslo každého disku a vezme názov, vydavateľa, rok, hráčov a
  oblasť z databázy PlayStation RetroArch alebo databáz obalov. Zmeňte čokoľvek v editore
  hier a zaškrtnite *Lock data*, aby ste to ponechali.

**Ostatné systémy** idú pod `RetroArch/roms/`, **jeden priečinok na systém, pomenovaný
ako sú pomenované databázy RetroArch** (priečinok sa vytvorí za vás): `Nintendo - Nintendo
Entertainment System`, `Nintendo - Super Nintendo Entertainment System`, `Sega - Mega Drive
- Genesis`, `Nintendo - Game Boy Advance`, `FBNeo - Arcade Games` (alebo `Arcade`), ...
Romky môžu zostať zipped. Na Pi, PC alebo Windows skenovanie ich číta samo a zapisuje
zoznamy RetroArch; na konzole si ich spustite **UpdateRoms** na PC (kapitola 5).

**Aplikácie** idú pod `Apps/<meno>/` s `app.ini` (menom, ikonou, čo spustiť) a `run.sh`.

**Motívy** idú pod `Themes/<meno>/` (`theme.json` a obrázky) - alebo vhoďte zip motívu do
`Themes/`.

### 3.10 PS1 BIOS

Na **PlayStation Classic** emulátor používa vlastný BIOS konzoly. Na **Raspberry Pi, PC a
Windows** vložte svoju PS1 BIOS do `System/Bios/`: `romw.bin` (US/European SCPH-5501/5502)
a `romJP.bin` (Japanese SCPH-5500). Inštalátory ich vyplní z balíčkov RetroArch BIOS, ak
tam už nie sú vaše vlastné súbory. Bez nich emulátor beží na svojej zabudovanej HLE BIOS,
ktorú mnohé hry tolerujú a niektoré nie.

### 3.11 Aktualizácie

- **Raspberry Pi, PC kľúč, Windows**: spúšťač kontroluje web pri štarte a raz denne
  (Options → *Updates* je kanál; *Software Update* v systémovej ponuke kontroluje teraz).
  Keď je novší AutoBleem alebo RetroArch, pýta sa: *Update now* stiahne všetko a znova
  spustí inštalátor s obrazovkou prvého štartu; *Remind me tomorrow* a *Skip this version*
  sú ostatné odpovede. Vaše hry a nastavenia zostanú; spúšťač sa znova proskenuje po
  aktualizácii.
- **PlayStation Classic**: spustite novší `AutoBleemInstaller.exe` cez kľúč (oddiel 2.1).

### 3.12 Rozšírenia a AutoBleem Store

**Rozšírenia** pridávajú svoje vlastné obrazovky do spúšťača. Sú v `Extensions/<meno>/`
na kľúči (na Raspberry Pi jeho oddiel dát, na Windows priečinok dát); na inštaláciu jedného
ho rozbaľte tam. **L2 + R2 → Extensions** ich uvádza: Cross spustí jednu, Triangle ju
vypne alebo znova zapne. Rozšírenie, ktoré potrebuje sieť, sa nespustí bez jednej a
rozšírenie, ktoré zastavilo spúšťač, sa vypne - zoznam to povie.

![Zoznam rozšírení](../images/en/extensions.jpg)

**AutoBleem Store** je prvé rozšírenie: aplikácie a hry na inštaláciu jedným stlačením, na
všetkých systémoch, na ktorých AutoBleem beží (PlayStation Classic potrebuje WiFi jadra
AutoBleem). Jeho štyri karty, L1 / R1 medzi nimi:

- **Apps** a **Games**: čo zdroje ponúkajú, každá s obrázkom, verziou, veľkosťou a
  favikonu zdroja. Nainštalované položky sú zašednuté. Cross inštaluje (alebo aktualizuje
  alebo skúsa znova po selhání), Triangle odstráni, čo Store nainštaloval. L2 / R2 alebo
  Vľavo / Vpravo otáčajú stránky, **Select** ukazuje jeden zdroj naraz, **Start** hľadá
  názvy. Obrázky položiek sú cachované a možno ich skúsiť znova, ak sa nezaložia.
- **Downloads**: čo sa stahuje, čaká, zlyhalo alebo inštaluje. Lišta priebehu sa neustále
  aktualizuje. Sťahovanie beží na pozadí tiež po opustení Store; spusti hru alebo vypnutie
  ho len pozastaví a zastavené sťahovanie pokračuje tam, kde sa zastavilo. Nainštalovaná
  hra sa objaví na polici po ďalšom skenovaní s obrázkom Store ako jej obal. Sťahovanie
  cez 2 GB funguje na všetkých platformách, vrátane 32bitových buildov.
- **Sources**: odkiaľ zoznamy pochádzajú - vlastný katalóg AutoBleem, TSV zoznam vhodený
  do `System/Extensions/store/sources/` a adresy, ktoré si pridáte s **Add a source URL**.
  Každý zdroj ukazuje svoju favikonu v zozname. Cross na tom, ktorý ste pridali, ho
  premenuje, zmení jeho adresu, prepne ho medzi `http://` a `https://` alebo ho odstráni.

![Karta Apps Store](../images/en/store-apps.jpg)

![Ponuka zdroja](../images/en/store-source-menu.jpg)

To, čo katalóg AutoBleem ponúka, je tiež uvedené na webe ke stiahnutiu,
`https://autobleem.retromenele.pl/store/`. **Ste zodpovední za to, čo obsahujú zdroje,
ktoré si pridáte.**

**Vaše vlastné hry vo vašej sieti**: `abstored`, server Store v LAN, obsluhuje priečinok
PS1 hier do Store v rovnakej sieti. Beží na ľubovoľnom Linux počítači - Raspberry Pi,
domáci server - a iba číta priečinok. Spustite ho s `abstored <priečinok hier>`, otvorte
`http://<ten počítač>:8124/` v prehliadači, aby ste videli, čo obsluhuje a ľubovoľné
problémy, ktoré našiel a pridajte `http://<ten počítač>:8124/store.tsv` ako zdroj. Hotové
programy pre Linux a Windows sú na stránke Store v záložke **LAN server**; nastavenie ako
službu je `INSTALL-linux.md` (`ext_store/server/` vo zdroji). **LAN Share** (oddiel 5.2)
dá hry a disky z PC na takový server.

### 3.13 Procesory skenovania

**Procesory skenovania** sú malé programy, ktoré každé skenovanie spustí ako prvé, než
prečítajú vaše hry. Jeden môže zmeniť formát, ktorý AutoBleem nečíta, na ten, ktorý číta -
napríklad zipped hru - alebo zmeniť dáta hry, napríklad preloženú opravy. Sú v
`System/Processors/<meno>/` na kľúči (na Raspberry Pi jeho oddiel dát, na Windows priečinok
dát); na inštaláciu jedného ho rozbaľte tam. Ďalšie skenovanie ho spustí.

- **Unzip prichádza s AutoBleem**: rozbalí zipped PS1 hry v `Games/` pred skenovaním ich
  čítajú a zipped ROM jednotlivo (arkádové sady ostávajú zipped). Aktualizácia AutoBleem
  ho aktualizuje a ponechá ho vypnutého, ak ste ho vypnuli.
- Procesor, ktorý sa už zaoberaní hrou, sa na nej nemusí znova spustiť, kým sa hra nemení.
- Zatiaľ čo procesor pracuje, bublinám vpravo hore sa zobrazuje, čo robí; upozornenie alebo
  selhanie sa objaví na riadku pod ním. `processors.log` v priečinku logu má detaily.
- Spustenie hry alebo RetroArch zastaví procesor, ktorý zmení súbory; ďalšie skenovanie
  dokončí svoju prácu.

**L2 + R2 → Scanner processors** ich uvádza v poradí, v akom bežia, jednu kartu pre PS1
hry a jednu pre ROM (L1 / R1). **Square** vezme procesor a Hore / Dole ho posunú - poradie
je dôležité: procesor, ktorý rozbaluje, musí byť pred tým, ktorý opravuje to, čo bol
rozbalený. **Cross** ho vypne alebo zapne, **Triangle** ho nechá podívať sa znova na
každú hru pri ďalšom skenovaní, **Circle** sa vrátí a spustí skenovanie, ak ste niečo
zmenili. Procesor postavený pre iný počítač zostane v zozname, zašednutý.

![Procesory skenovania](../images/en/processors.jpg)

Písanie vášho: stránka Unzip, `https://github.com/autobleem2/proc_unzip`, vysvetľuje
všetko, čo procesor musí robiť a `tools/proc_check.py` vo zdroji AutoBleem ho skontroluje
pred jeho zdieľaním.

<!-- pagebreak -->

## 4. Obrazovky

### 4.1 Správca hier

PS1 hry ako zoznam s ich priečinkami a vybrané obálky a detaily. Cross otvorí editor
hier, **Square odstráni hru** (jej priečinok a po druhej otázke jej body obnovenia),
Triangle odstráni každý PNG obalov vedľa hier (skenovanie ich vezme z databáz znova),
L2 / R2 listujú. Voľné miesto jednotky je vpravo hore. Správca hier čaká na dokončenie
skenovania.

![Správca hier](../images/en/game-manager.jpg)

### 4.2 Informácie o hardvéri

Fakty o počítači - systém, hardvér, úložsko s jeho voľným miestom, sieťové rozhrania,
displej a zvukový ovládač, pripojené herné ovládače - znovu čítané každú sekundu. Na
PlayStation Classic s jadrom AutoBleem sa táto položka otvorí **PSC-Bios** namiesto
(kapitola 6).

Prvé dva ovládače sú zobrazené ako Hráč 1 a Hráč 2 – porty, ktoré im emulátor PS1 priradí.
Akýkoľvek ďalší ovládač je zobrazený ako nepoužitý emulátorom PS1. RetroArch priradí ovládače
podľa svojich vlastných nastavení a môže ich usporiadať inak. Keď je ovládač pripojený alebo
odpojený, spúšťač krátko zobrazuje, ktorý pad je Hráč 1 a Hráč 2.

![Informácie o hardvéri](../images/en/hardware-info.jpg)

### 4.3 Sprievodca tlačidlami

Triangle na polici: každé tlačítko každej obrazovky na jednej stránke. Keď je pripojená
USB klávesnica alebo bola používaná, stĺpec Keyboard ukazuje klávesy vedľa tlačidiel
herného ovládača.

![Sprievodca tlačidlami](../images/en/button-guide.jpg)

### 4.4 Klávesnica na obrazovke

Kdekoľvek sa píše text - sada pamäťovej karty, názov hry, heslo WiFi, adresa zdroja -
rovnaká klávesnica, rozložená ako klávesnica telefónu: písmená, stránka symbolov (`/ \ : ?
& = % @ #` a zvyšok, čo adresa alebo heslo potrebuje) a dve stránky akcentovaných písmen,
so Shiftom, kľúčom stránky, Space, Backspace a Confirm na spodnom riadku. Smery sa
pohybujú, Cross píše, Triangle maže, Square je medzera, **L1** je Shift (dvakrát pre
caps lock), **R1** nasledujúca stránka, **L2 / R2** posúvajú kurzor, Start potvrdzuje,
Circle zrušuje. USB klávesnica píše kedykoľvek: Enter potvrdzuje, Esc zrušuje.

![Klávesnica na obrazovke](../images/en/keyboard.jpg)

<!-- pagebreak -->

## 5. Na PC

### 5.1 UpdateRoms - obnovenie kľúča konzoly

PlayStation Classic nemá sieť, takže zoznamy RetroArch a obaly jej kľúča sa robia na PC:
**UpdateRoms** robí na PC to, čo skenovanie spúšťača robí na Pi, so sieťou PC a cestami
konzoly, takže konzola bootuje a nájde všetko na mieste.

1. Skopírujte svoje ROM na kľúč pod `RetroArch/roms/<systém>/` (oddiel 3.9). Názvy
   priečinkov musia byť názvy databáz RetroArch; inštalátor si vytváril čiasty.
2. Spustite **`UpdateRoms\UpdateRoms.exe` z kľúča** (inštalátor ho tam umiestnila).
   Nájde kľúč z miesta, kde sedí, ukazuje riadok fázy, lištu priebehu a záznam a:
   - stiahuje balík databázy RetroArch, keď kľúč nemá a identifikuje každú ROM podľa
     neho - hra, ktorú databáza pozná, dostane správny názov;
   - zapisuje jeden zoznam prehrávame na systém do `RetroArch/bin/playlists/` s cestami
     konzoly, ponechávajúc cokľvek, čo tam RetroArch sám pridal;
   - stahuje obaly každej ROM, ktorá nemá žiadny, zo serverov miniatúr libretro do
     `RetroArch/bin/thumbnails/`.
3. Bezpečne vyberte kľúč a vložte ho späť do konzoly. Karta RetroArch výberu sady uvádza
   každý systém, ktorý má hry.

Spustite ho znova po zmene priečinkov ROM; priečinok, v ktorom sa nič nezmenilo, sa
preskočí, takže opätovné spustenie je rýchle. Záznam je `System/Logs/updateroms.log`.
Karta Pi v čítačke kariet môže byť obnovená rovnakým spôsobom (`UpdateRoms.exe <jednotka>
--target rpi`), aj keď Pi to robí sama, keď má sieť.

### 5.2 LAN Share - vaše hry a disky na serveri vašej siete

**LAN Share** (`LanShare.exe`, na stránke Store v záložke **LAN server**) dá vaše PS1
hry na server Store vašej domácej siete - `abstored` na Raspberry Pi, NAS alebo iný PC
- a číta PS1 disk v jednotke CD/DVD PC. Store na konzole, Pi alebo PC ho potom
inštaluje odtiaľ. Nič sa neinstaluje; nastavenia sú uchovávané v `%LOCALAPPDATA%\AutoBleem
LAN Share\`.

![Okno LAN Share](../images/en/lanshare.jpg)

1. **Server**: zadajte jeho adresu (`http://<jeho adresa>:<port>`, ako to má Store) a
   stlačte **Connect**. Jeho hry a ľubovoľné problémy, ktoré jeho skenovanie našlo, sú
   uvedené vľavo. Chcete-li tam dať hry, dejte jedno z:
   - **Share** - priečinok hier servera, ako je zdieľaný v sieti (Samba), napr.
     `\\raspberrypi\games`: LAN Share ho tam kopíruje a požiada server o skenovanie.
     Server sám zostáva iba na čítanie.
   - **Token** - keď bol server spustený s `--allow-uploads`: jeho token (server ho
     vytlačí pri štarte a ponechá ho v `<stav>/upload-token`). LAN Share nahrá cez HTTP
     a zastavené nahranie pokračuje tam, kde sa zastavilo.
2. **Hry na tomto PC**: vyberte priečinok hier (jeden priečinok na hru), zaškrtnite hry
   a stlačte **Publish the ticked games**. **Na serveri** hovorí, či má server hru už
   (podľa seriálneho čísla, inak podľa názvu); taká hra sa nikdy nepošle dvakrát. **Tick
   those not on the server** zaškrtne zvyšok.
3. **Disk**: vložte PS1 disk do jednotky a stlačte **Read a disc and publish it**. Disk
   sa prečíta celý do `.bin` + `.cue` (a `.sbi` pre LibCrypt hru, keď jednotka dá
   podkanál), pomenovaný podľa jeho názvu, skontrolovaný proti známemu správnemu výpisu
   (keď sú databázy vybrané) a zverejnený. Pre hru na niekoľkých diskoch zaškrtnite **The
   game has more than one disc**: LAN Share si požiada každý ďalší disk a zverejní ich
   spolu ako jednu hru.
4. **Remove from the server...** vezme vybrané hry zo servera. Nič nie je vymazané: každá
   sa presunie do priečinka `.removed` vedľa hier servera a jej premiestnenie ju vrátí
   späť.

**Databázy** - priečinok obalov AutoBleem (`coversU/P/J.db`) a RetroArch `Sony -
PlayStation.rdb` - dávajú názvy a kontrolu prečítaného disku; obidve sú voliteľné.
**Also share the games on this PC with the Store** (predvolene vypnuté) obsluhuje priečinok
na tomto PC pre Store priamo. Prvý krát sa Windows pýta na svoj firewall: povolte iba
privátne siete.

<!-- pagebreak -->

## 6. Nástroje konzoly (PlayStation Classic)

Dva nástroje pre kľúč PlayStation Classic. Oba kreslia v motíve spúšťača a jazyku a oba
sú riadené herným ovládačom - a v sprievodcovi herného ovládača tlačidlami konzoly.
**PSC-Bios** je rozšírenie, ktoré prichádza s balíkom konzoly: *Hardware Information*
v systémovej ponuke ho otvorí a je v zozname rozšírení. **ABFlashKit** je aplikácia
v sade aplikácií.

### 6.1 PSC-Bios

Rozšírenie, ktoré prichádza s balíkom konzoly, tiež dostupné na Raspberry Pi a PC kľúči.
Otvorí sa z položky *Network & Controllers* v systémovej ponuke (alebo zo zoznamu
rozšírení). Keď je rozšírenie nainštalované, ale vypnuté, položka *Network & Controllers*
v Rýchlej ponuke a Systémovej ponuke zostáva zašednutá s poznámkou "zapnite v Rozšíreniach"
- Cross tam otvorí zoznam Extensions u nej.

Otvárajúca sa obrazovka ukazuje fakty o počítači: čas, časové pásmo, adaptéry WiFi/
Ethernet/Bluetooth s ich adresami a každý pripojený herný ovládač s tým, či má mapovanie
tlačidiel. Sieť a časti Bluetooth potrebujú jadro AutoBleem na konzole (oddiel 6.2) alebo
systémové nástroje na Raspberry Pi / PC kľúči; sprievodca herného ovládača funguje na
ľubovoľnom systéme.

![PSC-Bios: Centrum Siete a ovládačov](../images/en/pscbios-main.jpg)

- **Select - Wi-Fi Network** (jadro alebo NetworkManager): názov siete (napísaný alebo
  vybraný zo skenovania), heslo, režim ovládača a *Apply / Restart Network*. Časové pásmo
  je tiež nastavené tu. IP adresa konzoly sa zobrazí po pripojení.
- **Square - Bluetooth Controllers**: skenovanie pre Bluetooth herné ovládače (DualShock
  4, atď.), na párovanie alebo odstránenie.
- **L1 - DualShock 3 Pairing**: pripojenie iba USB pre prvý DualShock 3 cez plugin sixaxis
  jadra.
- **R1 - Controller Mapping**: sprievodca mapovaním (nižšie).
- **Triangle - About**, **Circle - späť** do spúšťača.

**Sprievodca herného ovládača** ukazuje pripojený herný ovládač surový - každá os, tlačítko
a klobúk ako čísla a obrázok DualShock, ktorý sa rozsvietí, keď stlačíte. Pretože testovaný
herný ovládač je neveriaci, sprievodca je riadený **tlačidlami konzoly**: **RESET** prepne
na ďalší herný ovládač, **OPEN** spustí mapovanie (potom odpovie na každú otázku - stlačte
tlačítko osvetlené na obrázku alebo OPEN keď herný ovládač nemá takové tlačítko), **POWER**
zrušuje alebo opúšťa. Držanie Circle na hernom ovládači po dobu 2 sekúnd opúšťa sprievodcu
(lišta sa vyplní a nápoveda päty hovorí "Drž 2 s: Exit"). Zatiaľ čo herný ovládač nemá
mapovanie, držanie ľubovoľného tlačítka po dobu 2 sekúnd to robí ("Drž ľubovoľné tlačítko
2 s: Exit"). Krátke stlačenie sa mapuje ako obvykle. Na klávesnici sú Esc / Space / Enter
v mieste POWER / RESET / OPEN. Na konci sa nové mapovanie pridá na test a OPEN ho uloží
pod menom vašej voľby; spúšťač ho od tým načítava.

![PSC-Bios: Sprievodca mapovaním herného ovládača](../images/en/pscbios-wizard.jpg)

### 6.2 ABFlashKit - jadro AutoBleem

Jadro AutoBleem je voliteľný klon jadra Linux konzoly: prináša fungujúce hodiny, USB WiFi
a Bluetooth dongly (pre PSC-Bios a Bluetooth herné ovládače) a podporu tlačidiel na
prednej strane, ktorú emulátor používa na body obnovenia. ABFlashKit ho inštaluje,
najskôr konzolu zazálohuje a môže ju vrátiť na stock prostredníctvom vlastného obnovenia
Sony.

> **Tento nástroj zapisuje do flash pamäti konzoly.** Flash, ktorá je prerušená - napájanie
> prerušené, kľúč vytrhnutý - môže konzolu nechať nespúšťajúcu sa a inštalácia vlastného
> jadra ruší záruku. Konzolu udržujte zapnutú a kľúč v nej, kým sa sám nereštartuje.
> ABFlashKit sa otvorí s touto výstrahou; *I understand* ide ďalej, *Quit* opúšťa.

![Menu ABFlashKit](../images/en/abflashkit-menu.jpg)

- **Flash Kernel**: vytváral zálohový klon oddielov konzoly na kľúč (`LBOOT.EPB`), ak
  existuje žiadny, kontroluje ho a obrázok jadra, zapisuje jadro a systémové súbory
  AutoBleem a restartuje. *All done - keď sa obrazovka stane čiernou, odpojte napájací
  kábel*: vytiahnite napájanie konzoly a zapojte ho späť.
- **Full backup**: všetky štyri oddiely do `LBOOT.EPB` pre neskoršie obnovenie (predchádzajúca
  záloha sa prepíše po otázke).
- **Restore Mode**: kontroluje, že záloha je stock, nastavuje príznak obnovenia a
  restartuje do obnovenia Sony, ktoré obnovuje konzolu z `LBOOT.EPB` na kľúči - spôsob
  návratu na stock firmware.

Lišta priebehu pod každým krokom ukazuje, ako ďaleko je akcia. Nástroj odmietne blesknúť
konzolu, ktorá beží iný vlastný firmware (BleemSync, Project Eris): najskôr ju obnovte
do stock.

<!-- pagebreak -->

## 7. Ak sa niečo pokazí

- **Logy**: AutoBleem uchovává svoje logy v pamäti, takže kľúč nie je písaný neustále -
  dosiahnú `System/Logs/` na kľúči, karte alebo priečinku dát iba keď sa niečo pokazilo:
  pád spúšťača, PS1 hry alebo RetroArch ich uloží do `System/Logs/crash-<n>/` (posledné
  tri sú udržiavané) a spúšťač to povie raz, keď sa vrátí. Na uchovávanie všetkých logov
  zapnite *Options -> Diagnostics -> Keep logs on the stick* (od ďalšieho startu) alebo
  vytvorte prázdny súbor `System/Logs/keep` na PC. Na Pi alebo PC *Hardware Information*
  zobrazuje, kde sú logy a Square ich uloží do `System/Logs/saved-<n>/`. Súbory:
  `autobleem.log` (spúšťač), `launch.log` a `pcsx.log` (štart PS1 hry a výstup
  emulátora), `retroarch.log` a - vždy na kľúči - `update.log` (online aktualizácia) a
  `updateroms.log` (UpdateRoms).
- **Hra nie je na polici**: skontrolujte rozloženie priečinka (jeden priečinok na hru,
  formáty obrázkov z oddelu 3.9). *Game Manager* uvádza priečinky, ktoré skenovanie
  odmietlo po hrách, označené *Not added*, s dôvodom; Square takový priečinok odstráni.
  Re-Scan v systémovej ponuke znova spustí skenovanie.
- **Žiadne obaly**: databázy obalov neboli nainštalované (znova spustite inštalátor
  s nimi zaškrtnuto) alebo pre hry RetroArch na konzole nebol spustený UpdateRoms na PC.
- **Herný ovládač nič nerobí alebo má tlačítka zmiešane**: sprievodca herného ovládača
  PSC-Bios (konzola) ho mapuje; na Pi alebo PC stránka Hardware Information ukazuje,
  čo SDL vidí.
- **Konzola ukazuje čiernu obrazovku po hre**: AutoBleem si znova sám postaví svoje okno
  (až trikrát); ak ostáva čierna, podržte tlačítko napájania a zapnite konzolu znova.
- **Raspberry Pi**: `Alt+F2` dá prihlásiť výzvu na druhej konzole; SSH je zapnuté od
  prvého štartu. `sudo journalctl -u autobleem` ukazuje službu spúšťača; `sudo systemctl
  restart autobleem` ju restartuje. Prvý boot, ktorý nemohol skončiť (žiadna sieť), sa
  skúša znova pri ďalšom štarte.
- **Windows**: `Esc` opustí spúšťač; priečinok dát je ten vybraný v inštalácii
  (`Documents\AutoBleem` predvolene), logy sú v `System\Logs`.

AutoBleem je slobodný software (GNU GPL v3 alebo novší), bez záruky. Podpora a noviny:
server Discord prepojený na obrazovke About a https://autobleem.retromenele.pl/.

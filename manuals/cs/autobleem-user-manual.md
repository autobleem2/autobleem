# Uživatelský manuál AutoBleem 2

AutoBleem 2 je spouštěč her pro **PlayStation Classic** - a od verze 2 také pro
**Raspberry Pi**, **PC spouštěný z USB disku** a **Windows**. Zobrazuje vaše PS1 hry
jako polici s obálkami a detaily her, spouští je v zabudovaném emulátoru PCSX a
s nainstalovaným RetroArch také hraje hry dalších systémů. Tento manuál se zabývá
instalací na každou platformu, každodenním používáním a nástroji, které s AutoBleem
přicházejí.

> Stažení pro každou platformu jsou na **https://autobleem.retromenele.pl/**. Stránka je
> organizována podle platformy: panel *Install* každé z nich je to, co si stáhnete;
> sekce *Build inputs* pod ním je to, co si instalátoři stáhnou sami.

## 1. Co dostanete

- **Spouštěč** - kolotoč s obálkami, sady (PlayStation, RetroArch, aplikace), detaily
  her, systémová nabídka, možnosti, nástroje pro paměťové karty a body obnovení. Stejný
  program na všech platformách.
- **Dva emulátory PS1** - `pcsx-abnxt`, ten aktuální (výchozí), a `pcsx-ab`, klasický
  emulátorský balíček AutoBleem. Vybíráte si jeden v možnostech; oba používají stejné
  nastavení a paměťové karty.
- **RetroArch** (nepovinný na každé platformě) pro ostatní systémy: NES, SNES, Mega Drive,
  Game Boy, arkádu a mnoho dalších. AutoBleem si vytváří seznamy RetroArch z ROM, které si
  kopírujete, a spouští každou hru se správným jádrem.
- **Nástrojů konzole** (pouze PlayStation Classic): *PSC-Bios* pro WiFi, hodiny a mapování
  gamepadu a *ABFlashKit* pro instalaci jádra AutoBleem.
- **UpdateRoms** pro Windows: obnovuje seznamy RetroArch a obálky her na USB disku konzole na
  PC, protože konzola sama nemá síť.

![Spouštěč: Police s obálkami, detaily vybrané hry, nápověda tlačítek](../images/en/launcher.jpg)

<!-- pagebreak -->

## 2. Instalace

### 2.1 PlayStation Classic

Potřebujete PC s Windows, USB disk (USB 2.0, 8 GB nebo více; instalátor ho na požádání
naformátuje) a konzolu. AutoBleem běží z disku bez změn konzole. Disk musí být **FAT32**
pro konzolu bez AutoBleem jádra - její jádro neumí čtít exFAT. Jen konzola s
nainstalovaným jádrem AutoBleem (ABFlashKit, kapitola 6) také bootuje z exFAT disku,
což překonává limit FAT32 4 GB.

1. Stáhněte **AutoBleemInstaller-<verze>.zip** z panelu PlayStation Classic na webu a
   rozbalte jej kdekoli. Obsahuje `AutoBleemInstaller.exe` a balíček AutoBleem, který
   instaluje.
2. Zapojte disk a spusťte `AutoBleemInstaller.exe`. Vyberte disk nahoře. Zaškrtněte, co
   chcete:
   - **Format the stick** - pouze pro nový disk (vše na něm se smaže). Vyberte FAT32,
     pokud konzola nemá jádro AutoBleem.
   - **Cover databases** - obálky a detaily knihovny PS1 (zaškrtnuto standardně; přibližně
     300 MB).
   - **RetroArch** - RetroArch s jádry, dalšími aplikacemi (Doom, Quake, Amiga, ...) a
     assets libretro pro hry dalších systémů. Standardně vypnuto; lze přidat později
     spuštěním instalátoru znovu.
   - **BIOS files** - BIOS soubory, které jádra RetroArch potřebují (vyžaduje RetroArch).
   - **Sample games** - několik volných homebrew her, aby police nebyla prázdná.
3. Stiskněte **Install** a čekejte. Lišty průběhu a záznam ukazují každý krok; disk je
   na konci pojmenován `SONY` a na něj se umístí `UpdateRoms` (viz kapitola 5).
4. Bezpečně disk vyjměte, zapojte jej do **druhého USB portu** konzoly (pravého, hráč 2)
   a zapněte konzolu. AutoBleem se spustí místo standardní nabídky.

**Zapnutí a vypnutí.** S diskem uvnitř se konzola spustí, několik sekund bliká
(AutoBleem se vyzvedává) a pak jde do režimu standby dříve, než se cokoliv zobrazí -
to je vlastní způsob konzoly, jak připravit aktualizaci, což je jak se AutoBleem spustí.
Stiskněte **Power** jednou a spouštěč se objeví. *Power Off* v systémové nabídce nebo
tlačítko Power na konzole vloží konzolu do **standby AutoBleem**: disk se nejdřív
odpojí, pak se světlo změní na **červené** - znamení, že AutoBleem funguje správně - a
další stisk Power vrátí spouštěč zpět, v několika sekundách. **Když je světlo červené,
disk můžete vyjmout** a dát do PC bez toho, aby Windows žádalo jeho kontrolu; vrátíte ho
zpět před stisknutím Power. Odpojení napájení konzoly projde bootovacím standby znovu
příště.

Chcete-li **aktualizovat** disk, spusťte novější instalátor přes něj: vaše hry, uloženou
hru, nastavení a obsah RetroArch zůstanou; pouze AutoBleem vlastní soubory se
nahradí. Disk vytvořený s AutoBleem 1.0 nebo AutoBleem-NG se automaticky přenese do nového
rozložení.

> Konzola bez AutoBleem jádra nemá hodiny ani síť: data se zobrazují pouze po instalaci jádra
> AutoBleem (kapitola 6) a obálky her RetroArch pocházejí z UpdateRoms na PC (kapitola 5).

Hry se umístí do složky `Games` disku - jednu složku na hru - viz oddíl 3.9 pro rozložení.

### 2.2 Raspberry Pi

AutoBleem změní Pi na malou konzolu: bootuje se přímo do spouštěče bez desktopového
prostředí. Dva hotové obrázky jsou na webu - 32bitový a 64bitový - plus tarball pro
stávající Raspberry Pi OS Lite.

| Model | 32bitový obrázek | 64bitový obrázek | Poznámky |
|---|---|---|---|
| Raspberry Pi 5 | ano | ano | |
| Raspberry Pi 4 Model B, Pi 400 | ano | ano | |
| Raspberry Pi 3 Model B / B+ / A+ | ano | ano | v pořádku pro spouštěč a PS1 |
| Raspberry Pi Zero 2 W | ano | ano | 512 MB paměti: PS1 běží, těžší jádra RetroArch ne |
| Raspberry Pi 2 Model B | ano | jen v1.2 | pomalé pro cokoli 3D |
| Raspberry Pi 1, Zero, Zero W | ne | ne | ARMv6 - žádný obrázek neběží |

**32bitový obrázek je doporučený** pro PS1 hry: `pcsx-ab` má rychlý ARM recompiler pouze
pro 32bitový, takže 64bitový build spouští PS1 hry pomaleji. 64bitový obrázek má větší
sadu jader RetroArch.

**S Raspberry Pi Imager:**

1. Nainstalujte Raspberry Pi Imager (raspberrypi.com/software). V *Choose OS* vyberte
   *Use custom* a stažený `autobleem-<verze>-rpi-armhf.img.xz` (32bitový) nebo
   `-arm64.img.xz` (64bitový) - nebo přidejte URL úložiště
   `https://autobleem.retromenele.pl/rpi-imager/os_list.json` v nastavení aplikace a
   vyberte AutoBleem ze seznamu.
2. Pomocí Imager obrazovky přizpůsobení (ozubené kolo nebo otázka po *Next*) nastavte
   **jméno uživatele a heslo, WiFi síť a zemi a zapněte SSH**. AutoBleem potřebuje síť
   při prvním startu.
3. Napište kartu, vložte ji do Pi s obrazovkou a klávesnicí nebo gamepadu připojeným a
   zapněte ji.

**První boot** trvá 5 až 25 minut a na obrazovce ukazuje, co dělá. Bez sítě se
ptá na jednu (seznam WiFi, heslo), pak se ptá, zda instalovat RetroArch (minuta bez
odpovědi znamená ano), zvětšuje systémový oddíl, vytváří oddíl `AUTOBLEEM` z
zbytku karty, instaluje RetroArch a jeho jádra, BIOS balíčky a vzorové hry a restartuje
do spouštěče.

Odpovědi lze zadat předem v **`autobleem.txt`** na bootovacím oddílu karty (upravitelný na
libovolném PC před prvním spuštěním):

| Klíč | Výchozí | Význam |
|---|---|---|
| `root_gib` | 8 | Velikost systémového oddílu v GiB; zbytek se stane oddílem her. |
| `hdmi_mode` | 1920x1080@60 | Režim obrazovky pro celý boot (`1280x720@60` pro starší TV). |
| `retroarch` | (asked) | `yes` / `no` - RetroArch a další systémy, nebo jen PS1. |
| `thumbnails` | none | `boxarts` zrcadluje celou sadu obálek pro offline pokrytí (~9000 souborů). |
| `bios`, `downloads`, `samples` | yes | Nastavte na `no` pro přeskočení BIOS balíčků, každého stažení nebo vzorových her. |

**Na stávajícím Raspberry Pi OS Lite** (Bookworm nebo Trixie): zkopírujte
`autobleem-rpi.tar.gz` (nebo 64bitový) na Pi, rozbalte a spusťte `sudo bash install.sh`.
Ptá se na stejné otázky, vytváří oddíl dat zmenšením kořene při dalším startu
(`--shrink-root <GiB>`), a umístí spouštěč na první konzolu.

Po instalaci je **`AUTOBLEEM` oddíl** karty (exFAT) to, co si vyplníte: vyjměte kartu a
otevřete si ji na libovolném PC nebo si ji zkopírujte přes síť (SSH je zapnuto).
`Games/` pro PS1 hry, `RetroArch/roms/<systém>/` pro ostatní systémy, `System/Bios/`
pro PS1 BIOS (oddíl 3.10), `Themes/` pro motivy.

### 2.3 USB disk PC

Stejné zařízení pro jakýkoliv PC, který bootuje z USB - 32bitový systém, takže staré
stroje také fungují:

1. Stáhněte si `autobleem-<verze>-pcusb-i386.img.xz` z panelu PC a napište jej na disk 8
   GB nebo větší s Raspberry Pi Imager (*Use custom*), balenaEtcher nebo Rufus (režim DD).
2. Bootujte PC z disku (klávesa nabídky bootování vašeho PC - F12, F8, Esc...). Funguje
   jak BIOS, tak UEFI boot; **Secure Boot musí být vypnuto**.
3. První boot je stejný jako u Pi: otázka na síť, pokud není kabel, otázka na RetroArch,
   pak instalace - přibližně osm minut s drátěnou sítí - a restart do spouštěče.

Disk pak má oddíl `AUTOBLEEM` pro vaše hry, viditelný na Windows 10 (1903 a novější)
jako druhá jednotka, když zapojíte disk do běžícího PC. `autobleem.txt` je na prvním
oddílu se stejnými klíči jako na Pi (žádný `hdmi_mode` - PC používá nativní režim
obrazovky).

### 2.4 Windows

AutoBleem jako program pro Windows: na celou obrazovku, emulátory a RetroArch spuštěné
jako programy.

1. Stáhněte si **AutoBleemSetup-<verze>.exe** a spusťte jej. Instaluje se na uživatele bez
   práv správce: program pod `%LOCALAPPDATA%\Programs\AutoBleem`, data (hry, nastavení,
   motivy, RetroArch) do složky, kterou si vyberete - standardně `Documents\AutoBleem`.
2. Zaškrtněte komponenty - databáze obálek, RetroArch (oficiální Windows build a jeho jádra),
   BIOS soubory, vzorové hry - a nechte pomocníka instalace stáhnout je.
3. Spusťte AutoBleem z nabídky Start nebo z plochy. Na PC funguje klávesnice jako gamepad
   (oddíl 3.2).

Spuštění novější instalace jej aktualizuje a uchovává složku dat. Spouštěč také jednou
denně kontroluje web a nabídne aktualizaci, když je (oddíl 3.11).

<!-- pagebreak -->

## 3. Používání AutoBleem

### 3.1 Spouštěč

Spouštěč se otevře na polici: obálky aktuální sady, vybraná v prostředku, její detaily
vedle - vydavatel, rok, seriál, oblast, hráči, kdy byla naposledy hraná - a tlačítko play.
Lišta dole uvádí, co tlačítka dělají. Skenování složky her běží na pozadí při každém
startu; během jeho běhu bublina vpravo nahoře ukazuje jeho průběh a nové hry se objevují
na polici, jak jsou nalezeny.

![Výběr sady: tři karty a skupiny aktuální sady s počty her](../images/en/set-picker.jpg)

### 3.2 Ovládání

| Tlačítko | Na polici |
|---|---|
| Vlevo / Vpravo | Předchozí / následující hra. Držením se scrolluje. |
| L1 / R1 | Přeskok na předchozí / následující první písmeno nadpisu. |
| Cross | Spusť vybranou hru (PS1 hru v emulátoru PS1; hru RetroArch v jejím jádře; aplikaci po jejím čtení). |
| Square | Spusť vybranou PS1 hru v RetroArch místo toho. |
| Triangle | Průvodce tlačítky. |
| Start | Náhodná hra z aktuální sady. |
| Select | Výběr sady: karty PlayStation / RetroArch / Apps (L1 / R1), skupiny karty (Nahoru / Dolů, L2 / R2 strana), Cross vybere. |
| Dolů | Otevřete řadu ikon pod hrou (Nastavení, Hra, Paměťová karta, Obnovit). Nahoru ji zavře. |
| L2 + R2 | Systémová nabídka (oddíl 3.4). |

**S klávesnicí** (PC bez gamepadu, nebo USB klávesnice na konzole, Pi nebo PC disku) klíče
stojí: **Šipky** = d-pad, **Enter** = Cross, **Esc nebo Backspace** = Circle, **Tab** =
Triangle, **Space** = Square, **F1 / F2** = Select / Start, **Page Up / Page Down** = L1 /
R1, **Home / End** = L2 / R2, **F10** = systémová nabídka. Na vývojařském počítači Esc
zavře program a Space je Start.

V každém seznamu a nabídce: Nahoru / Dolů se pohybují, **L2 / R2 listují stránky**, L1 /
R1 skáčou na první / poslední řadu, **Cross vybírá, Circle zpět**. Obrazovka s
nastavením ji uloží, když ji opustíte pomocí Circle.

![Řada ikon pod vybranou hrou](../images/en/launcher-icons.jpg)

### 3.3 Sady

**Select** otevře výběr sady. Karta PlayStation uvádí *All Games*, *Internal Games*
(dvacet zabudovaných her konzoly na PlayStation Classic), každou složku, kterou jste
vytvořili pod `Games/` (hra v podsložce patří do dané skupiny), *Favorite Games*, *Game
History* a když je některá hra označena jako taková, *Lightgun Games*. Karta RetroArch
uvádí jednu skupinu na systém, který má hry, plus vlastní Favorites a History RetroArch.
Karta Apps skupinuje aplikace podle typu: *All apps*, pak *Games*, *Emulators*, *Tools*,
*Media* a *Other* (kategorie je nastavena v souboru `app.ini` každé aplikace). Každý řádek
ukazuje, kolik položek obsahuje; skupina bez jakýchkoli se otevře na prázdné polici s řadou
ikon ukazující pouze Nastavení.

### 3.4 Rychlá nabídka

**Nahoru** ve spouštěči, nebo **ikona ozubeného kola** v řadě ikon (kde jsou Nastavení /
Hra / Paměťová karta / Obnovit): rychlá nabídka pro akce, které chcete z kolotoče. Krátký
seznam: *Re-Scan Games* (spustí skenování nyní), *Store* (AutoBleem Store pro stažení
rozšíření), *Network & Controllers* (pouze kde nainstalované rozšíření poskytuje položku
`network` - PSC-Bios na konzole, Pi a PC disku: Wi-Fi, párování Bluetooth, průvodce
mapováním gamepadu - viz oddíl 6; zašedlé s "zapněte v Rozšířeních", když je rozšíření
vypnuto - Cross otevře seznam Extensions) a *System menu...* (úplná nabídka níže). Nahoru
/ Dolů se pohybují (zabaleno), Cross vybere, Circle zpět. Zde není nic jedinečného - každá
položka je také v systémové nabídce.

### 3.5 Systémová nabídka

**L2 + R2** (společně, v libovolném pořadí) otevře systémovou nabídku přes policí. Nabídka
je rozdělena do sekcí:

| Sekce | Položka | Co dělá |
|---|---|---|
| (nahoře) | Re-Scan Games | Vyhledá nové, změněné nebo odebrané hry nyní (skenování také sleduje složku samo). |
| | Extensions | Rozšíření na disku - AutoBleem Store a další (oddíl 3.12). |
| **Library** | Game Manager | PS1 hry jako seznam s jejich složkami: smazat hru, vyčistit obálky. Zakázáno během skenování. |
| | Memory Cards | Vaše sady paměťových karet (oddíl 3.7). |
| | Scanner processors | Programy, které každé skenování spustí jako první - jejich pořadí, zapnuto nebo vypnuto (oddíl 3.13). Zakázáno během skenování. |
| **System** | Options | Nastavení AutoBleem (oddíl 3.6). |
| | Network & Controllers | Pouze kde nainstalované rozšíření poskytuje položku `network` (`Provides=network` v `extension.ini` - PSC-Bios na konzole, Pi a PC disku) - Wi-Fi, párování Bluetooth ovladače, nastavení DualShock 3 a průvodce mapováním gamepadu - viz kapitola 6. Když je rozšíření nainstalováno, ale vypnuto, tato položka zůstává zašedlá s poznámkou "zapněte v Rozšířeních" - Cross otevře seznam Extensions u něj. |
| | Hardware Information | Fakta o počítači: systém, CPU, úložiště, síťová rozhraní, časové pásmo, displej, gamepady a jejich mapování. Na konzole s jádrem AutoBleem to otevře PSC-Bios (kapitola 6); na ostatních počítačích se zobrazí tato informační stránka. |
| | Software Update | (Raspberry Pi a PC) Kontrola webu pro novější AutoBleem nebo RetroArch nyní. |
| | About | Kredity a licence. |
| **Leave** | RetroArch | Opustí spouštěč pro vlastní nabídku RetroArch. Zavření RetroArch se vrátí. |
| | Power Off | Po potvrzení: na konzole standby AutoBleem - disk odpojen, světlo červené, Power vrátí spouštěč zpět (oddíl 2.1); na Pi nebo PC se počítač vypne. |

![Systémová nabídka](../images/en/system-menu.jpg)

### 3.6 Možnosti

Nastavení jsou v skupinách; Nahoru / Dolů se pohybují mezi nimi, Vlevo / Vpravo změní
hodnotu, Circle opustí a uloží. Každá změna se použije okamžitě.

| Skupina / nastavení | Co dělá |
|---|---|
| **Interface**: AutoBleem Theme | Vzhled. Motivy jsou v `Themes/`; zip motivu tam vhozený se rozbalí při dalším návštěvě. Motivy, které AutoBleem zasílá, se obnovují s každou aktualizací - pro přizpůsobení si nejdřív jeden zkopírujte pod novým jménem. |
| Cover Style | Rám v průhledu krytu nakresleného kolem obálek PS1. |
| Language | Jazyk spouštěče, použitý okamžitě (17 jazyků). |
| Use Font from Theme / Font | Písmo klasických obrazovek: z motivu, nebo libovolné `.ttf`/`.otf` z `resources/fonts`, `RetroArch/fonts` nebo složky motivu. |
| Showing Timeout | Jak dlouho zůstane oznámení "Showing: ..." vidět, v sekundách (0 = navždy). |
| **Sound**: Music, Background Music | Kterou skladbu hrají pod spouštěčem (od motivu nebo soubor z `resources/music`) a zda se nějaká hraje. |
| **Emulation**: PS1 Emulator | `pcsx-abnxt` (výchozí: aktuální PCSX-ReARMed s doplňky AutoBleem) nebo `pcsx-ab` (klasický). Bod obnovení uložený jedním pokračuje v druhém, pokud hra neběžela bez BIOS souboru. |
| Widescreen | Tvar obrázku emulátoru PS1 pro každou hru. |
| Play all PSX games with RA | Každá PS1 hra se spustí v jádře PS1 RetroArch. |
| Update RA Config | AutoBleem zapisuje nastavení do konfigurace RetroArch, když tam spustí hru. |
| **Library**: Show Internal Games | Zabudované hry konzoly v seznamech PlayStation (pouze PlayStation Classic). |
| Fetch box art online | Skenování stahuje chybějící obálky ze serverů libretro (Raspberry Pi, PC, Windows). |
| **Updates** | (Raspberry Pi, PC, Windows) `stable`, `latest` (včetně předvydaní) nebo `off`. |

![Možnosti v skupinách](../images/en/options.jpg)

### 3.7 Nastavení hry

S vybranou hrou, **Dolů** otevře její řadu ikon: **Settings** (výše uvedené možnosti),
**Game** (vlastní nastavení hry), **Memory Card** (její paměťová karta) a **Resume**
(její body obnovení). Cross otevře ten pod kurzorem.

**Editor her** ukazuje detaily hry vpravo a její nastavení vlevo, ve třech skupinách:

- **Game**: *Favorite* (ve skupině Oblíbené hry), *Lightgun Game* (hra se světelnou
  pistolí - vstoupí do skupiny Lightgun a vždy se spustí v RetroArch, jehož jádro PS1
  má GunCon), *Play using RA* (tato hra běží v RetroArch), *Lock data* (skener ponechá
  název, seriál a seznam disků hry tak, jak jste je nastavili).
- **Video**: vysoké rozlišení, obrazové řádky a jejich úroveň, přeskočení snímků, plugin
  GPU a *Filter* - jak se obrázek škáluje: Off (prosté pixely), Linear (vyhlazené) nebo
  Sharp (ostré pixely bez blikání; jen `pcsx-abnxt` - klasický `pcsx-ab` a RetroArch jej
  zobrazují jako Off).
- **Emulator**: SpeedHack, frekvence CPU, interpolace SPU, logo při spuštění (vypnuto
  přeskočí shell BIOS - pro homebrew disk, jehož vlastní logo rozbije boot), a s
  `pcsx-abnxt` filtr *Smoothing* a přepínač *Sony hacks*.

Triangle přejmenuje hru, Square změní její paměťovou kartu, Start sdílí novou kartu. Circle
uloží a opustí.

**Nastavení uložená v emulátoru.** Vlastní nabídka emulátoru má *Save settings for this
game*. Jakmile má hra tam uložená nastavení, jsou to ta, která hraje, a editor her ukazuje
své řady Video a Emulator zašedlé s těmito hodnotami pod nadpisem *Saved in the emulator*.
Chcete-li se vrátit k nastavení editoru her, vyberte **Unlock the settings** a potvrďte:
to smaže nastavení, která emulátor uložil, a řady se mohou opět měnit. Oba emulátory,
`pcsx-ab` a `pcsx-abnxt`, čtou a zapisují stejné uložené nastavení.

![Editor her](../images/en/game-editor.jpg)

### 3.7 Paměťové karty a body obnovení

Každá PS1 hra má standardně svou paměťovou kartu (uchovávané s jejími body obnovení v
`Games/!SaveStates/<složka her>/`). **Memory Cards** v systémové nabídce spravuje
**sdílené sady** - kartu, kterou používá několik her, uchovávané v `Games/!MemCards/`:
vytvořte jednu (Square, s klávesnicí na obrazovce), přejmenujte (Cross), smažte (Triangle).
Hra je umístěna na sadu s *Change memory card* v jejím editoru nebo z její ikony Memory
Card.

**Editor paměťové karty** (ikona Memory Card) ukazuje kartu hry a druhou kartu vedle sebe,
s každou ikonou uloženou hry a nadpisem: zkopírujte uloženou hru mezi těmito dvěma
(Square), smažte jednu (Triangle), defragmentujte kartu (Select). Start vyměňuje kartu
vpravo za jinou sadu.

![Editor paměťové karty](../images/en/memory-card-editor.jpg)

**Body obnovení**: když opustíte PS1 hru tlačítkem Reset konzoly (nebo nabídkou
emulátoru na Pi nebo PC), AutoBleem uchová bod obnovení, kde jste byli a nabídne jej pod
ikonou **Resume** - čtyři pozice, každá s obrázkem okamžiku. Cross pokračuje z pozice,
Triangle ji smaže. Hra s bodem obnovení ukazuje malý obrázek na ikoně Resume.

### 3.8 Spouštění her, RetroArch a aplikací

**Cross** spustí vybranou hru. PS1 hra běží ve zvoleném emulátoru PS1 (oddíl 3.5), na
celou obrazovku, dokud ji neopustíte - na konzole předním tlačítkem **Reset** (zpět do
spouštěče s bodem obnovení) nebo **Power** (konzola se vypne); na Pi nebo PC prostřednictvím
nabídky emulátoru během hry (Select + Start na gamepadu, nebo Esc na klávesnici). **Square**
spustí PS1 hru v RetroArch místo toho.

Hra **RetroArch** se spustí v RetroArch s jádrem, které si spouštěč vybral pro svůj
systém; *Close Content* nebo *Quit RetroArch* v jeho nabídce se vrátí do spouštěče.
Položka RetroArch v systémové nabídce otevře vlastní nabídku RetroArch (XMB) bez ničeho
nahrané, pro její nastavení a vlastní seznamy obsahu.

Aplikace (**Apps** sada: nástroje konzoly a na konzole dodatečné aplikace, které balíček
RetroArch přináší - Doom, Quake, Amiga, ...) nejdřív zobrazí své čtení; Cross ji
spustí, Circle se vrátí.

![Čtení aplikace před její spuštěním](../images/en/app-start.jpg)

### 3.9 Přidávání her

**PS1 hry** jdou do složky `Games`, **jednu složku na hru**, pojmenovanou po hře:

```
Games/
  Crash Bandicoot/            Crash Bandicoot.cue + Crash Bandicoot.bin
  Final Fantasy VII/          Final Fantasy VII (Disc 1).chd, (Disc 2).chd, (Disc 3).chd
  Platformers/                složka her: skupiny ve výběru sady
    Klonoa/                   Klonoa.pbp
```

- Formáty: `.cue` + `.bin` (nebo `.img`), `.pbp`, `.chd` (také zstd), `.ecm` (dekódovaný
  skenováním), `.iso`. Zipsovaná hra také funguje: procesor **Unzip** ji rozbalí před
  skenováním (oddíl 3.13).
- Hra s více disky je jedna složka se všemi disky v ní; složky pojmenované `Game (Disc 1)`,
  `Game (Disc 2)` ... jsou skenováním sloučeny do jedné složky `Game`.
- Hry vhozené přímo do `Games/` (volné soubory) jsou skenováním seřazeny do složek.
- **Obálka** je PNG vedle obrázku hry, pojmenovaná jako on. Bez jedné obálka přichází z
  databází obálek, nebo - s nainstalovaným RetroArch - z thumbnailsetu libretro; na Pi, PC
  nebo Windows chybějící je stažena online (Options → *Fetch box art online*).
- Skenování čte seriálové číslo každého disku a vezme nadpis, vydavatele, rok, hráče a
  oblast z databáze PlayStation RetroArch nebo databází obálek. Změňte cokoli v editoru
  her a zaškrtněte *Lock data*, abyste to ponechali.

**Ostatní systémy** jdou pod `RetroArch/roms/`, **jednu složku na systém, pojmenovanou jak
jsou pojmenované databáze RetroArch** (složka se vytváří pro vás): `Nintendo - Nintendo
Entertainment System`, `Nintendo - Super Nintendo Entertainment System`, `Sega - Mega Drive
- Genesis`, `Nintendo - Game Boy Advance`, `FBNeo - Arcade Games` (nebo `Arcade`), ...
Romky mohou zůstat zipsované. Na Pi, PC nebo Windows skenování je čte samo a zapisuje
seznamy RetroArch; na konzoli si je spusťte **UpdateRoms** na PC (kapitola 5).

**Aplikace** jdou pod `Apps/<jméno>/` s `app.ini` (jménem, ikonou, co spustit) a `run.sh`.

**Motivy** jdou pod `Themes/<jméno>/` (`theme.json` a obrázky) - nebo vhoďte zip motivu do
`Themes/`.

### 3.10 PS1 BIOS

Na **PlayStation Classic** emulátor používá vlastní BIOS konzole. Na **Raspberry Pi, PC a
Windows** vložte svůj PS1 BIOS do `System/Bios/`: `romw.bin` (US/European SCPH-5501/5502)
a `romJP.bin` (Japanese SCPH-5500). Instalátoři je vyplní z balíčků RetroArch BIOS, pokud
tam už nejsou vaše vlastní soubory. Bez nich emulátor běží na svém vestavěném HLE BIOS,
který mnoho her toleruje a některé ne.

### 3.11 Aktualizace

- **Raspberry Pi, PC disk, Windows**: spouštěč kontroluje web při startu a jednou denně
  (Options → *Updates* je kanál; *Software Update* v systémové nabídce kontroluje nyní).
  Když je novější AutoBleem nebo RetroArch se ptá: *Update now* stáhne všechno a znovu
  spustí instalátor s obrazovkou prvního startu; *Remind me tomorrow* a *Skip this version*
  jsou ostatní odpovědi. Vaše hry a nastavení zůstanou; spouštěč se znovu proskenuje po
  aktualizaci.
- **PlayStation Classic**: spusťte novější `AutoBleemInstaller.exe` přes disk (oddíl 2.1).

### 3.12 Rozšíření a AutoBleem Store

**Rozšíření** přidají své vlastní obrazovky do spouštěče. Jsou v `Extensions/<jméno>/` na
disku (na Raspberry Pi jeho oddíl dat, na Windows složka dat); k instalaci jednoho jej
rozbalte tam. **L2 + R2 → Extensions** je uvádí: Cross spustí jednu, Triangle ji vypne
nebo znovu zapne. Rozšíření, které potřebuje síť, se nesp spustí bez jedné a rozšíření,
které zastavilo spouštěč, se vypne - seznam to řekne.

![Seznam rozšíření](../images/en/extensions.jpg)

**AutoBleem Store** je první rozšíření: aplikace a hry k instalaci jedním stisknutím, na
všech systémech, na kterých AutoBleem běží (PlayStation Classic potřebuje WiFi jádra
AutoBleem). Jeho čtyři karty, L1 / R1 mezi nimi:

- **Apps** a **Games**: co zdroje nabízejí, každá s obrázkem, verzí, velikostí a favikonu
  zdroje. Nainstalované položky jsou zašedlé. Cross instaluje (nebo aktualizuje, nebo zkoušit
  znovu po selhání), Triangle odstraní, co Store nainstaloval. L2 / R2 nebo Vlevo / Vpravo
  se listují stránky, **Select** ukazuje jeden zdroj najednou, **Start** hledá nadpisy.
  Obrázky položek jsou cachovány a lze je zkusit znovu, pokud se selhají načíst.
- **Downloads**: co se stahuje, čeká, selhalo nebo instaluje. Lišta průběhu se aktualizuje
  neustále. Stažení běží na pozadí také poté, co opustíte Store; spuštění hry nebo vypnutí
  jej pouze pozastaví a zastavené stažení pokračuje tam, kde se zastavilo. Nainstalovaná
  hra se objeví na polici po příštím skenování s obrázkem Store jako její obálka. Stažení
  přes 2 GB funguje na všech platformách, včetně 32bitových buildů.
- **Sources**: odkud seznamy pocházejí - vlastní katalog AutoBleem, TSV seznam vhozený do
  `System/Extensions/store/sources/`, a adresy, které si přidáte s **Add a source URL**.
  Každý zdroj ukazuje svou favikonu v seznamu. Cross na tom, který jste přidali, jej
  přejmenuje, změní jeho adresu, přepne jej mezi `http://` a `https://` nebo jej odstraní.

![Karta Apps Store](../images/en/store-apps.jpg)

![Nabídka zdroje](../images/en/store-source-menu.jpg)

To, co katalog AutoBleem nabízí, je také uvedeno na webu ke stažení,
`https://autobleem.retromenele.pl/store/`. **Jste odpovědní za to, co obsahují zdroje,
které si přidáte.**

**Vaše vlastní hry ve vaší síti**: `abstored`, server Store v LAN, obsluhuje složku PS1
her do Store v téže síti. Běží na jakémkoliv Linux počítači - Raspberry Pi, domácí server
- a pouze čte složku. Spusťte ji s `abstored <složka her>`, otevřete `http://<ten
počítač>:8124/` v prohlížeči, abyste viděli, co obsluhuje a jakékoliv problémy, které
našel, a přidejte `http://<ten počítač>:8124/store.tsv` jako zdroj. Hotové programy pro
Linux a Windows jsou na stránce Store v záložce **LAN server**; nastavení jako službu je
`INSTALL-linux.md` (`ext_store/server/` ve zdroji). **LAN Share** (oddíl 5.2) dá hry a
disky z PC na takový server.

### 3.13 Procesory skenování

**Procesory skenování** jsou malé programy, které každé skenování spustí dříve, než přečte
vaše hry. Jeden může změnit formát, který AutoBleem nečte, na ten, který čte - například
zipsovanou hru - nebo změnit data hry, jako je translační patch. Jsou v
`System/Processors/<jméno>/` na disku (na Raspberry Pi jeho oddíl dat, na Windows složka
dat); k instalaci jednoho jej rozbalte tam. Příští skenování jej spustí.

- **Unzip přichází s AutoBleem**: rozbalí zipsované PS1 hry v `Games/` před skenováním je
  čte a zipsované ROMy jednu po druhé (sady arcade zůstávají zipsované). Aktualizace
  AutoBleem ji aktualizuje a ponechá ji vypnutou, pokud jste ji vypnuli.
- Procesor, který již s hrou jednal, se na ní znovu nemusí spustit, dokud se hra nemění.
- Zatímco procesor pracuje, bublina vpravo nahoře ukazuje, co dělá; varování nebo selhání
  se objeví na řádku pod ní. `processors.log` ve složce logu má detaily.
- Spuštění hry nebo RetroArch zastaví procesor, který změní soubory; příští skenování
  dokončí svou práci.

**L2 + R2 → Scanner processors** je ukazuje v pořadí, v jakém běží, jednu kartu pro PS1 hry
a jednu pro ROMy (L1 / R1). **Square** vezme procesor a Nahoru / Dolů ho přesunou - pořadí
je důležité: procesor, který rozbaluje, musí být dříve toho, který opravuje to, co byl
rozbalený. **Cross** jej vypne nebo zapne, **Triangle** jej nechá podívat se znovu na
každou hru při příštím skenování, **Circle** se vrátí a spustí skenování, pokud jste
něco změnili. Procesor postavený pro jiný počítač zůstane v seznamu, zašedlý.

![Procesory skenování](../images/en/processors.jpg)

Psaní vlastního: stránka Unzip, `https://github.com/autobleem2/proc_unzip`, vysvětluje
vše, co procesor musí dělat, a `tools/proc_check.py` v zdroji AutoBleem jej kontroluje,
než jej sdílíte.

<!-- pagebreak -->

## 4. Obrazovky

### 4.1 Správce her

PS1 hry jako seznam s jejich složkami a vybraná obálka a detaily. Cross otevře editor
her, **Square smaže hru** (její složku a po druhé otázce její body obnovení), Triangle
smaže každý PNG obalů vedle her (skenování je vezme z databází znovu), L2 / R2 listují.
Volné místo jednotky je vpravo nahoře. Správce her čeká na dokončení skenování.

![Správce her](../images/en/game-manager.jpg)

### 4.2 Informace o hardwaru

Fakta o počítači - systém, hardware, úložiště s jeho volným místem, síťové adresy, displej
a zvuková ovladače, připojené gamepady - znovu čten každou sekundu. Na PlayStation Classic
s jádrem AutoBleem se tato položka otevře **PSC-Bios** místo toho (kapitola 6).

První dva řadiče jsou zobrazeny jako Player 1 a Player 2 – porty, které jim přiděluje emulátor PS1.
Jakýkoli další řadič je zobrazen jako nepoužitý emulátorem PS1. RetroArch přiřazuje řadiče podle
vlastního nastavení a může je objednat jinak. Když je řadič připojen nebo odpojen, spouštěč krátce
zobrazuje, který pad je Player 1 a Player 2.

![Informace o hardwaru](../images/en/hardware-info.jpg)

### 4.3 Průvodce tlačítky

Triangle na polici: každé tlačítko každé obrazovky na jedné stránce. Když je připojena USB
klávesnice nebo byla používána, sloupec Keyboard ukazuje klíče vedle tlačítek gamepadu.

![Průvodce tlačítky](../images/en/button-guide.jpg)

### 4.4 Klávesnice na obrazovce

Kdekoli se psává text - sada paměťové karty, název hry, heslo WiFi, adresa zdroje - stejná
klávesnice, rozložená jako klávesnice telefonu: písmena, stránka symbolů (`/ \ : ? & = % @ #`
a zbytek, co adresa nebo heslo potřebuje) a dvě stránky akcentovaných písmen, se Shiftem,
klíčem stránky, Space, Backspace a Confirm na spodní řadě. Směry se pohybují, Cross
psuje, Triangle maže, Square je mezera, **L1** je Shift (dvakrát pro caps lock), **R1**
následující stránka, **L2 / R2** posunují kurzor, Start potvrzuje, Circle zrušuje. USB
klávesnice psuje kdykoli: Enter potvrzuje, Esc zrušuje.

![Klávesnice na obrazovce](../images/en/keyboard.jpg)

<!-- pagebreak -->

## 5. Na PC

### 5.1 UpdateRoms - obnovení disku konzole

PlayStation Classic nemá síť, takže seznamy RetroArch a obálky její disku se dělají na PC:
**UpdateRoms** dělá na PC to, co skenování spouštěče dělá na Pi, se sítí PC a cestami
konzole, takže konzola bootuje a najde všechno na místě.

1. Zkopírujte své ROMy na disk pod `RetroArch/roms/<systém>/` (oddíl 3.9). Názvy složek
   musí být názvy databází RetroArch; instalátor si vytváří běžné.
2. Spusťte **`UpdateRoms\UpdateRoms.exe` z disku** (instalátor jej tam umístil). Najde disk
   z místa, kde sedí, ukazuje řádek fáze, lištu průběhu a záznam, a:
   - stahuje balíček databáze RetroArch, když disk nemá, a identifikuje každou ROM podle ní
     - hra, kterou databáze zná, dostane správný název;
   - zapisuje jeden seznam přehrávání na systém do `RetroArch/bin/playlists/` s cestami
     konzole, ponechávající cokoli, co tam RetroArch sám přidal;
   - stahuje obálky každé ROMy, která žádnou nemá, ze serverů miniatur libretro do
     `RetroArch/bin/thumbnails/`.
3. Bezpečně vyjměte disk a vložte jej zpět do konzole. Karta RetroArch výběru sady uvádí
   každý systém, který má hry.

Spusťte jej znovu poté, co se změní složky ROM; složka, která se nic nezměnila, se
přeskočí, takže opětovné spuštění je rychlé. Záznam je `System/Logs/updateroms.log`.
Karta Pi v čtečce karet lze obnovit stejným způsobem (`UpdateRoms.exe <jednotka>
--target rpi`), i když Pi to dělá sama, když má síť.

### 5.2 LAN Share - vaše hry a disky na serveru vaší sítě

**LAN Share** (`LanShare.exe`, na stránce Store v záložce **LAN server**) dá vaše PS1
hry na server Store vaší domácí sítě - `abstored` na Raspberry Pi, NAS nebo jiný PC - a
čte PS1 disk v jednotce CD/DVD PC. Store na konzole, Pi nebo PC pak je instaluje odtud.
Nic se neinstaluje; nastavení se uchovávají v `%LOCALAPPDATA%\AutoBleem LAN Share\`.

![Okno LAN Share](../images/en/lanshare.jpg)

1. **Server**: zadejte jeho adresu (`http://<jeho adresa>:<port>`, jak to má Store) a
   stiskněte **Connect**. Jeho hry a jakékoliv problémy, které jej skenování našlo, jsou
   uvedeny vlevo. Chcete-li tam dát hry, dejte jedno z:
   - **Share** - složka her serveru, jak je sdílena v síti (Samba), např. `\\raspberrypi\games`:
     LAN Share je tam kopíruje a požádá server o skenování. Server sám zůstává pouze pro čtení.
   - **Token** - když byl server spuštěn s `--allow-uploads`: jeho token (server jej vytiskne
     na startu a ponechá jej v `<stav>/upload-token`). LAN Share nahrává přes HTTP a
     zastavené nahrání pokračuje tam, kde se zastavilo.
2. **Hry na tomto PC**: vyberte složku her (jednu složku na hru), zaškrtněte hry a stiskněte
   **Publish the ticked games**. **Na serveru** říká, zda má server hru již (podle seriálu,
   jinak podle nadpisu); taková hra se nikdy nepošle dvakrát. **Tick those not on the
   server** zaškrtne zbytek.
3. **Disk**: vložte PS1 disk do jednotky a stiskněte **Read a disc and publish it**. Disk
   se přečte celý do `.bin` + `.cue` (a `.sbi` pro LibCrypt hru, když jednotka dá podkanál),
   pojmenovaný podle jeho nadpisu, kontrolován proti známému správnému výpisu (když jsou
   databáze vybrány) a zveřejněn. Pro hru na několika discích zaškrtněte **The game has
   more than one disc**: LAN Share si řekne o každý další disk a zveřejní je společně jako
   jednu hru.
4. **Remove from the server...** vezme vybrané hry ze serveru. Nic není smazáno: každá se
   přesune do `.removed` složky vedle her serveru a přesun jej zpět vloží zpět.

**Databáze** - složka obálek AutoBleem (`coversU/P/J.db`) a RetroArch `Sony - PlayStation.rdb`
- dávají nadpisy a kontrolu přečteného disku; obě jsou nepovinné. **Also share the games
on this PC with the Store** (standardně vypnuto) obsluhuje složku na tomto PC pro Store
přímo. První krát se Windows ptá na svůj firewall: povolte pouze privátní sítě.

<!-- pagebreak -->

## 6. Nástroje konzole (PlayStation Classic)

Dva nástroje pro PlayStation Classic disk. Oba kreslí v motivu spouštěče a jazyce a oba
jsou řízeny gamepádem - a v průvodci gamepádem tlačítky konzole. **PSC-Bios** je rozšíření,
které přichází s balíčkem konzole: *Hardware Information* v systémové nabídce jej otevře a
je v seznamu rozšíření. **ABFlashKit** je aplikace v sadě aplikací.

### 6.1 PSC-Bios

Rozšíření, které přichází s balíčkem konzole, také dostupné na Raspberry Pi a PC disku.
Otevírá se z položky *Network & Controllers* v systémové nabídce (nebo ze seznamu rozšíření).
Když je rozšíření nainstalováno, ale vypnuto, je položka *Network & Controllers* v Rychlé
nabídce a Systémové nabídce zašedlá s poznámkou "zapněte v Rozšířeních" - Cross tam otevře
seznam rozšíření u ní.

Otevírací obrazovka ukazuje fakta o počítači: čas, časové pásmo, adaptéry sítě WiFi/Ethernet/
Bluetooth s jejich adresami a každý připojený ovladač s tím, zda má mapování tlačítek. Síť
a části Bluetooth potřebují jádro AutoBleem na konzole (oddíl 6.2) nebo systémové nástroje
na Raspberry Pi / PC disku; průvodce gamepádem funguje na jakémkoliv systému.

![PSC-Bios: Centrum Sítě a ovladačů](../images/en/pscbios-main.jpg)

- **Select - Wi-Fi Network** (jádro nebo NetworkManager): název sítě (psaný, nebo vybraný ze
  skenování), heslo, režim ovladače a *Apply / Restart Network*. Časové pásmo je také
  nastaveno zde. IP adresa konzole se zobrazí po připojení.
- **Square - Bluetooth Controllers**: skenování pro Bluetooth gamepady (DualShock 4, atd.),
  k párování nebo odebrání.
- **L1 - DualShock 3 Pairing**: připojení pouze USB pro první DualShock 3 přes plugin sixaxis
  jádra.
- **R1 - Controller Mapping**: průvodce mapováním (níže).
- **Triangle - About**, **Circle - zpět** do spouštěče.

**Průvodce gamepádem** ukazuje připojený pad surový - každá osa, tlačítko a klobouk jako
čísla a obrázek DualShock, který se rozsvítí, když stisknete. Protože testovaný pad nelze
věřit, průvodce je řízen **tlačítky konzole**: **RESET** přepne na následující pad, **OPEN**
spustí mapování (pak odpovídá každé otázce - stiskněte tlačítko osvětlené na obrázku, nebo
OPEN když pad nemá takové tlačítko), **POWER** zrušuje nebo opouští. Držení Circle na padu
po dobu 2 sekund opustí průvodce (lišta se vyplní a nápověda zápatí říká "Drž 2 s: Exit").
Zatímco pad nemá mapování, držení jakéhokoliv tlačítka po dobu 2 sekund to dělá ("Drž
jakékoliv tlačítko 2 s: Exit"). Krátký stisk se mapuje jako obvykle. Na klávesnici jsou
Esc / Space / Enter v místě POWER / RESET / OPEN. Na konci se nové mapování přidá k testu
a OPEN jej uloží pod jménem vaší volby; spouštěč jej od té doby načítá.

![PSC-Bios: Průvodce mapováním ovladače](../images/en/pscbios-wizard.jpg)

### 6.2 ABFlashKit - jádro AutoBleem

Jádro AutoBleem je volitelná náhrada jádra Linux konzoly: přináší fungující hodiny, USB WiFi
a Bluetooth dongly (pro PSC-Bios a Bluetooth gamepady) a podporu tlačítek na přední straně,
kterou emulátor používá pro body obnovení. ABFlashKit jej instaluje, nejdřív konzolu zálohuje
a může ji vrátit do akcií prostřednictvím vlastního obnovení Sony.

> **Tento nástroj zapisuje do flash paměti konzoly.** Flash, která je přerušena - napájení
> přerušeno, disk vytažen - může konzolu nechat neschopnou spuštění a instalace vlastního
> jádra ruší záruku. Konzolu udržujte zapnutou a disk v ní, dokud se sám nerestartuje.
> ABFlashKit se otevírá s touto upozorněním; *I understand* jde dál, *Quit* odchází.

![Nabídka ABFlashKit](../images/en/abflashkit-menu.jpg)

- **Flash Kernel**: vytváří zálohu obnovení oddílů konzole na disk (`LBOOT.EPB`), pokud
  neexistuje, kontroluje ji a obrázek jádra, zapisuje jádro a systémové soubory AutoBleem a
  restartuje. *All done - když se obrazovka zčení, odpojte napájecí kabel*: vytáhněte
  napájení konzoly a zapojte jej zpět.
- **Full backup**: všechny čtyři oddíly do `LBOOT.EPB` pro pozdější obnovení (předchozí
  záloha se přepíše po otázce).
- **Restore Mode**: kontroluje, že záloha je akcií, nastavuje příznak obnovení a restartuje
  do obnovy Sony, která obnovuje konzolu ze `LBOOT.EPB` na disku - cesta zpět na původní
  firmware.

Lišta průběhu pod každým krokem ukazuje, jak daleko je akce. Nástroj odmítá bleskat konzolu,
která běží jiný vlastní firmware (BleemSync, Project Eris): nejdřív ji obnovte na
akcie.

<!-- pagebreak -->

## 7. Pokud se něco pokazí

- **Logy**: AutoBleem uchovává své logy v paměti, takže disk není psán neustále - dosáhnou
  `System/Logs/` na disku, kartě nebo složce dat pouze, když se něco pokazilo: pád spouštěče,
  PS1 hry nebo RetroArch je uloží do `System/Logs/crash-<n>/` (poslední tři jsou uchovávány) a
  spouštěč to řekne jednou, když se vrátí. Chcete-li každý log, zapněte *Options ->
  Diagnostics -> Keep logs on the stick* (od dalšího startu), nebo vytvořte prázdný soubor
  `System/Logs/keep` na PC. Na Pi nebo PC, *Hardware Information* ukazuje, kde jsou logy a
  Square je uloží do `System/Logs/saved-<n>/`. Soubory: `autobleem.log` (spouštěč),
  `launch.log` a `pcsx.log` (start PS1 hry a výstup emulátoru), `retroarch.log`, a -
  vždy na disku - `update.log` (online aktualizace) a `updateroms.log` (UpdateRoms).
- **Hra není na polici**: zkontrolujte rozložení složky (jednu složku na hru, formáty obrázků
  z oddílu 3.9). *Game Manager* uvádí složky, které skenování odmítlo, po hrách, označené
  *Not added*, s důvodem; Square takovou složku smaže. Re-Scan v systémové nabídce znovu
  spustí skenování.
- **Žádné obálky**: databáze obálek nebyly nainstalované (znovu spusťte instalátor s nimi
  zaškrtnuto), nebo pro hry RetroArch na konzole nebyl spuštěn UpdateRoms na PC.
- **Gamepad nic nedělá nebo má tlačítka promíchání**: průvodce gamepádem PSC-Bios (konzola)
  jej mapuje; na Pi nebo PC seznam Hardware Information ukazuje, co SDL vidí.
- **Konzola ukazuje černou obrazovku po hře**: AutoBleem si znovu postaví své okno samo (až
  třikrát); pokud zůstane černá, podržte tlačítko napájení a zapněte konzolu znovu.
- **Raspberry Pi**: `Alt+F2` dá přihlašovací výzvu na druhé konzole; SSH je zapnuto od prvního
  startu. `sudo journalctl -u autobleem` ukazuje službu spouštěče; `sudo systemctl restart
  autobleem` ji restartuje. První boot, který se nepovedl dokončit (bez sítě), se zkouší
  znovu při dalším startu.
- **Windows**: `Esc` opustí spouštěč; složka dat je ta vybraná v instalaci
  (`Documents\AutoBleem` standardně), logy jsou v `System\Logs`.

AutoBleem je svobodný software (GNU GPL v3 nebo novější), bez záruky. Podpora a noviny:
server Discord propojený na obrazovce About a https://autobleem.retromenele.pl/.

# AutoBleem 2 Brugervejledning

AutoBleem 2 er en spillauncher til **PlayStation Classic** - og siden version 2 også til **Raspberry Pi**,
en **PC boot fra USB-stick** og **Windows**. Den viser dine PS1-spil som en hylde af omslag med deres
kasseart og detaljer, starter dem i den medfølgende PCSX-emulator, og med RetroArch installeret ved siden
af kan den også spille spil fra andre systemer. Denne vejledning dækker installation på hver platform,
daglig brug og de værktøjer, der kommer med det.

> Download til hver platform findes på **https://autobleem.retromenele.pl/**. Siden er organiseret efter
> platform: panelet *Install* på hver enkelt er det, du downloader; *Build inputs* under det er det, som
> installerne henter selv.

## 1. Hvad du får

- **Launcher** - karrusellen af omslag, sæt (PlayStation, RetroArch, Apps), detaljer om spil, systemmenuen,
  muligheder, memory card- og save-state-værktøjer. Samme program på hver platform.
- **To PS1-emulatorer** - `pcsx-abnxt`, den aktuelle (standard), og `pcsx-ab`, den klassiske som AutoBleem
  altid har sendt. Du vælger den i muligheder; begge bruger de samme indstillinger og memory cards.
- **RetroArch** (valgfrit på hver platform) til andre systemer: NES, SNES, Mega Drive, Game Boy, arkade
  og meget mere. AutoBleem bygger dets RetroArch-lister fra ROM'erne, du kopierer ind, og starter hvert spil
  med den rigtige kerne.
- **Konsolværktøjerne** (kun PlayStation Classic): *PSC-Bios* til WiFi, ur og gamepad-kortlægning, og
  *ABFlashKit* til installation af AutoBleem-kernelen.
- **UpdateRoms** til Windows: genopfrisker RetroArch-listerne og kasseart på en konsolstick fra en PC, fordi
  konsollen selv ikke har netværk.

![Launcher: hylden af omslag, udvalgt spils detaljer, knapvejledninger](../images/da/launcher.jpg)

<!-- pagebreak -->

## 2. Installation

### 2.1 PlayStation Classic

Du har brug for en Windows-PC, en USB-stick (USB 2.0, 8 GB eller mere; installeren formaterer den hvis du
spørger) og stock-konsollen. AutoBleem køres fra stickjen uden ændring til konsollen. Stickjen skal være
**FAT32** til en stock-konsol - dens kernel kan ikke læse exFAT. Kun en konsol med AutoBleem-kernelen
installeret (ABFlashKit, kapitel 6) kan også starte fra exFAT-stick, hvilket fjerner FAT32's 4 GB-grænse.

1. Download **AutoBleemInstaller-<version>.zip** fra PlayStation Classic-panelet på siden og pak det ud
   overalt. Det indeholder `AutoBleemInstaller.exe` og AutoBleem-pakken, som det installerer.
2. Sæt stickjen i og start `AutoBleemInstaller.exe`. Vælg drevet øverst. Sæt flueben hvor du ønsker:
   - **Format stickjen** - kun til en frisk stick (alt på den slettes). Vælg FAT32 medmindre konsollen
     har AutoBleem-kernelen.
   - **Databaser med omslag** - kasseart og detaljer fra PS1-biblioteket (afkrydset som standard; cirka
     300 MB).
   - **RetroArch** - RetroArch med dets kerner, ekstra programmer (Doom, Quake, Amiga, ...) og libretro-assets
     til andre systemers spil. Slået fra som standard; kan tilføjes senere ved at køre installeren igen.
   - **BIOS-filer** - BIOS-filerne som RetroArch-kernerne har brug for (kræver RetroArch).
   - **Eksempelspil** - nogle få frie homebrew-spil så hylden ikke er tom.
3. Tryk **Install** og vent. Fremskridtslinjer og loggen viser hvert trin; stickjen hedder `SONY` til
   sidst, og `UpdateRoms` sættes på den (se kapitel 5).
4. Tag stickjen ud sikkert, sæt den i konsolens **anden USB-port** (den højre, spiller 2) og tænd for
   konsollen. AutoBleem starter i stedet for standard-menuen.

**Tænd og sluk.** Med stickjen i booter konsollen, blinker sit lys i nogle få sekunder (AutoBleem bliver
opfanget) og går så i standby før noget vises - det er konsolens egen måde at forberede en opdatering på,
som er hvordan AutoBleem får mulighed for at køre. Tryk **Power** en gang og launcher kommer op. *Power
Off* i systemmenuen eller konsolens power-knap sætter konsollen i **AutoBleem's standby**: stickjen
afkoblets først, derefter bliver lyset **rødt** - tegnet på at AutoBleem fungerer som det skal - og
næste tryk på Power bringer launcher direkte tilbage på få sekunder. **Mens lyset er rødt kan stickjen
trækkes ud** og sættes i en PC uden at Windows spørger efter at kontrollere den; sæt den tilbage før du
trykker Power. Hvis du tager konsolens strøm ud starter boot-standby'en igen næste gang.

For at **opdatere** en stick skal du køre en nyere installer over den: dine spil, gemmede data, indstillinger
og RetroArch-indhold bliver tilbage; kun AutoBleem's egne filer bliver erstattet. En stick lavet med AutoBleem
1.0 eller AutoBleem-NG bliver automatisk bragt til det nye layout.

> Stock-konsollen har ingen ur og ingen netværk: datoer vises kun efter AutoBleem-kernelen er installeret
> (kapitel 6), og kasseart til RetroArch-spil kommer fra UpdateRoms på PC'en (kapitel 5).

Spil går ind i `Games`-mappen på stickjen, en mappe pr. spil - se afsnit 3.9 for layoutet.

### 2.2 Raspberry Pi

AutoBleem gør en Pi til en lille konsol: den starter direkte i launcher uden skrivebord. To færdige images
er på siden - 32-bit og 64-bit - plus en tarball til en eksisterende Raspberry Pi OS Lite.

| Model | 32-bit image | 64-bit image | Noter |
|---|---|---|---|
| Raspberry Pi 5 | ja | ja | |
| Raspberry Pi 4 Model B, Pi 400 | ja | ja | |
| Raspberry Pi 3 Model B / B+ / A+ | ja | ja | fin til launcher og PS1 |
| Raspberry Pi Zero 2 W | ja | ja | 512 MB RAM: PS1 kører, tyngre RetroArch-kerner gør ikke |
| Raspberry Pi 2 Model B | ja | kun v1.2 | langsom for alt 3D |
| Raspberry Pi 1, Zero, Zero W | nej | nej | ARMv6 - ingen af imagerne køres |

**32-bit image er den anbefalede** til PS1-spil: `pcsx-ab`'s hurtige ARM-rekompiler er kun 32-bit, så
64-bit-bygningen kører PS1-spil langsommere. 64-bit-imagenen har det større sæt RetroArch-kerner.

**Med Raspberry Pi Imager:**

1. Installer Raspberry Pi Imager (raspberrypi.com/software). I *Choose OS* vælg *Use custom* og det
   `autobleem-<version>-rpi-armhf.img.xz` (32-bit) eller `-arm64.img.xz` (64-bit), du downloadede - eller
   tilføj repositorie-URL'en `https://autobleem.retromenele.pl/rpi-imager/os_list.json` i appens indstillinger
   og vælg AutoBleem fra listen.
2. Brug Imager's tilpasselseskærm (tandet hjul eller spørgsmål efter *Next*) til at indstille **brugernavn
   og adgangskode, WiFi-netværk og land og aktivere SSH**. AutoBleem har brug for et netværk ved første start.
3. Skriv kortet, sæt det i Pi'en med skærm og tastatur eller pad tilsluttet, og tænd.

**Første start** tager 5 til 25 minutter og viser hvad den gør på skærmen. Uden netværk spørger den efter
et (WiFi-liste, adgangskode), derefter spørger den om du vil installere RetroArch (et minut uden svar betyder
ja), ekspanderer systempartitionen, laver `AUTOBLEEM`-datapartitionen fra resten af kortet, installerer
RetroArch og dets kerner, BIOS-pakkerne og eksempelspilene og genstarter i launcher.

Svarene kan gives på forhånd i **`autobleem.txt`** på kortets boot-partition (redigerbar på enhver PC før
første start):

| Nøgle | Standard | Betydning |
|---|---|---|
| `root_gib` | 8 | Systempartitionens størrelse i GiB; resten bliver spillpartitionen. |
| `hdmi_mode` | 1920x1080@60 | Skærmtilstand for hele boot'en (`1280x720@60` til ældre tv). |
| `retroarch` | (spurgt) | `yes` / `no` - RetroArch og andre systemer eller kun PS1. |
| `thumbnails` | ingen | `boxarts` spejler hele kasseart-sættet offline (~9000 filer). |
| `bios`, `downloads`, `samples` | ja | Sæt til `no` for at springe BIOS-pakker, alle downloads eller eksempelspil over. |

**På en eksisterende Raspberry Pi OS Lite** (Bookworm eller Trixie): kopier `autobleem-rpi.tar.gz` (eller
arm64-versionen) til Pi'en, pak den ud og kør `sudo bash install.sh`. Den stiller de samme spørgsmål,
laver datapartitionen ved at krympe root ved næste start (`--shrink-root <GiB>`), og sætter launcher på
første konsol.

Efter installation er kortets **`AUTOBLEEM`-partition** (exFAT) hvad du udfylder: tag kortet ud og åbn det
på enhver PC eller kopier over netværk (SSH er på). `Games/` til PS1-spil, `RetroArch/roms/<system>/` til
andre systemer, `System/Bios/` til PS1 BIOS (afsnit 3.10), `Themes/` til temaer.

### 2.3 PC USB-stick

Den samme apparat til enhver PC, der starter fra USB - et 32-bit-system, så gamle maskiner fungerer også:

1. Download `autobleem-<version>-pcusb-i386.img.xz` fra PC-panelet og skriv det til en stick på 8 GB eller
   mere med Raspberry Pi Imager (*Use custom*), balenaEtcher eller Rufus (DD-tilstand).
2. Boot PC'en fra stickjen (boot-menu-tasten på din PC - F12, F8, Esc...). Både BIOS og UEFI boot fungerer;
   **Secure Boot skal være slået fra**.
3. Første start er Pi'ens: et netværksspørgsmål hvis der ikke er kabel, RetroArch-spørgsmål, derefter
   installation - cirka otte minutter med trådbundet netværk - og genbboot i launcher.

Stickjen har så en `AUTOBLEEM`-partition til dine spil, synlig på Windows 10 (1903 og nyere) som et
andet drev når du sætter stickjen i en kørende PC. `autobleem.txt` er på første partition med de samme
nøgler som på Pi'en (ingen `hdmi_mode` - PC'en bruger skærmens native tilstand).

### 2.4 Windows

AutoBleem som Windows-program: fuldskærm, emulatorer og RetroArch startet som programmer.

1. Download **AutoBleemSetup-<version>.exe** og kør den. Den installerer pr. bruger uden
   administrator-rettigheder: programmet under `%LOCALAPPDATA%\Programs\AutoBleem`, data (spil,
   indstillinger, temaer, RetroArch) i en mappe du vælger - `Documents\AutoBleem` som standard.
2. Sæt flueben ved komponenter - databaser med omslag, RetroArch (det officielle Windows-build og dets
   kerner), BIOS-filer, eksempelspil - og lad setup-hjælperen downloade dem.
3. Start AutoBleem fra Start-menuen eller Skrivebord. På en PC fungerer tastaturet som en pad (afsnit 3.2).

Kørsel af et nyere setup over det opdaterer programmet og holder datamappen. Launcher tjekker også siden
en gang dagligt og tilbyder en opdatering når der er en (afsnit 3.11).

<!-- pagebreak -->

## 3. Brug af AutoBleem

### 3.1 Launcher

Launcher åbner på hylden: omslaget fra det aktuelle sæt, det valgte i midten, dets detaljer ved siden af -
udgiver, år, serienummer, region, spillere, hvornår det sidst blev spillet - og en afspilningsknap. Linjen
nederst viser hvad knapperne gør. En scanning af spillemappen kører i baggrunden ved hver start; mens den
kører viser en boble øverst til højre dens fremskridt, og nye spil vises på hylden efterhånden som de findes.

![Sæt-vælger: tre faner og grupperne fra det aktuelle med deres spilletal](../images/da/set-picker.jpg)

### 3.2 Kontroller

| Knap | På hylden |
|---|---|
| Venstre / Højre | Forrige / næste spil. Holdt scrolles videre. |
| L1 / R1 | Spring til forrige / næste første bogstav i titler. |
| Cross | Start det valgte spil (PS1-spil i PS1-emulator; RetroArch-spil i dets kerne; App efter læst mig). |
| Square | Start det valgte PS1-spil i RetroArch i stedet. |
| Triangle | Knap-vejledningen. |
| Start | Et tilfældigt spil fra det aktuelle sæt. |
| Select | Sæt-vælger: PlayStation / RetroArch / Apps faner (L1 / R1), grupperne fra fanen (Op / Ned, L2 / R2 en side), Cross vælger. |
| Ned | Åbn ikorrækken under spillet (Indstillinger, Spil, Memory Card, Genoptag). Op lukker den. |
| L2 + R2 | Systemmenuen (afsnit 3.4). |

**Med tastatur** (en PC uden pad eller USB-tastatur på konsol, Pi eller PC-stick) indsætter tasterne sig selv:
**Piletaster** = d-pad, **Enter** = Cross, **Esc eller Backspace** = Circle, **Tab** = Triangle, **Mellemrum**
= Square, **F1 / F2** = Select / Start, **Page Up / Page Down** = L1 / R1, **Home / End** = L2 / R2, **F10**
= systemmenuen. På en udviklingmaskine lukker Esc programmet og Mellemrum er Start.

I hver liste og menu: Op / Ned bevæger, **L2 / R2 vendersider**, L1 / R1 hop til første / sidste række,
**Cross vælger, Circle går tilbage**. En skærm med indstillinger gemmer dem når du forlader den med Circle.

![Ikorrækken under det valgte spil](../images/da/launcher-icons.jpg)

### 3.3 Sæt

**Select** åbner sæt-vælgeren. PlayStation-fanen viser *All Games*, *Internal Games* (konsolens
indbyggede tyve på PlayStation Classic), hver mappe du lavede under `Games/` (et spil i en undermappen
hører til den gruppe), *Favorite Games*, *Game History* og når noget spil er markeret som sådan *Lightgun
Games*. RetroArch-fanen viser en gruppe pr. system der har spil plus RetroArch's egne Favoritter og
Historie. Apps-fanen grupperer programmer efter type: *All apps*, derefter *Games*, *Emulators*, *Tools*,
*Media* og *Other* (kategorien indstilles i hver apps `app.ini`-fil). Hver række viser hvor mange elementer
den indeholder; en gruppe uden nogen åbner på en tom hylde med ikorrækken der kun viser Indstillinger.

### 3.4 Quick-menuen

**Op** i launcher eller **gear-ikonet** i ikorrækken (hvor Indstillinger / Spil / Memory Card / Genoptag er):
Quick-menuen til handlinger du når efter fra karrusellen. En kort liste: *Re-Scan Games* (starter en
scanning nu), *Store* (AutoBleem Store til download af udvidelser), *Network & Controllers* (kun hvor en
installeret udvidelse giver `network`-posten - PSC-Bios på konsol, Pi og PC-stick: WiFi, Bluetooth-
parring, gamepad-kortlægningsguide - se afsnit 6; nedtonet med "enable it in Extensions" når den udvidelse
er deaktiveret - Cross åbner Udvidelser-listen), og *System menu...* (fuld menu nedenfor). Op / Ned bevæger
(ombryder), Cross vælger, Circle tilbage. Intet er unikt her - hvert element er også i systemmenuen.

### 3.5 Systemmenuen

**L2 + R2** (sammen, i enhver rækkefølge) åbner systemmenuen over hylden. Menuen er grupperet i afsnit:

| Afsnit | Element | Hvad det gør |
|---|---|---|
| (top) | Re-Scan Games | Leder efter nye, ændrede eller fjernede spil nu (scanning overvåger også mappen selv). |
| | Extensions | Udvidelserne på stickjen - AutoBleem Store og andre (afsnit 3.12). |
| **Library** | Game Manager | PS1-spillene som en liste med deres mapper: slet et spil, tøm omslaget. Deaktiveret mens en scanning kører. |
| | Memory Cards | Dit memory card-sæt (afsnit 3.7). |
| | Scanner processors | Programmer hver scanning kører først - deres rækkefølge, til eller fra (afsnit 3.13). Deaktiveret mens en scanning kører. |
| **System** | Options | AutoBleem's indstillinger (afsnit 3.6). |
| | Network & Controllers | Kun hvor en installeret udvidelse giver `network`-posten (`Provides=network` i dens `extension.ini` - PSC-Bios på konsol, Pi og PC-stick) - WiFi, Bluetooth-controller-parring, DualShock 3-opsætning og gamepad-kortlægningsguide - se kapitel 6. Når den udvidelse er installeret men deaktiveret bliver dette element netonet med en note "enable it in Extensions" - Cross åbner Udvidelser-listen på det. |
| | Hardware Information | Maskinens fakta: system, CPU, lagerplads, netværksgrænseflader, tidszone, display, pads og deres kortlægninger. På en konsol med AutoBleem-kernelen åbner dette PSC-Bios (kapitel 6); på andre maskiner viser det denne informationsside. |
| | Software Update | (Raspberry Pi og PC) Tjek siden for nyere AutoBleem eller RetroArch nu. |
| | About | Kreditter og licens. |
| **Leave** | RetroArch | Forlader launcher til RetroArch's egen menu. Lukning af RetroArch kommer tilbage. |
| | Power Off | Efter bekræftelse: på konsol AutoBleem's standby - stickjen afkoblet, lyset rødt, Power bringer launcher tilbage (afsnit 2.1); på Pi eller PC lukker maskinen ned. |

![Systemmenuen](../images/da/system-menu.jpg)

### 3.6 Muligheder

Indstillinger er i grupper; Op / Ned bevæger mellem dem, Venstre / Højre ændrer værdi, Circle forlader og
gemmer. Hver ændring påføres øjeblikkeligt.

| Gruppe / indstilling | Hvad det gør |
|---|---|
| **Interface**: AutoBleem Theme | Udseendet. Temaer bor i `Themes/`; et tema zip som sættes der bliver pakket ud ved næste besøg. AutoBleem's temaer som shipper opdateres med hver opdatering - for at tilpasse et skal du kopiere det under et nyt navn først. |
| Cover Style | Juvelerramdelen tegnet omkring PS1-omslag. |
| Language | Launcher's sprog påført straks (17 sprog). |
| Use Font from Theme / Font | Klassiske skærmes skrifttype: temats eller ethvert `.ttf`/`.otf` fra `resources/fonts`, `RetroArch/fonts` eller temats mappe. |
| Showing Timeout | Hvor længe "Showing: ..."-notificering vises i sekunder (0 = altid). |
| **Sound**: Music, Background Music | Hvilken nummer der spilles under launcher (temats eller en fil fra `resources/music`) og om en spilles overhovedet. |
| **Emulation**: PS1 Emulator | `pcsx-abnxt` (standard: nuværende PCSX-ReARMed med AutoBleem's tilføjelser) eller `pcsx-ab` (klassisk). Et genoptag-punkt gemt af en fortsætter i den anden medmindre spillet kørte uden BIOS-fil. |
| Widescreen | PS1-emulatorens billedform til hvert spil. |
| Play all PSX games with RA | Hvert PS1-spil starter i RetroArch's PS1-kerne. |
| Update RA Config | AutoBleem skriver dets indstillinger ind i RetroArch's konfiguration når det starter et spil der. |
| **Library**: Show Internal Games | Konsolens indbyggede spil i PlayStation-listerne (kun PlayStation Classic). |
| Fetch box art online | Scanning henter manglende omslag fra libretro's servere (Raspberry Pi, PC, Windows). |
| **Updates** | (Raspberry Pi, PC, Windows) `stable`, `latest` (pre-udgivelserne også) eller `off`. |

![Muligheder i grupper](../images/da/options.jpg)

### 3.7 Et spils indstillinger

Med et spil valgt åbner **Ned** dets ikorrække: **Settings** (muligheder ovenfor), **Game** (spillets egne
indstillinger), **Memory Card** (dets memory card) og **Resume** (dets gemte tilstande). Cross åbner den
under markøren.

**Spil-editoren** viser spillets detaljer til højre og dets indstillinger til venstre i tre grupper:

- **Game**: *Favorite* (i Favorite Games-gruppen), *Lightgun Game* (et light-gun-spil - det deltager i
  Lightgun-gruppen og kører altid i RetroArch hvis PS1-kerne har GunCon), *Play using RA* (dette spil kører i
  RetroArch), *Lock data* (scanner lader spillets titel, serienummer og disc-liste være som du sætter dem).
- **Video**: høj opløsning, skanlinjer og deres niveau, billedspring, GPU-plugin og *Filter* - hvordan
  billedet skaleres: Off (plain pixels), Linear (smoothed) eller Sharp (crisp pixels uden shimmer;
  `pcsx-abnxt` kun - klassisk `pcsx-ab` og RetroArch viser det som Off).
- **Emulator**: SpeedHack, CPU-ur, SPU-interpolation, boot-logo (off springer BIOS-shell over - til homebrew
  disc hvis brugerdefinerede logo bryder boot), og med `pcsx-abnxt` *Smoothing*-filter og *Sony hacks*-toggle.

Triangle omdøber spillet, Square ændrer dets memory card, Start deler en nyt kort. Circle gemmer og forlader.

**Indstillinger gemt i emulatoren.** Emulatorens egen menu har *Save settings for this game*. Når et spil
har indstillinger gemt der bliver de som det spiller med og spil-editoren viser dets Video og Emulator-
rækker nedtonet med de værdier under overskrift *Saved in the emulator*. For at gå tilbage til spil-
editorens indstillinger vælg **Unlock the settings** og bekræft: dette sletter indstillinger emulatoren
gemte og rækkerne kan ændres igen. Begge emulatorer `pcsx-ab` og `pcsx-abnxt` læser og skriver samme
gemte indstillinger.

![Spil-editoren](../images/da/game-editor.jpg)

### 3.7 Memory cards og gemte tilstande

Hvert PS1-spil har dets eget memory card som standard (holdt med sine gemte tilstande i `Games/!SaveStates/
<spil-mappe>/`). **Memory Cards** i systemmenuen styrer **delte sæt** - et kort flere spil bruger holdt i
`Games/!MemCards/`: opret et (Square med på-skærm tastatur), omdøb (Cross), slet (Triangle). Et spil sættes
på et sæt med *Change memory card* i dets editor eller fra dets Memory Card-ikon.

**Memory card-editoren** (Memory Card-ikonet) viser spillets kort og et andet kort side ved siden med hvert
gems ikon og titel: kopier et gem mellem de to (Square), slet et (Triangle), defragmenter et kort (Select).
Start bytter kortet til højre til et andet sæt.

![Memory card-editoren](../images/da/memory-card-editor.jpg)

**Genoptag-punkter**: når du forlader PS1-spil med konsolens Reset-knap (eller emulatorens menu på Pi eller
PC) holder AutoBleem fast en gemt tilstand af hvor du var og tilbyder det under **Resume**-ikonet - fire
slots hver med billede af øjeblikket. Cross fortsætter fra slot'en, Triangle sletter det. Et spil med et
genoptag-punkt viser et lille billede på dets Resume-ikon.

### 3.8 Start af spil, RetroArch og Apps

**Cross** starter det valgte spil. PS1-spil kører i valgt PS1-emulator (afsnit 3.5) fuldskærm indtil du
forlader det - på konsol med front-**Reset**-knappen (tilbage til launcher med genoptag-punkt) eller **Power**
(konsol slukker); på Pi eller PC gennem emulatorens in-game menu (Select + Start på pad eller Esc på tastatur).
**Square** starter PS1-spil i RetroArch i stedet.

Et **RetroArch**-spil starter i RetroArch med kernen launcher valgte for dets system; *Close Content* eller
*Quit RetroArch* i dens menu kommer tilbage til launcher. RetroArch-posten i systemmenuen åbner RetroArch's
egen menu (XMB) med intet lastet til for dets indstillinger og dens egne indholds-lister.

En **App** (Apps-sættet: konsolværktøjerne og på konsol de ekstra programmer RetroArch's pakke bringer -
Doom, Quake, Amiga, ...) viser dets læs-mig først; Cross starter det, Circle går tilbage.

![Apps læs-mig før den starter](../images/da/app-start.jpg)

### 3.9 Tilføjelse af spil

**PS1-spil** går i `Games`-mappen **en mappe pr. spil** navngivet efter spillet:

```
Games/
  Crash Bandicoot/            Crash Bandicoot.cue + Crash Bandicoot.bin
  Final Fantasy VII/          Final Fantasy VII (Disc 1).chd, (Disc 2).chd, (Disc 3).chd
  Platformers/                en mappe med spil: en gruppe af dens egen i sæt-vælger
    Klonoa/                   Klonoa.pbp
```

- Formater: `.cue` + `.bin` (eller `.img`), `.pbp`, `.chd` (zstd også), `.ecm` (afkodet ved scanning), `.iso`.
  Et zippet spil fungerer også: **Unzip**-processoren pakker det ud før scanning (afsnit 3.13).
- Multi-disc-spil er en mappe med hver disc i det; mapper navngivet `Game (Disc 1)`, `Game (Disc 2)` ... bliver
  flettet til en `Game`-mappe ved scanning.
- Spil som sættes direkte i `Games/` (løse filer) bliver sorteret i mapper ved scanning.
- En **omslag** er PNG ved siden af spillets billede navngivet som det. Uden en kommer kasseart fra
  omslag-databaseerne eller - med RetroArch installeret - fra libretro's thumbnail-sæt; på Pi, PC eller
  Windows bliver en manglende hentet online (Muligheder → *Fetch box art online*).
- Scanning læser hver discs serienummer og tager titel, udgiver, år, spillere og region fra RetroArch's
  PlayStation-database eller omslag-databaseerne. Skift hvad som helst i spil-editoren og sæt *Lock data*
  for at holde det.

**Andre systemer** går under `RetroArch/roms/` **en mappe pr. system navngivet som RetroArch's databaser**
(mappen bliver lavet for dig): `Nintendo - Nintendo Entertainment System`, `Nintendo - Super Nintendo
Entertainment System`, `Sega - Mega Drive - Genesis`, `Nintendo - Game Boy Advance`, `FBNeo - Arcade Games`
(eller `Arcade`), ... ROM'er kan blive zippet. På Pi, PC eller Windows læser scanning dem af sig selv og
skriver RetroArch's afspilliste; på konsol-stick skal du køre **UpdateRoms** på PC'en (kapitel 5).

**Apps** går under `Apps/<name>/` med en `app.ini` (navn, ikon, hvad der skal køres) og en `run.sh`.

**Temaer** går under `Themes/<name>/` (`theme.json` og billeder) - eller drop temats zip i `Themes/`.

### 3.10 PS1 BIOS

På en **PlayStation Classic** bruger emulatoren konsolens eget BIOS. På en **Raspberry Pi, PC og Windows**
sæt din eget PS1 BIOS i `System/Bios/`: `romw.bin` (US/European SCPH-5501/5502) og `romJP.bin` (Japanese
SCPH-5500). Installerne udfylder dem fra RetroArch BIOS-pakkerne medmindre dine egne filer allerede er der.
Uden dem kører emulatoren på sin byggede HLE BIOS som mange spil tolererer og nogle ikke.

### 3.11 Opdateringer

- **Raspberry Pi, PC-stick, Windows**: launcher tjekker siden ved start og en gang dagligt (Muligheder →
  *Updates* er kanalen; *Software Update* i systemmenuen tjekker nu). Når der er nyere AutoBleem eller
  RetroArch spørger det: *Update now* downloader alt og kører installer igen med første-start fremskridts-
  skærm; *Remind me tomorrow* og *Skip this version* er de andre svar. Dine spil og indstillinger bliver
  tilbage; launcher genscannes en gang efter en opdatering.
- **PlayStation Classic**: kør nyere `AutoBleemInstaller.exe` over stickjen (afsnit 2.1).

### 3.12 Udvidelser og AutoBleem Store

**Udvidelser** tilføjer deres egne skærme til launcher. De bor i `Extensions/<name>/` på stickjen (på
Raspberry Pi dens data-partition, på Windows data-mappen); for at installere en skal du pak dens zip der.
**L2 + R2 → Extensions** lister dem: Cross kører en, Triangle slår den fra eller til igen. En udvidelse
der har brug for netværk starter ikke uden et og en der stoppede launcher bliver slået fra - listen siger det.

![Udvidelser-listen](../images/da/extensions.jpg)

**AutoBleem Store** er første udvidelse: Apps og spil at installere med ét tryk på hvert system AutoBleem
kører på (en PlayStation Classic har brug for AutoBleem-kernels WiFi). Dets fire faner L1 / R1 mellem dem:

- **Apps** og **Games**: hvad kilder tilbyder hvert med billede, version, størrelse og kilde-favicon.
  Installerede elementer er nedtonet. Cross installerer (eller opdaterer eller prøver igen efter fejl),
  Triangle fjerner hvad Store installerede. L2 / R2 eller Venstre / Højre vender sider, **Select** viser en
  kilde ad gangen, **Start** søger titler. Element-billeder bliver cachelagret og kan blive genprøvet hvis
  de ikke indlæser.
- **Downloads**: hvad der downloades, venter, fejlslagne eller installerede. Fremskridtslinjen opdateres
  jævnt. Downloads går videre i baggrunden også efter du forlader Store; start af spil eller lukning af
  strøm stanser dem kun og et stoppet download genoptages hvor det stoppede. Et installeret spil dukker op
  på hylden efter næste scanning med Store's billede som omslag. Downloads over 2 GB fungerer på alle
  platforme, inklusiv 32-bit builds.
- **Sources**: hvor lister kommer fra - AutoBleem's egen katalog en TSV-liste som sættes i
  `System/Extensions/store/sources/` og adresser du tilføjer med **Add a source URL**. Hver kilde viser
  sit favicon i listen. Cross på en du tilføjede omdøber det, ændrer dets adresse eller fjerner det.

![Store's Apps-fane](../images/da/store-apps.jpg)

![En kilde's menu](../images/da/store-source-menu.jpg)

Hvad AutoBleem's katalog tilbyder er også listet på download-siden `https://autobleem.retromenele.pl/store/`.
**Du er ansvarlig for hvad kilder du tilføjer indeholder.**

**Dine egne spil på dit netværk**: `abstored` Store's LAN-server betjener en mappe PS1-spil til Store på
samme netværk. Det kører på enhver Linux-maskine - en Raspberry Pi, en hjemmeserver - og læser kun mappen.
Start det med `abstored <spil-mappe>` åbn `http://<den maskine>:8124/` i browser for at se hvad det
betjener og problemer det fandt og tilføj `http://<den maskine>:8124/store.tsv` som kilde. Klare programmer
for Linux og Windows er på Store-siden i dens **LAN server**-fane; at sætte det op som service er
`INSTALL-linux.md` (`ext_store/server/` i kilden). **LAN Share** (afsnit 5.2) sætter spil og disc fra
PC på sådan server.

### 3.13 Scanning-processorer

**Scanning-processorer** er små programmer som hver scanning kører før den læser dine spil. En kan gøre et
format AutoBleem ikke læser til et det gør - et zippet spil for eksempel - eller ændre et spils data sådan
som en oversættelses-patch. De bor i `System/Processors/<name>/` på stickjen (på Raspberry Pi dens data-
partition på Windows data-mappen); for at installere en skal du pak mappen der. Næste scanning kører den.

- **Unzip kommer med AutoBleem**: det pakker zippede PS1-spil i `Games/` ud før scanning læser dem og
  zippede ROM'er en ad gangen (arkade-sæt bliver zippet). Opdatering af AutoBleem opdaterer det også og
  lader det blive slået fra hvis du slog det fra.
- En processor som allerede havde håndteret et spil køres ikke på det igen indtil spillet ændres.
- Mens en processor arbejder viser boblen øverst til højre hvad den gør; en advarsel eller fejl vises på
  linjen under det. `processors.log` i log-mappen har detaljer.
- Start af spil eller RetroArch stopper en processor som ændrer filer; næste scanning afslutter dens arbejde.

**L2 + R2 → Scanner processors** viser dem i rækkefølge de kører en fane til PS1-spil og en til ROM'erne
(L1 / R1). **Square** plukker en processor op og Op / Ned flytter den - rækkefølge betyder: en processor
som pakker ud skal komme før en som patcher hvad blev pakket ud. **Cross** slår en fra eller til **Triangle**
har den se på hvert spil igen ved næste scanning, **Circle** går tilbage og starter en scanning hvis du
ændrede noget. En processor bygget til anden maskine bliver på listen nedtonet.

![Scanning-processorer](../images/da/processors.jpg)

Skriv din egen: Unzip-siden `https://github.com/autobleem2/proc_unzip` forklarer alt en processor skal gøre
og `tools/proc_check.py` i AutoBleem's kilde tjekker en før du deler det.

<!-- pagebreak -->

## 4. Skærme

### 4.1 Game Manager

PS1-spillene som liste med deres mapper og det valgte omslag og detaljer. Cross åbner spil-editoren **Square
sletter spillet** (dets mappe og efter et andet spørgsmål dets gemte tilstande) Triangle sletter hvert
omslag PNG ved siden af spillene (scanning tager dem fra databaseren igen) L2 / R2 side. Fri plads på
drevet er øverst til højre. Game Manager venter mens en scanning kører.

![Game Manager](../images/da/game-manager.jpg)

### 4.2 Hardware Information

Maskinens fakta - system, hardware, lagerplads med dens frie plads netværk-adresser display og audio-drivere
tilsluttede pads - genleser hvert sekund. På PlayStation Classic med AutoBleem-kernelen åbner dette element
**PSC-Bios** i stedet (kapitel 6).

De to første controllers vises som Spiller 1 og Spiller 2 – portene som PS1-emulatoren giver dem. Enhver
yderligere controller vises som ikke brugt af PS1-emulatoren. RetroArch tildeler controllers efter egne
indstillinger og kan ordne dem anderledes. Når en controller tilsluttes eller fjernes, viser launcher'en
kort, hvilken pad der er Spiller 1 og Spiller 2.

![Hardware Information](../images/da/hardware-info.jpg)

### 4.3 Knap-vejledningen

Triangle på hylden: hver knap på hver skærm på en side. Når USB-tastatur er tilsluttet eller er blevet brugt
viser en Keyboard-kolonne tasterne ved siden af pad-knapperne.

![Knap-vejledningen](../images/da/button-guide.jpg)

### 4.4 Tastaturet på skærmen

Overalt hvor tekst bliver tastet - memory card-sæt et spils titel WiFi-adgangskode en sources adresse -
samme tastatur lagt ud som telefonens: bogstaver en side med symboler (`/ \ : ? & = % @ #` og resten en
adresse eller adgangskode har brug for) og to sider med accenterede bogstaver med Shift side-key Mellemrum
Backspace og Bekræft på den nederste række. Retninger bevæger Cross taster Triangle sletter Square er
mellemrum **L1** er Shift (to gange for caps lock) **R1** næste side **L2 / R2** bevæger markøren Start
bekræfter Circle annullerer. USB-tastatur taster til enhver tid: Enter bekræfter Esc annullerer.

![Tastaturet på skærmen](../images/da/keyboard.jpg)

<!-- pagebreak -->

## 5. På PC'en

### 5.1 UpdateRoms - genopfrisking af konsol-stick

PlayStation Classic har ingen netværk så RetroArch-listerne og kasseart på dens stick bliver lavet på PC'en:
**UpdateRoms** gør på PC'en hvad launcher's scanning gør på Pi med PC'ens netværk og konsolens veje så
konsollen starter og finder alt på plads.

1. Kopier dine ROM'er på stickjen under `RetroArch/roms/<system>/` (afsnit 3.9). Mappe-navne skal være
   RetroArch's database-navne; installer laver de almindelige.
2. Start **`UpdateRoms\UpdateRoms.exe` fra stickjen** (installer sætter den der). Den finder stickjen fra
   hvor den sidder viser en fase-linje en fremskridt-linje og en log og:
   - downloader RetroArch's database-bundle når stickjen ikke har et og identificerer hvert ROM efter det -
     et spil databasen kender får dets rigtige navn;
   - skriver en afspilnings-liste pr. system ind i `RetroArch/bin/playlists/` med konsolens veje og holder
     hvad RetroArch selv tilføjede der;
   - henter kasseart fra hvert ROM som ikke har nogen fra libretro's thumbnail-servere ind i
     `RetroArch/bin/thumbnails/`.
3. Tag stickjen sikkert ud og sæt den tilbage i konsollen. RetroArch-fanen af sæt-vælgeren lister hvert
   system som har spil.

Kør det igen efter hver ændring til ROM-mapper; en mappe som ingenting ændrede i bliver sprunget over så
en genstart er hurtig. Loggen er `System/Logs/updateroms.log`. Et Raspberry Pi-kort i kortlæser kan blive
genopfrisket samme vej (`UpdateRoms.exe <drev> --target rpi`) selvom Pi gør det af sig selv når det har netværk.

### 5.2 LAN Share - dine spil og disc på serveren på dit netværk

**LAN Share** (`LanShare.exe` på Store-siden i dens **LAN server**-fane) sætter dine PS1-spil på Store's
server på dit hjemmenetværk - en `abstored` på en Raspberry Pi NAS eller anden PC - og læser PS1-disc i PC'ens
CD/DVD-drev til det. Store på konsol Pi eller PC installerer dem da fra der. Intet at installere; indstillinger
bliver holdt i `%LOCALAPPDATA%\AutoBleem LAN Share\`.

![LAN Share-vinduet](../images/da/lanshare.jpg)

1. **Serveren**: indtast dens adresse (`http://<dens adresse>:<port>` som Store har det) og tryk **Connect**.
   Dets spil og problemer dets scanning fandt bliver listet til venstre. For at sætte spil på det giv en af:
   - **Share** - serverens spil-mappe som den er delt på netværk (Samba) f.eks. `\\raspberrypi\games`:
     LAN Share kopierer spillene der og beder serveren skanningen. Serveren selv bliver læse-kun.
   - **Token** - når serveren blev startet med `--allow-uploads`: dens token (serveren udskriver det ved
     start og holder det i `<state>/upload-token`). LAN Share uploader over HTTP og en stoppet upload går
     videre hvor den stoppede.
2. **Spil på denne PC**: vælg mappe af spil (en mappe pr. spil) sæt flueben på spil og tryk **Publish the
   ticked games**. **On the server** siger om serveren allerede har et spil (efter dets serienummer eller efter
   titel); sådan spil sendes aldrig to gange. **Tick those not on the server** sætter flueben på resten.
3. **En disc**: put PS1-disc i drev og tryk **Read a disc and publish it**. Discs bliver læst helt ind i
   `.bin` + `.cue` (og `.sbi` til LibCrypt-spil når drev giver subkanalerne) navngivet efter dets titel
   tjekket mod kendt godt dump (når databaseerne bliver valgt) og publiceret. Til spil på flere disc sæt
   flueben på **The game has more than one disc**: LAN Share spørger for hver næste disc og publicerer dem
   sammen som et spil.
4. **Remove from the server...** tager de valgte spil fra serveren. Intet bliver slettet: hvert blir flyttet
   i `.removed`-mappe ved siden af serverens spil og at flytte det tilbage sætter det tilbage.

**Databaseerne** - AutoBleem's omslag-mappe (`coversU/P/J.db`) og RetroArch's `Sony - PlayStation.rdb` -
giver titler og tjek af læst disc; begge valgfrit. **Also share the games on this PC with the Store**
(som standard fra) betjener mappen på denne PC til Store direkte. Første gang spørger Windows om dets
firewall: tillad kun private netværk.

<!-- pagebreak -->

## 6. Konsolværktøjerne (PlayStation Classic)

To værktøjer til PlayStation Classic-stick. Begge tegner i launcher's tema og sprog og begge bliver kørt af
pad - og i gamepad-guiden af konsolens front-knapper. **PSC-Bios** er udvidelse der kommer med konsol-pakken:
*Hardware Information* i systemmenuen åbner det og det er i Udvidelser-listen. **ABFlashKit** er App i
Apps-sættet.

### 6.1 PSC-Bios

En udvidelse der kommer med konsol-pakken også tilgængelig på Raspberry Pi og PC-stick. Det bliver åbnet fra
Systemmenuen's *Network & Controllers*-element (eller fra Udvidelser-listen). Når den udvidelse er
installeret men deaktiveret er *Network & Controllers*-elementet i Quick-menu og Systemmenuen nedtonet med
note "enable it in Extensions" - Cross der åbner Udvidelser-listen på det.

Åbnings-skærmen viser maskine-fakta: tid tidszone WiFi/Ethernet/Bluetooth-netværksadaptere med deres adresser
og hver tilsluttet controller med om det har knap-kortlægning. Netværk og Bluetooth-dele har brug for
AutoBleem-kernelen på konsol (afsnit 6.2) eller systemværktøjer på Raspberry Pi / PC-stick; gamepad-guiden
fungerer på ethvert system.

![PSC-Bios: Network & Controllers-hub](../images/da/pscbios-main.jpg)

- **Select - Wi-Fi Network** (kernel eller NetworkManager): netværks-navn (tastet eller plukket fra scan)
  adgangskode driver-mode og *Apply / Restart Network*. Tidszone indstilles her også. Konsolens IP-adresse
  vises når forbundet.
- **Square - Bluetooth Controllers**: scan for Bluetooth-gamepads (DualShock 4 osv.) at parre eller fjerne.
- **L1 - DualShock 3 Pairing**: USB-kun forbindelse til første DualShock 3 gennem kernels sixaxis-plugin.
- **R1 - Controller Mapping**: kortlægnings-guiden (nedenfor).
- **Triangle - About**, **Circle - tilbage** til launcher.

**Gamepad-guiden** viser tilsluttede pad rå - hver akse knap og hat som tal og DualShock-billede som
lyser op når du trykker. Fordi pad'et under test ikke kan stoles på bliver guiden kørt af konsolens **front-
knapper**: **RESET** skifter til næste pad **OPEN** starter kortlægning (derefter svar hver spørgsmål - tryk
knap tændt på billede eller OPEN når pad ikke har sådan knap) **POWER** annullerer eller forlader. Hold Circle
på pad i 2 sekunder forlader guide (en bar fyldes og footer-hint siger "Hold 2 s: Exit"). Mens pad ikke har
kortlægning endnu holder nogen knap i 2 sekunder gør det ("Hold any button 2 s: Exit"). Et kort tryk
kortlægges som normalt. På tastatur står Esc / Mellemrum / Enter ind for POWER / RESET / OPEN. Til slut
bliver nye kortlægning tilføjet til test og OPEN gemmer det under navn du vælger; launcher indlæser det
derefter.

![PSC-Bios: controller-kortlægnings-guiden](../images/da/pscbios-wizard.jpg)

### 6.2 ABFlashKit - AutoBleem-kernelen

AutoBleem-kernelen er valgfrit erstatning af konsolens Linux-kernel: det bringer arbejdende ur USB-WiFi og
Bluetooth-dongel (til PSC-Bios og Bluetooth-pads) og front-knap-support emulatoren bruger til genoptag-
punkter. ABFlashKit installerer det, backer konsollen først og kan sætte konsollen tilbage til stock gennem
Sony's egen recovery.

> **Dette værktøj skriver til konsolens flash-hukommelse.** En flash der bliver afbrudt - strøm afbrydt stick
> trukket ud - kan efterlade konsollen ustand til start og installation af brugerdefineret kernel ophæver
> dens garanti. Hold konsollen strøm og stick i indtil den genstarter af sig selv. ABFlashKit åbner på denne
> advarsel; *I understand* går videre *Quit* forlader.

![ABFlashKit's menu](../images/da/abflashkit-menu.jpg)

- **Flash Kernel**: laver recovery backup af konsolens partitioner på stick (`LBOOT.EPB`) hvis der ikke er
  en endnu, tjekker den og kernel-image skriver kernel og AutoBleem's system-filer og genstarter. *All done
  - when the screen goes black replace power cord*: tag konsolens strøm ud og stik den tilbage i.
- **Full backup**: alle fire partitioner til `LBOOT.EPB` til restore senere (tidligere backup bliver
  overskrevet efter spørgsmål).
- **Restore Mode**: tjekker backupen er en stock-en sætter recovery-flag og genstarter ind i Sony's recovery
  som restorer konsollen fra `LBOOT.EPB` på stickjen - vejen tilbage til stock firmware.

En fremskridts-linje under hvert trin viser hvor langt handlingen er. Værktøjet nægtede at flashe konsol der
kører anden brugerdefineret firmware (BleemSync Project Eris): restore den til stock først.

<!-- pagebreak -->

## 7. Hvis noget går galt

- **Logs**: AutoBleem holder sine logs i hukommelse så stickjen ikke bliver skrevet til hele tiden - de når
  `System/Logs/` på stick kort eller data-mappe kun når noget går galt: krasch af launcher PS1-spil eller
  RetroArch gemmer dem til `System/Logs/crash-<n>/` (de sidste tre bliver holdt) og launcher siger det en
  gang når det kommer tilbage. For at holde hvert log skal du skifte til *Options -> Diagnostics -> Keep
  logs on the stick* (fra næste start) eller opret tom fil `System/Logs/keep` på PC. På Pi eller PC viser
  *Hardware Information* hvor logs er og Square gemmer dem til `System/Logs/saved-<n>/`. Filerne:
  `autobleem.log` (launcher) `launch.log` og `pcsx.log` (PS1-spils start og emulators output)
  `retroarch.log` og - altid på stick - `update.log` (online opdatering) og `updateroms.log` (UpdateRoms).
- **Et spil er ikke på hylden**: tjek mappen layout (en mappe pr. spil billede-formater fra afsnit 3.9).
  *Game Manager* lister mapper scanning nægtede efter spillene markeret *Not added* med grund; Square sletter
  sådan mappe. Re-Scan i systemmenuen kører scanning igen.
- **Ingen omslag**: omslag-databaseerne blev ikke installerede (kør installer igen med dem afkrydset) eller
  til RetroArch-spil på konsol UpdateRoms er ikke blevet kørt på PC'en.
- **En pad gør intet eller har sine knapper blandet**: PSC-Bios's gamepad-guide (en konsol) kortlægger det;
  på Pi eller PC viser Hardware Information-siden hvad SDL ser.
- **Konsol viser sort skærm efter spil**: AutoBleem genbygger sit vindue af sig selv (op til tre gange); hvis
  det bliver sort hold power-knap og tænd konsol igen.
- **Raspberry Pi**: `Alt+F2` giver login-prompt på anden konsol; SSH er aktiveret fra første start. `sudo
  journalctl -u autobleem` viser launcher's service; `sudo systemctl restart autobleem` genstartet det. En
  første start som ikke kunne afsluttet (intet netværk) forsøger igen ved næste start.
- **Windows**: `Esc` forlader launcher; data-mappen er den valgte i setup (`Documents\AutoBleem` som
  standard) logs er i dens `System\Logs`.

AutoBleem er frit software (GNU GPL v3 eller senere) uden garanti. Support og nyheder: Discord-serveren
linket på About-skærmen og https://autobleem.retromenele.pl/.

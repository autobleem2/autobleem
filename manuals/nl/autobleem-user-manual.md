# AutoBleem 2 Gebruikershandleiding

AutoBleem 2 is een gamelauncher voor de **PlayStation Classic** - en sinds versie 2 ook voor een **Raspberry Pi**, een **PC die via USB-stick opstart** en **Windows**. Het toont uw PS1-games als een boekenplank vol hoezen met hun artwork en details, start ze in de meegeleverde PCSX-emulator en kan, wanneer RetroArch ernaast is geïnstalleerd, ook games van andere systemen spelen. Deze handleiding behandelt de installatie op elk platform, dagelijks gebruik en de meegeleverde tools.

> De downloads voor elk platform staan op **https://autobleem.retromenele.pl/**. De pagina is ingedeeld per platform: het *Install*-paneel van elk platform is wat u moet downloaden; de *Build inputs* eronder zijn wat de installers zelf ophalen.

## 1. Wat u krijgt

- **De launcher** - de boekenplank met hoezen, de sets (PlayStation, RetroArch, Apps), gamedetails, systeemmenu, instellingen, geheugenkaarten en savestate-tools. Hetzelfde programma op elk platform.
- **Twee PS1-emulatoren** - `pcsx-abnxt`, de huidige (standaard) en `pcsx-ab`, de klassieke emulator die AutoBleem altijd heeft meegeleverd. U kiest er een in de instellingen; beide gebruiken dezelfde instellingen en geheugenkaarten.
- **RetroArch** (optioneel op elk platform) voor andere systemen: NES, SNES, Mega Drive, Game Boy, arcade en veel meer. AutoBleem bouwt zijn RetroArch-lijsten uit de ROM's die u kopieert en start elk spel met de juiste core.
- **Console tools** (alleen PlayStation Classic): *PSC-Bios* voor WiFi, klok en controller-mapping, en *ABFlashKit* voor het installeren van de AutoBleem-kernel.
- **UpdateRoms** voor Windows: vernieuwt de RetroArch-lijsten en artwork van een console-stick op een PC, omdat de console zelf geen netwerk heeft.

![De launcher: de boekenplank met hoezen, de details van het geselecteerde spel, de buttonhints](../images/en/launcher.jpg)

<!-- pagebreak -->

## 2. Installatie

### 2.1 PlayStation Classic

U hebt een Windows-PC, een USB-stick (USB 2.0, 8 GB of meer; de installer formateert deze op verzoek) en de originele console nodig. AutoBleem draait vanaf de stick zonder wijzigingen aan de console. De stick moet **FAT32** zijn voor een originele console - de kernel kan exFAT niet lezen. Alleen een console met de AutoBleem-kernel geïnstalleerd (ABFlashKit, hoofdstuk 6) kan ook van een exFAT-stick starten, waardoor de 4 GB-limiet van FAT32 wordt opgeheven.

1. Download **AutoBleemInstaller-<version>.zip** van het PlayStation Classic-paneel van de website en pak het ergens uit. Het bevat `AutoBleemInstaller.exe` en het AutoBleem-pakket dat het installeert.
2. Steek de stick in en start `AutoBleemInstaller.exe`. Selecteer het station bovenaan. Vink aan wat u wilt:
   - **Stick formatteren** - alleen voor een nieuwe stick (alles erop wordt gewist). Kies FAT32 tenzij de console de AutoBleem-kernel heeft.
   - **Artwork-databases** - de hoezen en details van de PS1-bibliotheek (standaard aangevinkt; ongeveer 300 MB).
   - **RetroArch** - RetroArch met cores, extra toepassingen (Doom, Quake, Amiga, ...) en libretro-assets voor games van andere systemen. Standaard uit; kan later toegevoegd worden door de installer opnieuw uit te voeren.
   - **BIOS-bestanden** - de BIOS-bestanden die RetroArch-cores nodig hebben (vereist RetroArch).
   - **Voorbeeldspellen** - enkele gratis homebrew-games zodat de boekenplank niet leeg is.
3. Klik **Install** en wacht. De voortgangstaven en log tonen elke stap; de stick wordt aan het einde `SONY` genoemd en `UpdateRoms` wordt erop geplaatst (zie hoofdstuk 5).
4. Verwijder de stick veilig, steek deze in de **tweede USB-poort** van de console (rechts, speler 2) en zet de console aan. AutoBleem start in plaats van het originele menu.

**In- en uitschakelen.** Met de stick erin start de console, knippert het licht enkele seconden (AutoBleem wordt opgehaald) en gaat dan in standby voordat iets wordt weergegeven - dit is de eigen updatemanier van de console, waarmee AutoBleem kan starten. Druk eenmaal op **Power** en de launcher verschijnt. *Power Off* in het systeemmenu of de Power-knop van de console zet de console in **AutoBleem-standby**: de stick wordt eerst verwijderd, dan wordt het licht **rood** - het teken dat AutoBleem goed werkt - en de volgende Power-knop brengt de launcher in enkele seconden terug. **Terwijl het licht rood is, kunt u de stick verwijderen** en op een PC steken zonder dat Windows deze wil controleren; steek deze terug voordat u Power indrukt. Het stroomkabel uittrekken gaat de volgende keer opnieuw door de boot-standby.

Om een stick **bij te werken**, voert u een nieuwere installer erover uit: uw games, saves, instellingen en RetroArch-inhoud blijven; alleen AutoBleem's eigen bestanden worden vervangen. Een stick gemaakt met AutoBleem 1.0 of AutoBleem-NG wordt automatisch naar de nieuwe layout geconverteerd.

> De originele console heeft geen klok en geen netwerk: datums worden alleen weergegeven nadat de AutoBleem-kernel is geïnstalleerd (hoofdstuk 6) en artwork voor RetroArch-games komt van UpdateRoms op de PC (hoofdstuk 5).

Games gaan in de `Games`-map van de stick, één map per game - zie sectie 3.9 voor de layout.

### 2.2 Raspberry Pi

AutoBleem maakt van een Pi een kleine console: deze start rechtstreeks in de launcher, zonder bureaublad. Twee kant-en-klare images staan op de website - 32-bits en 64-bits - plus een tarball voor een bestaande Raspberry Pi OS Lite.

| Model | 32-bits image | 64-bits image | Opmerkingen |
|---|---|---|---|
| Raspberry Pi 5 | ja | ja | |
| Raspberry Pi 4 Model B, Pi 400 | ja | ja | |
| Raspberry Pi 3 Model B / B+ / A+ | ja | ja | prima voor launcher en PS1 |
| Raspberry Pi Zero 2 W | ja | ja | 512 MB RAM: PS1 werkt, zwaardere RetroArch-cores niet |
| Raspberry Pi 2 Model B | ja | alleen v1.2 | traag voor 3D-spellen |
| Raspberry Pi 1, Zero, Zero W | nee | nee | ARMv6 - geen image ondersteund |

**De 32-bits image is aanbevolen** voor PS1-games: de snelle ARM-recompiler van `pcsx-ab` is alleen 32-bits beschikbaar, dus de 64-bits build voert PS1-games langzamer uit. De 64-bits image heeft een groter aanbod RetroArch-cores.

**Met Raspberry Pi Imager:**

1. Installeer Raspberry Pi Imager (raspberrypi.com/software). Kies bij *Choose OS* voor *Use custom* en selecteer de `autobleem-<version>-rpi-armhf.img.xz` (32-bits) of `-arm64.img.xz` (64-bits) die u hebt gedownload - of voeg de repository-URL `https://autobleem.retromenele.pl/rpi-imager/os_list.json` toe in de app-instellingen en selecteer AutoBleem uit de lijst.
2. Gebruik het aanpassingsscherm van Imager (het tandwiel, of de vraag na *Next*) om de **gebruikersnaam en wachtwoord, WiFi-netwerk en land in te stellen en SSH in te schakelen**. AutoBleem heeft een netwerk nodig op de eerste boot.
3. Schrijf de kaart, plaats deze in de Pi met een scherm en toetsenbord of controller aangesloten, en zet deze aan.

**De eerste boot** duurt 5 tot 25 minuten en toont op het scherm wat het doet. Zonder netwerk vraagt het om een netwerk (WiFi-lijst, wachtwoord), dan of RetroArch moet worden geïnstalleerd (een minuut zonder antwoord betekent ja), groeit de systeempartitie, maakt de `AUTOBLEEM`-gegevenspartitie van de rest van de kaart, installeert RetroArch en cores, BIOS-packs en voorbeeldspellen, en herstart in de launcher.

Antwoorden kunnen van tevoren worden gegeven in **`autobleem.txt`** op de boot-partitie van de kaart (bewerkbaar op elke PC voor de eerste boot):

| Sleutel | Standaard | Betekenis |
|---|---|---|
| `root_gib` | 8 | Grootte van de systeempartitie in GiB; de rest wordt de gamepartitie. |
| `hdmi_mode` | 1920x1080@60 | Schermstand voor de hele boot (`1280x720@60` voor oudere TV). |
| `retroarch` | (gevraagd) | `yes` / `no` - RetroArch en andere systemen, of alleen PS1. |
| `thumbnails` | geen | `boxarts` spiegelt de hele box-art-set offline (~9000 bestanden). |
| `bios`, `downloads`, `samples` | ja | Stel in op `no` om BIOS-packs, alle downloads of voorbeeldspellen over te slaan. |

**Op bestaande Raspberry Pi OS Lite** (Bookworm of Trixie): kopieer `autobleem-rpi.tar.gz` (of de arm64-versie) naar de Pi, pak uit en voer `sudo bash install.sh` uit. Het stelt dezelfde vragen, maakt de gegevenspartitie door root te verkleinen bij de volgende boot (`--shrink-root <GiB>`), en plaatst de launcher op de eerste console.

Na installatie is de **`AUTOBLEEM`-partitie** van de kaart (exFAT) wat u vult: haal de kaart eruit en open deze op elke PC, of kopieer via het netwerk (SSH is ingeschakeld). `Games/` voor PS1-games, `RetroArch/roms/<system>/` voor games van andere systemen, `System/Bios/` voor de PS1-BIOS (sectie 3.10), `Themes/` voor thema's.

### 2.3 PC USB-stick

Dezelfde appliance voor elke PC die van USB kan starten - een 32-bits systeem, dus ook oudere machines werken:

1. Download `autobleem-<version>-pcusb-i386.img.xz` van het PC-paneel en schrijf deze naar een stick van 8 GB of meer met Raspberry Pi Imager (*Use custom*), balenaEtcher of Rufus (DD-modus).
2. Start de PC van de stick (de boot-menutoets van uw PC - F12, F8, Esc...). Zowel BIOS als UEFI-boot werken; **Secure Boot moet uit staan**.
3. De eerste boot is die van de Pi: een netwerkbevraagingsteken als er geen kabel is, de RetroArch-vraag, dan installatie - ongeveer acht minuten met kabelnetwerk - en herstart in de launcher.

De stick heeft dan een `AUTOBLEEM`-partitie voor uw games, zichtbaar op Windows 10 (1903 en nieuwer) als een tweede station wanneer u de stick in een lopende PC steekt. `autobleem.txt` staat op de eerste partitie, met dezelfde sleutels als op de Pi (geen `hdmi_mode` - de PC gebruikt de native modus van het scherm).

### 2.4 Windows

AutoBleem als Windows-programma: volledig scherm, emulatoren en RetroArch gestart als programma's.

1. Download **AutoBleemSetup-<version>.exe** en voer uit. Het installeert per gebruiker, zonder beheerdersrechten: het programma onder `%LOCALAPPDATA%\Programs\AutoBleem`, gegevens (games, instellingen, thema's, RetroArch) in een map van uw keuze - standaard `Documents\AutoBleem`.
2. Vink componenten aan - artwork-databases, RetroArch (het officiële Windows-build en cores), BIOS-bestanden, voorbeeldspellen - en laat de setup-helper deze downloaden.
3. Start AutoBleem vanuit het Startmenu of Bureaublad. Op een PC werkt het toetsenbord als een controller (sectie 3.2).

Het uitvoeren van een nieuwere setup erover werkt het programma bij en behoudt de gegevensmap. De launcher controleert ook de website eenmaal per dag en biedt een update aan wanneer er een is (sectie 3.11).

<!-- pagebreak -->

## 3. AutoBleem gebruiken

### 3.1 De launcher

De launcher opent op de boekenplank: de hoezen van de huidige set, degene die in het midden is geselecteerd, de details ervan - uitgever, jaar, serienummer, regio, spelers, wanneer het voor het laatst is gespeeld - en een playknop. De balk onderaan toont wat de knoppen doen. Een scan van de gamemap wordt op elke start op de achtergrond uitgevoerd; terwijl deze wordt uitgevoerd, toont een bubble rechtsboven de voortgang, en verschijnen nieuwe games op de boekenplank naarmate ze worden gevonden.

![De set-selector: drie tabbladen en de groepen van het huidige tabblad met hun aantallen](../images/en/set-picker.jpg)

### 3.2 Besturing

| Knop | Op de boekenplank |
|---|---|
| Links / Rechts | Vorig / volgende spel. Ingedrukt houden scrolt. |
| L1 / R1 | Spring naar de vorige / volgende eerste letter van titels. |
| Cross | Start het geselecteerde spel (PS1-spel in PS1-emulator; RetroArch-spel in zijn core; App na leesmij). |
| Square | Start het geselecteerde PS1-spel in RetroArch. |
| Triangle | Buttonhulpgids. |
| Start | Willekeurig spel uit de huidige set. |
| Select | Set-selector: PlayStation / RetroArch / Apps tabbladen (L1 / R1), groepen van het tabblad (Omhoog / Omlaag, L2 / R2 pagina's), Cross selecteert. |
| Omlaag | Open de iconrij onder het spel (Instellingen, Spel, Geheugenkaart, Hervatten). Omhoog sluit. |
| L2 + R2 | Systeemmenu (sectie 3.4). |

**Met toetsenbord** (PC zonder controller, of USB-toetsenbord op console, Pi of PC-stick): toetsen vervangen: **Pijltjestoetsen** = d-pad, **Enter** = Cross, **Esc of Backspace** = Circle, **Tab** = Triangle, **Spatiebalk** = Square, **F1 / F2** = Select / Start, **Page Up / Page Down** = L1 / R1, **Home / End** = L2 / R2, **F10** = systeemmenu. Op een ontwikkelingsmachine sluit Esc het programma en Space is Start.

In elke lijst en menu: Omhoog / Omlaag verplaatsen, **L2 / R2 pagina's draaien**, L1 / R1 naar eerste / laatste rij, **Cross selecteert, Circle terug**. Een scherm met instellingen slaat deze op wanneer u het met Circle verlaat.

![De iconrij onder het geselecteerde spel](../images/en/launcher-icons.jpg)

### 3.3 De sets

**Select** opent de set-selector. Het PlayStation-tabblad vermeldt *Alle Games*, *Interne Games* (de ingebouwde twintig op PlayStation Classic), elke map die u maakte onder `Games/` (een spel in een submap behoort tot die groep), *Favoriete Games*, *Spelgeschiedenis* en, wanneer een spel zo is gemarkeerd, *Lightgun Games*. Het RetroArch-tabblad vermeldt één groep per systeem met games, plus RetroArch's eigen Favorieten en Geschiedenis. Het Apps-tabblad groepeert applicaties op type: *Alle apps*, vervolgens *Games*, *Emulatoren*, *Tools*, *Media* en *Overig* (de categorie wordt ingesteld in het `app.ini`-bestand van elke app). Elke rij toont hoeveel items zij bevat; een groep zonder items opent op een lege boekenplank met alleen Instellingen in de iconrij.

### 3.4 Het snelmenu

**Omhoog** in de launcher, of het **tandwielpictogram** in de iconrij (waar Instellingen / Spel / Geheugenkaart / Hervatten zijn): het snelmenu voor acties die u snel wilt bereiken. Een korte lijst: *Spellen opnieuw scannen* (start nu een scan), *Store* (AutoBleem Store voor extensies), *Netwerk en Controllers* (alleen waar een geïnstalleerde extensie de `network`-vermelding levert - PSC-Bios op console, Pi en PC-stick: WiFi, Bluetooth-koppeling, controller-mapping-wizard - zie hoofdstuk 6; grijs met "enable it in Extensions" wanneer die extensie is uitgeschakeld - Cross opent de Extensies-lijst) en *Systeemmenu...* (het volledige menu hieronder). Omhoog / Omlaag verplaatsen (rond), Cross selecteert, Circle terug. Hier is niets uniek - elk item staat ook in het systeemmenu.

### 3.5 Het systeemmenu

**L2 + R2** (samen, in willekeurige volgorde) opent het systeemmenu over de boekenplank. Het menu is ingedeeld in secties:

| Sectie | Item | Wat het doet |
|---|---|---|
| (bovenkant) | Spellen opnieuw scannen | Zoekt nu naar nieuwe, gewijzigde of verwijderde spellen (de scan controleert de map ook zelf). |
| | Extensies | De extensies op de stick - AutoBleem Store en anderen (sectie 3.12). |
| **Bibliotheek** | Gamebeheerder | PS1-spellen als lijst met hun mappen: verwijder een spel, wis artwork. Uitgeschakeld terwijl scan wordt uitgevoerd. |
| | Geheugenkaarten | Uw geheugenkaartsets (sectie 3.7). |
| | Scanner-processors | Programma's die elke scan eerst uitvoert - hun volgorde, aan of uit (sectie 3.13). Uitgeschakeld terwijl scan wordt uitgevoerd. |
| **Systeem** | Instellingen | AutoBleem's instellingen (sectie 3.6). |
| | Netwerk en Controllers | Alleen waar een geïnstalleerde extensie de `network`-vermelding levert (`Provides=network` in zijn `extension.ini` - PSC-Bios op console, Pi en PC-stick) - WiFi, Bluetooth-controller-koppeling, DualShock 3-setup en controller-mapping-wizard - zie hoofdstuk 6. Wanneer die extensie is geïnstalleerd maar uitgeschakeld, blijft dit item grijs met "enable it in Extensions" - Cross opent de Extensies-lijst ernaartoe. |
| | Hardwaregegevens | Machinefeiten: systeem, CPU, opslag, netwerkinterfaces, tijdzone, beeldscherm, controllers en hun toewijzingen. Op een PlayStation Classic met de AutoBleem-kernel opent **PSC-Bios** (hoofdstuk 6); op andere machines toont deze informatiepagina. |
| | Software-update | (Raspberry Pi en PC) Controleer de website nu op nieuwere AutoBleem of RetroArch. |
| | Over | Credits en licentie. |
| **Verlaten** | RetroArch | Verlaat de launcher voor RetroArch's eigen menu. RetroArch sluiten komt terug. |
| | Uitschakelen | Na bevestiging: op console AutoBleem-standby - stick verwijderd, licht rood, Power brengt launcher terug (sectie 2.1); op Pi of PC schakelt machine uit. |

![Het systeemmenu](../images/en/system-menu.jpg)

### 3.6 Instellingen

De instellingen zijn in groepen; Omhoog / Omlaag beweegt tussen groepen, Links / Rechts verandert een waarde, Circle verlaat en slaat op. Elke wijziging wordt onmiddellijk toegepast.

| Groep / Instelling | Wat het doet |
|---|---|
| **Interface**: AutoBleem Theme | Het uiterlijk. Thema's staan in `Themes/`; een theme-zip die daar wordt neergezet, wordt bij het volgende bezoek uitgepakt. AutoBleem's thema's worden bij elke update vernieuwd - om er een aan te passen, kopieert u deze eerst onder een nieuwe naam. |
| Cover Style | Het sieraad-frame rond PS1-hoezen. |
| Language | Launchers taal, direct toegepast (17 talen). |
| Use Font from Theme / Font | Lettertype van klassieke schermen: die van het thema, of een `.ttf`/`.otf` van `resources/fonts`, `RetroArch/fonts` of themamapje. |
| Showing Timeout | Hoe lang de "Showing: ..."-melding blijft, in seconden (0 = altijd). |
| **Sound**: Music, Background Music | Welk nummer onder de launcher speelt (die van het thema, of een bestand uit `resources/music`), en of er überhaupt een speelt. |
| **Emulation**: PS1 Emulator | `pcsx-abnxt` (standaard: huidge PCSX-ReARMed met AutoBleem-toevoegingen) of `pcsx-ab` (klassiek). Een opgeslagen hervattingspunt gaat verder in de ander, behalve als het spel zonder BIOS-bestand liep. |
| Widescreen | De beeldverhoudingsaanpassing van PS1-emulator voor elk spel. |
| Play all PSX games with RA | Elk PS1-spel start in RetroArch's PS1-core. |
| Update RA Config | AutoBleem schrijft zijn instellingen in RetroArch's config wanneer het daar een spel start. |
| **Library**: Show Internal Games | De ingebouwde games van de console in PlayStation-lijsten (alleen PlayStation Classic). |
| Fetch box art online | De scan haalt ontbrekende hoezen van libretro's servers (Raspberry Pi, PC, Windows). |
| **Updates** | (Raspberry Pi, PC, Windows) `stable`, `latest` (ook pre-releases) of `off`. |

![Instellingen in groepen](../images/en/options.jpg)

### 3.7 Instellingen van een spel

Met een spel geselecteerd, **Omlaag** opent de iconrij ervan: **Instellingen** (hierboven), **Spel** (de eigen instellingen), **Geheugenkaart** (de geheugenkaart ervan) en **Hervatten** (de savestates). Cross opent degene onder de cursor.

De **gameeditor** toont rechts de details van het spel en links de instellingen in drie groepen:

- **Spel**: *Favoriet* (in Favoriete Games-groep), *Lightgun-spel* (een lightgun-spel - het voegt zich bij Lightgun-groep en loopt altijd in RetroArch, waarvan de PS1-core GunCon heeft), *Met RA afspelen* (dit spel loopt in RetroArch), *Gegevens vergrendelen* (scanner handhaaft gametitel, serienummer en schijflijst zoals u ze ingesteld hebt).
- **Video**: hoge resolutie, scanlines en hun niveau, frame skip, GPU-plugin en *Filter* - hoe het beeld wordt geschaald: Uit (zuivere pixels), Lineair (glad) of Scherp (scherpe pixels zonder shimmer; alleen `pcsx-abnxt` - klassiek `pcsx-ab` en RetroArch tonen als Uit).
- **Emulator**: SpeedHack, CPU-klok, SPU-interpolatie, startlogo (uit slaat BIOS-shell over - voor homebrewschijf waarvan aangepast logo boot breekt) en met `pcsx-abnxt` *Smoothing*-filter en *Sony hacks*-toggle.

Triangle hernoemt het spel, Square wijzigt de geheugenkaart, Start deelt een nieuwe. Circle slaat op en verlaat.

**Instellingen opgeslagen in de emulator.** Het eigen menu van de emulator heeft *Instellingen voor dit spel opslaan*. Als een spel instellingen daar heeft opgeslagen, zijn die wat het speelt, en de gameeditor toont rijen Video en Emulator grijs met die waarden onder titel *Opgeslagen in emulator*. Om terug te gaan naar gameeditor's instellingen, kiest u **Instellingen ontgrendelen** en bevestigt: dit verwijdert de door emulator opgeslagen instellingen, en kunnen die rijen opnieuw worden gewijzigd. Beide emulatoren, `pcsx-ab` en `pcsx-abnxt`, lezen en schrijven dezelfde opgeslagen instellingen.

![Gameeditor](../images/en/game-editor.jpg)

### 3.7 Geheugenkaarten en savestates

Elk PS1-spel heeft standaard zijn eigen geheugenkaart (bijgehouden met savestates in `Games/!SaveStates/<game folder>/`). **Geheugenkaarten** in systeemmenu beheert **gedeelde sets** - kaarten die meerdere spellen gebruiken, bijgehouden in `Games/!MemCards/`: maak er een (Square, met schermtoetsenbord), hernoem (Cross), verwijder (Triangle). Een spel wordt op een set geplaatst met *Geheugenkaart wijzigen* in de editor, of vanuit Geheugenkaart-pictogram.

De **geheugenkaarteditor** (het Geheugenkaart-pictogram) toont de kaart van het spel en een tweede kaart naast elkaar, met het pictogram en titel van elk opslag: kopieer een opslag tussen de twee (Square), verwijder een (Triangle), defragmenteer een kaart (Select). Start ruilt de kaart aan de rechterkant voor een ander set.

![Geheugenkaarteditor](../images/en/memory-card-editor.jpg)

**Hervattingspunten**: wanneer u een PS1-spel verlaat met de Reset-knop van de console (of het menu van de emulator op Pi of PC), bewaart AutoBleem een savestate van waar u was en biedt het onder het **Hervatten**-pictogram aan - vier slots, elk met een afbeelding van het moment. Cross gaat verder met de slot, Triangle verwijdert het. Een spel met een hervattingspunt toont een klein afbeelding op zijn Hervatten-pictogram.

### 3.8 Spellen, RetroArch en Apps starten

**Cross** start het geselecteerde spel. Een PS1-spel loopt in de gekozen PS1-emulator (sectie 3.5), volledig scherm, totdat u het verlaat - op de console met de voorkant **Reset**-knop (terug naar launcher met hervattingspunt) of **Power** (console schakelt uit); op Pi of PC via het in-game menu van de emulator (Select + Start op de controller, of Esc op toetsenbord). **Square** start een PS1-spel in RetroArch.

Een **RetroArch**-spel start in RetroArch met de core die de launcher voor zijn systeem koos; *Close Content* of *Quit RetroArch* in het menu komt terug naar launcher. Het RetroArch-item in het systeemmenu opent RetroArch's eigen menu (XMB) met niets geladen, voor zijn instellingen en eigen content-lijsten.

Een **App** (de Apps-set: de console tools, en op een console de extra toepassingen die RetroArch's pakket meebrengt - Doom, Quake, Amiga, ...) toont eerst zijn leesmij; Cross start het, Circle gaat terug.

![Leesmij van een App voordat het start](../images/en/app-start.jpg)

### 3.9 Spellen toevoegen

**PS1-spellen** gaan in de `Games`-map, **één map per spel**, genoemd naar het spel:

```
Games/
  Crash Bandicoot/            Crash Bandicoot.cue + Crash Bandicoot.bin
  Final Fantasy VII/          Final Fantasy VII (Disc 1).chd, (Disc 2).chd, (Disc 3).chd
  Platformers/                een map van spellen: een eigen groep in de set-selector
    Klonoa/                   Klonoa.pbp
```

- Formaten: `.cue` + `.bin` (of `.img`), `.pbp`, `.chd` (zstd ook), `.ecm` (gedecodeerd door de scan), `.iso`. Een gezipt spel werkt ook: de **Unzip**-processor pakt het uit voordat de scan (sectie 3.13).
- Een multi-disc-spel is één map met elk disc erin; mappen genoemd `Game (Disc 1)`, `Game (Disc 2)` ... worden samengevoegd tot één `Game`-map door de scan.
- Spellen direct in `Games/` gezet (losse bestanden) worden door de scan in mappen gesorteerd.
- Een **cover** is een PNG naast de afbeelding van het spel, genoemd ernaar. Zonder een wordt de artwork uit de cover-databases genomen, of - wanneer RetroArch is geïnstalleerd - uit libretro's thumbnail-set; op Pi, PC of Windows wordt een ontbrekende online gehaald (Opties → *Fetch box art online*).
- De scan leest het serienummer van elk disc en haalt titel, uitgever, jaar, spelers en regio uit RetroArch's PlayStation-database of de cover-databases. Wijzig iets in de gameeditor en vink *Gegevens vergrendelen* aan om het te houden.

**Andere systemen** gaan onder `RetroArch/roms/`, **één map per systeem, genoemd als RetroArch's databases** (de map wordt voor u gemaakt): `Nintendo - Nintendo Entertainment System`, `Nintendo - Super Nintendo Entertainment System`, `Sega - Mega Drive - Genesis`, `Nintendo - Game Boy Advance`, `FBNeo - Arcade Games` (of `Arcade`), ... ROM's mogen gezipt blijven. Op Pi, PC of Windows leest de scan ze zelf en schrijft RetroArch's speellijsten; op een console-stick, voer **UpdateRoms** op de PC uit (hoofdstuk 5).

**Apps** gaan onder `Apps/<name>/` met een `app.ini` (de naam, het pictogram, wat te draaien) en een `run.sh`.

**Thema's** gaan onder `Themes/<name>/` (`theme.json` en de afbeeldingen) - of zet de theme-zip in `Themes/`.

### 3.10 De PS1-BIOS

Op een **PlayStation Classic** gebruikt de emulator de BIOS van de console zelf. Op een **Raspberry Pi, PC en Windows** zet uw eigen PS1-BIOS in `System/Bios/`: `romw.bin` (de US/Europese SCPH-5501/5502) en `romJP.bin` (de Japanse SCPH-5500). De installers vullen ze uit de RetroArch-BIOS-packs tenzij uw eigen bestanden al daar zijn. Zonder hen draait de emulator op zijn ingebouwde HLE-BIOS, dat veel spellen tolereren en sommige niet.

### 3.11 Updates

- **Raspberry Pi, PC-stick, Windows**: de launcher controleert de website bij start en eenmaal per dag (Opties → *Updates* is het kanaal; *Software-update* in het systeemmenu controleert nu). Wanneer er een nieuwere AutoBleem of RetroArch is, vraagt het: *Update nu* downloadt alles en voert de installer opnieuw uit met het first-boot-progress-scherm; *Remind me tomorrow* en *Skip this version* zijn de andere antwoorden. Uw games en instellingen blijven; de launcher herscant eenmaal na een update.
- **PlayStation Classic**: voer een nieuwere `AutoBleemInstaller.exe` over de stick uit (sectie 2.1).

### 3.12 Extensies en de AutoBleem Store

**Extensies** voegen hun eigen schermen toe aan de launcher. Ze staan in `Extensions/<name>/` op de stick (op Pi de data-partitie, op Windows de data-map); om er een te installeren, pakt u de zip daar uit. **L2 + R2 → Extensies** toont ze: Cross voert er een uit, Triangle schakelt het uit of aan. Een extensie die het netwerk nodig heeft wordt niet gestart zonder een, en een die de launcher stopte wordt uitgeschakeld - de lijst zegt dit.

![De Extensies-lijst](../images/en/extensions.jpg)

**De AutoBleem Store** is de eerste extensie: Apps en spellen te installeren met één druk, op elk systeem waarop AutoBleem loopt (een PlayStation Classic heeft de AutoBleem-kernel's WiFi nodig). Zijn vier tabbladen, L1 / R1 ertussen:

- **Apps** en **Games**: wat de bronnen aanbieden, elk met afbeelding, versie, grootte en bron favicon. Geïnstalleerde items zijn grijs. Cross installeert (of werkt bij, of probeert opnieuw na mislukking), Triangle verwijdert wat de Store installeerde. L2 / R2 of Links / Rechts draaien pagina's, **Select** toont één bron tegelijk, **Start** doorzoekt de titels. Item-afbeeldingen worden gecacht en kunnen opnieuw proberen als ze niet laden.
- **Downloads**: wat downloadt, wacht, mislukt of is geïnstalleerd. De voortgangsbalk werkt constant. Downloads gaan door op de achtergrond, ook als u de Store verlaat; een spel starten of uitschakelen pauzeert ze alleen, en een gestopt downloaden gaat verder waar het stopte. Een geïnstalleerd spel verschijnt op de boekenplank na volgende scan, met de afbeelding van de Store als cover. Downloads groter dan 2 GB werken op alle platforms, inclusief 32-bits builds.
- **Bronnen**: waar de lijsten vandaan komen - AutoBleem's eigen catalogus, een TSV-lijst in `System/Extensions/store/sources/` geplaatst, en adressen die u met **Add a source URL** toevoegt. Elke bron toont zijn favicon in de lijst. Cross op een die u toevoegde hernoemt het, wijzigt het adres, schakelt het tussen `http://` en `https://`, of verwijdert het.

![De Store-Apps-tabblad](../images/en/store-apps.jpg)

![Menu van een bron](../images/en/store-source-menu.jpg)

Wat AutoBleem's catalogus aanbiedt wordt ook op de download-site vermeld, `https://autobleem.retromenele.pl/store/`. **U bent verantwoordelijk voor wat de bronnen die u toevoegt bevatten.**

**Uw eigen spellen op uw netwerk**: `abstored`, de Store's LAN-server, serveert een map van PS1-spellen aan de Store op hetzelfde netwerk. Het loopt op elke Linux-machine - een Raspberry Pi, een thuisserver - en leest alleen de map. Start het met `abstored <games folder>`, open `http://<that machine>:8124/` in een browser om te zien wat het serveert en eventuele problemen, en voeg `http://<that machine>:8124/store.tsv` toe als bron. Gereedde programma's voor Linux en Windows staan op de Store-pagina, in zijn **LAN server**-tabblad; het instellen als service is `INSTALL-linux.md` (`ext_store/server/` in de bron). **LAN Share** (sectie 5.2) zet spellen en discs van een PC op zo'n server.

### 3.13 Scanner-processors

**Scanner-processors** zijn kleine programma's die elke scan uitvoert voordat hij uw spellen leest. Een kan een formaat dat AutoBleem niet leest in een formaat zetten dat het wel doet - een gezipt spel bijvoorbeeld - of de gegevens van een spel wijzigen, zoals een vertalingspatch. Ze staan in `System/Processors/<name>/` op de stick (op Pi de data-partitie, op Windows de data-map); om er een te installeren, pakt u de map daar uit. De volgende scan voert het uit.

- **Unzip komt met AutoBleem**: het pakt gezipte PS1-spellen in `Games/` uit voordat de scan ze leest, en gezipte ROM's één voor één (arcade-sets blijven gezipt). AutoBleem bijwerken werkt het ook bij, en laat het uitgeschakeld als u het uitschakelde.
- Een processor die al met een spel heeft omgegaan wordt niet opnieuw uitgevoerd totdat het spel wijzigt.
- Terwijl een processor werkt, toont de bubble rechtsboven wat het doet; een waarschuwing of mislukking verschijnt op de lijn eronder. `processors.log` in de log-map heeft de details.
- Het starten van een spel of RetroArch stopt een processor die bestanden wijzigt; de volgende scan maakt zijn werk af.

**L2 + R2 → Scanner-processors** toont ze in de volgorde waarin ze lopen, één tabblad voor PS1-spellen en één voor ROM's (L1 / R1). **Square** pakt een processor op en Omhoog / Omlaag verplaatsen het - de volgorde doet ertoe: een processor die uitpakt moet voor een die patches wat was uitgepakt. **Cross** schakelt er een uit of aan, **Triangle** laat het naar elk spel kijken bij volgende scan, **Circle** gaat terug en start een scan als u iets wijzigde. Een processor gebouwd voor een ander toestel blijft in de lijst, grijs.

![Scanner-processors](../images/en/processors.jpg)

Uw eigen schrijven: Unzip's pagina, `https://github.com/autobleem2/proc_unzip`, legt alles uit wat een processor moet doen, en `tools/proc_check.py` in AutoBleem's bron controleert er een voordat u deze deelt.

<!-- pagebreak -->

## 4. Schermen

### 4.1 Gamebeheerder

De PS1-spellen als lijst met hun mappen en de cover en details van degene die is geselecteerd. Cross opent de gameeditor, **Square verwijdert het spel** (zijn map en, na een tweede vraag, zijn savestates), Triangle verwijdert elke cover-PNG naast de spellen (de scan haalt ze opnieuw uit de databases), L2 / R2 pagina. De vrije ruimte van het station staat rechtsboven. Gamebeheerder wacht terwijl een scan loopt.

![Gamebeheerder](../images/en/game-manager.jpg)

### 4.2 Hardwaregegevens

De machinefeiten - systeem, hardware, opslag met vrije ruimte, netwerkadres, de beeldscherm- en audiostuurprogramma's, de aangesloten controllers - opnieuw gelezen elke seconde. Op een PlayStation Classic met de AutoBleem-kernel opent dit item **PSC-Bios** (hoofdstuk 6).

De eerste twee controllers worden weergegeven als Speler 1 en Speler 2 – de poorten die de PS1-emulator aan hen toekent.
Elke verdere controller wordt weergegeven als niet gebruikt door de PS1-emulator. RetroArch wijst controllers toe
volgens zijn eigen instellingen en kan ze anders ordenen. Wanneer een controller wordt ingesteld of verwijderd, toont
de launcher kort welke pad Speler 1 en Speler 2 is.

![Hardwaregegevens](../images/en/hardware-info.jpg)

### 4.3 De buttonhulpgids

Triangle op de boekenplank: elke button van elk scherm op één pagina. Wanneer een USB-toetsenbord is aangesloten of is gebruikt, toont een Toetsenbord-kolom de toetsen naast de controller-knoppen.

![De buttonhulpgids](../images/en/button-guide.jpg)

### 4.4 Het schermtoetsenbord

Overal waar tekst wordt getypt - een geheugenkaartset, titel van een spel, WiFi-wachtwoord, adres van een bron - hetzelfde toetsenbord, uitgelegd als dat van een telefoon: letters, een pagina symbolen (`/ \ : ? & = % @ #` en de rest wat een adres of wachtwoord nodig heeft) en twee pagina's met accenten, met Shift, de pagina-toets, Spatiebalk, Backspace en Bevestigen op de onderste rij. De richtingen verplaatsen, Cross typt, Triangle verwijdert, Square is een spatie, **L1** is Shift (twee keer voor caps lock), **R1** volgende pagina, **L2 / R2** cursor verplaatsen, Start bevestigt, Circle annuleert. Een USB-toetsenbord typt op elk moment: Enter bevestigt, Esc annuleert.

![Het schermtoetsenbord](../images/en/keyboard.jpg)

<!-- pagebreak -->

## 5. Op de PC

### 5.1 UpdateRoms - console-stick vernieuwen

De PlayStation Classic heeft geen netwerk, dus de RetroArch-lijsten en artwork van zijn stick worden op de PC gemaakt: **UpdateRoms** doet op de PC wat de scan van de launcher op Pi doet, met het netwerk van de PC en de paden van de console, dus de console start en vindt alles op zijn plaats.

1. Kopieer uw ROM's op de stick onder `RetroArch/roms/<system>/` (sectie 3.9). De mapnamen moeten RetroArch's database-namen zijn; de installer maakt de gebruikelijke.
2. Start **`UpdateRoms\UpdateRoms.exe` van de stick** (de installer zette het daar neer). Het vindt de stick van waar het zit, toont een stagelijn, voortgangsbalk en log, en:
   - downloadt RetroArch's database-bundel wanneer de stick geen heeft, en identificeert elk ROM daarmee - een spel dat de database kent krijgt zijn juiste naam;
   - schrijft één speellijst per systeem in `RetroArch/bin/playlists/` met de paden van de console, behoudt wat RetroArch zelf daar toevoegde;
   - haalt de artwork van elk ROM dat geen heeft van libretro's thumbnail-servers in `RetroArch/bin/thumbnails/`.
3. Haal de stick veilig eruit en zet deze terug in de console. Het RetroArch-tabblad van de set-selector toont elk systeem dat spellen heeft.

Voer het opnieuw uit na elke wijziging in de ROM-mappen; een map waarin niets wijzigde wordt overgeslagen, dus een herloop is snel. Het log is `System/Logs/updateroms.log`. Een Raspberry Pi-kaart in een kaartlezer kan dezelfde manier worden vernieuwd (`UpdateRoms.exe <drive> --target rpi`), hoewel Pi dit zelf doet wanneer het een netwerk heeft.

### 5.2 LAN Share - uw spellen en discs op de server op uw netwerk

**LAN Share** (`LanShare.exe`, op de Store-pagina in zijn **LAN server**-tabblad) zet uw PS1-spellen op de Store's server op uw thuisnetwerk - een `abstored` op Raspberry Pi, een NAS of een andere PC - en leest een PS1-disc in het CD/DVD-station van de PC ervan. De Store op de console, Pi of PC installeert ze dan van daar. Niets te installeren; de instellingen worden bijgehouden in `%LOCALAPPDATA%\AutoBleem LAN Share\`.

![Het LAN Share-venster](../images/en/lanshare.jpg)

1. **De server**: voer het adres in (`http://<its address>:<port>`, zoals de Store het heeft) en druk **Connect**. Zijn spellen en eventuele problemen die zijn scan vond worden aan de linkerkant vermeld. Om spellen erop te zetten, geeft u een van:
   - **Share** - de map van de server's spellen zoals deze op het netwerk is gedeeld (Samba), bijv. `\\raspberrypi\games`: LAN Share kopieert de spellen daar en vraagt de server om te scannen. De server zelf blijft read-only.
   - **Token** - wanneer de server is gestart met `--allow-uploads`: het token ervan (de server drukt het af bij start en houdt het in `<state>/upload-token`). LAN Share uploadt via HTTP, en een gestopt upload gaat verder waar het stopte.
2. **Spellen op deze PC**: kies een map met spellen (één map per spel), vink spellen aan en druk **Publish the ticked games**. **On the server** zegt of de server al een spel heeft (op serienummer, anders op titel); zo'n spel wordt nooit twee keer verzonden. **Tick those not on the server** vink de rest aan.
3. **Een disc**: zet een PS1-disc in het station en druk **Read a disc and publish it**. De disc wordt heel gelezen in een `.bin` + `.cue` (en een `.sbi` voor een LibCrypt-spel, wanneer het station de subkanaal geeft), genoemd naar zijn titel, gecontroleerd tegen de bekende goede dump (wanneer databases zijn gekozen) en gepubliceerd. Voor een spel op meerdere discs vink **The game has more than one disc** aan: LAN Share vraagt om elke volgende disc en publiceert ze samen als één spel.
4. **Remove from the server...** haalt de geselecteerde spellen van de server. Niets wordt verwijderd: elk wordt verplaatst naar een `.removed`-map naast de server's spellen, en het erterug verplaatsen zet het terug.

De **Databases** - AutoBleem's covers-map (`coversU/P/J.db`) en RetroArch's `Sony - PlayStation.rdb` - geven de titels en de controle van een gelezen disc; beide zijn optioneel. **Also share the games on this PC with the Store** (standaard uit) serveert de map op deze PC aan de Store rechtstreeks. De eerste keer vraagt Windows over zijn firewall: sta alleen particuliere netwerken toe.

<!-- pagebreak -->

## 6. De console tools (PlayStation Classic)

Twee tools voor een PlayStation Classic-stick. Beide tekenen in het thema en de taal van de launcher, en beide worden door de controller aangestuurd - en in de controller-wizard door de voorkant-knoppen van de console. **PSC-Bios** is een extensie die met het console-pakket komt: *Hardwaregegevens* in het systeemmenu opent het, en het staat in de Extensies-lijst. **ABFlashKit** is een App in de Apps-set.

### 6.1 PSC-Bios

Een extensie die met het console-pakket komt, ook beschikbaar op Raspberry Pi en de PC-stick. Deze wordt geopend vanuit het item *Netwerk en Controllers* van het systeemmenu (of vanuit de Extensies-lijst). Wanneer die extensie is geïnstalleerd maar uitgeschakeld, is het item *Netwerk en Controllers* in het snelmenu en systeemmenu grijs met opmerking "enable it in Extensions" - Cross daar opent de Extensies-lijst ernaartoe.

Het openingsscherm toont machinefeiten: tijd, tijdzone, WiFi/Ethernet/Bluetooth-netadapters met hun adressen, en elke aangesloten controller met of deze een button-mapping heeft. De netwerk- en Bluetooth-onderdelen hebben de AutoBleem-kernel op de console nodig (sectie 6.2) of systeemtools op Raspberry Pi / PC-stick; de controller-wizard werkt op elk systeem.

![PSC-Bios: het Netwerk en Controllers-centrum](../images/en/pscbios-main.jpg)

- **Select - Wi-Fi Network** (kernel of NetworkManager): de netwerknaam (getypt, of gekozen uit scan), wachtwoord, bestuurdersmodus, en *Apply / Restart Network*. De tijdzone wordt hier ook ingesteld. Het IP-adres van de console wordt weergegeven zodra verbonden.
- **Square - Bluetooth Controllers**: een scan voor Bluetooth-controllers (DualShock 4, enz.), om te koppelen of te verwijderen.
- **L1 - DualShock 3 Coupling**: USB-enige verbinding voor de eerste DualShock 3, via de sixaxis-plugin van de kernel.
- **R1 - Controller Mapping**: de mapping-wizard (hieronder).
- **Triangle - About**, **Circle - back** naar launcher.

**De controller-wizard** toont de aangesloten controller rauw - elke as, button en hat als getallen, en een DualShock-afbeelding die oplicht als u drukt. Omdat de controller onder test niet kan worden vertrouwd, wordt de wizard aangestuurd door de **voorkant-knoppen** van de console: **RESET** gaat naar de volgende controller, **OPEN** start de mapping (beantwoordt dan elke vraag - druk de knop die op de afbeelding oplicht in, of OPEN wanneer de controller die knop niet heeft), **POWER** annuleert of verlaat. Circle 2 seconden ingedrukt houden op de controller verlaat de wizard (een balk vult en de footer zegt "Hold 2 s: Exit"). Terwijl de controller nog geen mapping heeft, elke knop 2 seconden ingedrukt houden doet het ("Hold any button 2 s: Exit"). Een kort indrukken wordt normaal gemapped. Op toetsenbord, Esc / Spatiebalk / Enter staan in voor POWER / RESET / OPEN. Aan het einde wordt de nieuwe mapping toegevoegd voor een test en OPEN slaat het op onder een naam van uw keuze; de launcher laadt het er van dan af.

![PSC-Bios: de controller-mapping-wizard](../images/en/pscbios-wizard.jpg)

### 6.2 ABFlashKit - de AutoBleem-kernel

De AutoBleem-kernel is een optionele vervanging van de Linux-kernel van de console: het brengt een werkende klok, USB-WiFi en Bluetooth-dongles (voor PSC-Bios en Bluetooth-controllers) en ondersteuning voor voorkant-knoppen die de emulator voor hervattingspunten gebruikt. ABFlashKit installeert het, back de console eerst up, en kan de console teruggaan naar stock via Sony's eigen recovery.

> **Dit hulpprogramma schrijft naar de flash-geheugen van de console.** Een flash die wordt onderbroken - stroom uitgetrokken, stick uitgetrokken - kan de console onbruikbaar maken, en het installeren van een aangepaste kernel stelt de garantie ongeldig. Houd de console ingeschakeld en de stick erin totdat het zichzelf opnieuw opstart. ABFlashKit opent deze waarschuwing; *I understand* gaat verder, *Quit* verlaat.

![ABFlashKit-menu](../images/en/abflashkit-menu.jpg)

- **Flash Kernel**: maakt een recovery-backup van de console-partities op de stick (`LBOOT.EPB`) als die nog niet bestaat, controleert deze en de kernel-afbeelding, schrijft de kernel en AutoBleem's systeembestanden, en herstart. *All done - when the screen goes black replace power cord*: trek de stroom van de console uit en steek deze weer in.
- **Full backup**: alle vier partities in `LBOOT.EPB`, voor later terugzetten (de vorige backup wordt na een vraag overschreven).
- **Restore Mode**: controleert dat de backup een originele is, stelt de recovery-vlag in en herstart in Sony's recovery, die de console van `LBOOT.EPB` op de stick herstelt - de manier terug naar originele firmware.

Een voortgangsbalk onder elke stap toont hoe ver de actie is. Het hulpprogramma weigert een console te flashen die andere aangepaste firmware draait (BleemSync, Project Eris): zet het eerst terug naar stock.

<!-- pagebreak -->

## 7. Als iets fout gaat

- **Logs**: AutoBleem bewaart logs in geheugen, zodat de stick niet constant wordt weggeschreven - ze bereiken `System/Logs/` op de stick, kaart of data-map alleen wanneer iets fout gaat: een crash van de launcher, PS1-spel of RetroArch slaat deze op in `System/Logs/crash-<n>/` (de laatste drie worden behouden), en de launcher zegt dit eenmaal wanneer het terugkomt. Om elk log te houden, schakelt u *Options -> Diagnostics -> Keep logs on the stick* in (van de volgende start af), of maakt u een leeg bestand `System/Logs/keep` op een PC. Op Pi of PC toont *Hardwaregegevens* waar logs zijn en Square slaat deze op in `System/Logs/saved-<n>/`. De bestanden: `autobleem.log` (launcher), `launch.log` en `pcsx.log` (PS1-spel starten en emulator-output), `retroarch.log`, en - altijd op stick - `update.log` (online update) en `updateroms.log` (UpdateRoms).
- **Spel staat niet op boekenplank**: controleer de mapindeling (één map per spel, de afbeeldingsformaten van sectie 3.9). De *Gamebeheerder* toont de mappen die de scan weigerde na de spellen, gemarkeerd als *Not added*, met de reden; Square verwijdert zo'n map. Rescan in het systeemmenu voert de scan opnieuw uit.
- **Geen artwork**: de artwork-databases zijn niet geïnstalleerd (voer de installer opnieuw uit met deze aangevinkt), of, voor RetroArch-spellen op een console, UpdateRoms is niet op de PC uitgevoerd.
- **Controller doet niets of heeft knoppen verwisseld**: PSC-Bios's controller-wizard (console) kaart het in; op Pi of PC toont de Hardware Information-pagina wat SDL ziet.
- **Console toont zwart scherm na spel**: AutoBleem herbouwt zijn venster zelf (tot drie keer); als het zwart blijft, houd de Power-knop ingedrukt en zet de console opnieuw aan.
- **Raspberry Pi**: `Alt+F2` geeft een login-prompt op de tweede console; SSH is ingeschakeld vanaf eerste boot. `sudo journalctl -u autobleem` toont de launcher's service; `sudo systemctl restart autobleem` herstart het. Een eerste boot die niet kon worden afgerond (geen netwerk) probeert het opnieuw bij volgende boot.
- **Windows**: `Esc` verlaat de launcher; de data-map is die gekozen in de setup (`Documents\AutoBleem` standaard), logs zijn in zijn `System\Logs`.

AutoBleem is vrije software (GNU GPL v3 of hoger), zonder garantie. Ondersteuning en nieuws: de Discord-server gelinkt op het About-scherm, en https://autobleem.retromenele.pl/.

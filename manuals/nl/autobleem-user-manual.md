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

**In- en uitschakelen.** Met de stick erin start de console, knippert het licht enkele seconden (AutoBleem wordt opgehaald) en gaat dan in standby voordat iets wordt weergegeven - dit is de eigen updatemanier van de console, waarmee AutoBleem kan starten. Druk eenmaal op **Power** en de launcher verschijnt. *Power Off* in het systeemmenu of de Power-knop van de console zet de console in **AutoBleem-standby**: de stick wordt eerst verwijderd, dan wordt het licht **rood** - het teken dat AutoBleem goed werkt - en de volgende Power-knop brengt de launcher in enkele seconden terug. **Terwijl het licht rood is, kunt u de stick verwijderen** en op een PC steken zonder dat Windows deze wil controleren; steek deze terug voordat u Power indrukt. Het stroomkabel verwijderen gaat de volgende keer opnieuw door het startup-standby.

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

![De game-set-selector: drie tabbladen en de groepen van het huidige tabblad met aantallen games](../images/en/set-picker.jpg)

### 3.2 Besturing

| Knop | Op de boekenplank |
|---|---|
| Links / Rechts | Vorig / volgende spel. Ingedrukt houden scrolt. |
| L1 / R1 | Spring naar de vorige / volgende eerste letter van titels. |
| Cross | Start het geselecteerde spel (PS1-spel in PS1-emulator; RetroArch-spel in zijn core; App na leesmij). |
| Square | Start het geselecteerde PS1-spel in RetroArch. |
| Triangle | Buttonhulpgids. |
| Start | Willekeurig spel uit de huidige set. |
| Select | Game-set-selector: PlayStation / RetroArch / Apps tabbladen (L1 / R1), groepen van het tabblad (Omhoog / Omlaag, L2 / R2 pagina's), Cross selecteert. |
| Omlaag | Open de iconrij onder het spel (Instellingen, Spel, Geheugenkaart, Hervatten). Omhoog sluit. |
| L2 + R2 | Systeemmenu (sectie 3.4). |

**Met toetsenbord** (PC zonder controller, of USB-toetsenbord op console, Pi of PC-stick): toetsen vervangen: **Pijltjestoetsen** = d-pad, **Enter** = Cross, **Esc of Backspace** = Circle, **Tab** = Triangle, **Spatiebalk** = Square, **F1 / F2** = Select / Start, **Page Up / Page Down** = L1 / R1, **Home / End** = L2 / R2, **F10** = systeemmenu. Op een ontwikkelingsmachine sluit Esc het programma en Space is Start.

In elke lijst en menu: Omhoog / Omlaag verplaatsen, **L2 / R2 pagina's draaien**, L1 / R1 naar eerste / laatste rij, **Cross selecteert, Circle terug**. Een scherm met instellingen slaat deze op wanneer u het met Circle verlaat.

![De iconrij onder het geselecteerde spel](../images/en/launcher-icons.jpg)

### 3.3 De sets

**Select** opent de game-set-selector. Het PlayStation-tabblad vermeldt *Alle Games*, *Interne Games* (de ingebouwde twintig op PlayStation Classic), elke map die u maakte onder `Games/` (een spel in een submap behoort tot die groep), *Favoriete Games*, *Spelgeschiedenis* en, wanneer een spel zo is gemarkeerd, *Lightgun Games*. Het RetroArch-tabblad vermeldt één groep per systeem met games, plus RetroArch's eigen Favorieten en Geschiedenis. Het Apps-tabblad groepeert applicaties op type: *Alle apps*, vervolgens *Games*, *Emulatoren*, *Tools*, *Media* en *Overig* (de categorie wordt ingesteld in het `app.ini`-bestand van elke app). Elke rij toont hoeveel items zij bevat; een groep zonder items opent op een lege boekenplank met alleen Instellingen in de iconrij.

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

Instellingen zijn in groepen; Omhoog / Omlaag beweegt tussen groepen, Links / Rechts verandert een waarde, Circle verlaat en slaat op. Elke wijziging wordt onmiddellijk toegepast.

| Groep / Instelling | Wat het doet |
|---|---|
| **Interface**: AutoBleem Theme | Het uiterlijk. Thema's staan in `Themes/`; een theme-zip die daar wordt neergezet, wordt bij het volgende bezoek uitgepakt. AutoBleem's thema's worden bij elke update vernieuwd - om er een aan te passen, kopieert u deze eerst onder een nieuwe naam. |
| Artwork-stijl | Het sieraad-frame rond PS1-hoezen. |
| Taal | Launchers taal, direct toegepast (17 talen). |
| Font van thema gebruiken / Font | Lettertype van klassieke schermen: die van het thema, of een `.ttf`/`.otf` van `resources/fonts`, `RetroArch/fonts` of themamapje. |
| Showtimeout | Hoe lang de "Showing: ..."-melding blijft, in seconden (0 = altijd). |
| **Geluid**: Muziek, Achtergrondmuziek | Welk nummer onder de launcher speelt (die van het thema, of die van gebruiker uit `resources/music`), en of er überhaupt een speelt. |
| **Emulatie**: PS1-emulator | `pcsx-abnxt` (standaard: huidge PCSX-ReARMed met AutoBleem-toevoegingen) of `pcsx-ab` (klassiek). Een opgeslagen hervattingspunt gaat verder in de ander, behalve als het spel zonder BIOS-bestand liep. |
| Breedbeeld | De beeldverhoudingsaanpassing van PS1-emulator voor elk spel. |
| Alle PSX-spellen met RA afspelen | Elk PS1-spel start in RetroArch's PS1-core. |
| RA-config bijwerken | AutoBleem schrijft zijn instellingen in RetroArch's config wanneer het daar een spel start. |
| **Bibliotheek**: Interne Games tonen | De ingebouwde games van de console in PlayStation-lijsten (alleen PlayStation Classic). |
| Online artwork ophalen | De scan haalt ontbrekende hoezen van libretro's servers (Raspberry Pi, PC, Windows). |
| **Updates** | (Raspberry Pi, PC, Windows) `stable`, `latest` (ook pre-releases) of `off`. |

![Instellingen in groepen](../images/en/options.jpg)

### 3.7 Instellingen van een spel

Met een spel geselecteerd, **Omlaag** opent de iconrij ervan: **Instellingen** (hierboven), **Spel** (de eigen instellingen), **Geheugenkaart** (de geheugenkaart ervan) en **Hervatten** (de savestates). Cross opent degene onder de cursor.

De **gameeditor** toont rechts de details van het spel en links de instellingen in drie groepen:

- **Spel**: *Favoriet* (in Favoriete Games-groep), *Lightgun-spel* (een lightgun-spel - het voegt zich bij Lightgun-groep en loopt altijd in RetroArch, waarvan de PS1-core GunCon heeft), *Met RA afspelen* (dit spel loopt in RetroArch), *Gegevens vergrendelen* (scanner handhaaft gametitel, serienummer en schijflijst zoals u ze ingesteld hebt).
- **Video**: hoge resolutie, scanlines en hun niveau, frame skip, GPU-plugin en *Filter* - hoe het beeld wordt geschaald: Uit (zuivere pixels), Lineair (glad) of Scherp (scherpe pixels zonder shimmer; alleen `pcsx-abnxt` - klassiek `pcsx-ab` en RetroArch tonen als Uit).
- **Emulator**: SpeedHack, CPU-klok, SPU-interpolatie, startlogo (uit slaat BIOS-shell over - voor homebrewschijf waarvan aangepast logo boot breekt) en met `pcsx-abnxt` *Smoothing*-filter en *Sony hacks*-toggle.

Triangle hernoemt het spel, Square wijzigt de geheugenkaart, Start deelt een nieuwe. Circle slaat op en verlaat.

**Instellingen in de emulator opgeslagen.** Het eigen menu van de emulator heeft *Instellingen voor dit spel opslaan*. Als een spel instellingen daar heeft opgeslagen, zijn die wat het speelt, en de gameeditor toont rijen Video en Emulator grijs met die waarden onder titel *Opgeslagen in emulator*. Om terug te gaan naar gameeditor's instellingen, kiest u **Instellingen ontgrendelen** en bevestigt: dit verwijdert de door emulator opgeslagen instellingen, en kunnen die rijen opnieuw worden gewijzigd. Beide emulatoren, `pcsx-ab` en `pcsx-abnxt`, lezen en schrijven dezelfde opgeslagen instellingen.

![Gameeditor](../images/en/game-editor.jpg)

### 3.7 Geheugenkaarten en savestates

Elk PS1-spel heeft standaard zijn eigen geheugenkaart (bijgehouden met savestates in `Games/!SaveStates/<game folder>/`). **Geheugenkaarten** in systeemmenu beheert **gedeelde sets** - kaart die meerdere spellen gebruiken, bijgehouden in `Games/!MemCards/`: maak er een (Square, met schermtoetsenbord), hernoem (Cross), verwijder (Triangle). Een spel wordt op een set geplaatst met *Geheugenkaart wijzigen* in de editor, of vanuit Geheugenkaart-pictogram.

De **geheugenkaarteditor** (Geheugenkaart-pictogram) toont kaart van het spel en een tweede kaart naast elkaar, met elk saves pictogram en titel: kopieer een save ertussen (Square), verwijder een (Triangle), defragmenteer kaart (Select). Start wisselt kaart rechts voor een ander set.

![Geheugenkaarteditor](../images/en/memory-card-editor.jpg)

**Hervattingspunten**: wanneer u PS1-spel verlaat met Resetknop console (of emulatormenu op Pi of PC), behoudt AutoBleem savestate waar u was en biedt deze onder **Hervatten**-pictogram - vier slots, elk met afbeelding. Cross gaat verder van slot, Triangle verwijdert. Spel met hervattingspunt toont klein plaatje op Hervatten-pictogram.

### 3.8 Spellen, RetroArch en Apps starten

**Cross** start het geselecteerde spel. PS1-spel loopt in gekozen PS1-emulator (sectie 3.5), volledig scherm, totdat u het verlaat - op console met front-**Reset**-knop (terug naar launcher met hervattingspunt) of **Power** (console uitschakelen); op Pi of PC via emulatormenu in-spel (Select + Start op controller, of Esc op toetsenbord). **Square** start PS1-spel in RetroArch.

**RetroArch**-spel start in RetroArch met core die launcher voor systeem koos; *Inhoud sluiten* of *RetroArch afsluiten* in menu keert terug naar launcher. RetroArch-item in systeemmenu opent RetroArch's eigen menu (XMB) met niets geladen, voor instellingen en inhoudlijsten.

**App** (Apps-set: console tools, en op console extra apps die RetroArch-pakket levert - Doom, Quake, Amiga...) toont eerst leesmij; Cross start, Circle terug.

![Leesmij app voordat deze start](../images/en/app-start.jpg)

### 3.9 Games toevoegen

**PS1-spellen** gaan in `Games`-map, **één map per spel**, genoemd naar spel:

```
Games/
  Crash Bandicoot/            Crash Bandicoot.cue + Crash Bandicoot.bin
  Final Fantasy VII/          Final Fantasy VII (Disc 1).chd, (Disc 2).chd, (Disc 3).chd
  Platformers/                een map games: eigen groep in set-picker
    Klonoa/                   Klonoa.pbp
```

- Formaten: `.cue` + `.bin` (of `.img`), `.pbp`, `.chd` (ook zstd), `.ecm` (gedecodeerd door scan), `.iso`. Gezipt spel werkt ook: **Unzip**-processor pakt voor scan uit (sectie 3.13).
- Multi-disc-spel is één map met elke disc; mappen met namen `Game (Disc 1)`, `Game (Disc 2)`... worden samengevoegd in één `Game`-map door scan.
- Spellen direct in `Games/` (losse bestanden) worden door scan in mappen gesorteerd.
- **Artwork** is PNG naast gameafbeelding, gelijk genoemd. Zonder hulp komt artwork van artwork-databases, of - met RetroArch geïnstalleerd - libretro's thumbnailset; op Pi, PC of Windows ontbrekende haald online (Options → *Artwork online ophalen*).
- Scan leest elk discs serienummer en haalt titel, uitgever, jaar, spelers, regio van RetroArch's PlayStation-database of artwork-databases. Wijzig iets in gameeditor en vink *Gegevens vergrendelen* in om het te behouden.

**Andere systemen** gaan onder `RetroArch/roms/`, **één map per systeem, genaamd als RetroArch-databases** (map wordt voor u gemaakt): `Nintendo - Nintendo Entertainment System`, `Nintendo - Super Nintendo Entertainment System`, `Sega - Mega Drive - Genesis`, `Nintendo - Game Boy Advance`, `FBNeo - Arcade Games` (of `Arcade`)... ROM's kunnen gezipt blijven. Op Pi, PC of Windows leest scan ze zelf en schrijft RetroArch-playlists; op console-stick voert u **UpdateRoms** op PC (hoofdstuk 5).

**Apps** gaan onder `Apps/<name>/` met `app.ini` (naam, pictogram, wat uit te voeren) en `run.sh`.

**Thema's** gaan onder `Themes/<name>/` (`theme.json` en afbeeldingen) - of drop themmazzip in `Themes/`.

### 3.10 PS1-BIOS

Op **PlayStation Classic** gebruikt emulator console's eigen BIOS. Op **Raspberry Pi, PC en Windows** plaats uw eigen PS1-BIOS in `System/Bios/`: `romw.bin` (VS/Europese SCPH-5501/5502) en `romJP.bin` (Japanse SCPH-5500). Installers vullen ze van RetroArch-BIOS-packs tenzij uw bestanden er al zijn. Zonder loopt emulator op ingebouwde HLE-BIOS, wat veel spellen verdragen en sommige niet.

### 3.11 Updates

- **Raspberry Pi, PC-stick, Windows**: launcher controleert site bij start en eenmaal per dag (Options → *Updates* is kanaal; *Software-update* in systeemmenu controleert nu). Wanneer nieuwere AutoBleem of RetroArch, vraagt: *Update nu* downloadt alles en voert installer opnieuw uit met eerste-boot-voortgang; *Herinner morgen* en *Versie overslaan* andere antwoorden. Uw games en instellingen blijven; launcher scant opnieuw na update.
- **PlayStation Classic**: voer nieuwere `AutoBleemInstaller.exe` op stick uit (sectie 2.1).

### 3.12 Extensies en AutoBleem Store

**Extensies** voegen eigen schermen toe aan launcher. Ze staan in `Extensions/<name>/` op stick (op Raspberry Pi gegevenspartitie, Windows datamap); om er een te installeren, pakt u zip daar uit. **L2 + R2 → Extensies** lijsten ze: Cross voert uit, Triangle zet uit of terug aan. Extensie die netwerk nodig heeft start niet zonder; stopte launcher wordt uitgeschakeld - lijst zegt.

![Extensieslijst](../images/en/extensions.jpg)

**AutoBleem Store** is eerste extensie: Apps en games installeren met één druk, op elk platform dat AutoBleem draait (PlayStation Classic nodig AutoBleem-kernel's WiFi). Vier tabbladen, L1 / R1 ertussen:

- **Apps** en **Games**: wat bronnen bieden, elk met afbeelding, versie, grootte en bron-favicon. Geïnstalleerde items grijs. Cross installeert (of werkt bij, of herprobeert na faling), Triangle verwijdert wat Store installeerde. L2 / R2 of Links / Rechts draaien pagina's, **Select** toont één bron tegelijk, **Start** zoekt titels. Item-afbeeldingen gecacht en opnieuw proberend als laden mislukt.
- **Downloads**: wat downloadt, wacht, mislukt of geïnstalleerd. Voortgangsbalk vernieuwt steady. Downloads gaan door op achtergrond, ook weg van Store; spel starten of uitschakelen pauzeert alleen, gestopte download gaat waar het stopte verder. Geïnstalleerd spel op boekenplank na volgende scan, met Store's afbeelding als artwork.
- **Bronnen**: waar lijsten vandaan - AutoBleem's catalog, TSV-lijst neergezet in `System/Extensions/store/sources/`, adressen die u toevoegt via **Bron-URL toevoegen**. Elke bron toont favicon in lijst. Cross op die u toevoegde hernoemt, wijzigt adres, schakelt `http://` en `https://`, of verwijdert.

![Store's Apps-tabblad](../images/en/store-apps.jpg)

![Bronmenu](../images/en/store-source-menu.jpg)

AutoBleem's catalog biedt staat ook op downloadsite, `https://autobleem.retromenele.pl/store/`. **U bent verantwoordelijk voor wat bronnen die u toevoegde bevatten.**

**Uw eigen games op uw netwerk**: `abstored`, Store's LAN-server, dient PS1-gameсmap aan Store op zelfda netwerk. Het draait op elke Linux-machine - Raspberry Pi, thuis-server - en leest alleen map. Start met `abstored <games folder>`, open `http://<that machine>:8124/` in browser om te zien wat het dient en problemen, en voeg `http://<that machine>:8124/store.tsv` als bron toe. Kant-en-klare programma's voor Linux en Windows op Store's pagina, in **LAN-server**-tabblad; als service `INSTALL-linux.md` (`ext_store/server/` in bron). **LAN Share** (sectie 5.2) plaatst games en schijven van PC op zo'n server.

### 3.13 Scanner-processors

**Scanner-processors** zijn kleine programma's die elke scan eerst draait. Een kan formaat waarvan AutoBleem niet kan veranderen in wat het kan - gezipt spel bijvoorbeeld - of spelgegevens wijzigen, zoals patchvertaling. Ze staan in `System/Processors/<name>/` op stick (op Raspberry Pi gegevenspartitie, Windows datamap); om installeren, pakt u map daar uit. Volgende scan voert uit.

- **Unzip zit in AutoBleem**: het pakt gezipt PS1-spellen in `Games/` voor scan uit, gezipt ROM's één voor één (arcade-sets blijven gezipt). AutoBleem updaten werkt het bij, houdt uitgeschakeld als u dat deed.
- Processor die al spel bewerkte draait niet opnieuw totdat spel verandert.
- Terwijl processor werkt, toont bubble rechtsboven wat het doet; waarschuwing of faling op lijn daaronder. `processors.log` in logsmap heeft details.
- Spel starten of RetroArch stopt processor dat bestanden wijzigt; volgende scan werkt af.

**L2 + R2 → Scanner-processors** toont in volgorde dat ze draaien, één tabblad PS1-spellen, één ROM's (L1 / R1). **Square** pakt processor op, Omhoog / Omlaag verplaatst - volgorde telt: processor die uitpakt moet voor die patcht wat uitpakt. **Cross** schakelt aan/uit, **Triangle** laat het alle spellen volgende scan opnieuw bekijken, **Circle** terug en scant als u veranderde. Processor gebouwd voor ander toestel blijft in lijst, grijs.

![Scanner-processors](../images/en/processors.jpg)

Schrijf eigen: Unzip's pagina `https://github.com/autobleem2/proc_unzip` verklaart wat processor moet doen, AutoBleem-bron `tools/proc_check.py` controleert één voordat u deelt.

<!-- pagebreak -->

## 4. Schermen

### 4.1 Gamebeheerder

PS1-spellen als lijst met mappen en geselecteerde gameplay artwork en details. Cross opent gameeditor, **Square verwijdert spel** (map en na tweede vraag savestates), Triangle verwijdert elk artwork-PNG naast spellen (scan haalt ze opnieuw uit databases), L2 / R2 pagina. Vrije schijfruimte rechtsboven. Gamebeheerder wacht terwijl scan draait.

![Gamebeheerder](../images/en/game-manager.jpg)

### 4.2 Hardwaregegevens

Machinefeiten - systeem, hardware, opslag met vrije ruimte, netwerkaddressen, beeldscherm- en audiobestuurders, verbonden controllers - elke seconde opnieuw gelezen. Op PlayStation Classic met AutoBleem-kernel opent **PSC-Bios** (hoofdstuk 6); op ander toont informatiepagina.

![Hardwaregegevens](../images/en/hardware-info.jpg)

### 4.3 Buttonhulpgids

Triangle op boekenplank: elke knop van elk scherm op één pagina. Wanneer USB-toetsenbord aangesloten of gebruikt, Keyboard-kolom toont toetsen naast controllerknappen.

![Buttonhulpgids](../images/en/button-guide.jpg)

### 4.4 Schermtoetsenbord

Waar tekst wordt ingevoerd - geheugenkaartset, gametitel, WiFi-wachtwoord, bronadres - zelfde toetsenbord, phonelay-out: letters, symboolpagina (`/ \ : ? & = % @ #` en rest adres/wachtwoord nodig) en twee geaccentueerde-letterpagina's, Shift, paginaryknop, Space, Backspace en Confirm onderaan. Richtingen verplaatsen, Cross typt, Triangle verwijdert, Square is spatie, **L1** is Shift (twee keer voor CapsLock), **R1** volgende pagina, **L2 / R2** cursor verplaatsen, Start bevestigen, Circle annuleren. USB-toetsenbord typt altijd: Enter bevestigt, Esc annuleert.

![Schermtoetsenbord](../images/en/keyboard.jpg)

<!-- pagebreak -->

## 5. Op de PC

### 5.1 UpdateRoms - console-stick vernieuwen

PlayStation Classic geen netwerk, RetroArch-lijsten en artwork ervan op PC: **UpdateRoms** doet op PC wat launcher's scan op Pi doet, met PC's netwerk en console's paden, zodat console start en alles vindt.

1. Kopieer ROM's naar stick onder `RetroArch/roms/<system>/` (sectie 3.9). Mapnamen moeten RetroArch-databasenamen zijn; installer maakt veelgebruikte.
2. Start **`UpdateRoms\UpdateRoms.exe` van stick** (installer zette daar). Het vindt stick waar het zit, toont stadiumregel, voortgangsbalk en log, en:
   - downloadt RetroArch-databundel als stick geen heeft, identificeert elk ROM ervan - database kent spel krijgt juiste naam;
   - schrijft één playlist per systeem in `RetroArch/bin/playlists/` met console-paden, houdt wat RetroArch zelf daar toevoegde;
   - haalt artwork van elk ROM zonder van libretro's thumbnailservers in `RetroArch/bin/thumbnails/`.
3. Verwijder stick veilig en terug in console. RetroArch-tabblad set-picker lijdt elk systeem met spellen.

Draai na elk ROM-mapwijziging opnieuw; map zonder verandering springt, re-run is snel. Log is `System/Logs/updateroms.log`. Raspberry Pi-kaart in kaartlezer kan hetzelfde worden vernieuwd (`UpdateRoms.exe <drive> --target rpi`), hoewel Pi het zelf doet netwerk.

### 5.2 LAN Share - uw games en schijven op server op uw netwerk

**LAN Share** (`LanShare.exe`, op Store's pagina in **LAN-server**-tabblad) zet uw PS1-games op Store-server op uw thuisnetwerk - `abstored` op Raspberry Pi, NAS of ander PC - en leest PS1-schijf in PC's CD/DVD-station. Store op console, Pi of PC installeert dan van daar. Niets installeren; instellingen in `%LOCALAPPDATA%\AutoBleem LAN Share\`.

![LAN Share-venster](../images/en/lanshare.jpg)

1. **Server**: voer adres in (`http://<its address>:<port>`, zoals Store heeft) en druk **Connect**. Games en problemen links getoond. Games erop geven één van:
   - **Share** - server's gamesmap gedeeld op netwerk (Samba), bijv. `\\raspberrypi\games`: LAN Share kopieert daar en vraagt server scan. Server blijft alleen-lezen.
   - **Token** - server startte met `--allow-uploads`: token (server print bij start, houdt in `<state>/upload-token`). LAN Share upload via HTTP, gestopte upload gaat waar het stopte.
2. **Games op deze PC**: kies gamesermap (elke game één map), vink games, druk **Gepubliceerde geteste games**. **Op server** zegt server al spel heeft (reeks, anders titel); zo spel niet tweezijdig. **Vink die niet op server** vink rest.
3. **Schijf**: PS1-schijf in station, druk **Schijf lezen en publiceren**. Schijf lees geheel in `.bin` + `.cue` (LibCrypt `.sbi` als station subchannel) genoemd naar titel, getest tegen bekende goed dump (database gekozen) en gepost. Games op meerdere schijven vink **Game op meerdere schijven**: LAN Share vraagt elk volgende schijf en publiceert samen als spel.
4. **Verwijderen van server...** haalt geselecteerde games van server. Niets verwijderd: elk in `.removed`-map naast server's games, terugzetten zet terug.

**Databases** - AutoBleem's afdekmap (`coversU/P/J.db`) en RetroArch's `Sony - PlayStation.rdb` - geven titels en schijftest; beide optioneel. **Deel ook games op deze PC met Store** (standaard uit) dient PC-map aan Store rechtstreeks. Eerst Windows vega firewall: alleen privénetwerken.

<!-- pagebreak -->

## 6. Consoletools (PlayStation Classic)

Twee tools voor PlayStation Classic-stick. Beide tekenen in launcher's thema en taal, beide bestuurd door controller - en in hand-controller-wizard door console's frontknop. **PSC-Bios** is extensie met console-pakket: *Hardwaregegevens* in systeemmenu opent, Extensieslijst in. **ABFlashKit** is App in Apps-set.

### 6.1 PSC-Bios

Extensie met console-pakket, ook op Raspberry Pi en PC-stick. Geopend van systeemmenu's *Netwerk en Controllers* item (of Extensieslijst). Wanneer extensie geïnstalleerd maar uitgeschakeld, *Netwerk en Controllers* in Quick menu en systeemmenu grijs met "enable it in Extensions" - Cross daar opent Extensieslijst ernaartoe.

Scherm toont machinefeiten: tijd, tijdzone, WiFi/Ethernet/Bluetooth netwerkadapters met adressen, elke controller met knoptoewijzing. Netwerk- en Bleutooth-bits nodig AutoBleem-kernel op console (sectie 6.2) of systeem tools op Raspberry Pi / PC-stick; handcontroller-wizard werkt op elk systeem.

![PSC-Bios: Netwerk en Controllers-hub](../images/en/pscbios-main.jpg)

- **Select - WiFi Network** (kernel of NetworkManager): netwerknaam (getyped of scan), wachtwoord, bestuurmodus, *Toepassen / Netwerk opnieuw starten*. Tijdzone ook hier. Console IP na verbinding.
- **Square - Bluetooth Controllers**: scan Bluetooth-controllers (DualShock 4 etc), koppel/verwijder.
- **L1 - DualShock 3 Koppeling**: eerste DualShock 3 USB-enig via kernel's sixaxis-plugin.
- **R1 - Controller Mapping**: mappingwizard (hieronder).
- **Triangle - Over**, **Circle - terug** naar launcher.

**Handcontroller-wizard** toont aangesloten controller rauw - elke as, knop en hoed als getal, DualShock-afbeelding die oplicht terwijl u drukt. Omdat te testen controller niet vertrouwd kan, wizard door **frontknop** console: **RESET** volgende controller, **OPEN** start mapping (dan elk vraag - druk knop lit op afbeelding, of OPEN geen knop), **POWER** annuleer/verlaat. Circle 2 sec ingedrukt op controller verlaat wizard (staaf vult, voettekst "Hold 2 s: Exit"). Geen mapping, knop 2 sec ("Hold any 2 s: Exit"). Short-druk normaal. Toetsenbord Esc / Space / Enter POWER / RESET / OPEN. End nieuwe mapping test toegevoegd, OPEN slaat als gekozen naam op; launcher laadt er van dan.

![PSC-Bios: Handcontroller-mapping-wizard](../images/en/pscbios-wizard.jpg)

### 6.2 ABFlashKit - AutoBleem-kernel

AutoBleem-kernel optioneel console Linux-kernel-vervanger: werkende klok, USB-WiFi en Bluetooth-dongles (PSC-Bios, Bluetooth-controllers) en front-knop-ondersteuning emulator-resume. ABFlashKit installeer, backup console eerst, kan console terug-naar-aandelenstok Sony's eigen recovery.

> **Dit tool schrijft console's flash. ** Interrupt flash - stroom uit, stick uit - kan console onbruikbaar maken, custom-kernel voiding garanentie. Houd console aan en stick tot het zelf herstart. ABFlashKit dit waarschuwing op; *I understand* verder, *Quit* verlaat.

![ABFlashKit-menu](../images/en/abflashkit-menu.jpg)

- **Flash Kernel**: console-partitie recovery-backup op stick (`LBOOT.EPB`) als geen, controleer, schrijf kernel en AutoBleem systeem-bestanden, herstart. *Gereed - scherm zwart vervang snoer*: trek console-stroom, plug terug.
- **Volledige backup**: alle vier partities naar `LBOOT.EPB` voor later herstellen (vorige backup na vraag).
- **Restore Mode**: controleert backup is aandelenstok, zet recovery-vlag, herstart Sony's recovery, herstelt console van `LBOOT.EPB` op stick - weg terug naar aandelenstok firmware.

Voortgangsbalk onder elke stap toont ver actie. Tool weigert flash console ander aangepast firmware (BleemSync, Project Eris): eerst restory-aandelenstok.

<!-- pagebreak -->

## 7. Als iets fout gaat

- **Logs**: AutoBleem houdt logs in geheugen, stick niet constant geschreven - ze bereiken stick, kaart of datamapje `System/Logs/` alleen bij fout: launcher-, PS1-spel- of RetroArch-crash sparen naar `System/Logs/crash-<n>/` (laatste drie gehouden), launcher zegt eenmaal terug. Elke log houden, zet *Options -> Diagnostics -> Keep logs on the stick* aan (vanaf volgende start), of maak leeg bestand `System/Logs/keep` op PC. Pi of PC, *Hardwaregegevens* toont waar logs, Square slaat naar `System/Logs/saved-<n>/`. Bestanden: `autobleem.log` (launcher), `launch.log` en `pcsx.log` (PS1 start en emulator-output), `retroarch.log` en - altijd stick - `update.log` (online update) en `updateroms.log` (UpdateRoms).
- **Spel niet op boekenplank**: controleer mapindeling (elke spel één map, formaten sectie 3.9). *Gamebeheerder* lijdt scan-geweigerde mappen na spellen, *Niet toegevoegd* gemarkeerd, reden; Square wist zo map. Systeem-herscanned scan opnieuw.
- **Geen artwork**: artwork-databases niet geïnstalleerd (voer installer opnieuw uit aangevinkt), of console RetroArch-spellen, PC UpdateRoms niet.
- **Controller doet niets/knoppen mixed**: PSC-Bios handcontroller-wizard (console) toewijzen; Pi of PC Hardware-info toont SDL.
- **Spel-zwart scherm console**: AutoBleem herstelt window zelf (tot drie keer); blijft zwart, hou power en zet console opnieuw.
- **Raspberry Pi**: `Alt+F2` geeft login tweede console; SSH van eerste boot. `sudo journalctl -u autobleem` launcher's service; `sudo systemctl restart autobleem` herstart. Niet afgeronde eerste boot (geen netwerk) herprobeert volgende boot.
- **Windows**: `Esc` verlaat launcher; datamapje is gekozen (standaard `Documents\AutoBleem`), logs in `System\Logs`.

AutoBleem is vrije software (GNU GPL v3 of hoger), zonder garantie. Ondersteuning en nieuws: Discord-server op About-scherm, en https://autobleem.retromenele.pl/.

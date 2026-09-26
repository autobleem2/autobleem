# AutoBleem 2 Käyttöopas

AutoBleem 2 on peliä käynnistävä sovellus **PlayStation Classicille** - ja version 2 jälkeen myös
**Raspberry Pie**, **USB-tikulta käynnistettävälle PC:lle** ja **Windowsille**. Se näyttää PS1-pelisi
hyllylle järjestettynä kansineen kunnoineen ja tiedoineen, käynnistää ne mukana tulevalla PCSX-emulaattorilla
ja asentamalla RetroArchin rinnalle voi pelata muistakin järjestelmistä pelejä. Tämä opas käsittelee
asennusta jokaiselle alustalle, päivittäistä käyttöä ja mukana tulevat työkalut.

> Kaikkien alustojen lataukset ovat osoitteessa **https://autobleem.retromenele.pl/**. Sivu on järjestetty
> alustan mukaan: *Install*-paneeli kussakin on se mitä lataat; sen alle jäävät *Build inputs* ovat mitä
> asentajat lataavat itse.

## 1. Mitä saat

- **Käynnistäjä** - kansikarusellli, sarjat (PlayStation, RetroArch, Apps), pelin tiedot, järjestelmävalikko,
  asetukset, muistikortti- ja tallennuspisteiden työkalut. Sama ohjelma kaikilla alustoilla.
- **Kaksi PS1-emulaattoria** - `pcsx-abnxt` nykyinen (oletusarvo) ja `pcsx-ab` klassinen jonka AutoBleem
  on aina toimittanut. Valitset sen asetuksista; molemmat käyttävät samoja asetuksia ja muistikortteja.
- **RetroArch** (valinnainen kaikilla alustoilla) muille järjestelmille: NES, SNES, Mega Drive, Game Boy,
  arcade ja paljon muuta. AutoBleem rakentaa RetroArch-luettelonsa kopioimistasi ROM-levyistä ja käynnistää
  jokaisen pelin oikealla ytimellä.
- **Konsolityökalut** (vain PlayStation Classic): *PSC-Bios* WiFi:lle, kellonajalle ja ohjaimen kartoitukselle
  ja *ABFlashKit* AutoBleem-kernelin asennukselle.
- **UpdateRoms** Windowsille: päivittää RetroArch-luettelot ja kansikauden konsoluntikulla PC:lle koska
  konsolilla ei ole verkkoa.

![Käynnistäjä: kansikkaiden hylly valitun pelin tiedot painikkeiden vinkit](../images/fi/launcher.jpg)

<!-- pagebreak -->

## 2. Asennus

### 2.1 PlayStation Classic

Tarvitset Windows-PC:n USB-tikun (USB 2.0 8 GB tai enemmän; asentaja muotoilee sen jos pyydät) ja
stock-konsoolin. AutoBleem käynnistyy tikusta ilman muutoksia konsoliin. Tikun on oltava **FAT32** stock-
konsolille - sen kernel ei voi lukea exFAT:ia. Vain AutoBleem-kernelin asennetulla konsolilla (ABFlashKit
luku 6) voi käynnistyä exFAT-tikusta mikä poistaa FAT32:n 4 Gt rajan.

1. Lataa **AutoBleemInstaller-<version>.zip** sivun PlayStation Classic -paneelista ja pura se mihin tahansa.
   Se sisältää `AutoBleemInstaller.exe`:n ja AutoBleem-paketin jonka se asentaa.
2. Liitä tikku ja käynnistä `AutoBleemInstaller.exe`. Valitse asema ylhäältä. Valitse mitä haluat:
   - **Format the stick** - vain uudelle tikulle (kaikki sillä poistetaan). Valitse FAT32 elleivät konsolilla
     ole AutoBleem-kernelia.
   - **Cover databases** - PS1-kirjaston kansikauden ja tiedot (oletuksena valittu; noin 300 MB).
   - **RetroArch** - RetroArch ydinillä ekstra sovelluksilla (Doom Quake Amiga ...) ja libretro-omaisuuksilla
     muille järjestelmien peleille. Oletuksena pois; voidaan lisätä myöhemmin ajamalla asentaja uudelleen.
   - **BIOS files** - BIOS-tiedostot joita RetroArch-ytimet tarvitsevat (vaatii RetroArchin).
   - **Sample games** - muutama ilmainen homebrew-peli jotta hylly ei ole tyhjä.
3. Paina **Install** ja odota. Edistymispalkit ja loki näyttävät jokaisen vaiheen; tikku on nimeltään
   `SONY` lopussa ja `UpdateRoms` asetetaan siihen (katso luku 5).
4. Ota tikku turvallisesti ulos liitä se konsolinin **toiseen USB-porttiin** (oikea pelaaja 2) ja kytke
   konsoli päälle. AutoBleem käynnistyy stock-valikon sijaan.

**Kytkeistä päälle ja pois päältä.** Tikun ollessa sisällä konsoli käynnistyy vilkkuu valollaan muutamia
sekunteja (AutoBleem haetaan) ja menee sitten valmiustilaan ennen kuin mitään näytetään - tämä on konsolinin
oma tapa valmistella päivitys jonka ansiosta AutoBleem pääsee käynnistymään. Paina **Power** kerran ja
käynnistäjä tulee esille. *Power Off* järjestelmävalikossa tai konsolinin power-nappi asettaa konsolinin
**AutoBleemin valmiustilaan**: tikku irrotetaan ensin sen jälkeen valo kääntyy **punaiseksi** - merkki
että AutoBleem toimii oikein - ja seuraava power-painallus tuo käynnistäjän takaisin muutamassa sekunnissa.
**Kun valo on punainen voidaan tikku vetää ulos** ja laittaa PC:hen ilman että Windows kysyy sen tarkastamista;
laita se takaisin ennen kuin painat Power. Konsolinin virran vetäminen käy boot-valmiustilan uudelleen
seuraavalla kerralla.

**Tikun päivittämiseksi** ajaa uudempi asentaja sen yli: pelisi tallennetut tiedot asetukset ja RetroArch-
sisältö jäävät; vain AutoBleemin omat tiedostot korvataan. AutoBleem 1.0:lla tai AutoBleem-NG:lla tehty
tikku tuodaan automaattisesti uuteen asetteluun.

> Stock-konsolilla ei ole kelloa eikä verkkoa: päivämäärät näytetään vasta AutoBleem-kernelin asennuksen
> jälkeen (luku 6) ja RetroArch-pelien kansikauden haetaan UpdateRomsilla PC:llä (luku 5).

Pelit menevät tikun `Games`-kansioon yksi kansio per peli - katso osio 3.9 asettelulle.

### 2.2 Raspberry Pi

AutoBleem muuttaa Pi:n pieneksi konsoliksi: se käynnistyy suoraan käynnistäjään ilman työpöytää. Sivulla
on kaksi valmiiksi tehtua kuvaa - 32-bittinen ja 64-bittinen - sekä tarball olemassa olevalle Raspberry
Pi OS Litelle.

| Malli | 32-bittinen kuva | 64-bittinen kuva | Huomautukset |
|---|---|---|---|
| Raspberry Pi 5 | kyllä | kyllä | |
| Raspberry Pi 4 Model B Pi 400 | kyllä | kyllä | |
| Raspberry Pi 3 Model B / B+ / A+ | kyllä | kyllä | hyvä käynnistäjälle ja PS1:lle |
| Raspberry Pi Zero 2 W | kyllä | kyllä | 512 MB RAM: PS1 käy raskaammat RetroArch-ytimet eivät |
| Raspberry Pi 2 Model B | kyllä | vain v1.2 | hidas 3D-asioille |
| Raspberry Pi 1 Zero Zero W | ei | ei | ARMv6 - kumpikaan kuva ei käy |

**32-bittiset kuva on suositeltu** PS1-peleille: `pcsx-ab`:n nopea ARM-rekompileri on vain 32-bittinen joten
64-bittiset rakentuma käyttää PS1-pelejä hitaammin. 64-bittiset kuva on isompi RetroArch-ytimien sarja.

**Raspberry Pi Imagerinä:**

1. Asenna Raspberry Pi Imager (raspberrypi.com/software). *Choose OS:ssa* valitse *Use custom* ja lataamasi
   `autobleem-<version>-rpi-armhf.img.xz` (32-bittinen) tai `-arm64.img.xz` (64-bittinen) - tai lisää
   säilöpalvelu-URL `https://autobleem.retromenele.pl/rpi-imager/os_list.json` sovelluksen asetuksiin ja
   valitse AutoBleem luettelosta.
2. Käytä Imagerin mukauttamisnäyttöä (hammasrata tai kysymys *Nextin* jälkeen) asettaaksesi **käyttäjänimi
   ja salasana WiFi-verkon ja maanpaikan ja ota SSH käyttöön**. AutoBleem tarvitsee verkon ensimmäisellä
   käynnistyskerralla.
3. Kirjoita kortti laita se Pi:hin näytön ja näppäimistön tai ohjain liitettynä ja kytke päälle.

**Ensimmäinen käynnistys** kestää 5-25 minuuttia ja näyttää mitä se tekee näytöllä. Ilman verkkoa se kysyy
yhtä (WiFi-luettelo salasana) sitten kysyy haluatko asentaa RetroArchin (minuutin vastaus tarkoittaa kyllä)
kasvattaa järjestelmäosion tekee `AUTOBLEEM`-dataosuus loput kortista asentaa RetroArchin ja ytimet BIOS-
paketeista näytepelit ja käynnistää uudelleen käynnistäjään.

Vastaukset voidaan antaa etukäteen **`autobleem.txt`**:ssa kortin boot-osiossa (muokattavissa millä tahansa
PC:llä ennen ensimmäistä käynnistystä):

| Avain | Oletusarvo | Merkitys |
|---|---|---|
| `root_gib` | 8 | Järjestelmäosion koko GiB:ssä; loput tulee peli-osioksi. |
| `hdmi_mode` | 1920x1080@60 | Näyttötila koko käynnistykselle (`1280x720@60` vanhemmalle TV:lle). |
| `retroarch` | (kysytty) | `yes` / `no` - RetroArch ja muut järjestelmät tai vain PS1. |
| `thumbnails` | ei | `boxarts` peilaa koko kansikauden sarjan offline (~9000 tiedostoa). |
| `bios` `downloads` `samples` | kyllä | Aseta `no` ohittaaksesi BIOS-paketit jokaisen latauksen tai näytepelit. |

**Olemassa olevalle Raspberry Pi OS Litelle** (Bookworm tai Trixie): kopioi `autobleem-rpi.tar.gz` (tai
arm64-versio) Pi:hin pura se ja aja `sudo bash install.sh`. Se kysyy samoja kysymyksiä tekee dataosuus
pienentämällä juurta seuraavalla käynnistyskerralla (`--shrink-root <GiB>`) ja laittaa käynnistäjän
ensimmäiselle konsolille.

Asennuksen jälkeen kortin **`AUTOBLEEM`-osio** (exFAT) on mitä täytät: ota kortti pois ja avaa se millä
tahansa PC:llä tai kopioi verkon yli (SSH on päällä). `Games/` PS1-peleille `RetroArch/roms/<system>/`
muille järjestelmille `System/Bios/` PS1 BIOS:lle (osio 3.10) `Themes/` teemoille.

### 2.3 PC USB-tikku

Sama laite mihin tahansa USB:sta käynnistyvään PC:hen - 32-bittiset järjestelmät vanhatsaatkin koneet
toimivat:

1. Lataa `autobleem-<version>-pcusb-i386.img.xz` PC-paneelista ja kirjoita se 8 Gt tai suuremmalle tikulle
   Raspberry Pi Imagerinä (*Use custom*) balenaEtcher:llä tai Rufuksella (DD-tila).
2. Käynnistä PC tikusta (PC:si boot-menu-näppäin - F12 F8 Esc...). Sekä BIOS että UEFI boot toimivat;
   **Secure Boot on oltava pois päältä**.
3. Ensimmäinen käynnistys on Pi:n: verkkokysymys jos kaapelia ei ole RetroArch-kysymys sitten asennus -
   noin kahdeksan minuuttia langallisella verkolla - ja käynnistys uudelleen käynnistäjään.

Tiku on sitten `AUTOBLEEM`-osio peleille näkyvissä Windows 10:ssä (1903 ja uudempi) toisena asemana kun
laitat tikun käynnissä olevaan PC:hen. `autobleem.txt` on ensimmäisellä osiolla samoilla avaimilla kuin
Pi:llä (ei `hdmi_mode` - PC käyttää näytön natiivitilaa).

### 2.4 Windows

AutoBleem Windows-ohjelmaksi: kokoruutu emulaattorit ja RetroArch käynnistetty ohjelmina.

1. Lataa **AutoBleemSetup-<version>.exe** ja aja se. Se asentaa käyttäjää kohden ilman ylläpitäjän oikeuksia:
   ohjelma `%LOCALAPPDATA%\Programs\AutoBleemin` datassa (pelit asetukset teemat RetroArch) kansioon jonka
   valitset - `Documents\AutoBleem` oletuksena.
2. Valitse komponentit - kansikauden tietokannat RetroArch (virallinen Windows-rakenne ja ytimet) BIOS-
   tiedostot näytepelit - ja anna asennusapu ladata ne.
3. Käynnistä AutoBleem Start-valikosta tai Työpöydältä. PC:llä näppäimistö toimii ohjaimen kaltaisesti
   (osio 3.2).

Uudempi asennus sen yli päivittää ohjelman ja pitää datakansion. Käynnistäjä tarkistaa myös sivua kerran
päivässä ja tarjoaa päivitystä kun on (osio 3.11).

<!-- pagebreak -->

## 3. AutoBleemin käyttäminen

### 3.1 Käynnistäjä

Käynnistäjä avautuu hyllylle: nykyisten joukosta kannet valittu keskellä oikealla sen tiedot - julkaisija
vuosi sarjanumero alue pelaajat milloin se viimeksi pelattiin - ja toista-painike. Alaosassa oleva palkki
luettelee mitä painikkeet tekevät. Pelikansion skannaus käynnistyy taustalla jokaisen käynnistyksen yhteydessä;
sen käydessä kupla oikealla ylhäällä näyttää sen edistyksen uudet pelit ilmestyvät hyllylle sitä mukaa
kuin ne löydetään.

![Sarja-valitsin: kolme välilehteä ja nykyisen joukosta ryhmät pelien laskujen kera](../images/fi/set-picker.jpg)

### 3.2 Ohjaimet

| Painike | Hyllyllä |
|---|---|
| Vasen / Oikea | Edellinen / seuraava peli. Pitäminen vierittää eteenpäin. |
| L1 / R1 | Hyppää edelliseen / seuraavaan otsikoiden ensimmäiseen kirjaimeen. |
| Cross | Käynnistä valittu peli (PS1-peli PS1-emulaattorissa; RetroArch-peli sen ytimessä; App sen readme:n jälkeen). |
| Square | Käynnistä valittu PS1-peli RetroArchissa sijaan. |
| Triangle | Painikeohjeet. |
| Start | Satunnainen peli nykyisestä joukosta. |
| Select | Sarja-valitsin: PlayStation / RetroArch / Apps välilehdet (L1 / R1) sarjan ryhmät (Ylös / Alas L2 / R2 sivu) Cross valitsee. |
| Alas | Avaa pelin alla olevan kuvakepalkki (Asetukset Peli Muistikortti Jatka). Ylös sulkee sen. |
| L2 + R2 | Järjestelmävalikko (osio 3.4). |

**Näppäimistöllä** (PC ilman ohjainta tai USB-näppäimistö konsolissa Pi:ssä tai PC-tikulla) näppäimet
menevät sisään: **Nuolinäppäimet** = d-pad **Enter** = Cross **Esc tai Backspace** = Circle **Tab** = Triangle
**Välilyönti** = Square **F1 / F2** = Select / Start **Page Up / Page Down** = L1 / R1 **Home / End** = L2 / R2
**F10** = järjestelmävalikko. Kehityskonella Esc sulkee ohjelman ja välilyönti on Start.

Joka luettelossa ja valikossa: Ylös / Alas liikkuvat **L2 / R2 kääntävät sivuja** L1 / R1 hyppäävät ensimmäiseen
/ viimeiseen riviin **Cross valitsee Circle menee takaisin**. Asetuksilla varustettu näyttö tallentaa ne kun
poistut sen Circle:llä.

![Kuvakepalkki valitun pelin alla](../images/fi/launcher-icons.jpg)

### 3.3 Sarjat

**Select** avaa sarja-valitsimen. PlayStation-välilehti luettelee *All Games* *Internal Games* (konsolinin
sisäänrakennettu kaksikymmentä PlayStation Classicsissa) jokaisen kansion jonka teit `Games/`:n alle (peli
alikansiossa kuuluu kyseiseen ryhmään) *Favorite Games* *Game History* ja kun mikä tahansa peli on
merkitty sellaiseksi *Lightgun Games*. RetroArch-välilehti luettelee yhden ryhmän per järjestelmä jonka
peleillä on RetroArchin omat Suosikit ja Historia. Apps-välilehti ryhmittelee sovelluksia tyypin mukaan:
*All apps* sitten *Games* *Emulators* *Tools* *Media* ja *Other* (kategoria asetetaan kussakin sovelluksessa
`app.ini`-tiedostossa). Jokainen rivi näyttää kuinka monta kohdetta se pitää sisällään; ryhmä ilman mitään
avautuu tyhjälle hyllylle kuvakepalkissa näkyviin Asetuksilla vain.

### 3.4 Pika-valikko

**Ylös** käynnistäjässä tai **hammasrattaikoni** kuvakepalkissa (jossa Asetukset / Peli / Muistikortti /
Jatka ovat): pika-valikko karrusellissa tavoitelemillesi toiminnoille. Lyhyt luettelo: *Re-Scan Games*
(aloittaa skannauksen nyt) *Store* (AutoBleem Store laajennuksien lataamiseksi) *Network & Controllers*
(vain jos asennettu laajennus tarjoaa `network`-merkinnän - PSC-Bios konsolissa Pi:ssä ja PC-tikulla: WiFi
Bluetooth-paritus ohjaimen kartoitussuutari - katso osio 6; harmaalla "enable it in Extensions" kun laajennus
on poistettu käytöstä - Cross avaa Extensions-luettelon) ja *System menu...* (täysi valikko alla). Ylös / Alas
liikkuvat (kiertävät) Cross valitsee Circle takaisin. Mikään ei ole ainutlaatuista täällä - jokainen kohde
on myös järjestelmävalikossa.

### 3.5 Järjestelmävalikko

**L2 + R2** (yhdessä missä järjestyksessä tahansa) avaa järjestelmävalikon hyllyn yli. Valikko on jaettu
osiin:

| Osio | Kohde | Mitä se tekee |
|---|---|---|
| (ylä) | Re-Scan Games | Etsii uusia muutettuja tai poistettuja pelejä nyt (skannaus valvoo myös kansiota itse). |
| | Extensions | Laajenukset tikulla - AutoBleem Store ja muut (osio 3.12). |
| **Library** | Game Manager | PS1-pelit luettelona kansioineen: poista peli tyhjennä kannet. Poistettu käytöstä kun skannaus käynnissä. |
| | Memory Cards | Muistikorttisarja (osio 3.7). |
| | Scanner processors | Ohjelmat jokainen skannaus ajaa ensin - niiden järjestys pois päältä (osio 3.13). Poistettu käytöstä kun skannaus käynnissä. |
| **System** | Options | AutoBleemin asetukset (osio 3.6). |
| | Network & Controllers | Vain jos asennettu laajennus tarjoaa `network`-merkinnän (`Provides=network` sen `extension.ini`:ssä - PSC-Bios konsolissa Pi:ssä ja PC-tikulla) - WiFi Bluetooth-ohjaimen paritus DualShock 3 asennus ja ohjaimen kartoitussuutari - katso luku 6. Kun laajennus on asennettu mutta poistettu käytöstä tämä kohde harmaa muistiinpanolla "enable it in Extensions" - Cross avaa Extensions-luettelon siinä. |
| | Hardware Information | Koneen faktat: järjestelmä CPU tallennustila verkkokäyttöliittymät aikavyöhyke näyttö ohjaimet ja niiden kartoitukset. Konsolissa AutoBleem-kernelin kanssa avaa tämä PSC-Bios:in (luku 6); muissa koneissa näyttää tämän tietosivun. |
| | Software Update | (Raspberry Pi ja PC) Tarkista sivulla uudempi AutoBleem tai RetroArch nyt. |
| | About | Tunnustukset ja lisenssi. |
| **Leave** | RetroArch | Poistuu käynnistäjästä RetroArchin omaan valikkoon. RetroArchin sulkeminen palaa. |
| | Power Off | Vahvistuksen jälkeen: konsolissa AutoBleemin valmiustila - tikku irrotettu valo punainen Power tuo käynnistäjän takaisin (osio 2.1); Pi:ssä tai PC:ssä kone sammuu. |

![Järjestelmävalikko](../images/fi/system-menu.jpg)

### 3.6 Asetukset

Asetukset ovat ryhmissä; Ylös / Alas liikkuvat niiden välillä Vasen / Oikea muuttaa arvoa Circle poistuu ja
tallentaa. Jokainen muutos käytetään välittömästi.

| Ryhmä / asetus | Mitä se tekee |
|---|---|
| **Interface**: AutoBleem Theme | Ulkonäkö. Teemat asuvat `Themes/`-kansiossa; teeman zip joka sinne pudotetaan puretaan seuraavalla käynnillä. AutoBleemin mukana tulevat teemat päivitetään jokaisen päivityksen yhteydessä - mukauttaaksesi sitä kopioi se ensin uuden nimen alla. |
| Cover Style | Juveleriraami PS1-kannet ympärille. |
| Language | Käynnistäjän kieli käytössä heti (17 kieltä). |
| Use Font from Theme / Font | Klassinen näyttöjen kirjasin: teeman tai mikä tahansa `.ttf`/`.otf` osoitteesta `resources/fonts` `RetroArch/fonts` tai teeman kansiosta. |
| Showing Timeout | Kuinka kauan "Showing: ..."-ilmoitus näytetään sekunnissa (0 = ikuisesti). |
| **Sound**: Music Background Music | Mitä kappaletta soitetaan käynnistäjän alla (teeman tai tiedosto `resources/music`-kansiosta) ja soitetaanko sitä lainkaan. |
| **Emulation**: PS1 Emulator | `pcsx-abnxt` (oletus: nykyinen PCSX-ReARMed AutoBleemin lisäyksillä) tai `pcsx-ab` (klassinen). Yhdellä tallennettu jatkopiste jatkuu toisessa ellei peli ollut käynnissä ilman BIOS-tiedostoa. |
| Widescreen | PS1-emulaattorin kuvasuhde jokaiselle pelille. |
| Play all PSX games with RA | Jokainen PS1-peli käynnistyy RetroArchin PS1-ytimessä. |
| Update RA Config | AutoBleem kirjoittaa asetuksensa RetroArchin määritykseen kun se käynnistää pelin siellä. |
| **Library**: Show Internal Games | Konsolinin sisäänrakennetut pelit PlayStation-luetteloissa (vain PlayStation Classic). |
| Fetch box art online | Skannaus noutaa puuttuvat kannet libretron palvelimista (Raspberry Pi PC Windows). |
| **Updates** | (Raspberry Pi PC Windows) `stable` `latest` (esijulkaisut myös) tai `off`. |

![Asetukset ryhmissä](../images/fi/options.jpg)

### 3.7 Pelin asetukset

Kun peli on valittu avaa **Alas** sen kuvakepalkki: **Settings** (asetukset yllä) **Game** (pelin omat
asetukset) **Memory Card** (sen muistikortti) ja **Resume** (sen tallennuspisteet). Cross avaa sen kursorin
alla.

**Pelin muokkain** näyttää pelin tiedot oikealla ja asetukset vasemmalla kolmessa ryhmässä:

- **Game**: *Favorite* (Suosikki Games -ryhmässä) *Lightgun Game* (valoaseen peli - se liittyy Lightgun-ryhmään
  ja käynnistyy aina RetroArchissa jonka PS1-ytimella on GunCon) *Play using RA* (tämä peli käynnistyy
  RetroArchissa) *Lock data* (skannaus jättää pelin otsikon sarjanumeron ja levyluettelon asetuksiksi).
- **Video**: korkea resoluutio skanliniat ja niiden taso kehys ohitus GPU-plugin ja *Filter* - kuinka kuva
  skaalataan: Off (perus pikselit) Linear (pehmeä) tai Sharp (terävät pikselit ilman värinää; vain
  `pcsx-abnxt` - klassinen `pcsx-ab` ja RetroArch näyttävät sen Off:ina).
- **Emulator**: SpeedHack CPU-kello SPU-interpolointi boot-logo (off ohittaa BIOS-kuoren - kotitekoisen levyn
  mukauttua logo rikkoo käynnistyksen) ja `pcsx-abnxt`:lla *Smoothing*-suodin ja *Sony hacks*-kytkin.

Triangle nimeää uudelleen pelin Square muuttaa sen muistikorttia Start jakaa uuden kortin. Circle tallentaa
ja poistuu.

**Asetukset tallennettu emulaattoriin.** Emulaattorin omassa valikossa on *Save settings for this game*.
Kun peli on asetukset tallennettuna siellä ne ovat mitä se pelaa ja pelin muokkain näyttää sen Video ja
Emulator-rivit harmaalla noilla arvoilla *Saved in the emulator* -otsikon alla. Palaksesi pelin muokkain
asetuksiin valitse **Unlock the settings** ja vahvista: tämä poistaa asetukset emulaattori tallensi
rivit voidaan muuttaa jälleen. Molemmat emulaattorit `pcsx-ab` ja `pcsx-abnxt` lukevat ja kirjoittavat
samat tallennetut asetukset.

![Pelin muokkain](../images/fi/game-editor.jpg)

### 3.7 Muistikortin ja tallennuspisteet

Jokaisen PS1-pelin omalla muistikortti oletuksena (pidetään tallennus pisteillä `Games/!SaveStates/<peli-
kansio>/`). **Memory Cards** järjestelmävalikossa hallinnoi **jaetuja sarjoja** - kortti useita pelejä käytön
pidetty `Games/!MemCards/`: luo yksi (Square näytön näppäimistöllä) nimeä uudelleen (Cross) poista
(Triangle). Peli laitetaan sarjaan muokkaimessa *Change memory card* tai sen Memory Card -kuvakkeesta.

**Muistikortin muokkain** (Memory Card -kuvake) näyttää pelin kortin ja toisen kortin vierekkäin
kunkin tallenteen kuvake ja otsikko: kopioi tallennus kahden välillä (Square) poista yksi (Triangle)
poista vaaditusta kortista (Select). Start vaihtaa kortin oikealla toiseen sarjaan.

![Muistikortin muokkain](../images/fi/memory-card-editor.jpg)

**Jatkopisteet**: kun poistut PS1-pelistä konsolinin Reset-painikkeella (tai emulaattorin valikosta Pi:ssä tai
PC:ssä) AutoBleem pitää tallennus pisteen siitä missä olit ja tarjoaa sen **Resume**-kuvakkeen alla -
neljä paikkaa kussakin kuvan hetkenä. Cross jatkaa paikasta Triangle poistaa sen. Peli jonka jatkopiste
näyttää pienen kuvan sen Resume-kuvakkeella.

### 3.8 Pelien RetroArchin ja Appsin käynnistäminen

**Cross** käynnistää valitun pelin. PS1-peli käynnistyy valitussa PS1-emulaattorissa (osio 3.5) kokoruudulla
kunnes poistut siitä - konsolissa etu-**Reset**-painikkeella (takaisin käynnistäjään jatkopisteellä) tai
**Power** (konsoli sammuu); Pi:ssä tai PC:ssä emulaattorin in-game-valikosta (Select + Start ohjaimella tai
Esc näppäimistöllä). **Square** käynnistää PS1-pelin RetroArchissa sijaan.

**RetroArch**-peli käynnistyy RetroArchissa ytimellä jonka käynnistäjä valitsi sen järjestelmälle;
*Close Content* tai *Quit RetroArch* sen valikosta palaa käynnistäjään. RetroArchin kohde järjestelmävalikossa
avaa RetroArchin oman valikon (XMB) tyhjällä sisällöllä sen asetuksille ja omia sisältö luetteloille.

**App** (Apps-sarja: konsolivälineet ja konsolissa lisä ohjelmat RetroArch-pakkaus tuo - Doom Quake Amiga ...)
näyttää ensin sen readme:n; Cross käynnistää sen Circle menee takaisin.

![Appsin readme ennen sen käynnistämistä](../images/fi/app-start.jpg)

### 3.9 Pelien lisääminen

**PS1-pelit** menevät `Games`-kansioon **yksi kansio per peli** pelin mukaan nimetty:

```
Games/
  Crash Bandicoot/            Crash Bandicoot.cue + Crash Bandicoot.bin
  Final Fantasy VII/          Final Fantasy VII (Disc 1).chd (Disc 2).chd (Disc 3).chd
  Platformers/                pelikansio: oman ryhmän sarja valitsimessa
    Klonoa/                   Klonoa.pbp
```

- Muodot: `.cue` + `.bin` (tai `.img`) `.pbp` `.chd` (zstd liian) `.ecm` (dekoodattu skannauksessa) `.iso`.
  Zipsattu peli toimii myös: **Unzip**-prosessori purkaa sen ennen skannusta (osio 3.13).
- Multi-levy peli on yksi kansio jokaisen levyn kanssa; kansiot nimetty `Game (Disc 1)` `Game (Disc 2)` ...
  yhdistetään yhdeksi `Game`-kansioksi skannauksessa.
- Pelit jotka pudotetaan suoraan `Games/`-kansioon (irralliset tiedostot) lajitellaan kansioihin skannauksessa.
- **Kansi** on PNG pelin kuvan vieressä samalla tavalla nimetty. Ilman sitä kansikausi tulee kansikausi
  tietokannoista tai - asennetulla RetroArchilla - libretron pikkukuva sarjasta; Pi:ssä PC:ssä tai
  Windowsissa puuttuva haetaan verkon yli (Asetukset → *Fetch box art online*).
- Skannaus lukee kunkin levyn sarjanumeron ja ottaa otsikon julkaisijan vuoden pelaajat ja alueen
  RetroArchin PlayStation tietokannasta tai kansikausi tietokannoista. Muuta mitään pelin muokkaimessa
  ja valitse *Lock data* pitääksesi sen.

**Muut järjestelmät** menevät `RetroArch/roms/`-kansioon **yksi kansio per järjestelmä nimetty kuin RetroArchin
tietokannat** (kansio luodaan sinulle): `Nintendo - Nintendo Entertainment System` `Nintendo - Super Nintendo
Entertainment System` `Sega - Mega Drive - Genesis` `Nintendo - Game Boy Advance` `FBNeo - Arcade Games` (tai
`Arcade`) ... ROM:eja voidaan säilyttää zipattuna. Pi:ssä PC:ssä tai Windowsissa skannaus lukee ne itse ja
kirjoittaa RetroArchin soitto luettelot; konsoli tikulla ajaa **UpdateRomsin** PC:ssä (luku 5).

**Apps** menevät `Apps/<name>/`-kansioon `app.ini`-tiedostolla (nimi kuvake mitä ajaa) ja `run.sh`-tiedostolla.

**Teemat** menevät `Themes/<name>/`-kansioon (`theme.json` ja kuvat) - tai pudota teeman zip `Themes/`:iin.

### 3.10 PS1 BIOS

**PlayStation Classicsissa** emulaattori käyttää konsolinin omaa BIOS:ia. **Raspberry Pi:ssä PC:ssä ja
Windowsissa** laita oma PS1 BIOS:isi `System/Bios/`-kansioon: `romw.bin` (US/European SCPH-5501/5502) ja
`romJP.bin` (Japanese SCPH-5500). Asentajat täyttävät ne RetroArch BIOS-paketeista ellei omat tiedostosi
ole jo siellä. Ilman niitä emulaattori käynnistyy sisäänrakennetulla HLE BIOS:lla jonka monet pelit sietävät
ja jotkut eivät.

### 3.11 Päivitykset

- **Raspberry Pi PC-tikku Windows**: käynnistäjä tarkistaa sivua käynnistyksellä ja kerran päivässä (Asetukset
  → *Updates* on kanava; *Software Update* järjestelmävalikossa tarkistaa nyt). Kun uudempi AutoBleem tai
  RetroArch on se kysyy: *Update now* lataa kaiken ja ajaa asennusohjelman uudelleen ensimmäisen käynnistyksen
  edistymis näyttöä; *Remind me tomorrow* ja *Skip this version* ovat muita vastauksia. Pelisi ja asetukset
  pysyvät; käynnistäjä skannaa uudelleen kerran päivityksen jälkeen.
- **PlayStation Classic**: aja uudempi `AutoBleemInstaller.exe` tikun yli (osio 2.1).

### 3.12 Laajennukset ja AutoBleem Store

**Laajennukset** lisäävät omia näyttöjä käynnistäjään. Ne asuvat `Extensions/<name>/`-kansiossa tikulla (Pi:ssä
sen dataosuus Windowsissa datakansio); asennaksesi yhden pura sen zip sinne. **L2 + R2 → Extensions** listaa
ne: Cross ajaa yhden Triangle poistaa sen käytöstä tai uudelleen käyttöön. Laajennus joka tarvitsee verkkoa
ei käynnisty ilman sitä ja se joka pysäytti käynnistäjän poistetaan käytöstä - luettelo sanoo sen.

![Laajennukset luettelo](../images/fi/extensions.jpg)

**AutoBleem Store** on ensimmäinen laajennus: sovellukset ja pelit asentaa yhdellä painikkeella jokaisella
järjestelmällä AutoBleem käynnistyy (PlayStation Classic tarvitsee AutoBleem-kernelin WiFi:n). Sen neljä
välilehteä L1 / R1 niiden välillä:

- **Apps** ja **Games**: mitä lähteet tarjoavat jokaisen kuvan version koon ja lähteen favicon. Asennetut
  kohteet harmaat. Cross asentaa (tai päivittää tai yrittää uudelleen virheen jälkeen) Triangle poistaa mitä
  Store asensi. L2 / R2 tai Vasen / Oikea kääntyvät sivuja **Select** näyttää yhden lähteen kerralla **Start**
  hakee otsikoita. Kohteen kuvat välimuistiin ja voidaan yrittää uudelleen ellei ne lataudu.
- **Downloads**: mitä ladataan odottaa epäonnistuneita tai asennettuja. Edistymispalkki päivittyy tasaisesti.
  Lataukset jatkuvat taustalla myös kun poistut Storesta; pelin käynnistäminen tai virran sammuttaminen
  keskeyttää ne vain ja pysäytetty lataus jatkuu missä se pysähtyi. Asennettu peli ilmestyy hyllylle
  seuraavalla skannauksella Storen kuva kantenaan. Yli 2 Gt:n lataukset toimivat kaikilla alustoilla
  mukaan lukien 32-bittiset rakenteet.
- **Sources**: mistä luettelot tulevat - AutoBleemin oma luettelo TSV-luettelo pudotettu `System/Extensions/
  store/sources/`-kansioon ja osoitteet jotka lisäät **Add a source URL**:lla. Jokainen lähde näyttää
  favicon:nsa luettelossa. Cross yhdessä jonka lisäsit nimeää uudelleen muuttaa sen osoitetta tai poistaa sen.

![Storen Apps-välilehti](../images/fi/store-apps.jpg)

![Lähteen valikko](../images/fi/store-source-menu.jpg)

Mitä AutoBleemin luettelo tarjoaa on myös lueteltu lataus sivulla `https://autobleem.retromenele.pl/store/`.
**Olet vastuussa siitä mitä lähteet joita lisäät sisältävät.**

**Omasi pelit verkossasi**: `abstored` Storen LAN-palvelin palvelee PS1-pelien kansiota Storelle samalla
verkolla. Se käynnistyy millä tahansa Linux-koneella - Raspberry Pi kotitalon palvelin - ja vain lukee
kansion. Aloita se `abstored <peli-kansio>`-komennolla avaa `http://<se kone>:8124/` selaimessa nähdäksesi
mitä se palvelee ja mitkä ongelmat se löysi ja lisää `http://<se kone>:8124/store.tsv` lähteeksi. Valmiit
ohjelmat Linux:ille ja Windowsille ovat Store-sivulla sen **LAN server**-välilehdessä; sen asettaminen
palveluksi on `INSTALL-linux.md` (`ext_store/server/` lähteessä). **LAN Share** (osio 5.2) asettaa pelit
ja levyt PC:stä sellaiselle palvelimelle.

### 3.13 Skannaus prosessorit

**Skannaus prosessorit** ovat pieniä ohjelmia jokainen skannaus ajaa ennen kuin se lukee pelisi. Yksi voi
muuttaa muodon AutoBleem ei lue yhdeksi se tekee - zippattu peli esimerkiksi - tai muuttaa pelin dataa
kuten käännös patch. Ne asuvat `System/Processors/<name>/`-kansiossa tikulla (Pi:ssä sen dataosuus
Windowsissa datakansio); asennaksesi yhden pura kansio sinne. Seuraava skannaus ajaa sen.

- **Unzip tulee AutoBleemin kanssa**: se purkaa zippuja PS1-pelejä `Games/`-kansiosta ennen skannaamisen
  lukemista niitä ja zippuja ROM:ia yksittäin (arcade-sarjat pysyvät zipattuna). AutoBleemin päivittäminen
  päivittää sen myös ja jättää sen pois käytöstä jos poistit sen käytöstä.
- Prosessori joka jo käsitteli pelia ei aja sitä uudelleen kunnes peli muuttuu.
- Kun prosessori toimii kupla oikealla ylhäällä näyttää mitä se tekee; varoitus tai virhe näkyy sen alla
  olevalla rivillä. `processors.log` lokansiossa on tiedot.
- Pelin tai RetroArchin käynnistäminen pysäyttää prosessorin joka muuttaa tiedostoja; seuraava skannaus
  lopettaa sen työn.

**L2 + R2 → Scanner processors** näyttää ne ajojärjestyksessä yksi välilehti PS1-peleille ja yksi ROM:ille
(L1 / R1). **Square** nostaa prosessorin ylös ja Ylös / Alas siirtää sen - järjestys merkitsee: prosessori
joka purkaa on tultava ennen sitä joka korjaa mitä purettiin. **Cross** kytketään pois tai päälle **Triangle**
on se katsoa jokaista peliä uudelleen seuraavalla skannauksella **Circle** menee takaisin ja aloittaa skannauksen
jos muutit mitään. Prosessori rakennettu toiselle koneelle pysyy luettelossa harmaalla.

![Skannaus prosessorit](../images/fi/processors.jpg)

Kirjoita omiasi: Unzip-sivu `https://github.com/autobleem2/proc_unzip` selittää kaiken mitä prosessori
täytyy tehdä ja `tools/proc_check.py` AutoBleemin lähteessä tarkistaa yhden ennen kuin jaat sen.

<!-- pagebreak -->

## 4. Näytöt

### 4.1 Peli Manager

PS1-pelit luettelona kansioineen ja valitut omansa ja tiedot. Cross avaa pelin muokkain **Square poistaa
pelin** (sen kansio ja toisen kysymyksen jälkeen sen tallennuspisteet) Triangle poistaa jokaisen kannen PNG
pelajen vierestä (skannaus ottaa ne tietokannoista uudelleen) L2 / R2 sivu. Aseman vapaa tila on oikealla
ylhäällä. Peli Manager odottaa kun skannaus käynnissä.

![Peli Manager](../images/fi/game-manager.jpg)

### 4.2 Laitteiston tieto

Koneen faktat - järjestelmä laitteisto tallennustila vapaa tila verkkoosoitteet näyttö ja ääni ajurit
liitetyt ohjaimet - uudelleen luettu jokainen sekunti. PlayStation Classicsissa AutoBleem-kernelin kanssa
tämä avaa **PSC-Bios**:in sijaan (luku 6).

![Laitteiston tieto](../images/fi/hardware-info.jpg)

### 4.3 Painikeo opas

Triangle hyllyllä: jokainen painike jokaisen näytön sivulla. Kun USB-näppäimistö on liitetty tai on käytetty
Keyboard-sarake näyttää näppäimet ohjaimen painikkeiden rinnalla.

![Painikeo opas](../images/fi/button-guide.jpg)

### 4.4 Näytön näppäimistö

Kaikkialla missä teksti kirjoitetaan - muistikortti sarja pelin otsikko WiFi salasana lähteen osoite - sama
näppäimistö asettelu kuin puhelimessa: kirjaimet sivu symboleja (`/ \ : ? & = % @ #` ja loput osoite tai
salasana tarvitsee) ja kaksi sivua aksentoituja kirjaimia Shift sivunäppäimellä Välilyönti Backspace ja
Vahvista alaosassa. Suunnat liikkuvat Cross kirjoittaa Triangle poistaa Square välilyönti **L1**
Shift (kahdesti isoille kirjaimille) **R1** seuraava sivu **L2 / R2** liikkuvat kursoria Start vahvistaa
Circle peruuttaa. USB-näppäimistö kirjoittaa milloin tahansa: Enter vahvistaa Esc peruuttaa.

![Näytön näppäimistö](../images/fi/keyboard.jpg)

<!-- pagebreak -->

## 5. PC:lla

### 5.1 UpdateRoms - konsolintikun päivittäminen

PlayStation Classicsilla ei ole verkkoa joten RetroArch-luettelot ja kansikauden tikullaan tehdään PC:llä:
**UpdateRoms** tekee PC:llä mitä käynnistäjän skannaus tekee Pi:ssä PC:n verkolla ja konsolinin teillä
joten konsoli käynnistyy ja löytää kaiken paikallaan.

1. Kopioi ROM:isi tikulle `RetroArch/roms/<system>/`-kansioon (osio 3.9). Kansionimet on oltava RetroArchin
   tietokannan nimet; asentaja tekee yleiset.
2. Käynnistä **`UpdateRoms\UpdateRoms.exe` tikusta** (asentaja laittaa sen sinne). Se löytää tikun siitä
   missä se istuu näyttää vaiherivin edistymispalkin ja login ja:
   - lataa RetroArchin tietokannan paketin kun tikulla ei ole ja tunnistaa jokaisen ROM:in sen jälkeen -
     peli jonka tietokanta tuntee saa oikean nimensä;
   - kirjoittaa yksi soitto luettelo per järjestelmä `RetroArch/bin/playlists/`-kansioon konsolinin teillä
     ja pitää mitä RetroArch itse lisäsi sinne;
   - noutaa kansikauden jokaisesta ROM:ista jolla ei ole mitään libretron pikkokuva palvelimista
     `RetroArch/bin/thumbnails/`-kansioon.
3. Ota tikku turvallisesti pois ja laita se takaisin konsoliin. RetroArchin välilehti sarja valitsimessa
   listaa jokaisen järjestelmän jolla on pelejä.

Aja se uudelleen jokaisen muutoksen jälkeen ROM-kansioissa; kansio jonka mikään ei muuttunut ohitetaan joten
uudelleen ajo on nopea. Loki on `System/Logs/updateroms.log`. Raspberry Pi kortti kortin lukijassa voidaan
päivittää samalla tavalla (`UpdateRoms.exe <asema> --target rpi`) vaikka Pi tekee sen itse kun sillä on
verkkoa.

### 5.2 LAN Share - pelisi ja levysi palvelimella verkollasi

**LAN Share** (`LanShare.exe` Store-sivulla sen **LAN server**-välilehdessä) asettaa PS1-pelisi Storen
palvelimelle kotiverkollasi - `abstored` Raspberry Pi:llä NAS:lla tai toisella PC:llä - ja lukee PS1-levyn
PC:n CD/DVD-asemasta sille. Store konsolissa Pi:ssä tai PC:ssä asentaa ne sieltä. Mitään asennettavaa;
asetukset säilytetään `%LOCALAPPDATA%\AutoBleem LAN Share\`-kansiossa.

![LAN Share-ikkuna](../images/fi/lanshare.jpg)

1. **Palvelin**: kirjoita sen osoite (`http://<sen osoite>:<portti>` kuten Store siinä) ja paina **Connect**.
   Sen pelit ja ongelmat sen skannaus löysi ovat listattuna vasemmalla. Pelien laittamiseksi siihen anna yksi:
   - **Share** - palvelimen pelien kansio kuten se on jaettu verkossa (Samba) esim. `\\raspberrypi\games`:
     LAN Share kopioi pelit sinne ja pyytää palvelinta skannia. Palvelin itse pysyy vain luettavissa.
   - **Token** - kun palvelin käynnistettiin `--allow-uploads`:lla: sen tunnus (palvelin tulostaa sen
     käynnistyksellä ja pitää sitä `<state>/upload-token`-kansiossa). LAN Share lataa HTTP:n yli ja
     pysäytetty lataus jatkuu missä se pysähtyi.
2. **Pelit tässä PC:ssä**: valitse pelikansio (yksi kansio per peli) raksita pelit ja paina **Publish
   the ticked games**. **On the server** sanoo onko palvelimella peli jo (sen sarjanumeron mukaan tai
   otsikon mukaan); sellaista peliä ei lähetetä koskaan kahdesti. **Tick those not on the server** raksittaa
   loput.
3. **Levy**: laita PS1-levy asemaan ja paina **Read a disc and publish it**. Levy luetaan kokonaan `.bin`
   + `.cue`-tiedostoksi (ja `.sbi` LibCrypt peliin kun asema antaa alikanaavat) nimetty sen otsikon mukaan
   tarkistettu tunnetun hyvän dump:in mukaan (kun tietokannat valitaan) ja julkaistu. Peliin usealla levyllä
   raksita **The game has more than one disc**: LAN Share kysyy jokaisen seuraavan levyn ja julkaisee ne
   yhdessä yhtenä pelinä.
4. **Remove from the server...** ottaa valitut pelit pois palvelimelta. Mitään ei poisteta: jokainen
   siirretään `.removed`-kansioon palvelimen pelien vieressä ja sen siirtäminen takaisin laittaa sen takaisin.

**Tietokannat** - AutoBleemin kansikauden kansio (`coversU/P/J.db`) ja RetroArchin `Sony - PlayStation.rdb` -
antavat otsikot ja levy tarkistus; molemmat valinnaiset. **Also share the games on this PC with the Store**
(oletuksena pois) palvelee tämän PC:n kansiota Storelle suoraan. Ensimmäisellä kerralla Windows kysyy
palomuuria: salli vain yksityiset verkot.

<!-- pagebreak -->

## 6. Konsolivälineet (PlayStation Classic)

Kaksi välinettä PlayStation Classic-tikulle. Molemmat piirtävät käynnistäjän teemalla ja kielellä ja molemmat
ajetaan ohjaimella - ja ohjaimen ohjaussuutarissa konsolinin etupainikkeilla. **PSC-Bios** on laajennus joka
tulee konsolepaketin kanssa: järjestelmävalikon *Network & Controllers* avaa sen ja se on laajennuksien
luettelossa. **ABFlashKit** on App Apps-sarjassa.

### 6.1 PSC-Bios

Laajennus joka tulee konsolepaketin kanssa myös saatavana Raspberry Pi:llä ja PC-tikulla. Se avataan
Järjestelmävalikon *Network & Controllers*-kohteesta (tai laajennuksien luettelosta). Kun laajennus on
asennettu mutta poistettu käytöstä *Network & Controllers*-kohde Quick-valikossa ja järjestelmävalikossa on
harmaa muistiinpanolla "enable it in Extensions" - Cross avaa laajennuksien luettelon siinä.

Avaus näyttö näyttää koneen faktat: aika aikavyöhyke WiFi/Ethernet/Bluetooth verkkokäyttöliittymät osoitteillaan
ja jokainen liitetty ohjain olivatko sillä painike karttoitus. Verkon ja Bluetooth-osat tarvitsevat AutoBleem-
kerneliin konsolilla (osio 6.2) tai järjestelmä välineisiin Pi:llä / PC-tikulla; ohjaimen ohjaussuutari
toimii millä tahansa järjestelmällä.

![PSC-Bios: Network & Controllers-keskityttin](../images/fi/pscbios-main.jpg)

- **Select - Wi-Fi Network** (kernel tai NetworkManager): verkon nimi (kirjoitettu tai poimittu skannauksesta)
  salasana ajaja tilaksi ja *Apply / Restart Network*. Aikavyöhyke asetetaan myös täällä. Konsolinin
  IP-osoite näytetään kun yhdistetty.
- **Square - Bluetooth Controllers**: skannaus Bluetooth-ohjaimille (DualShock 4 jne.) yhdistämiselle tai
  poistamiselle.
- **L1 - DualShock 3 Pairing**: vain USB-yhteys ensimmäiselle DualShock 3:lle ytimen sixaxis-liitännäisen
  kautta.
- **R1 - Controller Mapping**: karttoitus ohjaussuutari (alla).
- **Triangle - About** **Circle - takaisin** käynnistäjään.

**Ohjaimen ohjaussuutari** näyttää liitetyn ohjaimen raakana - jokainen akseli painike ja hattu numeroina
ja DualShock-kuva joka syttyy kun painat. Koska testi ohjainta ei voi luottaa ohjaussuutari ajetaan
konsolinin **etupainikkeilla**: **RESET** vaihtaa seuraavaan ohjaimen **OPEN** aloittaa karttoitusta (sitten
vastaa jokaiseen kysymykseen - paina valaistusta painiketta kuvassa tai OPEN kun ohjaimella ei ole sellaista
painiketta) **POWER** peruuttaa tai poistuu. Pidä Circle ohjaimella 2 sekuntia poistuu ohjaussuutarista
(palkki täyttyy ja jalan vihje sanoo "Hold 2 s: Exit"). Kun ohjaimella ei ole kartoitusta pidä mitään
painiketta 2 sekuntia tekee sen ("Hold any button 2 s: Exit"). Lyhyt paina karttoitetaan normaalisti.
Näppäimistöllä Esc / Välilyönti / Enter seisovat POWER / RESET / OPEN:lle. Lopussa uusi karttoitus lisätään
testiksi ja OPEN tallentaa sen nimen joka valitset; käynnistäjä lataa sen siitä.

![PSC-Bios: ohjaimen karttoitus ohjaussuutari](../images/fi/pscbios-wizard.jpg)

### 6.2 ABFlashKit - AutoBleem kernel

AutoBleem kernel on valinnainen korvaus konsolinin Linux-kernelille: se tuo työskenttelevän kellon USB-WiFi
ja Bluetooth-sovitin (PSC-Bios:iin ja Bluetooth-ohjaimiin) ja etupainike tuki jonka emulaattori käyttää
jatkopisteille. ABFlashKit asentaa sen varmuuskopioi konsolinin ensin ja voi laittaa konsolinin takaisin
stock:iin Sony:n oman palautuksen kautta.

> **Tämä väline kirjoittaa konsolinin flash-muistiin.** Flash joka katkeaa - virta katkaistaan tikku vedettäviin
> voidaan jättää konsoli käynnistymiskyvyttömäksi ja mukautettu kernel asennettavaksi mitätöi sen takuun.
> Pidä konsoli virta ja tikku sisällä kunnes se käynnistyy itse uudelleen. ABFlashKit avautuu tähän
> varoitukseen; *I understand* menee eteenpäin *Quit* poistuu.

![ABFlashkit:n valikko](../images/fi/abflashkit-menu.jpg)

- **Flash Kernel**: tekee palautus varmuuskopian konsolinin osioista tikulle (`LBOOT.EPB`) jos sitä ei ole
  vielä tarkistaa sen ja kernel-kuvan kirjoittaa kerneliin ja AutoBleemin järjestelmä tiedostoihin ja
  käynnistyy uudelleen. *All done - when the screen goes black replace power cord*: vedä konsolinin virta
  irti ja liitä se takaisin sisään.
- **Full backup**: kaikki neljä osiota `LBOOT.EPB`:iin palautusta varten myöhemmin (edellinen varmuuskopio
  ylikirjoitetaan kysymyksen jälkeen).
- **Restore Mode**: tarkistaa varmuuskopion on stock yksi asettaa palautus lipun ja käynnistyy Sony:n
  palautukseen joka palauttaa konsolinin `LBOOT.EPB`:stä tikulla - tie takaisin stock firmware:iin.

Edistymispalkin jokaisen vaiheen alle näkee kuinka pitkällä toiminta on. Väline kieltäytyy flash:in konsolista
joka käyttää muuta mukautettua firmware:a (BleemSync Project Eris): palauta se takaisin stock:iin ensin.

<!-- pagebreak -->

## 7. Jos jokin menee pieleen

- **Lokit**: AutoBleem pitää lokinsa muistissa joten tikku ei kirjoita koko ajan - ne saavuttavat
  `System/Logs/`-kansioon tikulla kortilla tai datankansiossa vain kun jokin menee pieleen: käynnistäjä
  kaatuu PS1-peli tai RetroArch tallentavat ne `System/Logs/crash-<n>/`-kansioon (kolme viimeistä pidetään)
  ja käynnistäjä sanoo niin kerran kun se tulee takaisin. Jokaisen lokin pitämiseksi vaihtaa *Options ->
  Diagnostics -> Keep logs on the stick* (seuraavasta käynnistyskerrasta) tai luo tyhjä tiedosto
  `System/Logs/keep` PC:lle. Pi:ssä tai PC:ssä *Hardware Information* näyttää missä lokit ovat ja Square
  tallentaa ne `System/Logs/saved-<n>/`-kansioon. Tiedostot: `autobleem.log` (käynnistäjä) `launch.log`
  ja `pcsx.log` (PS1-pelin käynnistys ja emulaattori lähtö) `retroarch.log` ja - aina tikulla - `update.log`
  (online-päivitys) ja `updateroms.log` (UpdateRoms).
- **Peli ei ole hyllyllä**: tarkista kansion asettelu (yksi kansio per peli kuvat muodot osion 3.9).
  *Game Manager* listaa kansiota skannaus kielsi pelien jälkeen merkitty *Not added* syyn kanssa; Square poistaa
  tällaisen kansion. Re-Scan järjestelmävalikossa ajaa skannauksen uudelleen.
- **Ei kansia**: kansikausi tietokannat eivät asennettu (aja asentaja uudelleen ne raksitettuna) tai
  RetroArch-peleille konsolissa UpdateRomsia ei ole ajettu PC:llä.
- **Ohjain ei tee mitään tai se on painikkeet sekoitettu**: PSC-Bios:in ohjaimen ohjaussuutari (konsoli)
  kartoittaa sen; Pi:ssä tai PC:ssä *Hardware Information* sivu näyttää mitä SDL näkee.
- **Konsoli näyttää mustaa näyttöä pelin jälkeen**: AutoBleem rakentaa ikkunansa uudelleen itse (jopa
  kolme kertaa); jos se pysyy mustana pidä power-painiketta ja kytke konsoli takaisin päälle.
- **Raspberry Pi**: `Alt+F2` antaa kirjautumis kehotteen toisessa konsolissa; SSH on käytössä ensimmäisestä
  käynnistyskerrasta. `sudo journalctl -u autobleem` näyttää käynnistäjän palvelun; `sudo systemctl restart
  autobleem` käynnistää sen uudelleen. Ensimmäinen käynnistys joka ei voinut valmistua (verkkoa) yrittää
  uudelleen seuraavalla käynnistyskerralla.
- **Windows**: `Esc` poistuu käynnistäjästä; datakansio on asennuksessa valitsemasi (`Documents\AutoBleem`
  oletuksena) lokit ovat sen `System\Logs`-kansiossa.

AutoBleem on vapaaa ohjelmistoa (GNU GPL v3 tai myöhempi) ilman takuuta. Tuki ja uutiset: Discord-palvelin
linkitetty About-näytöllä ja https://autobleem.retromenele.pl/.

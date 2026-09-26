# Manual utilizator AutoBleem 2

AutoBleem 2 este un launcher de jocuri pentru **PlayStation Classic** - și, din versiunea 2, pentru
**Raspberry Pi**, **PC pornit dintr-un stick USB** și **Windows**. Afișează jocurile PS1 ca un raft de
coperți cu cover art-ul și detaliile acestora, le pornește în emulatorul PCSX inclus și, cu RetroArch
instalat lângă acesta, joacă jocurile din alte sisteme. Acest manual acoperă instalarea pe fiecare
platformă, utilizarea zilnică și instrumentele care vin cu aceasta.

> Descărcările pentru fiecare platformă sunt la **https://autobleem.retromenele.pl/**. Pagina este
> organizată după platformă: panoul *Install* din fiecare este ceea ce descărcați; *Build inputs*
> de mai jos este ceea ce instalatoarele descarcă singure.

## 1. Ce primești

- **Lansatorul** - raftul de coperți, seturile (PlayStation, RetroArch, Apps), detaliile jocului,
  meniul sistem, opțiunile, instrumentele pentru carduri de memorie și puncte de reluare. Același
  program pe fiecare platformă.
- **Doi emulatoare PS1** - `pcsx-abnxt`, cel actual (cel implicit) și `pcsx-ab`, cel clasic pe care
  AutoBleem l-a livrat întotdeauna. Alegi unul din opțiuni; ambii folosesc aceleași setări și
  carduri de memorie.
- **RetroArch** (opțional pe fiecare platformă) pentru alte sisteme: NES, SNES, Mega Drive, Game
  Boy, arcade și multe altele. AutoBleem construiește listele RetroArch din ROM-urile pe care le
  copiezi și pornește fiecare joc cu nucleul corect.
- **Instrumentele pentru consolă** (numai PlayStation Classic): *PSC-Bios* pentru WiFi, ceas și
  maparea gamepad-ului, și *ABFlashKit* pentru instalarea kernel-ului AutoBleem.
- **UpdateRoms** pentru Windows: reîmprospătează listele RetroArch și cover art-ul unui stick de
  consolă pe un PC, deoarece consola în sine nu are rețea.

![Lansatorul: raftul de coperți, detaliile jocului selectat, indicii butoane](../images/en/launcher.jpg)

<!-- pagebreak -->

## 2. Instalare

### 2.1 PlayStation Classic

Ai nevoie de un PC cu Windows, un stick USB (USB 2.0, 8 GB sau mai mult; instalatorul îl formatează
dacă ceri) și consola standard. AutoBleem rulează din stick fără nici o schimbare în consolă. Stick-ul
trebuie să fie **FAT32** pentru o consolă standard - kernel-ul acesteia nu poate citi exFAT. Doar o
consolă cu kernel-ul AutoBleem instalat (ABFlashKit, capitolul 6) pornește și din stick exFAT, ceea ce
ridică limita de 4 GB a FAT32.

1. Descarcă **AutoBleemInstaller-<versiune>.zip** din panoul PlayStation Classic al site-ului și
   dezarhivează-l oriunde. Conține `AutoBleemInstaller.exe` și pachetul AutoBleem pe care îl
   instalează.
2. Conectează stick-ul și pornește `AutoBleemInstaller.exe`. Alege unitația din partea de sus.
   Bifează ce vrei:
   - **Formatează stick-ul** - doar pentru un stick nou (totul pe acesta este șters). Alege FAT32
     dacă nu are consola kernel-ul AutoBleem.
   - **Baze de date de coperți** - cover art-ul și detaliile bibliotecii PS1 (bifat implicit; circa
     300 MB).
   - **RetroArch** - RetroArch cu nucleele sale, aplicațiile suplimentare (Doom, Quake, Amiga, ...)
     și activele libretro, pentru jocurile din alte sisteme. Implicit oprit; poate fi adăugat mai
     târziu rulând din nou instalatorul.
   - **Fișiere BIOS** - fișierele BIOS pe care le necesită nucleele RetroArch (necesită RetroArch).
   - **Jocuri de exemplu** - câteva jocuri homebrew gratuite pentru ca raftul să nu fie gol.
3. Apasă **Install** și așteaptă. Barele de progres și jurnalul arată fiecare pas; stick-ul se
   numește `SONY` la sfârșit, iar `UpdateRoms` este pus pe acesta (vezi capitolul 5).
4. Scoate stick-ul în siguranță, conectează-l la **al doilea port USB al consolei** (cel din dreapta,
   jucătorul 2) și pornește consola. AutoBleem pornește în loc de meniul standard.

**Pornirea și oprirea.** Cu stick-ul conectat, consola pornește, luminile se aprind pentru câteva
secunde (AutoBleem este preluat) și apoi merge în standby înainte de a fi afișat ceva - aceasta este
modul propriu al consolei de a pregăti o actualizare, prin care AutoBleem ajunge să ruleze. Apasă
**Power** o dată și lansatorul apare. *Power Off* în meniu sistem, sau butonul Power al consolei,
pune consola în **standby-ul AutoBleem**: stick-ul este deconectat mai întâi, apoi lumina devine
**roșie** - semnul că AutoBleem funcționează așa cum este intenționat - și următoarea apăsare a
Power aduce lansatorul direct înapoi, în câteva secunde. **În timp ce lumina este roșie, stick-ul
poate fi tras** și pus într-un PC fără ca Windows să îi ceară să-l verifice; pune-l înapoi înainte
de a apăsa Power. Deconectarea puterii consolei merge din nou prin standby-ul de boot la următoarea
pornire.

Pentru a **actualiza** un stick, rulează un installer mai nou peste acesta: jocurile, salvările,
setările și conținutul RetroArch rămân; doar fișierele proprii ale AutoBleem sunt înlocuite. Un stick
făcut cu AutoBleem 1.0 sau AutoBleem-NG este adus la noul layout automat.

> Consola standard nu are ceas și nici rețea: datele sunt afișate doar după instalarea kernel-ului
> AutoBleem (capitolul 6), iar cover art-ul pentru jocurile RetroArch provine din UpdateRoms pe PC
> (capitolul 5).

Jocurile merg în folderul `Games` al stick-ului, un folder pe joc - vezi secțiunea 3.9 pentru layout.

### 2.2 Raspberry Pi

AutoBleem transformă un Pi într-o consolă mică: pornește direct în lansator, fără desktop. Două imagini
gata sunt pe site - 32-biți și 64-biți - plus o arhivă tar pentru un Raspberry Pi OS Lite existent.

| Model | Imagine 32-biți | Imagine 64-biți | Note |
|---|---|---|---|
| Raspberry Pi 5 | da | da | |
| Raspberry Pi 4 Model B, Pi 400 | da | da | |
| Raspberry Pi 3 Model B / B+ / A+ | da | da | fin pentru lansator și PS1 |
| Raspberry Pi Zero 2 W | da | da | 512 MB RAM: PS1 rulează, nucleele RetroArch mai grele nu |
| Raspberry Pi 2 Model B | da | doar v1.2 | lent pentru orice 3D |
| Raspberry Pi 1, Zero, Zero W | nu | nu | ARMv6 - nici una din imagini nu rulează |

**Imaginea 32-biți este recomandată** pentru jocuri PS1: recompilatorul rapid ARM al `pcsx-ab` este
doar 32-biți, deci versiunea 64-biți rulează jocurile PS1 mai lent. Imaginea 64-biți are setul mai
mare de nuclee RetroArch.

**Cu Raspberry Pi Imager:**

1. Instalează Raspberry Pi Imager (raspberrypi.com/software). În *Choose OS* alege *Use custom* și
   `autobleem-<versiune>-rpi-armhf.img.xz` (32-biți) sau `-arm64.img.xz` (64-biți) pe care ai
   descărcat - sau adaugă URL-ul repository-ului `https://autobleem.retromenele.pl/rpi-imager/os_list.json`
   în setările aplicației și alege AutoBleem din listă.
2. Folosește ecranul de personalizare al Imager (roțița, sau întrebarea după *Next*) pentru a seta
   **numele de utilizator și parola, rețeaua WiFi și țara, și activează SSH**. AutoBleem are nevoie
   de o rețea la prima pornire.
3. Scrie cardul, pune-l în Pi cu ecran și tastatură sau gamepad conectat, și pornește.

**Prima pornire** durează 5 până la 25 de minute și arată ce face pe ecran. Fără rețea cere una (o
listă WiFi, parola), apoi întreabă dacă să instaleze RetroArch (un minut fără răspuns înseamnă da),
mărește partiția sistem, face partiția de date `AUTOBLEEM` din restul cardului, instalează RetroArch
și nucleele sale, pachetele BIOS și jocurile de exemplu, și reboots în lansator.

Răspunsurile pot fi date în avans în **`autobleem.txt`** pe partiția de boot a cardului (editabilă pe
orice PC înainte de prima pornire):

| Cheie | Implicit | Sens |
|---|---|---|
| `root_gib` | 8 | Dimensiunea partiției sistem în GiB; restul devine partiția jocurilor. |
| `hdmi_mode` | 1920x1080@60 | Modul ecran pentru întregul boot (`1280x720@60` pentru un TV mai vechi). |
| `retroarch` | (cerut) | `da` / `nu` - RetroArch și alte sisteme, sau doar PS1. |
| `thumbnails` | niciuna | `boxarts` oglindește setul complet de cover art offline (~9000 fișiere). |
| `bios`, `downloads`, `samples` | da | Setează pe `nu` pentru a sări peste pachetele BIOS, fiecare descărcare sau jocuri de exemplu. |

**Pe un Raspberry Pi OS Lite existent** (Bookworm sau Trixie): copiază `autobleem-rpi.tar.gz`
(sau cel arm64) pe Pi, dezarhivează și rulează `sudo bash install.sh`. Cere aceleași întrebări, face
partiția de date prin micșorarea rădăcinii la următoarea pornire (`--shrink-root <GiB>`), și pune
lansatorul pe prima consolă.

După install, partiția cardului **`AUTOBLEEM`** (exFAT) este ceea ce completezi: scoate cardul și
deschide-l pe orice PC, sau copiază peste rețea (SSH este pornit). `Games/` pentru jocuri PS1,
`RetroArch/roms/<sistem>/` pentru alte sisteme, `System/Bios/` pentru BIOS PS1 (secțiunea 3.10),
`Themes/` pentru teme.

### 2.3 Stick PC USB

Același aparat pentru orice PC care pornește din USB - un sistem 32-biți, deci mașini vechi
funcționează și:

1. Descarcă `autobleem-<versiune>-pcusb-i386.img.xz` din panoul PC și scrie-l pe un stick de 8 GB
   sau mai mult cu Raspberry Pi Imager (*Use custom*), balenaEtcher sau Rufus (mod DD).
2. Pornește PC-ul din stick (cheia meniului de boot a PC-ului - F12, F8, Esc...). Ambele boot BIOS
   și UEFI funcționează; **Secure Boot trebuie să fie oprit**.
3. Prima pornire este a Pi: o întrebare de rețea dacă nu e cablu, întrebarea RetroArch, apoi
   instalarea - circa opt minute cu rețea conectată - și un reboot în lansator.

Stick-ul are apoi o partiție `AUTOBLEEM` pentru jocurile tale, vizibilă pe Windows 10 (1903 și mai
nou) ca al doilea unitate când pui stick-ul într-un PC în funcțiune. `autobleem.txt` este pe prima
partiție, cu aceleași chei ca pe Pi (fără `hdmi_mode` - PC-ul folosește modul nativ al ecranului).

### 2.4 Windows

AutoBleem ca program Windows: ecran plin, emulatoarele și RetroArch pornite ca programe.

1. Descarcă **AutoBleemSetup-<versiune>.exe** și rulează-l. Se instalează per utilizator, fără drepturi
   de administrator: programul sub `%LOCALAPPDATA%\Programs\AutoBleem`, datele (jocuri, setări, teme,
   RetroArch) într-un folder pe care-l alegi - `Documents\AutoBleem` implicit.
2. Bifează componentele - bazele de date de coperți, RetroArch (versiunea oficială Windows și
   nucleele sale), fișierele BIOS, jocurile de exemplu - și lasă asistentul de instalare să le
   descarce.
3. Pornește AutoBleem din Meniu Start sau Desktop. Pe un PC tastatura funcționează ca gamepad
   (secțiunea 3.2).

Rularea unui setup mai nou peste acesta actualizează programul și păstrează folderul de date.
Lansatorul verifică, de asemenea, site-ul o dată pe zi și oferă o actualizare când este una (secțiunea
3.11).

<!-- pagebreak -->

## 3. Utilizarea AutoBleem

### 3.1 Lansatorul

Lansatorul se deschide pe raft: coperțile setului curent, cea selectată în mijloc, detaliile acesteia
lângă - editor, an, serie, regiune, jucători, când a fost jucat ultima oară - și un buton de joc.
Bara din jos listează ce fac butoanele. O scanare a folderului de jocuri rulează în fundal la fiecare
pornire; în timp ce rulează, o bulă în colțul din dreapta sus arată progresul, iar jocuri noi apar pe
raft pe măsură ce sunt găsite.

![Selectorul de seturi: trei file și grupele curente cu numărătorile lor](../images/en/set-picker.jpg)

### 3.2 Controale

| Buton | Pe raft |
|---|---|
| Stânga / Dreapta | Jocul anterior / următor. Ținerea derulează pe. |
| L1 / R1 | Sari la prima literă anterioară / următoare a titlurilor. |
| Cross | Pornește jocul selectat (un joc PS1 în emulatorul PS1; un joc RetroArch în nucleul său; o aplicație după citirea sa). |
| Square | Pornește jocul PS1 selectat în RetroArch în loc. |
| Triangle | Ghidul butoanelor. |
| Start | Un joc aleatoriu din setul curent. |
| Select | Selectorul de seturi: file PlayStation / RetroArch / Apps (L1 / R1), grupele filei (Sus / Jos, L2 / R2 o pagină), Cross alege. |
| Jos | Deschide rândul de pictograme sub joc (Setări, Joc, Card Memorie, Reluare). Sus o închide. |
| L2 + R2 | Meniu sistem (secțiunea 3.4). |

**Cu o tastatură** (un PC fără gamepad, sau o tastatură USB pe consolă, Pi sau stick PC) tastele
stau în: **Săgeți** = d-pad, **Enter** = Cross, **Esc sau Backspace** = Circle, **Tab** = Triangle,
**Space** = Square, **F1 / F2** = Select / Start, **Page Up / Page Down** = L1 / R1, **Home / End** =
L2 / R2, **F10** = meniu sistem. Pe o mașină de dezvoltare Esc închide programul și Space este Start.

În fiecare listă și meniu: Sus / Jos se mută, **L2 / R2 întorc pagini**, L1 / R1 sar la rândul
primul / ultim, **Cross selectează, Circle merge înapoi**. Un ecran cu setări le salvează când îl
lași cu Circle.

![Rândul de pictograme sub jocul selectat](../images/en/launcher-icons.jpg)

### 3.3 Seturile

**Select** deschide selectorul de seturi. Fila PlayStation listează *Toate Jocurile*, *Jocuri Interne*
(cele douăzeci încorporate ale consolei, pe PlayStation Classic), fiecare folder pe care l-ai făcut
sub `Games/` (un joc într-un subfolder aparține acelui grup), *Jocuri Favorite*, *Istoric Jocuri* și,
când vreun joc este marcat ca uno, *Jocuri Lightgun*. Fila RetroArch listează un grup pe sistem care
are jocuri, plus Favorite și Istoric al RetroArch. Fila Apps grupează aplicații după tip: *Toate
aplicațiile*, apoi *Jocuri*, *Emulatoare*, *Instrumente*, *Media* și *Altele* (categoria se
stabilește în fișierul `app.ini` al fiecărei aplicații). Fiecare rând arată câte articole are; un
grup fără nici una se deschide pe un raft gol cu rândul de pictograme arătând doar Setări.

### 3.4 Meniul Rapid

**Sus** în lansator, sau **pictograma cu roată dințată** din rândul de pictograme (unde Setări / Joc
/ Card Memorie / Reluare sunt): meniu rapid pentru acțiuni pe care le ajungi din carrusel. O scurtă
listă: *Rescaneaza Jocurile* (pornește o scanare acum), *Store* (AutoBleem Store pentru descărcare
extensii), *Rețea și Controlere* (doar unde o extensie instalată oferă intrarea `network` - PSC-Bios
pe consolă, Pi și stick PC: WiFi, asociere Bluetooth, ghidul mapării gamepad - vezi secțiunea 6;
gri cu "pornește-o din Extensii" când acea extensie este dezactivată - Cross deschide lista
Extensii), și *Meniu sistem...*
(meniul complet de mai jos). Sus / Jos se mută (înfășurând), Cross alege, Circle înapoi. Nimic
nu este unic aici - fiecare articol este și în meniu sistem.

### 3.5 Meniu sistem

**L2 + R2** (împreună, în orice ordine) deschide meniu sistem peste raft. Meniul este grupat în
secțiuni:

| Secțiune | Articol | Ce face |
|---|---|---|
| (sus) | Rescaneaza Jocurile | Caută jocuri noi, modificate sau eliminate acum (scanarea și monitorizează folderul singură). |
| | Extensii | Extensiile de pe stick - AutoBleem Store și altele (secțiunea 3.12). |
| **Bibliotecă** | Manager Jocuri | Jocurile PS1 ca o listă cu folderele lor: șterge un joc, cuață coperțile. Dezactivat în timp ce o scanare rulează. |
| | Carduri Memorie | Seturile tale de carduri de memorie (secțiunea 3.7). |
| | Procesoare de scanare | Programele pe care fiecare scanare le rulează mai întâi - ordinea lor, pornite sau oprite (secțiunea 3.13). Dezactivate în timp ce o scanare rulează. |
| **Sistem** | Opțiuni | Setările AutoBleem (secțiunea 3.6). |
| | Rețea și Controlere | Doar unde o extensie instalată oferă intrarea `network` (`Provides=network` în `extension.ini` - PSC-Bios pe consolă, Pi și stick PC) - WiFi, asociere kontroler Bluetooth, setare DualShock 3, și ghidul mapării gamepad - vezi capitolul 6. Când acea extensie este instalată dar dezactivată, articolul rămâne gri cu o notă "pornește-o din Extensii" - Cross deschide lista Extensii la aceasta. |
| | Informatii Hardware | Faptele mașinii: sistem, CPU, stocare, interfețe de rețea, fus orar, ecran, gamepad-urile și mapările lor. Pe o consolă cu kernel AutoBleem aceasta deschide PSC-Bios (capitolul 6); pe alte mașini arată această pagină de informații. |
| | Actualizare software | (Raspberry Pi și PC) Verifică site-ul pentru un AutoBleem sau RetroArch mai nou acum. |
| | Despre | Credite și licență. |
| **Ieșire** | RetroArch | Lasă lansatorul pentru meniu propriu RetroArch. Închiderea RetroArch vine înapoi. |
| | Power Off | După confirmă: pe consolă standby-ul AutoBleem - stick-ul deconectat, lumina roșie, Power aduce lansatorul înapoi (secțiunea 2.1); pe Pi sau PC mașina se oprește. |

![Meniu sistem](../images/en/system-menu.jpg)

### 3.6 Opțiuni

Setările sunt în grupuri; Sus / Jos se mută între ele, Stânga / Dreapta schimbă o valoare, Circle
pleacă și salvează. Fiecare schimbare este aplicată imediat.

| Grup / setare | Ce face |
|---|---|
| **Interfață**: Tema AutoBleem | Aspectul. Temele trăiesc în `Themes/`; o temă zip aruncată acolo este dezarhivată la următoarea vizită. Temele pe care AutoBleem le livrez sunt reîmprospătate la fiecare actualizare - pentru a personaliza una, copiază-o sub un nume nou mai întâi. |
| Stil Coperta | Cadrul cutiei de bijuterii desenat în jurul coperților PS1. |
| Limba | Limba lansatorului, aplicată imediat (17 limbi). |
| Folosește Font din Temă / Font | Fontul ecranelor clasice: al temei, sau orice `.ttf`/`.otf` din `resources/fonts`, `RetroArch/fonts` sau folderul temei. |
| Afișare Timeout | Cât timp notificarea "Se arată: ..." rămâne, în secunde (0 = pentru totdeauna). |
| **Sunet**: Muzică, Muzică de Fundal | Ce cântec cântă sub lansator (al temei, sau un fișier din `resources/music`), și dacă cântă sau nu. |
| **Emulare**: Emulator PS1 | `pcsx-abnxt` (implicit: PCSX-ReARMed curent cu adăugirile AutoBleem) sau `pcsx-ab` (clasic). Un punct de reluare salvat de altul continuă în altul, dacă jocul nu a rulat fără un fișier BIOS. |
| Ecran Lat | Forma imaginii emulatorului PS1 pentru fiecare joc. |
| Joaca toate jocurile PSX cu RA | Fiecare joc PS1 pornește în nucleul PS1 al RetroArch. |
| Actualizează Configurație RA | AutoBleem scrie setările sale în configurația RetroArch când pornește un joc acolo. |
| **Bibliotecă**: Arată Jocuri Interne | Jocurile încorporate ale consolei în listele PlayStation (numai PlayStation Classic). |
| Preia cover art online | Scanarea prelucrează coperțile lipsă de pe serverele libretro (Raspberry Pi, PC, Windows). |
| **Actualizări** | (Raspberry Pi, PC, Windows) `stabil`, `cel mai nou` (și pre-versiunile) sau `oprit`. |

![Opțiunile, în grupuri](../images/en/options.jpg)

### 3.7 Setări ale unui joc

Cu un joc selectat, **Jos** deschide rândul său: **Setări** (opțiunile de mai sus), **Joc** (setările
proprii ale jocului), **Card Memorie** (cardul său) și **Reluare** (punctele sale salvate). Cross
deschide pe cel sub cursor.

**Editorul de joc** arată detaliile jocului pe dreapta și setările pe stânga, în trei grupuri:

- **Joc**: *Favorit* (în grupa Jocuri Favorite), *Joc Lightgun* (un joc cu pistol ușor - se alătură
  grupei Lightgun și rulează întotdeauna în RetroArch, al cărui nucleu PS1 are GunCon), *Joaca
  folosind RA* (acest joc rulează în RetroArch), *Restricționează date* (scanerul lasă titlul,
  seria și lista de discuri ale jocului așa cum le-ai stabilit).
- **Video**: rezoluție înaltă, liniile de scanare și nivelul acestora, omitere cadru, pluginul GPU,
  și *Filtru* - cum se scalează imaginea: Oprit (pixeli simpli), Liniar (netezit) sau Ascuțit
  (pixeli crisp fără tremurare; doar `pcsx-abnxt` - clasicul `pcsx-ab` și RetroArch o arată ca
  Oprit).
- **Emulator**: SpeedHack, frecvența CPU, interpolare SPU, logo de boot (oprit omite shell BIOS -
  pentru un disc homebrew al cărui logo personalizat rupe boot-ul), și cu `pcsx-abnxt` filtrul
  *Netezire* și comutatorul *Corecții Sony*.

Triangle redenumește jocul, Square schimbă cardul de memorie, Start partajează un card nou. Circle
salvează și pleacă.

**Setări salvate în emulator.** Meniu propriu emulatorului are *Salvează setări pentru acest joc*.
Odată ce un joc are setări salvate acolo, acelea sunt cu care se joacă, și editorul de joc arată
rândurile Video și Emulator gri, cu acele valori, sub titlul *Salvat în emulator*. Pentru a reveni
la setările editorului de joc, alege **Deblochează setările** și confirmă: aceasta șterge setările
pe care emulator le-a salvat, și rândurile pot fi din nou schimbate. Ambii emulatoare, `pcsx-ab` și
`pcsx-abnxt`, citesc și scriu aceleași setări salvate.

![Editorul de joc](../images/en/game-editor.jpg)

### 3.7 Carduri de memorie și puncte de reluare

Fiecare joc PS1 are propriul card de memorie implicit (păstrat cu punctele sale salvate în
`Games/!SaveStates/<folder joc>/`). **Carduri Memorie** din meniu sistem gestionează **seturi
partajate** - un card folosit de mai multe jocuri, păstrat în `Games/!MemCards/`: creează uno
(Square, cu tastatura pe ecran), redenumește (Cross), șterge (Triangle). Un joc se pune pe un set
cu *Schimbă cardul de memorie* din editorul său, sau din pictograma Card Memorie.

**Editorul de card de memorie** (pictograma Card Memorie) arată cardul jocului și un al doilea card
lângă, cu fiecare pictogramă și titlu de salvare: copiază o salvare între cele două (Square), șterge
una (Triangle), defragmentează un card (Select). Start schimbă cardul din dreapta cu un alt set.

![Editorul de card de memorie](../images/en/memory-card-editor.jpg)

**Puncte de reluare**: când lași un joc PS1 cu butonul Reset al consolei (sau meniu emulatorului pe
Pi sau PC), AutoBleem păstrează o stare salvată a locului și o oferă sub pictograma **Reluare** -
patru sloturi, fiecare cu o imagine a momentului. Cross continuă din slot, Triangle o șterge. Un
joc cu un punct de reluare arată o imagine mică pe pictograma Reluare.

### 3.8 Pornirea jocurilor, RetroArch și Apps

**Cross** pornește jocul selectat. Un joc PS1 rulează în emulatorul PS1 ales (secțiunea 3.5), pe
ecran plin, până când îl lași - pe consolă cu butonul **Reset** din față (înapoi în lansator cu un
punct de reluare) sau **Power** (consola se oprește); pe Pi sau PC prin meniu în-joc al emulatorului
(Select + Start pe pad, sau Esc pe tastatură). **Square** pornește un joc PS1 în RetroArch în loc.

Un joc **RetroArch** pornește în RetroArch cu nucleul pe care lansatorul l-a ales pentru sistemul
său; *Închide conținut* sau *Oprește RetroArch* din meniu vine înapoi în lansator. Articolul
RetroArch din meniu sistem deschide meniu propriu RetroArch (XMB) fără nimic încărcat, pentru
setările sale și listele de conținut proprii.

O **Aplicație** (setul Apps: instrumentele consolei, și pe o consolă aplicațiile suplimentare pe care
le aduce pachetul RetroArch - Doom, Quake, Amiga, ...) arată mai întâi informațiile sale; Cross o
pornește, Circle merge înapoi.

![Informațiile unei aplicații înainte de a porni](../images/en/app-start.jpg)

### 3.9 Adăugarea jocurilor

**Jocurile PS1** merg în folderul `Games`, **un folder pe joc**, numit după joc:

```
Games/
  Crash Bandicoot/            Crash Bandicoot.cue + Crash Bandicoot.bin
  Final Fantasy VII/          Final Fantasy VII (Disc 1).chd, (Disc 2).chd, (Disc 3).chd
  Platformers/                un folder de jocuri: un grup propriu în selectorul de seturi
    Klonoa/                   Klonoa.pbp
```

- Formate: `.cue` + `.bin` (sau `.img`), `.pbp`, `.chd` (și zstd), `.ecm` (decodat de scanare),
  `.iso`. Un joc zipat funcționează și: procesatorul **Unzip** îl dezarhivează înainte de scanare
  (secțiunea 3.13).
- Un joc multi-disc este un folder cu fiecare disc în el; folderele numite `Game (Disc 1)`, `Game
  (Disc 2)` ... sunt unite într-un folder `Game` de scanare.
- Jocurile aruncate direct în `Games/` (fișiere libere) sunt sortate în folderele de scanare.
- O **coperta** este un PNG lângă imaginea jocului, numit ca aceea. Fără una, cover art-ul vine din
  bazele de date de coperți, sau - cu RetroArch instalat - din setul de miniaturi libretro; pe Pi,
  PC sau Windows una lipsă este preluată online (Opțiuni → *Preia cover art online*).
- Scanarea citește seria fiecărui disc și ia titlul, editorul, anul, jucători și regiunea din baza
  de date PlayStation a RetroArch sau bazele de date de coperți. Schimbă orice în editorul de joc și
  bifează *Restricționează date* pentru a-l păstra.

**Alte sisteme** merg sub `RetroArch/roms/`, **un folder pe sistem, numit cum sunt bazele de date
ale RetroArch** (folderul este făcut pentru tine): `Nintendo - Nintendo Entertainment System`,
`Nintendo - Super Nintendo Entertainment System`, `Sega - Mega Drive - Genesis`, `Nintendo - Game
Boy Advance`, `FBNeo - Arcade Games` (sau `Arcade`), ... ROM-urile pot rămâne zipate. Pe Pi, PC sau
Windows scanarea le citește singură și scrie listele RetroArch; pe stick consolă, rulează
**UpdateRoms** pe PC (capitolul 5).

**Aplicațiile** merg sub `Apps/<nume>/` cu un `app.ini` (numele, pictograma, ce să ruleze) și o
`run.sh`.

**Temele** merg sub `Themes/<nume>/` (`theme.json` și imaginile) - sau aruncă zip-ul temei în
`Themes/`.

### 3.10 BIOS PS1

Pe **PlayStation Classic** emulatorul folosește BIOS propriu al consolei. Pe **Raspberry Pi, PC și
Windows** pune propriul BIOS PS1 în `System/Bios/`: `romw.bin` (SUA/European SCPH-5501/5502) și
`romJP.bin` (Japonez SCPH-5500). Instalatoarele le umplu din pachetele BIOS RetroArch dacă propriile
tale fișiere nu sunt deja acolo. Fără ele emulatorul rulează pe BIOS HLE încorporat, pe care multor
jocuri le place și unele nu.

### 3.11 Actualizări

- **Raspberry Pi, stick PC, Windows**: lansatorul verifică site-ul la start și o dată pe zi (Opțiuni →
  *Actualizări* este canalul; *Actualizare software* din meniu sistem verifică acum). Când este un
  AutoBleem sau RetroArch mai nou cere: *Actualizează acum* descarcă totul și re-rulează instalatorul
  cu ecranul de progres de prima pornire; *Amintește-mi mâine* și *Sari peste această versiune* sunt
  alte răspunsuri. Jocurile și setările tale rămân; lansatorul rescănează o dată după o actualizare.
- **PlayStation Classic**: rulează un `AutoBleemInstaller.exe` mai nou peste stick (secțiunea 2.1).

### 3.12 Extensii și AutoBleem Store

**Extensiile** adaugă propriile ecrane în lansator. Trăiesc în `Extensions/<nume>/` pe stick (pe
Raspberry Pi partiția de date, pe Windows folderul de date); pentru a instala una, dezarhivează
zip-ul acolo. **L2 + R2 → Extensii** le listează: Cross rulează una, Triangle o oprește sau o pornește
din nou. O extensie care necesită rețea nu este pornită fără una, și una care a oprit lansatorul este
oprită - lista spune asta.

![Lista Extensii](../images/en/extensions.jpg)

**AutoBleem Store** este prima extensie: Aplicații și jocuri de instalat cu o apăsare, pe fiecare
sistem pe care AutoBleem rulează (o PlayStation Classic necesită kernel-ul AutoBleem's WiFi). Patru
file, L1 / R1 între ele:

- **Aplicații** și **Jocuri**: ce oferă sursele, fiecare cu imaginea, versiunea, dimensiunea și
  faviconul sursă. Articolele instalate sunt gri. Cross instalează (sau actualizează, sau reîncearcă
  după o eșec), Triangle elimină ceea ce Store a instalat. L2 / R2 sau Stânga / Dreapta întorc
  pagini, **Select** arată o sursă la un moment, **Start** caută titlurile. Imaginile articolelor
  sunt cache și pot fi reîncearcate dacă eșuează să se încarce.
- **Descărcări**: ceea ce se descarcă, așteaptă, a eșuat sau este instalat. Bara de progres se
  actualizează constant. Descărcările continuă în fundal, și după ce lași Store; pornirea unui joc
  sau oprirea doar le pun în pauză, și o descărcare oprită se reia de unde s-a oprit. Un joc instalat
  apare pe raft după următoarea scanare, cu imaginea Store ca coperta. Descărcări peste 2 GB funcționează
  pe toate platformele, inclusiv versiuni 32-biți.
- **Surse**: de unde vin listele - catalogul propriu AutoBleem, o listă TSV aruncată în
  `System/Extensions/store/sources/`, și adresele pe care le adaugi cu **Adaugă un URL sursă**. Fiecare
  sursă arată faviconul în listă. Cross pe una pe care ai adăugat-o o redenumește, schimbă adresa,
  schimbă între `http://` și `https://`, sau o elimină.

![Fila Apps a Store](../images/en/store-apps.jpg)

![Meniu sursă](../images/en/store-source-menu.jpg)

Ceea ce catalogul AutoBleem oferă este, de asemenea, listat pe site-ul de descărcare,
`https://autobleem.retromenele.pl/store/`. **Ești responsabil pentru ceea ce conțin sursele pe care
le adaugi.**

**Jocurile tale proprii pe rețea**: `abstored`, serverul LAN al Store, servește un folder de jocuri
PS1 către Store pe aceeași rețea. Rulează pe orice mașină Linux - un Raspberry Pi, un server acasă -
și doar citește folderul. Pornește-l cu `abstored <folder jocuri>`, deschide `http://<acea
mașină>:8124/` într-un browser pentru a vedea ce servește și orice probleme găsite, și adaugă
`http://<acea mașină>:8124/store.tsv` ca sursă. Programe gata pentru Linux și Windows sunt pe pagina
Store, în fila **Server LAN**; setarea ca serviciu este `INSTALL-linux.md` (`ext_store/server/` în
sursă). **LAN Share** (secțiunea 5.2) pune jocuri și discuri din PC pe un server ca acesta.

### 3.13 Procesoare de scanare

**Procesoarele de scanare** sunt mici programe pe care fiecare scanare le rulează înainte să citească
jocurile. Unu poate transforma un format pe care AutoBleem nu îl citește într-uno pe care o face - un
joc zipat, de exemplu - sau schimbă datele unui joc, cum ar fi un patch de traducere. Trăiesc în
`System/Processors/<nume>/` pe stick (pe Raspberry Pi partiția de date, pe Windows folderul de date);
pentru a instala unu, dezarhivează folderul acolo. Următoarea scanare o rulează.

- **Unzip vine cu AutoBleem**: dezarhivează jocurile PS1 zipate în `Games/` înainte ca scanarea să le
  citească, și ROM-uri zipate unu câte unu (seturile arcade rămân zipate). Actualizarea AutoBleem o
  actualizează și, și o lasă oprită dacă ai oprit-o.
- Un procesor care deja a lucrat cu un joc nu este rulat pe acesta din nou până nu se schimbă jocul.
- Timp ce un procesor lucrează, bula din colțul din dreapta sus arată ce face; un avertisment sau o
  eșec apare pe linia de sub aceea. `processors.log` în folderul jurnalelor are detaliile.
- Pornirea unui joc sau RetroArch oprește un procesor care schimbă fișiere; următoarea scanare
  termină munca.

**L2 + R2 → Procesoare de scanare** le arată în ordinea în care rulează, o filă pentru jocurile PS1
și una pentru ROM-uri (L1 / R1). **Square** ridică un procesor și Sus / Jos îl mută - ordinea
conteaza: un procesor care dezarhivează trebuie să vină înainte de unu care patchez ceea ce a fost
dezarhivat. **Cross** o oprește sau pornește, **Triangle** face să uite la fiecare joc din nou la
următoarea scanare, **Circle** merge înapoi și pornește o scanare dacă ai schimbat ceva. Un procesor
construit pentru o altă mașină rămâne pe listă, gri.

![Procesoare de scanare](../images/en/processors.jpg)

Scrierea ta proprie: pagina Unzip, `https://github.com/autobleem2/proc_unzip`, explică totul pe care
un procesor trebuie să-l facă, și `tools/proc_check.py` în sursă AutoBleem verifică unu înainte să îl
partajezi.

<!-- pagebreak -->

## 4. Ecrane

### 4.1 Manager Jocuri

Jocurile PS1 ca o listă cu folderele lor și cel selectat și coperța și detaliile. Cross deschide
editorul de joc, **Square șterge jocul** (folderul și, după o a doua întrebare, punctele salvate),
Triangle șterge fiecare PNG coperta lângă jocuri (scanarea le ia din bazele de date din nou), L2 /
R2 pagini. Spațiul liber al unității este în colțul din dreapta sus. Manager Jocuri așteaptă o
scanare să ruleze.

![Manager Jocuri](../images/en/game-manager.jpg)

### 4.2 Informatii Hardware

Faptele mașinii - sistem, hardware, stocare cu spațiul liber, adrese de rețea, drivere de afișare și
audio, gamepad-urile conectate - re-cite o dată la fiecare secundă. Pe PlayStation Classic cu kernel
AutoBleem aceasta deschide **PSC-Bios** în loc (capitolul 6).

![Informatii Hardware](../images/en/hardware-info.jpg)

### 4.3 Ghidul butoanelor

Triangle pe raft: fiecare buton din fiecare ecran pe o pagină. Când o tastatură USB este conectată
sau a fost folosită, o coloană Tastatură arată tastele lângă butoanele pad.

![Ghidul butoanelor](../images/en/button-guide.jpg)

### 4.4 Tastatura pe ecran

Oriunde text este tastat - un set de card memorie, titlul jocului, o parolă WiFi, adresa sursă -
aceeași tastatură, pusă în pagini ca a unui telefon: litere, o pagină de simboluri (`/ \ : ? & = %
@ #` și restul o adresă sau o parolă are nevoie) și două pagini de litere accentuate, cu Shift,
cheia paginii, Space, Backspace și Confirma pe rândul de jos. Direcțiile se mută, Cross tasteaza,
Triangle șterge, Square este un spațiu, **L1** este Shift (de două ori pentru caps lock), **R1**
următoarea pagină, **L2 / R2** mută cursorul, Start confirmă, Circle anulează. O tastatură USB
tastează oricând: Enter confirmă, Esc anulează.

![Tastatura pe ecran](../images/en/keyboard.jpg)

<!-- pagebreak -->

## 5. Pe PC

### 5.1 UpdateRoms - reîmprospătarea stickului consolei

PlayStation Classic nu are rețea, deci listele RetroArch și cover art-ul stickului sunt făcute pe PC:
**UpdateRoms** face pe PC ce scanarea lansatorului face pe Pi, cu rețeaua PC și căile consolei, deci
consola pornește și găsește totul în loc.

1. Copiază ROM-urile pe stick sub `RetroArch/roms/<sistem>/` (secțiunea 3.9). Numele folderelor
   trebuie să fie numele bazelor de date ale RetroArch; instalatorul face pe cele comune.
2. Pornește **`UpdateRoms\UpdateRoms.exe` din stick** (instalatorul l-a pus acolo). Găsește stick-ul
   de unde stă, arată o linie de etapă, o bară de progres și un jurnal, și:
   - descarcă pachetul de baze de date RetroArch când stick-ul nu are nici unu, și identifică fiecare
     ROM din aceasta - un joc pe care baza de date îl cunoaște primește propriul nume;
   - scrie una listă de redare pe sistem în `RetroArch/bin/playlists/` cu căile consolei, păstrând
     orice RetroArch însuși adăugat acolo;
   - prelucrează cover art-ul fiecărui ROM care nu are nici unu de pe serverele de miniaturi libretro
     în `RetroArch/bin/thumbnails/`.
3. Scoate stick-ul în siguranță și pune-l înapoi în consolă. Fila RetroArch a selectorului de seturi
   listează fiecare sistem care are jocuri.

Rulează din nou după fiecare schimbare în folderele ROM; un folder nimic nu s-a schimbat în este
sarit, deci o re-rulare este rapidă. Jurnalul este `System/Logs/updateroms.log`. Un card Raspberry
Pi într-un cititor de carduri poate fi reîmprospătat același fel (`UpdateRoms.exe <unitate> --target
rpi`), deși un Pi o face singur când are o rețea.

### 5.2 LAN Share - jocurile și discurile tale pe serverul pe rețea ta

**LAN Share** (`LanShare.exe`, pe pagina Store din fila **Server LAN**) pune jocurile PS1 pe serverul
Store pe rețeaua acasă - un `abstored` pe un Raspberry Pi, un NAS sau alt PC - și citește un disc PS1
în unitatea CD/DVD a PC. Store pe consolă, Pi sau PC atunci instalează din acolo. Nimic de instalat;
setările sunt păstrate în `%LOCALAPPDATA%\AutoBleem LAN Share\`.

![Fereastra LAN Share](../images/en/lanshare.jpg)

1. **Serverul**: intră adresa (<http://<adresa sa>:<port>`, așa cum Store o are) și apasă **Conectare**.
   Jocurile și orice probleme pe care scanarea sa găsite sunt listate pe stânga. Pentru a pune jocuri
   pe aceasta, dă una din:
   - **Partajare** - folderul de jocuri al serverului cum este partajat pe rețea (Samba), de ex
     `\\raspberrypi\games`: LAN Share copiază jocurile acolo și cere serverului să scăneze. Serverul
     însuși rămâne numai citire.
   - **Simbol** - când serverul a fost pornit cu `--allow-uploads`: simbolul (serverul o tipărește la
     start și o păstrează în `<state>/upload-token`). LAN Share încarcă peste HTTP, și o încărcare
     oprită continuă de unde s-a oprit.
2. **Jocurile pe acest PC**: alege un folder de jocuri (un folder pe joc), bifează jocuri și apasă
   **Publică jocurile bifate**. **Pe server** spune dacă serverul deja are un joc (după seria, altfel
   după titlu); un joc ca acesta nu este niciodată trimis de două ori. **Bifează pe care nu sunt pe
   server** bifează restul.
3. **Un disc**: pune un disc PS1 în unitate și apasă **Citește un disc și publică-l**. Discul este
   citit complet într-un `.bin` + `.cue` (și un `.sbi` pentru un joc LibCrypt, când unitatea dă
   subcanalul), numit după titlu, verificat cu dumping cunoscut bun (când bazele de date sunt alese)
   și publicat. Pentru un joc pe mai multe discuri bifează **Jocul are mai de un disc**: LAN Share
   cere fiecare disc următor și le publică împreună ca un joc.
4. **Elimina din server...** ia jocurile selectate din server. Nimic nu este șters: fiecare este
   mutat în folder `.removed` lângă jocurile serverului, și mutarea înapoi le pune înapoi.

**Bazele de date** - folderul de coperți AutoBleem (`coversU/P/J.db`) și `Sony - PlayStation.rdb`
RetroArch - dau titlurile și verificarea discului citit; ambele sunt opționale. **Partajează și
jocurile pe acest PC cu Store** (implicit oprit) servește folderul pe acest PC către Store direct.
Prima oară, Windows cere despre firewall-ul: permite doar rețele private.

<!-- pagebreak -->

## 6. Instrumentele consolei (PlayStation Classic)

Două instrumente pentru un stick PlayStation Classic. Ambele desenează în tema și limba lansatorului,
și ambele sunt conduse de pad - și, în ghidul gamepad, de butoanele din față ale consolei. **PSC-Bios**
este o extensie care vine cu pachetul consolei: *Informatii Hardware* din meniu sistem o deschide, și
este în lista Extensii. **ABFlashKit** este o Aplicație din setul Apps.

### 6.1 PSC-Bios

O extensie care vine cu pachetul consolei, disponibilă și pe Raspberry Pi și stick PC. Este deschisă
din articolul *Rețea și Controlere* al meniului Sistem (sau din lista Extensii). Când acea extensie
este instalată dar dezactivată, articolul *Rețea și Controlere* în meniu rapid și meniu sistem este
gri cu o notă "pornește-o din Extensii" - Cross acolo deschide lista Extensii la aceasta.

Ecranul de deschidere arată fapte mașinei: timp, fus orar, adaptoare de rețea WiFi/Ethernet/Bluetooth
cu adresele lor, și fiecare controlator conectat cu dacă are o mapare buton. Părțile de rețea și
Bluetooth au nevoie de kernel AutoBleem pe consolă (secțiunea 6.2) sau instrumente de sistem pe
Raspberry Pi / stick PC; ghidul gamepad funcționează pe orice sistem.

![PSC-Bios: centrul Rețea și Controlere](../images/en/pscbios-main.jpg)

- **Select - Rețea WiFi** (kernel sau NetworkManager): numele rețelei (tastat, sau ales din scanare),
  parolă, mod driver, și *Aplică / Repornește Rețea*. Fusul orar este setat și aici. Adresa IP a
  consolei este arătată o dată conectată.
- **Square - Controlere Bluetooth**: o scanare pentru gamepad-uri Bluetooth (DualShock 4, etc.),
  pentru a asoca sau elimina.
- **L1 - Asociere DualShock 3**: conectare doar USB pentru primul DualShock 3, prin pluginul sixaxis
  al kernel.
- **R1 - Mapare Controlere**: ghidul mapării (de mai jos).
- **Triangle - Despre**, **Circle - înapoi** la lansator.

**Ghidul gamepad** arată pad-ul conectat brut - fiecare axă, buton și pălărie ca numere, și o imagine
DualShock care se luminează pe măsură ce apasă. Deoarece pad-ul sub test nu poate fi de încredere,
ghidul este condus de **butoanele din față ale consolei**: **RESET** comută la pad-ul următor, **OPEN**
pornește maparea (apoi răspunde la fiecare întrebare - apasă butonul iluminat pe imagine, sau OPEN
când pad-ul nu are un buton ca acela), **POWER** anulează sau lasă. Ținând Circle pe pad pentru 2
secunde lasă ghidul (o bară se umple și indiciul din footer spune "Ține 2 s: Ieșire"). Cât timp pad-ul
nu are o mapare, ținând orice buton pentru 2 secunde o face ("Ține orice buton 2 s: Ieșire"). O
apăsare scurtă este mapată ca de obicei. Pe o tastatură, Esc / Space / Enter stau în pentru POWER /
RESET / OPEN. La finit noua mapare este adăugată pentru o încercare și OPEN o salvează sub un nume pe
care îl alegi; lansatorul o încarcă din acel moment.

![PSC-Bios: ghidul mapării controlere](../images/en/pscbios-wizard.jpg)

### 6.2 ABFlashKit - kernel-ul AutoBleem

Kernel-ul AutoBleem este o înlocuire opțională a kernel-ului Linux al consolei: aduce un ceas care
lucrează, donguri WiFi și Bluetooth USB (pentru PSC-Bios și pad-uri Bluetooth) și suportul butonelor
din față pe care emulator îl folosește pentru puncte de reluare. ABFlashKit o instalează, face o
copie de siguranță a consolei mai întâi, și poate pune consola înapoi la standard prin recuperarea
proprie Sony.

> **Acest instrument scrie în memoria flash a consolei.** O flash care este întreruptă - curent tăiat,
> stick tras - poate lăsa consola incapabilă să pornească, și instalarea unui kernel personalizat anulează
> garanția. Ține consola alimentată și stick-ul în până se reboot singur. ABFlashKit se deschide la
> acest avertisment; *Înțeleg* merge mai departe, *Ieșire* lasă.

![Meniu ABFlashKit](../images/en/abflashkit-menu.jpg)

- **Instalează Kernel**: face o copie de siguranță a recuperării partiții consolei pe stick (`LBOOT.EPB`)
  dacă nu este nici una încă, o verifică și imaginea kernel, scrie kernel-ul și fișierele de sistem
  AutoBleem, și reboot. *Totul gata - când ecranul devine negru scoate cablul de putere*: trage puterea
  consolei și conectează-o înapoi.
- **Copie de siguranță completă**: toate patru partiții la `LBOOT.EPB`, pentru o restaurare mai târziu
  (copia de siguranță anterioară este suprascrisă după o întrebare).
- **Mod Restaurare**: verifică copia de siguranță este una standard, stabilește semnalul recuperării și
  reboot în recuperarea Sony, care restaurează consola din `LBOOT.EPB` pe stick - calea înapoi la
  firmware-ul standard.

O bară de progres sub fiecare pas arată cât de departe este acțiunea. Instrumentul refuză să instaleze
o consolă care rulează alt firmware personalizat (BleemSync, Project Eris): restaurează-o la standard
mai întâi.

<!-- pagebreak -->

## 7. Dacă ceva se duce rău

- **Jurnale**: AutoBleem ține jurnalele în memorie, deci stick-ul nu este scris la tot orice - ajung
  la `System/Logs/` pe stick, card sau folder de date doar când ceva se duce rău: o cădere a
  lansatorului, a unui joc PS1 sau a RetroArch le salvează la `System/Logs/crash-<n>/` (ultimele trei
  sunt păstrate), și lansatorul spune asta o dată când vine înapoi. Pentru a păstra fiecare jurnal,
  pornește *Opțiuni -> Diagnostice -> Păstrează jurnalele pe stick* (din următoarea pornire), sau
  creează un fișier gol `System/Logs/keep` pe un PC. Pe Pi sau PC, *Informatii Hardware* arată unde
  sunt jurnalele și Square le salvează la `System/Logs/saved-<n>/`. Fișierele: `autobleem.log`
  (lansatorul), `launch.log` și `pcsx.log` (pornirea unui joc PS1 și ieșirea emulatorului), `retroarch.log`,
  și - întotdeauna pe stick - `update.log` (o actualizare online) și `updateroms.log` (UpdateRoms).
- **Un joc nu este pe raft**: verifică layout-ul folderului (un folder pe joc, formaturile imagine din
  secțiunea 3.9). *Manager Jocuri* listează folderele pe care scanarea le-a refuzat după jocuri, marcate
  *Neadăugat*, cu motiv; Square șterge un folder ca acesta. Re-Scanare din meniu sistem rulează
  scanarea din nou.
- **Nicio coperta**: bazele de date de coperți nu erau instalate (rulează instalatorul din nou cu ele
  bifate), sau, pentru jocurile RetroArch pe consolă, UpdateRoms nu a fost rulat pe PC.
- **Un pad nu face nimic sau are butoanele amestecate**: ghidul gamepad PSC-Bios (o consolă) o mapează;
  pe Pi sau PC pagina Informatii Hardware listează ce SDL vede.
- **Consola arată un ecran negru după un joc**: AutoBleem reconstruiește fereastra singur (până la trei
  ori); dacă rămâne negru, ține apăsat butonul de putere și pornește consola din nou.
- **Raspberry Pi**: `Alt+F2` dă o invitație de conectare pe a doua consolă; SSH este activat din prima
  pornire. `sudo journalctl -u autobleem` arată serviciul lansatorului; `sudo systemctl restart autobleem`
  îl repornește. O primă pornire care nu a putut termina (fără rețea) reîncearcă la următoarea pornire.
- **Windows**: `Esc` lasă lansatorul; folderul de date este pe cel ales în instalare (`Documents\AutoBleem`
  implicit), jurnalele sunt în `System\Logs`.

AutoBleem este software liber (GNU GPL v3 sau mai nou), fără garanție. Suport și știri: serverul Discord
legat pe ecranul Despre, și https://autobleem.retromenele.pl/.

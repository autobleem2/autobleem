# Manual de l'utilizaire d'AutoBleem 2

AutoBleem 2 es un lançador de jòcs per la **PlayStation Classic** - e, dempuèi la version 2, per una
**Raspberry Pi**, una **PC que s'avie d'un bastó USB** e **Windows**. Vos aficha los vòstres jòcs PS1
coma una tablada de cobertas amb lor art en boita e de detalhs, los commence dins l'emulator PCSX fornit, e,
amb RetroArch installat a costat, lòc los jòcs d'autres sistèmas. Aqueste manual cobre l'installacion sus
cada plataforma, l'utilizacion quotidiana e los espleches que lo venon d'assemblada.

> Los telecargaments per cada plataforma son a **https://autobleem.retromenele.pl/**. La pagina es
> organizata per plataforma : lo panèl *Install* de cada una es lo que telecargatz ; los *Build inputs*
> avall son los que los installadors telecargon per elei-memes.

## 1. Çò que recebètz

- **Lo lançador** - la tablada de cobertas, los grupos (PlayStation, RetroArch, Aplicacions), los detalhs
  del jòc, lo menú sistèma, las opcions, los espleches de cartas memòria e de ponts de represa. Lo meteis
  programa sus cada plataforma.
- **Dos emulators PS1** - `pcsx-abnxt`, lo corrent (per defaut), e `pcsx-ab`, lo classic que AutoBleem
  a sempre livrat. Escaissiatz l'un o l'autre dins las opcions ; los dos utilison los meteis paramètres e
  cartas memòria.
- **RetroArch** (optional sus cada plataforma) pels autres sistèmas : NES, SNES, Mega Drive, Game Boy,
  arcada e beaucoup mai. AutoBleem baste sas listas de RetroArch a partir dels ROMs que copialitz e
  commence cada jòc amb lo còr apropiat.
- **Los espleches de consòla** (PlayStation Classic sols) : *PSC-Bios* per la WiFi, l'ora e l'associacion
  de las manetas, e *ABFlashKit* per installar lo nucli AutoBleem.
- **UpdateRoms** per Windows : rafraichís las listas de RetroArch e l'art en boita d'un bastó de consòla
  sus una PC, perque la consòla meteis a pas cap de ret.

![Lo lançador : la tablada de cobertas, los detalhs del jòc causit, las indicacions de botons](../images/en/launcher.jpg)

<!-- pagebreak -->

## 2. Installacion

### 2.1 PlayStation Classic

Avètz besoin d'una PC Windows, d'un bastó USB (USB 2.0, 8 GB o mai ; l'installador lo formata se lo
demandatz) e de la consòla stock. AutoBleem se balha dempuèi lo bastó sens cap de cambiament a la consòla.
Lo bastó deu èsser **FAT32** per una consòla stock - son nucli pòt pas legir exFAT. Sols una consòla amb lo
nucli AutoBleem installat (ABFlashKit, capítol 6) s'avie tanben dempuèi un bastó exFAT, çò que lève la
limitacion de 4 GB de FAT32.

1. Telecargatz **AutoBleemInstaller-<version>.zip** dempuèi lo panèl PlayStation Classic del site e
   descompressatz-lo n'importe ont. Conten `AutoBleemInstaller.exe` e lo paquetatge AutoBleem que
   s'installa.
2. Branchiaz lo bastó e avialz `AutoBleemInstaller.exe`. Causisètz lo lecteur al tope. Cocharolatz çò
   que volètz :
   - **Formatar lo bastó** - sols per un bastó nèu (tot lo que i es dins es suprimit). Causisètz FAT32 se la
     consòla a pas lo nucli AutoBleem.
   - **Basas de donadas de cobertas** - l'art en boita e los detalhs de la bibliotèca PS1 (cochat per
     defaut ; environ 300 MB).
   - **RetroArch** - RetroArch amb sos còrs, los aplicacions extras (Doom, Quake, Amiga, ...) e los
     actius libretro, pels jòcs d'autres sistèmas. Desactivat per defaut ; se pòt apondre mai tard en
     avialent l'installador tornamai.
   - **Fichièrs BIOS** - los fichièrs BIOS que los còrs RetroArch an besoin de (necessita RetroArch).
   - **Jòcs exemples** - qualques jòcs dempuèi hobbybuilt liures per que la tablada sigue pas voida.
3. Quichatz **Install** e esperatz. Las barras de progression e lo jornal afichon cada etapa ; lo bastó
   s'apelha `SONY` a la fin, e `UpdateRoms` s'i mèt (vejatz capítol 5).
4. Tiraz lo bastó en seguretat, branchiatz-lo al **second pòrt USB** de la consòla (lo dret, jogaire 2) e
   mesiatz la consòla. AutoBleem s'avie en plaça del menú stock.

**Mesiar e atudar.** Amb lo bastó adinc, la consòla s'avie, cliget sa lum quelques segondas
(AutoBleem se pren) e se bucha en repos davant que res se mostrégue - aiquó es la propria manera de la
consòla de preparar una mesa a jorn, çò que laíssa AutoBleem de s'aviar. Quichatz **Power** un còp e lo
lançador puja. *Atudar* dins lo menú sistèma, o lo boton Power de la consòla, met la consòla en **repos
AutoBleem** : lo bastó se desconecta d'en primièr, puèi la lum se fa **roja** - lo signe que AutoBleem
fonciona coma se deuriá - e lo prossime quichament de Power ramèna lo lançador drech, en qualques
segondas. **Mentre la lum es roja lo bastó se pòt tirar** e se metre dins una PC sens que Windows
demande de verificar-lo ; metètz-lo aicí après de quichatz Power. Lo despluç de la consòla passa per lo
repos d'aviada d'en tornamai al prossime encendut.

Per **metre a jorn** un bastó, avialz un installador mai récent sus-aquó : vòstres jòcs, sauvegardes,
paramètres e lo contengut RetroArch rèstan ; los propres fichièrs d'AutoBleem sí se rempláçon. Un bastó
fait amb AutoBleem 1.0 o AutoBleem-NG se porte a la novèla disposicion automaticament.

> La consòla stock a pas cap d'ora e pas cap de ret : las datas se mostradon sols après que lo nucli
> AutoBleem se sigue installat (capítol 6), e l'art en boita pels jòcs RetroArch vèn de UpdateRoms sus la
> PC (capítol 5).

Los jòcs van dins lo dorsièr `Games` del bastó, un dorsièr per jòc - vejatz la seccion 3.9 per la
disposicion.

### 2.2 Raspberry Pi

AutoBleem convertís una Pi en una petita consòla : s'avie drech dins lo lançador, sens cap de bureau. Doas
imatges prèstas son sus lo site - 32 bits e 64 bits - mai un tarball per un Pi OS Lite existent.

| Modèl | Imatge 32 bits | Imatge 64 bits | Notas |
|---|---|---|---|
| Raspberry Pi 5 | yes | yes | |
| Raspberry Pi 4 Model B, Pi 400 | yes | yes | |
| Raspberry Pi 3 Model B / B+ / A+ | yes | yes | bon pel lançador e PS1 |
| Raspberry Pi Zero 2 W | yes | yes | 512 MB de RAM : PS1 fonciona, los còrs RetroArch mai pesats non |
| Raspberry Pi 2 Model B | yes | v1.2 sols | lent per totes çò que es 3D |
| Raspberry Pi 1, Zero, Zero W | no | no | ARMv6 - cap de las imatges fonciona |

**L'imatge 32 bits es la recomandada** pels jòcs PS1 : lo recompilador ARM rapide de `pcsx-ab` es sols 32
bits, donc la compilacion 64 bits fai los jòcs PS1 mai lents. L'imatge 64 bits a la seria mai granda de
còrs RetroArch.

**Amb Raspberry Pi Imager :**

1. Installez Raspberry Pi Imager (raspberrypi.com/software). En *Choose OS* causisètz *Use custom* e lo
   `autobleem-<version>-rpi-armhf.img.xz` (32 bits) o `-arm64.img.xz` (64 bits) que telecargèrtz - o
   apondètz l'adreça del depòsit `https://autobleem.retromenele.pl/rpi-imager/os_list.json` dins los
   paramètres de l'aplicacion e causisètz AutoBleem dins la lista.
2. Utilizetz la pantala de personalizacion de l'Imager (l'enganta, o la demanda après *Next*) per
   establir lo **nom d'utilizaire e lo senhal, la ret WiFi e lo país, e activar SSH**. AutoBleem a besoin
   d'una ret al prossime aviada.
3. Escribitz la carta, metètz-la en la Pi amb un ecran e un clavièr o una maneta connectada, e mesiatz-la.

**Lo prossime aviada** dura 5 a 25 minutas e aficha çò qu'ella fa sus l'ecran. Sen una ret, demanda
d'una (una lista WiFi, lo senhal), puèi demanda se volètz installar RetroArch (una minuta sens cap de
resposta significa que yes), creis la particion sistèma, fait la particion `AUTOBLEEM` de donadas del
reste de la carta, installa RetroArch e sos còrs, los paquetatges BIOS e los jòcs exemples, e se ra
aviada dins lo lançador.

Las respuestas se podon donar de davansa en **`autobleem.txt`** sus la particion d'aviada de la carta
(modificabla sus qualcuna PC davant lo prossime aviada) :

| Clau | Defaut | Sens |
|---|---|---|
| `root_gib` | 8 | La talha de la particion sistèma en GiB ; lo reste devèn la particion de jòcs. |
| `hdmi_mode` | 1920x1080@60 | Lo mòde d'ecran per tota l'aviada (`1280x720@60` pel televise mai ancien). |
| `retroarch` | (demandat) | `yes` / `no` - RetroArch e los autres sistèmas, o sols PS1. |
| `thumbnails` | cap | `boxarts` espelha tota la seria de cobertas pels ecrans sens connexion (~9000 fichièrs). |
| `bios`, `downloads`, `samples` | yes | Establir a `no` per passar los paquetatges BIOS, totes los telecargaments, o los jòcs exemples. |

**Sus un Pi OS Lite existent** (Bookworm o Trixie) : copialitz `autobleem-rpi.tar.gz` (o lo arm64) a la
Pi, descompressatz-lo e avialtz `sudo bash install.sh`. Demanda los meteis demandas, fait la particion de
donadas en feisant retrair la raitz al prossime aviada (`--shrink-root <GiB>`), e met lo lançador sus la
prossima consòla.

Après l'installacion, la particion **`AUTOBLEEM`** de la carta (exFAT) es çò que remplissètz : tiraz la
carta e dobrítz-la sus qualcuna PC, o copialitz per la ret (SSH es activat). `Games/` pels jòcs PS1,
`RetroArch/roms/<sistèma>/` pels autres sistèmas, `System/Bios/` pel BIOS PS1 (seccion 3.10), `Themes/`
pels tèmas.

### 2.3 Bastó PC USB

Lo meteis aparelh per qualcuna PC que s'avie dempuèi USB - un sistèma 32 bits, donc los aparelhs ancians
funcionan :

1. Telecargatz `autobleem-<version>-pcusb-i386.img.xz` dempuèi lo panèl PC e escribitz-lo a un bastó de
   8 GB o mai amb Raspberry Pi Imager (*Use custom*), balenaEtcher o Rufus (mòde DD).
2. Avialtz la PC dempuèi lo bastó (la tèuca d'aviada de vòstra PC - F12, F8, Esc...). L'aviada BIOS e
   UEFI funcionon ; **Secure Boot deu èsser desactivat**.
3. L'aviada prossima es la de la Pi : una demanda de ret se a pas cap de cable, la demanda RetroArch,
   puèi l'installacion - environ uèch minutas amb una ret filada - e una ra aviada dins lo lançador.

Lo bastó a dempuèi una particion `AUTOBLEEM` pels vòstres jòcs, visibla sus Windows 10 (1903 e mai
récent) coma un deuxième lecteur quand branchiatz lo bastó en una PC en foncionament. `autobleem.txt`
es sus la prossima particion, amb las meteis claus que sus la Pi (cap de `hdmi_mode` - la PC utiliza lo
mòde nativ de l'ecran).

### 2.4 Windows

AutoBleem coma un programa Windows : l'ecran complèt, los emulators e RetroArch aviats coma programas.

1. Telecargatz **AutoBleemSetup-<version>.exe** e avialtz-lo. S'installa per utilizaire, sens cap de
   dreits administratòr : lo programa jos `%LOCALAPPDATA%\Programs\AutoBleem`, las donadas (jòcs,
   paramètres, tèmas, RetroArch) en un dorsièr que causisètz - `Documents\AutoBleem` per defaut.
2. Cocharolatz los componenxs - las basas de donadas de cobertas, RetroArch (la compilacion oficièla de
   Windows e sos còrs), los fichièrs BIOS, los jòcs exemples - e laissétz l'assistant de configuracion
   los telecargador.
3. Avialtz AutoBleem dempuèi lo menú Start o lo Bureau. Sus una PC lo clavièr fonciona coma una maneta
   (seccion 3.2).

Avialent un setup mai récent le met a jorn e garda lo dorsièr de donadas. Lo lançador verifican tanben
lo site una còp per jorn e ofrís una mesa a jorn quand n'i a una (seccion 3.11).

<!-- pagebreak -->

## 3. Utilizar AutoBleem

### 3.1 Lo lançador

Lo lançador s'avie sus la tablada : las cobertas del grup actual, la causida al mièg, sos detalhs a costat
- editor, annada, seria, region, jogaires, la darrièra còp que se jugèt - e un boton de jòc. La barra al
fons alista çò qu'aqueles botons fan. Un escandalhatge del dorsièr de jòcs se fai en fonç a cada aviada ;
mentre se fai, una bombola al tope drech aficha sa progression, e los jòcs novèls aparisson sus la
tablada coma se trobon.

![Lo causidor de grupós : tres ongletas, e los grupós del actual amb lors contes de jòcs](../images/en/set-picker.jpg)

### 3.2 Comandas

| Boton | Sus la tablada |
|---|---|
| Gaut / Dreit | Jòc precedent / segent. En lo mantenir se fai devalada. |
| L1 / R1 | Saltar fins a la letra precedenta / segenta de los títols. |
| Cross | Comenzar lo jòc causit (un jòc PS1 dins l'emulator PS1 ; un jòc RetroArch dins son còr ; una Aplicacion après sa lectura). |
| Square | Comenzar lo jòc PS1 causit dins RetroArch en plaça. |
| Triangle | La guida dels botons. |
| Start | Un jòc aleatòri dempuèi lo grup actual. |
| Select | Lo causidor de grupós : ongletas PlayStation / RetroArch / Aplicacions (L1 / R1), los grupós de l'ongalet (Amont / Avall, L2 / R2 una pagina), Cross causís. |
| Avall | Dobrir la linia d'icons jos lo jòc (Paramètres, Jòc, Carta memòria, Reprendre). Amont la tanca. |
| L2 + R2 | Lo menú sistèma (seccion 3.4). |

**Amb un clavièr** (una PC sens maneta, o un clavièr USB sus la consòla, una Pi o lo bastó PC) las tèclas
se mèton a la plaça : **Sagetas** = cros, **Enter** = Cross, **Esc o Retorn arrièr** = Circle, **Tab** =
Triangle, **Espaci** = Square, **F1 / F2** = Select / Start, **Page Amont / Avall** = L1 / R1, **Home /
End** = L2 / R2, **F10** = lo menú sistèma. Sus una maquina de desvolopament Esc tanca lo programa e
Espaci es Start.

En cada lista e menú : Amont / Avall se deplaçon, **L2 / R2 vireton las paginas**, L1 / R1 salton a la
prossima / darrera linia, **Cross causís, Circle se'n va**. Una pantala amb reglatges los salva quand la
quichatz amb Circle.

![La linia d'icons jos lo jòc causit](../images/en/launcher-icons.jpg)

### 3.3 Los grupós

**Select** dobrís lo causidor de grupós. L'ongalet PlayStation alista *Totes los jòcs*, *Jòcs intèrnes*
(los vint fornits de la consòla, sus una PlayStation Classic), cada dorsièr que fasètz jos `Games/` (un
jòc en un sots-dorsièr apartèn aqueste grup), *Jòcs favorits*, *Istoric dels jòcs* e, quand qualqu'un es
marchat coma un, *Jòcs Lightgun*. L'ongalet RetroArch alista un grup per sistèma qu'a de jòcs, mai los
Favorits e l'Istoric propris de RetroArch. L'ongalet Aplicacions agrupa los aplicacions per tipe : *Totas
las aplicacions*, puèi *Jòcs*, *Emulators*, *Espleches*, *Mèdia* e *Autre* (la categoria es establida en
lo fichièr `app.ini` de cada aplicacion). Cada linia aficha lo nombre d'elements qu'acasela ; un grup que
n'a pas cap se dobrís sus una tablada voida amb la linia d'icons afichant sols Paramètres.

### 3.4 Lo menú rapid

**Amont** dins lo lançador, o l'**icòn d'enganta** dins la linia d'icons (ont Paramètres / Jòc / Carta
memòria / Reprendre son) : lo menú rapid pels accions qu'atrapatz dempuèi la carousèla. Una lista
curteta : *Tornar escandalhar los jòcs* (comença un escandalhatge ara), *Store* (lo Store AutoBleem per
telecargador extensions), *Ret e manetas* (sols ont una extension installada provesís l'entrada `ret` -
PSC-Bios sus la consòla, una Pi e lo bastó PC : WiFi, aparatge Bluetooth, l'assistant de mapeig de
manetas - vejatz la seccion 6 ; estampada amb "activatz-la dins Extensions" quand aquela extension es
desactivada - Cross dobrís la lista d'Extensions), e *Menú sistèma...* (lo menú complèt avall). Amont /
Avall se deplaçon (que se buclanton), Cross causís, Circle se'n va. Res i es uniqua - cada element es
tanben dins lo menú sistèma.

### 3.5 Lo menú sistèma

**L2 + R2** (ensemble, en l'ordre que siáu) dobrís lo menú sistèma sus la tablada. Lo menú es agrupit en
seccios :

| Seccion | Element | Çò qu'ella fa |
|---|---|---|
| (tope) | Tornar escandalhar los jòcs | Cercar los jòcs novèls, modificats o suprimits ara (l'escandalhatge guardá tanben lo dorsièr per elei-matime). |
| | Extensions | Las extensions sus lo bastó - lo Store AutoBleem e d'autres (seccion 3.12). |
| **Bibliotèca** | Gestionari de jòcs | Los jòcs PS1 coma una lista ab lors dorsièrs : suprimir un jòc, voidar las cobertas. Desactivat mentre un escandalhatge se fai. |
| | Cartas memòria | Vòstres jorns de cartas memòria (seccion 3.7). |
| | Processors d'escaneig | Los programas que cada escandalhatge fai d'en primièr - lor òrdre, activats o desactivats (seccion 3.13). Desactivats mentre un escandalhatge se fai. |
| **Sistèma** | Opcions | Los paramètres d'AutoBleem (seccion 3.6). |
| | Ret e manetas | Sols ont una extension installada provesís l'entrada `ret` (`Provides=network` en son `extension.ini` - PSC-Bios sus la consòla, una Pi e lo bastó PC) - WiFi, aparatge Bluetooth del DualShock 3, e l'assistant de mapeig de manetas - vejatz lo capítol 6. Quand aquela extension s'installa mas es desactivada, aqueste element rèsta estampat amb una nòta "activatz-la dins Extensions" - Cross dobrís la lista d'Extensions a aquó. |
| | Entresenhas de material | Los faches de la maquina : sistèma, CPU, emmagazinatge, interfàcias de ret, fusonari, afichatge, las manetas e lors mapeigs. Sus una consòla amb lo nucli AutoBleem aquó dobrís PSC-Bios (capítol 6) ; sus autres maquinas aficha aquesta pagina d'entresenhas. |
| | Mesa a jorn logiciala | (Raspberry Pi e PC) Verificar lo site per una AutoBleem o RetroArch mai récenta ara. |
| | A prepaus | Credits e licéncia. |
| **Quitar** | RetroArch | Quita lo lançador pel menú propri de RetroArch. Tancar RetroArch tornarà. |
| | Atudar | Après una confirmacion : sus la consòla lo repos AutoBleem - lo bastó se desconecta, la lum roja, Power ramèna lo lançador (seccion 2.1) ; sus una Pi o PC la maquina s'atuada. |

![Lo menú sistèma](../images/en/system-menu.jpg)

### 3.6 Opcions

Los reglatges son en grupós ; Amont / Avall se deplaçon entre eis, Gaut / Dreit cambian una valòr, Circle
se'n va e salva. Cada cambiament s'aplica al moment.

| Grup / reglatge | Çò qu'ella fa |
|---|---|
| **Interfàcia** : Tèma AutoBleem | L'aspèct. Los tèmas duelhan en `Themes/` ; un tèma zippat drapat aicí se descompresa a la prossima visita. Los tèmas qu'AutoBleem livra se rafraichisson amb cada mesa a jorn - per personalizar un, lo copialitz jos un nòu nom d'en primièr. |
| Estil cobèrtura | Lo marc de boita en vidre drapat entorn de las cobertas PS1. |
| Lenga | La lenga del lançador, aplicada al moment (17 lengas). |
| Utilizar la polissa del tèma / Polissa | La polissa de las pantalas classics : la del tèma, o qualqu'una `.ttf`/`.otf` dempuèi `resources/fonts`, `RetroArch/fonts` o lo dorsièr del tèma. |
| Afichatge Timeout | Quant temps la notificacion "Afichatge : ..." rèsta, en segondas (0 = per tot còp). |
| **Son** : Musica, Musica de fons | Qual pista sona jos lo lançador (la del tèma, o un fichièr dempuèi `resources/music`), e se una sona o non. |
| **Emulacion** : Emulator PS1 | `pcsx-abnxt` (lo defaut : PCSX-ReARMed actualizada amb los apondiments AutoBleem) o `pcsx-ab` (lo classic). Un punt de represa salvat per un se contunha en l'autre, tóla que lo jòc aja escut fonccionar sens fichièr BIOS. |
| Grand ecran | La forma de l'imatge de l'emulator PS1 per cada jòc. |
| Jogar a totes los jòcs PSX amb RA | Cada jòc PS1 s'avie dins lo còr PS1 de RetroArch. |
| Metre a jorn la configuracion RA | AutoBleem escríu sos paramètres dins la configuracion de RetroArch quand l'avie un jòc aicí. |
| **Bibliotèca** : Afichar los jòcs intèrnes | Los jòcs fornits de la consòla en las listas PlayStation (PlayStation Classic sols). |
| Recuperar las cobertas en linha | L'escandalhatge telecarga las cobertas manquantas dempuèi los servidors de libretro (Raspberry Pi, PC, Windows). |
| **Mesas a jorn** | (Raspberry Pi, PC, Windows) `stable`, `latest` (los pre-sortidas tanben) o `off`. |

![Las opcions, en grupós](../images/en/options.jpg)

### 3.7 Los paramètres d'un jòc

Amb un jòc causit, **Avall** dobrís sa linia d'icons : **Paramètres** (las opcions avall), **Jòc** (los
paramètres propris del jòc), **Carta memòria** (sa carta memòria) e **Reprendre** (sos ponts de represa).
Cross dobrís la que es jos lo cursor.

Lo **modificador de jòc** aficha los detalhs del jòc a la drecha e sos paramètres a la squèrra, en tres
grupós :

- **Jòc** : *Favorits* (en lo grup Jòcs favorits), *Jòc Lightgun* (un jòc de pistolet - se jónh al grup
  Lightgun e sempre fonciona dins RetroArch, que lo còr PS1 d'aquó a lo GunCon), *Jogar amb RA* (aqueste
  jòc fonciona dins RetroArch), *Verrolhar las donadas* (l'escandalhatge guarda lo títol, la seria e la
  lista de disques coma vos los establissètz).
- **Vidèo** : nalta resolucion, linhas d'escansion e lor nivèl, enjamb de frame, lo plugin GPU, e lo
  *Filtre* - coma s'aumenta l'imatge : Off (pixels plans), Linear (lissats) o Sharp (pixels nets sens
  broglotas ; `pcsx-abnxt` sols - lo classic `pcsx-ab` e RetroArch los aficho coma Off).
- **Emulator** : SpeedHack, l'ora del CPU, l'interpolacion SPU, lo lògo d'aviada (desactivat salta la
  cocha BIOS - pel disc dempuèi caseta personalizat que trenca l'aviada), e amb `pcsx-abnxt` lo filtre
  *Lissatge* e lo commutator *Correctius Sony*.

Triangle renomèna lo jòc, Square cambia sa carta memòria, Start partejan una carta novèla. Circle salva e
se'n va.

**Paramètres salvats dins l'emulator.** Lo menú propri de l'emulator a *Salvagardar los paramètres pel jòc*.
Una còp qu'un jòc a de paramètres salvats aicí, es los qu'utiliza quand se lòc, e lo modificador de jòc
aficha sas linias Vidèo e Emulator estampadas, amb aquelas valòrs, jos l'encap *Enregistrat dins
l'emulator*. Per tornar als paramètres del modificador de jòc, causisètz **Desblocar los paramètres** e
confirmatz : aquó supresís los paramètres qu'l'emulator a salvat, e las linias se podon cambiar tornamai.
Los dos emulators, `pcsx-ab` e `pcsx-abnxt`, legisson e escribon los meteis paramètres salvats.

![Lo modificador de jòc](../images/en/game-editor.jpg)

### 3.7 Cartas memòria e ponts de represa

Cada jòc PS1 a sa propria carta memòria per defaut (gardada amb sos ponts de represa en `Games/!SaveStates/<dorsièr
del jòc>/`). **Cartas memòria** dins lo menú sistèma gerís los **jorns compartits** - una carta qu'usason mai
jòcs, gardada en `Games/!MemCards/` : ne crear una (Square, amb lo clavièr a l'ecran), renomenar (Cross),
suprimir (Triangle). Un jòc se met sus un jorn amb *Cambiar la carta memòria* en son modificador, o dempuèi
son icòn de Carta memòria.

Lo **modificador de carta memòria** (l'icòn de Carta memòria) aficha la carta del jòc e una deuxième carta
costat a costat, amb l'icòn e lo títol de cada sauvegardat : copiar un sauvegardat entre los dos (Square),
ne suprimir un (Triangle), desfragmentar una carta (Select). Start cambia la carta a la drecha per un autre
jorn.

![Lo modificador de carta memòria](../images/en/memory-card-editor.jpg)

**Ponts de represa** : quand quichatz un jòc PS1 amb lo boton Reinicializar de la consòla (o lo menú de
l'emulator sus una Pi o PC), AutoBleem garda un pont de represa de ont vos érez e l'ofrís jos l'icòn
**Reprendre** - quatre lòcs, cada un amb una pictura del moment. Cross contunha dempuèi lo lòc, Triangle lo
supresís. Un jòc amb un pont de represa aficha una petita pictura sus son icòn de Reprendre.

### 3.8 Comenzar los jòcs, RetroArch e las Aplicacions

**Cross** comença lo jòc causit. Un jòc PS1 fonciona dins l'emulator PS1 causit (seccion 3.5), l'ecran
complèt, fins que lo quichatz - sus la consòla amb lo boton **Reinicializar** de davant (tornar al lançador
amb un pont de represa) o **Power** (la consòla s'atuada) ; sus una Pi o PC per lo menú en jòc de l'emulator
(Select + Start sus la maneta, o Esc sus un clavièr). **Square** comença un jòc PS1 dins RetroArch en plaça.

Un jòc **RetroArch** s'avie dins RetroArch amb lo còr qu'o lançador a causit per son sistèma ; *Tancar lo
contengut* o *Quitar RetroArch* en son menú tornarà al lançador. L'element RetroArch dins lo menú sistèma
dobrís lo menú propri de RetroArch (XMB) amb res cargat, pels sos paramètres e sas proprias listas de
contengut.

Una **Aplicacion** (lo grup Aplicacions : los espleches de consòla, e sus una consòla los aplicacions extras
que lo paquetatge de RetroArch livra - Doom, Quake, Amiga, ...) aficha sa lectura d'en primièr ; Cross
l'avie, Circle se'n va.

![La lectura d'una Aplicacion avant d'aviar-la](../images/en/app-start.jpg)

### 3.9 Apondre de jòcs

**Los jòcs PS1** van en lo dorsièr `Games`, **un dorsièr per jòc**, nomenat apres lo jòc :

```
Games/
  Crash Bandicoot/            Crash Bandicoot.cue + Crash Bandicoot.bin
  Final Fantasy VII/          Final Fantasy VII (Disc 1).chd, (Disc 2).chd, (Disc 3).chd
  Platformers/                un dorsièr de jòcs : un grup per elei-matime en lo causidor
    Klonoa/                   Klonoa.pbp
```

- Formatas : `.cue` + `.bin` (o `.img`), `.pbp`, `.chd` (zstd tanben), `.ecm` (descompresat per
  l'escandalhatge), `.iso`. Un jòc zippat fonciona tanben : lo processor **Descomprimir** lo descompresa
  davant l'escandalhatge (seccion 3.13).
- Un jòc de mai d'un disc es un dorsièr amb cada disc adinc ; los dorsièrs nomenats `Game (Disc 1)`,
  `Game (Disc 2)` ... se fusionon en un dorsièr `Game` per l'escandalhatge.
- Los jòcs drappats dreit adinc `Games/` (fichièrs soluts) se classan en dorsièrs per l'escandalhatge.
- Una **cobèrta** es un PNG costat de l'imatge del jòc, nomenat coma aquó. Sen una, l'art en boita vèn
  de las basas de donadas de cobertas, o - amb RetroArch installat - de la seria de miniaturas de libretro ;
  sus una Pi, una PC o Windows una manquanta se telecarga en linha (Opcions → *Recuperar las cobertas en
  linha*).
- L'escandalhatge leís lo numèro de seria de cada disc e prèn lo títol, l'editor, l'annada, los jogaires e
  la region de la basa de donadas PlayStation de RetroArch o las basas de donadas de cobertas. Cambiar
  qualqu'un en lo modificador de jòc e marcar *Verrolhar las donadas* per lo gardar.

**Los altres sistèmas** van jos `RetroArch/roms/`, **un dorsièr per sistèma, nomenat coma las basas de
donadas de RetroArch** (lo dorsièr se fai per vos) : `Nintendo - Nintendo Entertainment System`, `Nintendo
- Super Nintendo Entertainment System`, `Sega - Mega Drive - Genesis`, `Nintendo - Game Boy Advance`,
`FBNeo - Arcade Games` (o `Arcade`), ... Los ROMs pòdon rèstar zippats. Sus una Pi, una PC o Windows
l'escandalhatge los leís per elei-matime e escríu las listas de lectura de RetroArch ; sus un bastó de
consòla, avialtz **UpdateRoms** sus la PC (capítol 5).

**Las Aplicacions** van jos `Apps/<nòm>/` amb un `app.ini` (lo nòm, l'icòn, çò qu'aviador) e una `run.sh`.

**Los tèmas** van jos `Themes/<nòm>/` (`theme.json` e las imatges) - o drappatz lo zip del tèma en
`Themes/`.

### 3.10 Lo BIOS de PS1

Sus una **PlayStation Classic** l'emulator utiliza lo BIOS propri de la consòla. Sus una **Raspberry Pi,
una PC e Windows** metètz vòstre propri BIOS PS1 en `System/Bios/` : `romw.bin` (la version USA/Europèa
SCPH-5501/5502) e `romJP.bin` (la Japonesa SCPH-5500). Los installadors los remplisson dempuèi los
paquetatges BIOS de RetroArch se vòstres propris fichièrs y son ja. Sen elei l'emulator fonciona sus son
BIOS integrat HLE, que beaucoup de jòcs toleran e qualques non.

### 3.11 Mesas a jorn

- **Raspberry Pi, bastó PC, Windows** : lo lançador verificá lo site a l'aviada e una còp per jorn (Opcions
  → *Mesas a jorn* es lo canal ; *Mesa a jorn logiciala* dins lo menú sistèma verificá ara). Quand n'i a una
  AutoBleem o RetroArch mai récenta demanda: *Metre a jorn ara* telecarga tot e ra-avie l'installador amb la
  pantala de progression d'aviada prossima ; *Me lo rapelar deman* e *Ignorar aquesta version* son los autres
  respuestas. Vòstres jòcs e paramètres rèstan ; lo lançador ra-escandalhatge una còp après una mesa a jorn.
- **PlayStation Classic** : avialtz un `AutoBleemInstaller.exe` mai récent sus lo bastó (seccion 2.1).

### 3.12 Extensions e lo Store AutoBleem

**Extensions** apondèn lors proprias pantalas al lançador. Duelhan en `Extensions/<nòm>/` sus lo bastó (sus
una Raspberry Pi sa particion de donadas, sus Windows lo dorsièr de donadas) ; per n'installar una,
descompressatz son zip aicí. **L2 + R2 → Extensions** las alista : Cross n'avie una, Triangle l'activa o la
desactiva tornamai. Una extension qu'a besoin de la ret se comença pas sens una, e una qu'a arrestat lo
lançador se desactiva - la lista o ditz.

![La lista d'Extensions](../images/en/extensions.jpg)

**Lo Store AutoBleem** es la prossima extension : Aplicacions e jòcs per installar amb un quichament, sus
cada sistèma ont AutoBleem fonciona (una PlayStation Classic a besoin del WiFi del nucli AutoBleem). Sas
quatre ongletas, L1 / R1 entre eis :

- **Aplicacions** e **Jòcs** : çò que los fonts ofreisson, cada un amb sa pictura, version, talha e favicon
  de font. Los elements installats son estampats. Cross installa (o metre a jorn, o tornar assaiar apres un
  fracàs), Triangle supresís çò qu'o Store a installat. L2 / R2 o Gaut / Dreit vireton las paginas, **Select**
  aficha una font a la còp, **Start** serc los títols. Las picturas de elements se gardadon en cache e se pòdon
  tornar assaiar se fracassan de cargar.
- **Telecargaments** : çò que se telecarga, qu'espèra, a fracassat o s'instala. La barra de progression se
  mòstra constantant. Los telecargaments se contunhadon en fonç, tanben apres que quichatz lo Store ; comenzar
  un jòc o atudar sols los pausan, e un telecargament arrestat se contunha ont s'avía arrestat. Un jòc
  installat aparís sus la tablada após lo prossime escandalhatge, amb la pictura del Store coma sa cobèrta.
  Los telecargaments que suson 2 GB funcionan sus totas las plataformas, inclusenent las compilacions 32 bits.
- **Fonts** : ont las listas venon - lo catalog propri d'AutoBleem, una lista TSV drappada en
  `System/Extensions/store/sources/`, e las adreças qu'apondetz amb **Apondre una adreça de font**. Cada font
  aficha son favicon en la lista. Cross sus una que vos avètz apondut la renomèna, cambia sa adreça, bascúla
  entre `http://` e `https://`, o l'efaça.

![L'ongalet Aplicacions del Store](../images/en/store-apps.jpg)

![Lo menú d'una font](../images/en/store-source-menu.jpg)

Çò qu'o catalog d'AutoBleem ofrís alista tanben sus lo site de telecargament, `https://autobleem.retromenele.pl/store/`. **Sètz responsables de çò qu'o contenon las fonts qu'apondetz.**

**Vòstres jòcs propris sus vòstra ret** : `abstored`, lo servidor LAN del Store, sèrv un dorsièr de jòcs
PS1 al Store sus la meteis ret. Fonciona sus qualcuna maquina Linux - una Raspberry Pi, un servidor de casa
- e sols leís lo dorsièr. Avialtz-lo amb `abstored <dorsièr de jòcs>`, dobrítz `http://<aquela maquina>:8124/`
en un navegaire per afichar çò qu'o sèrv e qualques problèmas qu'o trobèt, e apondetz `http://<aquela
maquina>:8124/store.tsv` coma una font. Los programas prèsts per Linux e Windows son sus la pagina del Store,
en son ongalet **Servidor LAN** ; configurar-lo coma un servici es `INSTALL-linux.md` (`ext_store/server/`
en las fonts). **Partaja LAN** (seccion 5.2) met los jòcs e los disques d'una PC sus aqueste servidor.

### 3.13 Processors d'escaneig

**Los processors d'escaneig** son de petits programas que cada escandalhatge fai davant que legís vòstres
jòcs. Un pòt convertir un format qu'AutoBleem leís pas en un qu'o fa - un jòc zippat, per exemple - o cambiar
las donadas d'un jòc, coma una traïcion patch. Duelhan en `System/Processors/<nòm>/` sus lo bastó (sus una
Raspberry Pi sa particion de donadas, sus Windows lo dorsièr de donadas) ; per n'installar un,
descompressatz son dorsièr aicí. L'escandalhatge prossime l'avie.

- **Descomprimir vièn ab AutoBleem** : o descompresa los jòcs zippats PS1 en `Games/` davant que l'escandalhatge
  los legís, e los ROMs zippats un per un (los ajostas d'arcada rèstan zippats). Metre a jorn AutoBleem o metre
  a jorn tanben, e guarda lo desactivat se o desactivèrtz.
- Un processor qu'a ja s'ocupat d'un jòc se executa pas tornamai fins que lo jòc se cambe.
- Mentre un processor fonciona, la bombola al tope drech aficha çò qu'o fai ; una avertiment o un fracàs aparís
  en la linia jos aquó. `processors.log` en lo dorsièr de los jornals a los detalhs.
- Comenzar un jòc o RetroArch arrestá un processor qu'o cambe los fichièrs ; l'escandalhatge prossime acheva
  sa tòca.

**L2 + R2 → Processors d'escaneig** los aficha en l'òrdre qu'o fon, un ongalet pels jòcs PS1 e un pels ROMs
(L1 / R1). **Square** en prèn un e Amont / Avall lo desplaçon - l'òrdre contí : un processor qu'o descompresa
deu venir davant un qu'o cambe çò qu'o descompresa. **Cross** n'activa o desactiva un, **Triangle** o fait
mirar cada jòc tornamai al prossime escandalhatge, **Circle** se'n va e comença un escandalhatge se cambèrtz
qualqu'un. Un processor fait per una autre maquina rèsta en la lista, estampat.

![Los processors d'escaneig](../images/en/processors.jpg)

Escriure lo vòstre : la pagina de Descomprimir, `https://github.com/autobleem2/proc_unzip`, explicá tot çò
qu'un processor deu far, e `tools/proc_check.py` en las fonts d'AutoBleem verificá un davant qu'o partejaitz.

<!-- pagebreak -->

## 4. Pantalas

### 4.1 Gestionari de jòcs

Los jòcs PS1 coma una lista amb lors dorsièrs e lo causit amb sa cobèrta e sos detalhs. Cross dobrís lo
modificador de jòc, **Square supresís lo jòc** (son dorsièr e, apres una deuxième demanda, sos ponts de
represa), Triangle supresís cada PNG de cobèrta costat de los jòcs (l'escandalhatge los prèn de las basas de
donadas tornamai), L2 / R2 pagina. L'espaci liure del lecteur es al tope dreit. Lo Gestionari de jòcs
spèra mentre un escandalhatge se fai.

![Lo Gestionari de jòcs](../images/en/game-manager.jpg)

### 4.2 Entresenhas de material

Los faches de la maquina - sistèma, material, emmagazinatge amb son espaci liure, adreças de ret, los motors
d'afichatge e àudio, las manetas connectadas - ra-legits cada segonda. Sus una PlayStation Classic amb lo
nucli AutoBleem aqueste element dobrís **PSC-Bios** en plaça (capítol 6).

![Entresenhas de material](../images/en/hardware-info.jpg)

### 4.3 La guida de los botons

Triangle sus la tablada : cada boton de cada pantala sus una pagina. Quand un clavièr USB es connectat o
s'utilizá, una colòmna Clavièr aficha las tèclas costat de los botons de la maneta.

![La guida de los botons](../images/en/button-guide.jpg)

### 4.4 Lo clavièr a l'ecran

Ont se pica text - un jorn de cartas memòria, lo títol d'un jòc, un senhal WiFi, l'adreça d'una font - lo
meteis clavièr, disposat coma lo d'un telefòn: letras, una pagina de simbèls (`/ \ : ? & = % @ #` e lo
reste qu'una adreça o un senhal a besoin) e doas paginas de letras acentadas, amb Maj, la tèuca de pagina,
Espaci, Retorn arrièr e Confirmar en la linia de baix. Los desplaçaments se deplaçon, Cross pica, Triangle
efaça, Square es un espaci, **L1** es Maj (doas còps pels majusculas), **R1** la pagina segenta, **L2 / R2**
deplaçon lo cursor, Start confirma, Circle anulla. Un clavièr USB pica a qualcuna moment : Enter confirma,
Esc anulla.

![Lo clavièr a l'ecran](../images/en/keyboard.jpg)

<!-- pagebreak -->

## 5. Sus la PC

### 5.1 UpdateRoms - rafraichir un bastó de consòla

La PlayStation Classic a pas cap de ret, donc las listas de RetroArch e l'art en boita de son bastó se fan
sus la PC : **UpdateRoms** fa sus la PC çò qu'o escandalhatge del lançador fa sus una Pi, amb la ret de la PC
e los camins de la consòla, donc la consòla s'avie e trba tot en plaça.

1. Copialitz vòstres ROMs sus lo bastó jos `RetroArch/roms/<sistèma>/` (seccion 3.9). Los noms de dorsièr
   deon èsser los noms de basas de donadas de RetroArch ; l'installador ne fai los comunals.
2. Avialtz **`UpdateRoms\UpdateRoms.exe` dempuèi lo bastó** (l'installador o meté aicí). O trba lo bastó de
   ont siáu, aficha una linia d'etapa, una barra de progression e un jornal, e :
   - telecarga lo paquetatge de basas de donadas de RetroArch quand lo bastó ne n'a pas cap, e identificá
     cada ROM per aquó - un jòc que la basa de donadas ne sap identificá ne pren son nòm apropiat ;
   - escríu una lista de lectura per sistèma en `RetroArch/bin/playlists/` ab los camins de la consòla,
     gardant çò qu'o RetroArch metís aicí elei-matime ;
   - telecarga l'art en boita de cada ROM qu'en n'a pas dempuèi los servidors de miniaturas de libretro en
     `RetroArch/bin/thumbnails/`.
3. Tiraz lo bastó en seguretat e metètz-lo aicí en la consòla. L'ongalet RetroArch del causidor de grupós
   alista cada sistèma qu'a de jòcs.

Avialtz-lo tornamai apres cada cambiament als dorsièrs de ROMs ; un dorsièr ont res se cambe se passa, donc
una ra-aviada es rapida. Lo jornal es `System/Logs/updateroms.log`. Una carta de Raspberry Pi en un lisant
de cartas pòt se rafraichir lo meteis mòde (`UpdateRoms.exe <lecteur> --target rpi`), tot que una Pi o fa
per elei-matime quand o a una ret.

### 5.2 Partaja LAN - vòstres jòcs e disques sul servidor de vòstra ret

**Partaja LAN** (`LanShare.exe`, sus la pagina del Store en son ongalet **Servidor LAN**) met vòstres jòcs
PS1 sul servidor del Store sus vòstra ret de casa - un `abstored` sus una Raspberry Pi, un NAS o una autre PC
- e leís un disc PS1 en lo lecteur CD/DVD de la PC. Lo Store sus la consòla, la Pi o la PC puèi los installa
dempuèi aicí. Res a installar ; los reglatges se gardon en `%LOCALAPPDATA%\AutoBleem LAN Share\`.

![La fenèstra de Partaja LAN](../images/en/lanshare.jpg)

1. **Lo servidor** : entralz sa adreça (`http://<sa adreça>:<port>`, coma o Store o ditz) e quichatz
   **Connectar**. Sos jòcs e qualques problèmas qu'o escandalhatge o trobèt se aliston a la squèrra. Per metre
   de jòcs sus aquó, donnatz un de :
   - **Partaja** - lo dorsièr de jòcs del servidor coma o es compartit sus la ret (Samba), p.ex.
     `\\raspberrypi\games` : Partaja LAN copía los jòcs aicí e demanda al servidor d'escandalhatge. Lo servidor
     elei-matime rèsta de sols lectura.
   - **Tocòs** - quand lo servidor se comença amb `--allow-uploads` : son tocòs (lo servidor l'aficha a l'aviada
     e o guarda en `<estat>/upload-token`). Partaja LAN telecarga per HTTP, e un telecargament arrestat se
     contunha ont s'avía arrestat.
2. **Jòcs sus aquesta PC** : causisètz un dorsièr de jòcs (un dorsièr per jòc), cocharolatz de jòcs e quichatz
   **Publicar los jòcs cocharolats**. **Sul servidor** ditz se lo servidor a ja un jòc (per sa seria, si non
   per son títol) ; aqueste jòc se mandiá pas doas còps. **Cocharolar los qu'i son pas sul servidor** cocharolon
   los autres.
3. **Un disc** : metètz un disc PS1 en lo lecteur e quichatz **Legir un disc e lo publicar**. Lo disc se leís
   tot en un `.bin` + `.cue` (e un `.sbi` pel un jòc LibCrypt, quand lo lecteur dóna lo subchannel), nomenat
   apres son títol, verificat contra lo dumpassat bon conegut (quand las basas de donadas son causidas) e
   publicat. Pel un jòc sus mai d'un disc cocharolatz **Lo jòc a mai d'un disc** : Partaja LAN demanda per cada
   disc segent e los publica ensemble coma un jòc.
4. **Suprimir dempuèi lo servidor...** tira los jòcs causits dempuèi lo servidor. Res se supresís : cada
   se desplaça en un dorsièr `.removed` costat de los jòcs del servidor, e lo desplaçar aicí de tornamai o
   met de tornamai.

Las **Basas de donadas** - lo dorsièr de cobertas d'AutoBleem (`coversU/P/J.db`) e lo `Sony - PlayStation.rdb`
de RetroArch - donadon los títols e la verificacion d'un disc legit ; totas dos son optionaulas. **Tanben partaja
los jòcs d'aquesta PC ab lo Store** (desactivat per defaut) sèrv lo dorsièr sus aquesta PC al Store dirèctament.
La prossima còp, Windows demanda de sa firewall : permettre las rets privadas sols.

<!-- pagebreak -->

## 6. Los espleches de consòla (PlayStation Classic)

Doas espleches per un bastó PlayStation Classic. Los dos dibujan amb lo tèma e la lenga del lançador, e los
dos se conducisson amb la maneta - e, en l'assistant de mapeig, ab los botons de davant de la consòla.
**PSC-Bios** es una extension que vèn ab lo paquetatge de consòla : *Entresenhas de material* en lo menú
sistèma l'obrís, e es en la lista d'Extensions. **ABFlashKit** es una Aplicacion en lo grup Aplicacions.

### 6.1 PSC-Bios

Una extension qu'o vèn ab lo paquetatge de consòla, tanben disponibla sus una Raspberry Pi e lo bastó PC.
S'obrís dempuèi l'element *Ret e manetas* del menú Sistèma (o dempuèi la lista d'Extensions). Quand
aquela extension s'installa mas es desactivada, l'element *Ret e manetas* en lo menú rapid e lo menú sistèma
rèsta estampat amb una nòta "activatz-la dins Extensions" - Cross aicí dobrís la lista d'Extensions a
aquó.

La pantala d'aviada aficha los faches de maquina : ora, fusonari, adaptatòrs de ret WiFi/Ethernet/Bluetooth
ab lors adreças, e cada controlatòr connectat ab se o ten una mapeig de botòs. Las parts de ret e Bluetooth
an besoin del nucli AutoBleem sus la consòla (seccion 6.2) o dels espleches de sistèma sus una Raspberry Pi /
bastó PC ; l'assistant de maneta fonciona sus qualcuna sistèma.

![PSC-Bios : lo centr de Ret e manetas](../images/en/pscbios-main.jpg)

- **Select - Ret WiFi** (nucli o NetworkManager) : lo nòm de la ret (picat, o causit dempuèi un
  escandalhatge), lo senhal, lo mòde de pilòt, e *Aplicar / Ra-aviar la Ret*. Lo fusonari se estabelís aicí
  tanben. L'adreça IP de la consòla s'aficha una còp connectada.
- **Square - Controlatòrs Bluetooth** : un escandalhatge de manetas Bluetooth (DualShock 4, etc.), per
  aparelhar o suprimir.
- **L1 - Aparelha DualShock 3** : connexion de sols USB per la prossima DualShock 3, per lo plugin sixaxis
  del nucli.
- **R1 - Mapeig de Maneta** : l'assistant de mapeig (avall).
- **Triangle - A prepaus**, **Circle - tornar** al lançador.

**L'assistant de maneta** aficha la maneta connectada en cru - cada ax, boton e sòth coma numèrs, e una
pictura DualShock qu'o s'illumina quand quichatz. Perque la maneta bas test se pòt pas fiançar, l'assistant
se conducís per los **botons de davant de la consòla** : **REINICIALIZAR** se cambe vers la maneta segenta,
**DOBRIR** comença lo mapeig (puèi respòn cada demanda - quichatz lo boton illuminat sus la pictura, o
DOBRIR quand la maneta n'a pas aqueste boton), **ENERGIA** anulla o se'n va. Lo manteniment de Circle sus la
maneta per 2 segondas se'n va de l'assistant (una barra o completá e l'indicacion de prat ditz "Manteniment 2
s : Quitar"). Mentre la maneta n'a pas cap de mapeig, lo manteniment de qualqu'un boton per 2 segondas l'o fa
("Manteniment qualqu'un boton 2 s : Quitar"). Un quichament cort se mapeja coma de costuma. Sus un clavièr,
Esc / Espaci / Enter se mèton a la plaça per ENERGIA / REINICIALIZAR / DOBRIR. A la fin lo mapeig novèl s'aficha
per un ensag e DOBRIR o salva jos un nòm de vòstra causida ; lo lançador o carga dempuèi.

![PSC-Bios : l'assistant de mapeig de maneta](../images/en/pscbios-wizard.jpg)

### 6.2 ABFlashKit - lo nucli AutoBleem

Lo nucli AutoBleem es una remplaçament optional del nucli Linux de la consòla : o porá una ora qu'o
fonciona, los dongles WiFi e Bluetooth USB (pel PSC-Bios e las manetas Bluetooth) e l'assisténcia de
botons de davant que l'emulator utiliza pels ponts de represa. ABFlashKit o instala, fait una copia de
seguretat de la consòla d'en primièr, e pòt tornar la consòla al stock per la recoperacion propria de Sony.

> **Aqueste espleche escríu a la memoria flash de la consòla.** Una flash qu'o se'n va pas - la corrent
> tirada, lo bastó tirat - pòt laissar la consòla incapaç de s'aviar, e installar un nucli personalizada
> anulla sa garantida. Gardatz la consòla electrificada e lo bastó adinc fins qu'o se ra-avie per elei-matime.
> ABFlashKit se dobrís sus aquesta avertiment ; *Comprèn* se'n va, *Quitar* se queda.

![Lo menú d'ABFlashKit](../images/en/abflashkit-menu.jpg)

- **Metter flash al nucli** : fai una copia de recoperacion de las particions de la consòla sus lo bastó
  (`LBOOT.EPB`) se n'i a pas una, la verificá e l'imatge del nucli, escríu lo nucli e los fichièrs de sistèma
  d'AutoBleem, e se ra-avie. *Tot fait - quand l'ecran se devient negre remplaçatz lo fil de corrent* : tiraz
  la corrent de la consòla e la remplaçatz.
- **Copia complèta** : las quatre particions a `LBOOT.EPB`, per una recoperacion après (la copia precedenta se
  remet apres una demanda).
- **Mòde Recoperacion** : verificá que la copia es una de stock, establís la bandièra de recoperacion e se ra-avie
  en la recoperacion de Sony, qu'o restaura la consòla dempuèi `LBOOT.EPB` sus lo bastó - lo camí de tornamai
  al micrològi de stock.

Una barra de progression jos cada etapa aficha quant lonc l'accion es. L'espleche refusa de mettre flash a una
consòla qu'o fonciona amb autre micrològi personalizada (BleemSync, Project Eris) : restauratz-la al stock
d'en primièr.

<!-- pagebreak -->

## 7. Se qualqu'un cau de trauc

- **Jornals** : AutoBleem guarda sos jornals en memòria, donc lo bastó se creís pas tota l'ora - o arribon a
  `System/Logs/` sus lo bastó, la carta o lo dorsièr de donadas sols quand quelqu'un cau de trauc : un plantatge
  del lançador, d'un jòc PS1 o de RetroArch o sauvegarda en `System/Logs/crash-<n>/` (los tres darrièrs se
  gardon), e lo lançador l'ditz una còp quand se tornarà. Per gardar cada jornal, bascúla *Opcions ->
  Diagnostic -> Gardar los jornals sus la clau* (dempuèi la prossima aviada), o crea un fichièr void
  `System/Logs/keep` sus una PC. Sus una Pi o una PC, *Entresenhas de material* aficha ont los jornals son e
  Square los salva a `System/Logs/saved-<n>/`. Los fichièrs : `autobleem.log` (lo lançador), `launch.log` e
  `pcsx.log` (l'aviada d'un jòc PS1 e la sortida de l'emulator), `retroarch.log`, e - togjorn sus lo bastó -
  `update.log` (una mesa a jorn en linha) e `updateroms.log` (UpdateRoms).
- **Un jòc es pas sus la tablada** : verificatz la disposicion de dorsièrs (un dorsièr per jòc, los formatas
  de imatge de la seccion 3.9). Lo *Gestionari de jòcs* alista los dorsièrs qu'o escandalhatge a refusat apres
  los jòcs, marcats *Pas apondut*, ab la razon ; Square en supresís un. Tornar escandalhar en lo menú sistèma
  avie l'escandalhatge tornamai.
- **Pas cap de cobertas** : las basas de donadas de cobertas e foron pas installadas (avialtz l'installador
  tornamai ab eis cocharolats), o, pels jòcs RetroArch sus una consòla, UpdateRoms se'n va pas aviat sur la PC.
- **Una maneta fa res o a sos botons baralhats** : lo PSC-Bios (una consòla) l'o mapeja ; sus una Pi o PC la
  pagina d'Entresenhas de material alista çò qu'o SDL o vèd.
- **La consòla aficha un ecran negre apres un jòc** : AutoBleem ra-baste sa fenèstra per elei-matime (fins a
  tres còps) ; se rèsta negre, mantensiétz lo boton power e mesiatz la consòla.
- **Raspberry Pi** : `Alt+F2` dóna un demanda de connexion sus la deuxième consòla ; SSH es activat dempuèi la
  prossima aviada. `sudo journalctl -u autobleem` aficha lo servici del lançador ; `sudo systemctl restart
  autobleem` lo ra-avie. Una prossima aviada qu'o podiá pas acabar (sens ret) tornarà assaiar a l'aviada
  segenta.
- **Windows** : `Esc` quita lo lançador ; lo dorsièr de donadas es lo qu'o causìs en lo setup (`Documents\AutoBleem`
  per defaut), los jornals son en `System\Logs`.

AutoBleem es un logicial liure (GNU GPL v3 o ulteriora), ses garantida. Sosten e novellas : lo servidor Discord
ligat en la pantala A prepaus, e https://autobleem.retromenele.pl/.

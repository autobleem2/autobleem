# AutoBleem 2 - Podręcznik użytkownika

AutoBleem 2 to launcher gier dla **PlayStation Classic** - a od wersji 2 także dla **Raspberry Pi**,
**komputera PC uruchamianego z pendrive'a** i **Windows**. Pokazuje gry PS1 jako półkę z okładkami, ich
grafiką i opisami, uruchamia je we wbudowanym emulatorze PCSX, a z zainstalowanym obok RetroArch gra też
w gry z innych systemów. Ten podręcznik opisuje instalację na każdej platformie, codzienne używanie oraz
dołączone narzędzia.

> Pliki do pobrania dla każdej platformy są na **https://autobleem.retromenele.pl/**. Strona jest podzielona
> na platformy: panel *Install* każdej z nich to to, co pobierasz; *Build inputs* pod nim to to, co
> instalatory pobierają same.

## 1. Co dostajesz

- **Launcher** - karuzela okładek, zestawy (PlayStation, RetroArch, Aplikacje), opis gry, menu systemowe,
  opcje, narzędzia do kart pamięci i stanów gry. Ten sam program na każdej platformie.
- **Dwa emulatory PS1** - `pcsx-abnxt`, aktualny (domyślny), oraz `pcsx-ab`, klasyczny, dostarczany z
  AutoBleem od zawsze. Wybierasz go w opcjach; oba używają tych samych ustawień i kart pamięci.
- **RetroArch** (opcjonalny na każdej platformie) dla innych systemów: NES, SNES, Mega Drive, Game Boy,
  automaty i wiele innych. AutoBleem buduje listy RetroArch z ROM-ów, które wgrasz, i uruchamia każdą grę
  właściwym rdzeniem.
- **Narzędzia konsolowe** (tylko PlayStation Classic): *PSC-Bios* do WiFi, zegara i mapowania padów oraz
  *ABFlashKit* do instalacji kernela AutoBleem.
- **UpdateRoms** dla Windows: odświeża listy RetroArch i okładki pendrive'a konsoli na PC, bo sama konsola
  nie ma sieci.

![Launcher: półka z okładkami, opis wybranej gry, podpowiedzi przycisków](../images/pl/launcher.jpg)

<!-- pagebreak -->

## 2. Instalacja

### 2.1 PlayStation Classic

Potrzebujesz komputera z Windows, pendrive'a (USB 2.0, 8 GB lub więcej; instalator go sformatuje, jeśli
poprosisz) i konsoli. AutoBleem działa z pendrive'a bez żadnej zmiany w konsoli. Pendrive musi być
**FAT32** dla fabrycznej konsoli - jej kernel nie czyta exFAT. Tylko konsola z zainstalowanym kernelem
AutoBleem (ABFlashKit, rozdział 6) uruchamia się także z pendrive'a exFAT, co znosi limit 4 GB na plik w FAT32.

1. Pobierz **AutoBleemInstaller-<wersja>.zip** z panelu PlayStation Classic na stronie i rozpakuj gdziekolwiek.
   W środku jest `AutoBleemInstaller.exe` i pakiet AutoBleem, który instaluje.
2. Włóż pendrive i uruchom `AutoBleemInstaller.exe`. Wybierz dysk u góry. Zaznacz, co chcesz:
   - **Formatowanie pendrive'a** - tylko dla nowego pendrive'a (wszystko na nim zostanie skasowane). Wybierz
     FAT32, chyba że konsola ma kernel AutoBleem.
   - **Bazy okładek** - grafika i opisy biblioteki PS1 (zaznaczone domyślnie; ok. 300 MB).
   - **RetroArch** - RetroArch z rdzeniami, dodatkowe aplikacje (Doom, Quake, Amiga, ...) i zasoby libretro,
     dla gier z innych systemów. Domyślnie wyłączone; można dodać później, uruchamiając instalator ponownie.
   - **Pliki BIOS** - BIOS-y, których potrzebują rdzenie RetroArch (wymaga RetroArch).
   - **Gry przykładowe** - kilka darmowych gier homebrew, żeby półka nie była pusta.
3. Naciśnij **Install** i poczekaj. Paski postępu i log pokazują każdy krok; na koniec pendrive dostaje
   nazwę `SONY`, a trafia na niego `UpdateRoms` (patrz rozdział 5).
4. Bezpiecznie odłącz pendrive, włóż go do **drugiego portu USB** konsoli (prawego, gracz 2) i włącz konsolę.
   Zamiast fabrycznego menu uruchamia się AutoBleem.

**Włączanie i wyłączanie.** Z włożonym pendrivem konsola startuje, przez kilka sekund miga diodą
(AutoBleem jest wykrywany) i przechodzi w stan czuwania, zanim cokolwiek się pokaże - tak konsola
sama przygotowuje aktualizację i dzięki temu AutoBleem w ogóle może się uruchomić. Naciśnij raz
**Power** i pojawi się launcher. *Wyłącz* w menu systemowym albo przycisk Power konsoli przełącza
konsolę w **czuwanie AutoBleem**: najpierw odłączany jest pendrive, potem dioda świeci **na czerwono** -
to znak, że AutoBleem działa tak, jak powinien - a następne naciśnięcie Power przywraca launcher w kilka
sekund. **Gdy dioda jest czerwona, pendrive można wyjąć** i włożyć do PC bez pytania Windows o
sprawdzenie dysku; włóż go z powrotem przed naciśnięciem Power. Po odłączeniu zasilania konsola
następnym razem znów przechodzi przez czuwanie przy starcie.

Żeby **zaktualizować** pendrive, uruchom na nim nowszy instalator: gry, zapisy, ustawienia i zawartość
RetroArch zostają; wymieniane są tylko pliki samego AutoBleem. Pendrive zrobiony AutoBleem 1.0 albo
AutoBleem-NG jest automatycznie przenoszony do nowego układu folderów.

> Fabryczna konsola nie ma zegara ani sieci: daty pokazują się dopiero po instalacji kernela AutoBleem
> (rozdział 6), a okładki gier RetroArch robi UpdateRoms na PC (rozdział 5).

Gry trafiają do folderu `Games` na pendrivie, po jednym folderze na grę - układ opisuje punkt 3.9.

### 2.2 Raspberry Pi

AutoBleem zamienia Pi w małą konsolę: uruchamia się prosto w launcherze, bez pulpitu. Na stronie są dwa
gotowe obrazy - 32-bitowy i 64-bitowy - oraz archiwum dla istniejącego Raspberry Pi OS Lite.

| Model | Obraz 32-bit | Obraz 64-bit | Uwagi |
|---|---|---|---|
| Raspberry Pi 5 | tak | tak | |
| Raspberry Pi 4 Model B, Pi 400 | tak | tak | |
| Raspberry Pi 3 Model B / B+ / A+ | tak | tak | wystarcza na launcher i PS1 |
| Raspberry Pi Zero 2 W | tak | tak | 512 MB RAM: PS1 działa, cięższe rdzenie RetroArch nie |
| Raspberry Pi 2 Model B | tak | tylko v1.2 | wolne dla czegokolwiek 3D |
| Raspberry Pi 1, Zero, Zero W | nie | nie | ARMv6 - żaden obraz nie działa |

**Obraz 32-bitowy jest zalecany** do gier PS1: szybki rekompilator ARM w `pcsx-ab` jest tylko 32-bitowy,
więc na 64 bitach gry PS1 chodzą wolniej. Obraz 64-bitowy ma za to większy zestaw rdzeni RetroArch.

**Przez Raspberry Pi Imager:**

1. Zainstaluj Raspberry Pi Imager (raspberrypi.com/software). W *Choose OS* wybierz *Use custom* i pobrany
   `autobleem-<wersja>-rpi-armhf.img.xz` (32-bit) albo `-arm64.img.xz` (64-bit) - albo dodaj w ustawieniach
   programu adres repozytorium `https://autobleem.retromenele.pl/rpi-imager/os_list.json` i wybierz
   AutoBleem z listy.
2. W ekranie dostosowania Imagera (koło zębate albo pytanie po *Next*) ustaw **nazwę użytkownika i hasło,
   sieć WiFi z krajem oraz włącz SSH**. AutoBleem potrzebuje sieci przy pierwszym uruchomieniu.
3. Zapisz kartę, włóż ją do Pi z podłączonym ekranem oraz klawiaturą lub padem i włącz zasilanie.

**Pierwsze uruchomienie** trwa od 5 do 25 minut i pokazuje na ekranie, co robi. Bez sieci pyta o nią
(lista WiFi, hasło), potem pyta, czy zainstalować RetroArch (minuta bez odpowiedzi oznacza tak), powiększa
partycję systemową, tworzy z reszty karty partycję danych `AUTOBLEEM`, instaluje RetroArch z rdzeniami,
paczki BIOS-ów i gry przykładowe, po czym restartuje się do launchera.

Odpowiedzi można podać z góry w pliku **`autobleem.txt`** na partycji rozruchowej karty (do edycji na
dowolnym PC przed pierwszym uruchomieniem):

| Klucz | Domyślnie | Znaczenie |
|---|---|---|
| `root_gib` | 8 | Rozmiar partycji systemowej w GiB; reszta staje się partycją gier. |
| `hdmi_mode` | 1920x1080@60 | Tryb ekranu dla całego rozruchu (`1280x720@60` dla starszego telewizora). |
| `retroarch` | (pytanie) | `yes` / `no` - RetroArch i inne systemy, albo samo PS1. |
| `thumbnails` | none | `boxarts` kopiuje cały zestaw okładek do pracy bez sieci (~9000 plików). |
| `bios`, `downloads`, `samples` | yes | Ustaw `no`, żeby pominąć paczki BIOS, wszystkie pobierania albo gry przykładowe. |

**Na istniejącym Raspberry Pi OS Lite** (Bookworm lub Trixie): skopiuj `autobleem-rpi.tar.gz` (albo wersję
arm64) na Pi, rozpakuj i uruchom `sudo bash install.sh`. Zadaje te same pytania, tworzy partycję danych
zmniejszając root przy następnym uruchomieniu (`--shrink-root <GiB>`) i uruchamia launcher na pierwszej
konsoli.

Po instalacji **partycja `AUTOBLEEM`** karty (exFAT) jest tym, co wypełniasz: wyjmij kartę i otwórz ją na
dowolnym PC albo kopiuj przez sieć (SSH jest włączone). `Games/` na gry PS1, `RetroArch/roms/<system>/` na
inne systemy, `System/Bios/` na BIOS PS1 (punkt 3.10), `Themes/` na motywy.

### 2.3 Pendrive dla PC

Ta sama "konsola" dla dowolnego komputera uruchamianego z USB - system 32-bitowy, więc działają też stare
maszyny:

1. Pobierz `autobleem-<wersja>-pcusb-i386.img.xz` z panelu PC i zapisz na pendrive 8 GB lub większym
   przez Raspberry Pi Imager (*Use custom*), balenaEtcher albo Rufus (tryb DD).
2. Uruchom PC z pendrive'a (klawisz menu rozruchowego twojego PC - F12, F8, Esc...). Działa rozruch BIOS
   i UEFI; **Secure Boot musi być wyłączony**.
3. Pierwsze uruchomienie jest takie jak na Pi: pytanie o sieć, jeśli nie ma kabla, pytanie o RetroArch,
   potem instalacja - około ośmiu minut z siecią kablową - i restart do launchera.

Pendrive ma potem partycję `AUTOBLEEM` na twoje gry, widoczną w Windows 10 (1903 i nowszych) jako drugi
dysk po włożeniu do działającego komputera. `autobleem.txt` jest na pierwszej partycji, z tymi samymi
kluczami co na Pi (bez `hdmi_mode` - PC używa natywnego trybu ekranu).

### 2.4 Windows

AutoBleem jako program dla Windows: pełny ekran, emulatory i RetroArch uruchamiane jako programy.

1. Pobierz **AutoBleemSetup-<wersja>.exe** i uruchom. Instaluje się dla użytkownika, bez uprawnień
   administratora: program w `%LOCALAPPDATA%\Programs\AutoBleem`, dane (gry, ustawienia, motywy, RetroArch)
   w wybranym folderze - domyślnie `Dokumenty\AutoBleem`.
2. Zaznacz składniki - bazy okładek, RetroArch (oficjalna wersja dla Windows z rdzeniami), pliki BIOS, gry
   przykładowe - i pozwól pomocnikowi instalacji je pobrać.
3. Uruchom AutoBleem z menu Start albo z pulpitu. Na PC klawiatura zastępuje pad (punkt 3.2).

Nowszy instalator uruchomiony na tym samym komputerze aktualizuje program, a folder danych zostaje.
Launcher raz dziennie sprawdza też stronę i proponuje aktualizację, gdy jest nowsza (punkt 3.11).

<!-- pagebreak -->

## 3. Używanie AutoBleem

### 3.1 Launcher

Launcher otwiera się na półce: okładki bieżącego zestawu, wybrana pośrodku, obok jej opis - wydawca, rok,
numer seryjny, region, liczba graczy, kiedy ostatnio grano - i przycisk odtwarzania. Pasek u dołu mówi, co
robią przyciski. Przy każdym starcie w tle działa skanowanie folderu gier; póki trwa, dymek w prawym górnym
rogu pokazuje postęp, a nowe gry pojawiają się na półce w miarę znajdowania.

![Wybór zestawu: trzy zakładki i grupy bieżącej z liczbą gier](../images/pl/set-picker.jpg)

### 3.2 Sterowanie

| Przycisk | Na półce |
|---|---|
| Lewo / Prawo | Poprzednia / następna gra. Przytrzymanie przewija dalej. |
| L1 / R1 | Skok do poprzedniej / następnej pierwszej litery tytułów. |
| Krzyżyk | Uruchom wybraną grę (grę PS1 w emulatorze PS1; grę RetroArch jej rdzeniem; aplikację po jej opisie). |
| Kwadrat | Uruchom wybraną grę PS1 w RetroArch. |
| Trójkąt | Przewodnik po przyciskach. |
| Start | Losowa gra z bieżącego zestawu. |
| Select | Wybór zestawu: zakładki PlayStation / RetroArch / Aplikacje (L1 / R1), grupy zakładki (Góra / Dół, L2 / R2 strona), Krzyżyk wybiera. |
| Dół | Otwiera rząd ikon pod grą (Ustawienia, Gra, Karta pamięci, Wznów). Góra go zamyka. |
| L2 + R2 | Menu systemowe (punkt 3.4). |

**Z klawiaturą** (PC bez pada, albo klawiatura USB na konsoli, Pi lub pendrivie PC) klawiatura zastępuje pad: **Strzałki** = d-pad, **Enter** = Krzyżyk, **Esc lub Backspace** = Kółko, **Tab** = Trójkąt, **Spacja** = Kwadrat, **F1 / F2** = Select / Start, **Page Up / Page Down** = L1 / R1, **Home / End** = L2 / R2, **F10** = menu systemowe. Na maszynie deweloperskiej Esc zamyka program, a Spacja to Start.

Na każdej liście i w każdym menu: Góra / Dół przesuwają, **L2 / R2 zmieniają stronę**, L1 / R1 skaczą do
pierwszego / ostatniego wiersza, **Krzyżyk wybiera, Kółko wraca**. Ekran z ustawieniami zapisuje je, gdy
wychodzisz Kółkiem.

![Rząd ikon pod wybraną grą](../images/pl/launcher-icons.jpg)

### 3.3 Zestawy

**Select** otwiera wybór zestawu. Zakładka PlayStation zawiera *Wszystkie gry*, *Gry wewnętrzne* (dwadzieścia
wbudowanych gier konsoli, na PlayStation Classic), każdy folder, który założysz w `Games/` (gra w
podfolderze należy do tej grupy), *Ulubione*, *Historię* oraz - gdy jakaś gra jest tak oznaczona - *Gry na
pistolet świetlny*. Zakładka RetroArch ma po jednej grupie na system z grami, a także własne Ulubione i
Historię RetroArch. Zakładka Aplikacje grupuje programy: *Wszystkie aplikacje*, a potem *Gry*, *Emulatory*, *Narzędzia*, *Multimedia* i *Inne* (kategorię ustawia plik `app.ini` każdej aplikacji). Każdy wiersz pokazuje liczbę elementów; grupa
bez gier otwiera pustą półkę z rzędem ikon pokazującym same Ustawienia.

### 3.4 Szybkie menu

**Góra** w launcherze, lub **ikona koła zębatego** w rzędzie ikon (gdzie Ustawienia / Gra / Karta pamięci / Wznowienie):
szybkie menu na dostęp do akcji, które robisz z półki. Krótka lista: *Skanuj gry ponownie* (skanowanie teraz),
*Sklep* (Sklep AutoBleem na rozszerzenia), *Sieć i kontrolery* (tam, gdzie zainstalowane rozszerzenie zapewnia wpis `network` - PSC-Bios na konsoli, Pi i pendrivie PC: Wi-Fi, parowanie
Bluetooth, kreator mapowania pada - patrz rozdział 6; wyszarzone z notatką "włącz w Rozszerzeniach", gdy to rozszerzenie jest wyłączone - Krzyżyk otwiera Rozszerzenia), i *Menu systemowe...* (pełne menu poniżej). Góra / Dół
przechodzą (z zawinięciem), Krzyżyk wybiera, Kółko wychodzi. Nic tu unikatowego - każda pozycja jest też w menu
systemowym.

### 3.5 Menu systemowe

**L2 + R2** (razem, w dowolnej kolejności) otwiera menu systemowe nad półką. Menu jest podzielone na sekcje:

| Sekcja | Pozycja | Co robi |
|---|---|---|
| (góra) | Skanuj gry ponownie | Szuka teraz nowych, zmienionych lub usuniętych gier (skan sam też obserwuje folder). |
| | Rozszerzenia | Rozszerzenia na pendrivie - Sklep AutoBleem i inne (punkt 3.12). |
| **Biblioteka** | Menedżer gier | Gry PS1 jako lista z folderami: usuwanie gry, czyszczenie okładek. Wyłączone podczas skanowania. |
| | Karty pamięci | Twoje zestawy kart pamięci (punkt 3.7). |
| | Procesory skanowania | Programy, które każde skanowanie uruchamia najpierw - ich kolejność, włączenie (punkt 3.13). Wyłączone podczas skanowania. |
| **System** | Opcje | Ustawienia AutoBleem (punkt 3.6). |
| | Sieć i kontrolery | Tam, gdzie zainstalowane rozszerzenie zapewnia wpis `network` (`Provides=network` w jego `extension.ini` - PSC-Bios na konsoli, Pi i pendrivie PC) - Wi-Fi, parowanie padów Bluetooth, konfiguracja DualShock 3 i kreator mapowania - patrz rozdział 6. Gdy to rozszerzenie jest zainstalowane, ale wyłączone, ta pozycja zostaje wyszarzona z notatką "włącz w Rozszerzeniach" - Krzyżyk otwiera Rozszerzenia. |
| | Informacje o sprzęcie | Fakty maszyny: system, CPU, dyski, interfejsy sieciowe, strefa czasowa, ekran, pady i ich mapowania. Na konsoli z kernelem AutoBleem otwiera PSC-Bios (rozdział 6); na pozostałych maszynach pokazuje tę stronę informacyjną. |
| | Aktualizacja | (Raspberry Pi i PC) Sprawdź teraz, czy na stronie jest nowszy AutoBleem lub RetroArch. |
| | O programie | Autorzy i licencja. |
| **Wyjście** | RetroArch | Wychodzi z launchera do własnego menu RetroArch. Zamknięcie RetroArch wraca. |
| | Wyłącz | Po potwierdzeniu: na konsoli czuwanie AutoBleem - pendrive odłączony, dioda czerwona, Power przywraca launcher (punkt 2.1); na Pi lub PC maszyna się wyłącza. |

![Menu systemowe](../images/pl/system-menu.jpg)

### 3.6 Opcje

Ustawienia są w grupach; Góra / Dół przechodzą między nimi, Lewo / Prawo zmieniają wartość, Kółko wychodzi
i zapisuje. Każda zmiana działa od razu.

| Grupa / ustawienie | Co robi |
|---|---|
| **Interfejs**: Motyw AutoBleem | Wygląd. Motywy leżą w `Themes/`; zip z motywem wrzucony tam jest rozpakowywany przy następnym wejściu. Motywy dostarczane z AutoBleem są odświeżane przy każdej aktualizacji - żeby zmienić któryś, najpierw skopiuj go pod nową nazwą. |
| Styl okładki | Ramka pudełka rysowana wokół okładek PS1. |
| Język | Język launchera, od razu (17 języków). |
| Czcionka z motywu / Czcionka | Czcionka klasycznych ekranów: z motywu albo dowolny `.ttf`/`.otf` z `resources/fonts`, `RetroArch/fonts` lub folderu motywu. |
| Czas wyświetlania | Jak długo zostaje powiadomienie "Wyświetla: ..." w sekundach (0 = zawsze). |
| **Dźwięk**: Muzyka, Muzyka w tle | Który utwór gra pod launcherem (z motywu albo plik z `resources/music`) i czy w ogóle gra. |
| **Emulacja**: Emulator PS1 | `pcsx-abnxt` (domyślny: aktualny PCSX-ReARMed z dodatkami AutoBleem) albo `pcsx-ab` (klasyczny). Punkt wznowienia zapisany w jednym da się kontynuować w drugim, chyba że gra działała bez pliku BIOS. |
| Szeroki ekran | Proporcje obrazu emulatora PS1 dla każdej gry. |
| Graj we wszystkie gry PSX w RA | Każda gra PS1 startuje w rdzeniu PS1 RetroArch. |
| Aktualizuj konfigurację RA | AutoBleem wpisuje swoje ustawienia do konfiguracji RetroArch, gdy uruchamia tam grę. |
| **Biblioteka**: Pokaż gry wewnętrzne | Wbudowane gry konsoli na listach PlayStation (tylko PlayStation Classic). |
| Pobieraj okładki z sieci | Skan pobiera brakujące okładki z serwerów libretro (Raspberry Pi, PC, Windows). |
| **Aktualizacje** | (Raspberry Pi, PC, Windows) `stable`, `latest` (także wydania wstępne) albo `off`. |

![Opcje, w grupach](../images/pl/options.jpg)

### 3.7 Ustawienia gry

Przy wybranej grze **Dół** otwiera jej rząd ikon: **Ustawienia** (opcje powyżej), **Gra** (ustawienia samej
gry), **Karta pamięci** (jej karta) i **Wznów** (jej stany gry). Krzyżyk otwiera tę pod kursorem.

**Edytor gry** pokazuje po prawej opis gry, a po lewej jej ustawienia w trzech grupach:

- **Gra**: *Ulubiona* (w grupie Ulubione), *Gra na pistolet świetlny* (trafia do grupy pistoletu i zawsze
  chodzi w RetroArch, którego rdzeń PS1 ma GunCon), *Graj w RA* (ta gra chodzi w RetroArch), *Zablokuj dane*
  (skaner zostawia tytuł, numer seryjny i listę płyt tak, jak je ustawisz).
- **Obraz**: wysoka rozdzielczość, scanlines i ich poziom, pomijanie klatek, wtyczka GPU oraz *Filtr* -
  jak obraz jest skalowany: Wyłączony (czyste piksele), Liniowy (wygładzony) albo Ostry (wyraźne piksele bez
  migotania; tylko `pcsx-abnxt` - klasyczny `pcsx-ab` i RetroArch traktują go jak Wyłączony).
- **Emulator**: SpeedHack, zegar CPU, interpolacja SPU, logo startowe (wyłączone pomija powłokę BIOS - dla
  płyty homebrew, której własne logo psuje start), a z `pcsx-abnxt` filtr *Wygładzanie* i przełącznik
  *Hacki Sony*.

Trójkąt zmienia nazwę gry, Kwadrat zmienia jej kartę pamięci, Start udostępnia nową kartę. Kółko zapisuje i
wychodzi.

**Ustawienia zapisane w emulatorze.** Własne menu emulatora ma pozycję *Save settings for this game*. Gdy
gra ma tam zapisane ustawienia, to z nimi jest uruchamiana, a edytor gry pokazuje jej wiersze Obraz i
Emulator wyszarzone, z tymi wartościami, pod nagłówkiem *Zapisane w emulatorze*. Aby wrócić do ustawień z
edytora gry, wybierz **Odblokuj ustawienia** i potwierdź: ustawienia zapisane przez emulator zostaną
usunięte, a wiersze znów da się zmieniać. Oba emulatory, `pcsx-ab` i `pcsx-abnxt`, czytają i zapisują te
same ustawienia.

![Edytor gry](../images/pl/game-editor.jpg)

### 3.8 Karty pamięci i stany gry

Każda gra PS1 ma domyślnie własną kartę pamięci (trzymaną razem z jej stanami w `Games/!SaveStates/<folder
gry>/`). **Karty pamięci** w menu systemowym zarządzają **zestawami wspólnymi** - kartą używaną przez kilka
gier, trzymaną w `Games/!MemCards/`: utwórz (Kwadrat, przez klawiaturę ekranową), zmień nazwę (Krzyżyk),
usuń (Trójkąt). Grę przypisujesz do zestawu przez *Zmień kartę pamięci* w jej edytorze albo z jej ikony
Karta pamięci.

**Edytor kart pamięci** (ikona Karta pamięci) pokazuje obok siebie kartę gry i drugą kartę, z ikoną i
tytułem każdego zapisu: kopiowanie zapisu między nimi (Kwadrat), usuwanie (Trójkąt), defragmentacja karty
(Select). Start zamienia prawą kartę na inny zestaw.

![Edytor kart pamięci](../images/pl/memory-card-editor.jpg)

**Punkty wznowienia**: gdy wychodzisz z gry PS1 przyciskiem Reset konsoli (albo menu emulatora na Pi lub
PC), AutoBleem zachowuje stan gry z tego miejsca i proponuje go pod ikoną **Wznów** - cztery sloty, każdy z
obrazkiem chwili. Krzyżyk kontynuuje ze slotu, Trójkąt go usuwa. Gra z punktem wznowienia pokazuje mały
obrazek na ikonie Wznów.

### 3.9 Uruchamianie gier, RetroArch i aplikacje

**Krzyżyk** uruchamia wybraną grę. Gra PS1 chodzi w wybranym emulatorze PS1 (punkt 3.5), na pełnym
ekranie, aż z niej wyjdziesz - na konsoli przednim przyciskiem **Reset** (powrót do launchera z punktem
wznowienia) albo **Power** (konsola się wyłącza); na Pi lub PC przez menu emulatora (Select + Start na
padzie albo Esc na klawiaturze). **Kwadrat** uruchamia grę PS1 w RetroArch.

Gra **RetroArch** startuje w RetroArch z rdzeniem, który launcher wybrał dla jej systemu; *Close Content*
albo *Quit RetroArch* w jego menu wraca do launchera. Pozycja RetroArch w menu systemowym otwiera własne
menu RetroArch (XMB) bez wczytanej gry, do jego ustawień i własnych list.

**Aplikacja** (zestaw Aplikacje: narzędzia konsolowe, a na konsoli dodatkowe programy z paczki RetroArch -
Doom, Quake, Amiga, ...) pokazuje najpierw swój opis; Krzyżyk ją uruchamia, Kółko wraca.

![Opis aplikacji przed jej uruchomieniem](../images/pl/app-start.jpg)

### 3.10 Dodawanie gier

**Gry PS1** trafiają do folderu `Games`, **po jednym folderze na grę**, nazwanym jak gra:

```
Games/
  Crash Bandicoot/            Crash Bandicoot.cue + Crash Bandicoot.bin
  Final Fantasy VII/          Final Fantasy VII (Disc 1).chd, (Disc 2).chd, (Disc 3).chd
  Platformowki/               folder gier: własna grupa w wyborze zestawu
    Klonoa/                   Klonoa.pbp
```

- Formaty: `.cue` + `.bin` (albo `.img`), `.pbp`, `.chd` (także zstd), `.ecm` (dekodowany przez skan), `.iso`.
  Działa też spakowana gra: procesor **Unzip** rozpakowuje ją przed skanowaniem (punkt 3.13).
- Gra wielopłytowa to jeden folder ze wszystkimi płytami; foldery `Gra (Disc 1)`, `Gra (Disc 2)` ... skan
  scala w jeden folder `Gra`.
- Gry wrzucone luzem prosto do `Games/` skan porządkuje do folderów.
- **Okładka** to PNG obok obrazu gry, nazwany tak jak on. Bez niego grafika pochodzi z baz okładek albo -
  z zainstalowanym RetroArch - z zestawu miniatur libretro; na Pi, PC i Windows brakująca jest pobierana z
  sieci (Opcje → *Pobieraj okładki z sieci*).
- Skan odczytuje numer seryjny każdej płyty i bierze tytuł, wydawcę, rok, liczbę graczy i region z bazy
  PlayStation RetroArch albo z baz okładek. Zmień cokolwiek w edytorze gry i zaznacz *Zablokuj dane*, żeby
  to zachować.

**Inne systemy** trafiają do `RetroArch/roms/`, **po jednym folderze na system, nazwanym jak bazy
RetroArch** (foldery są zakładane za ciebie): `Nintendo - Nintendo Entertainment System`, `Nintendo - Super
Nintendo Entertainment System`, `Sega - Mega Drive - Genesis`, `Nintendo - Game Boy Advance`, `FBNeo -
Arcade Games` (albo `Arcade`), ... ROM-y mogą zostać spakowane. Na Pi, PC i Windows skan czyta je sam i
zapisuje listy RetroArch; na pendrivie konsoli uruchom **UpdateRoms** na PC (rozdział 5).

**Aplikacje** trafiają do `Apps/<nazwa>/` z `app.ini` (nazwa, ikona, co uruchomić) i `run.sh`.

**Motywy** trafiają do `Themes/<nazwa>/` (`theme.json` i obrazy) - albo wrzuć zip motywu do `Themes/`.

### 3.11 BIOS PS1

Na **PlayStation Classic** emulator używa BIOS-u konsoli. Na **Raspberry Pi, PC i Windows** wgraj własny
BIOS PS1 do `System/Bios/`: `romw.bin` (amerykański/europejski SCPH-5501/5502) i `romJP.bin` (japoński
SCPH-5500). Instalatory wypełniają je z paczek BIOS RetroArch, chyba że twoje pliki już tam są. Bez nich
emulator działa na wbudowanym BIOS-ie HLE, który wiele gier toleruje, a niektóre nie.

### 3.12 Aktualizacje

- **Raspberry Pi, pendrive PC, Windows**: launcher sprawdza stronę przy starcie i raz dziennie (Opcje →
  *Aktualizacje* to kanał; *Aktualizacja* w menu systemowym sprawdza od razu). Gdy jest nowszy AutoBleem
  lub RetroArch, pyta: *Aktualizuj teraz* pobiera wszystko i uruchamia ponownie instalator z ekranem
  postępu z pierwszego uruchomienia; *Przypomnij jutro* i *Pomiń tę wersję* to pozostałe odpowiedzi. Gry i
  ustawienia zostają; po aktualizacji launcher raz skanuje ponownie.
- **PlayStation Classic**: uruchom nowszy `AutoBleemInstaller.exe` na pendrivie (punkt 2.1).

### 3.13 Rozszerzenia i Sklep AutoBleem

**Rozszerzenia** dodają do launchera własne ekrany. Mieszkają w `Extensions/<nazwa>/` na pendrivie (na
Raspberry Pi na partycji danych, w Windows w folderze danych); żeby zainstalować rozszerzenie, rozpakuj tam
jego zip. **L2 + R2 → Rozszerzenia** pokazuje ich listę: Krzyżyk uruchamia, Trójkąt wyłącza lub włącza
ponownie. Rozszerzenie, które potrzebuje sieci, nie uruchomi się bez niej, a takie, które zatrzymało
launcher, zostaje wyłączone - lista o tym mówi.

![Lista rozszerzeń](../images/pl/extensions.jpg)

**Sklep AutoBleem** to pierwsze rozszerzenie: aplikacje i gry instalowane jednym przyciskiem, na każdym
systemie, na którym działa AutoBleem (PlayStation Classic potrzebuje WiFi z kernela AutoBleem). Ma cztery
karty, L1 / R1 przełączają:

- **Aplikacje** i **Gry**: to, co oferują źródła, każda pozycja z obrazkiem, wersją, rozmiarem i ikoną źródła. Zainstalowane pozycje są wyszarzone. Krzyżyk instaluje (albo aktualizuje, albo próbuje ponownie po niepowodzeniu), Trójkąt usuwa to, co zainstalował Sklep.
  L2 / R2 lub Lewo / Prawo zmieniają stronę, **Select** pokazuje jedno źródło naraz, **Start** szuka w
  tytułach. Obrazki pozycji są buforowane i mogą być ponownie pobrane, jeśli ich wczytanie się nie powiedzie.
- **Pobrania**: co się pobiera, czeka, nie udało się lub jest zainstalowane. Pasek postępu aktualizuje się płynnie. Pobieranie trwa w tle, także
  po wyjściu ze Sklepu; uruchomienie gry lub wyłączenie tylko je wstrzymuje, a przerwane pobieranie wznawia
  się od miejsca, w którym stanęło. Zainstalowana gra pojawia się na półce po najbliższym skanie, z
  obrazkiem ze Sklepu jako okładką. Pobierania ponad 2 GB działają na wszystkich platformach, w tym na wydaniach 32-bitowych.
- **Źródła**: skąd pochodzą listy - własny katalog AutoBleem, lista TSV wrzucona do
  `System/Extensions/store/sources/` i adresy dodane przez **Dodaj adres źródła**. Każde źródło pokazuje swoją ikonę na liście. Krzyżyk na dodanym przez
  ciebie źródle pozwala zmienić jego nazwę, zmienić adres, przełączyć między `http://` a `https://` albo je
  usunąć.

![Karta Aplikacje w Sklepie](../images/pl/store-apps.jpg)

![Menu źródła](../images/pl/store-source-menu.jpg)

To, co oferuje katalog AutoBleem, jest też na stronie pobierania, `https://autobleem.retromenele.pl/store/`.
**Za zawartość dodanych przez ciebie źródeł odpowiadasz ty.**

**Własne gry w sieci domowej**: `abstored`, serwer sieciowy Sklepu, udostępnia folder gier PS1 Sklepowi w
tej samej sieci. Działa na dowolnym Linuksie - Raspberry Pi, domowym serwerze - i tylko czyta folder. Uruchom
go poleceniem `abstored <folder gier>`, otwórz w przeglądarce `http://<ta maszyna>:8124/`, żeby zobaczyć, co
udostępnia i jakie problemy znalazł, i dodaj `http://<ta maszyna>:8124/store.tsv` jako źródło. Gotowe
programy dla Linuksa i Windows są na stronie Sklepu, w zakładce **LAN server**; instrukcja instalacji jako
usługi to `INSTALL-linux.md` (`ext_store/server/` w źródłach). Gry i płyty z PC wgrywa na taki serwer
**LAN Share** (rozdział 5.2).

### 3.14 Procesory skanowania

**Procesory skanowania** to małe programy, które każde skanowanie uruchamia, zanim przeczyta gry. Procesor
może zamienić format, którego AutoBleem nie czyta, na taki, który czyta - na przykład spakowaną grę - albo
zmienić dane gry, jak łatka z tłumaczeniem. Mieszkają w `System/Processors/<nazwa>/` na pendrivie (na
Raspberry Pi na partycji danych, w Windows w folderze danych); żeby zainstalować procesor, rozpakuj tam jego
folder. Następne skanowanie go uruchomi.

- **Unzip jest dołączony do AutoBleem**: rozpakowuje spakowane gry PS1 w `Games/`, zanim skanowanie je
  przeczyta, oraz spakowane ROM-y, każdy osobno (zestawy arcade zostają spakowane). Aktualizacja AutoBleem
  aktualizuje też jego, a jeśli go wyłączyłeś, zostaje wyłączony.
- Procesor, który już zajął się grą, nie jest uruchamiany na niej ponownie, dopóki gra się nie zmieni.
- Gdy procesor pracuje, dymek w prawym górnym rogu pokazuje, co robi; ostrzeżenie albo błąd pojawia się w
  linii pod nim. Szczegóły są w `processors.log` w folderze logów.
- Uruchomienie gry albo RetroArch zatrzymuje procesor, który zmienia pliki; następne skanowanie kończy jego
  pracę.

**L2 + R2 → Procesory skanowania** pokazuje je w kolejności, w jakiej działają, na jednej karcie dla gier PS1,
na drugiej dla ROM-ów (L1 / R1). **Kwadrat** podnosi procesor, a góra / dół go przesuwają - kolejność ma
znaczenie: procesor, który rozpakowuje, musi być przed tym, który łata to, co rozpakowano. **Krzyżyk**
wyłącza go albo włącza, **Trójkąt** każe mu przy następnym skanowaniu przejrzeć wszystkie gry od nowa,
**Kółko** wraca i uruchamia skanowanie, jeśli coś zmieniłeś. Procesor zbudowany dla innego urządzenia zostaje
na liście, wyszarzony.

![Procesory skanowania](../images/pl/processors.jpg)

Własny procesor: strona procesora Unzip, `https://github.com/autobleem2/proc_unzip`, opisuje wszystko, co
procesor musi robić, a `tools/proc_check.py` w
źródłach AutoBleem sprawdza go, zanim się nim podzielisz.

<!-- pagebreak -->

## 4. Ekrany

### 4.1 Menedżer gier

Gry PS1 jako lista z folderami oraz okładka i opis wybranej. Krzyżyk otwiera edytor gry, **Kwadrat usuwa
grę** (jej folder, a po drugim pytaniu także stany gry), Trójkąt usuwa wszystkie PNG okładek obok gier (skan
weźmie je znów z baz), L2 / R2 zmieniają stronę. Wolne miejsce na dysku jest w prawym górnym rogu. Menedżer
gier czeka, gdy trwa skan.

![Menedżer gier](../images/pl/game-manager.jpg)

### 4.2 Informacje o sprzęcie

Fakty o maszynie - system, sprzęt, dyski z wolnym miejscem, adresy sieciowe, sterowniki ekranu i dźwięku,
podłączone pady - odczytywane co sekundę. Na PlayStation Classic z kernelem AutoBleem ta pozycja otwiera
zamiast tego **PSC-Bios** (rozdział 6).

Pierwsze dwa kontrolery są wyświetlane jako Gracz 1 i Gracz 2 – porty, które emulator PS1 im przydziela.
Każdy dodatkowy kontroler jest wyświetlany jako nieużywany przez emulator PS1. RetroArch przydziela kontrolery
według własnych ustawień i może je uporządkować inaczej. Gdy kontroler zostanie podłączony lub odłączony, launcher
krótko pokazuje, który pad to Gracz 1 i Gracz 2.

![Informacje o sprzęcie](../images/pl/hardware-info.jpg)

### 4.3 Przewodnik po przyciskach

Trójkąt na półce: wszystkie przyciski każdego ekranu na jednej stronie. Gdy klawiatura USB jest podłączona lub była używana, kolumna Klawiatura pokazuje klawiszę obok przycisków pada.

![Przewodnik po przyciskach](../images/pl/button-guide.jpg)

### 4.4 Klawiatura ekranowa

Wszędzie, gdzie wpisuje się tekst - zestaw kart pamięci, tytuł gry, hasło WiFi, adres źródła - ta sama
klawiatura, ułożona jak w telefonie: litery, strona symboli (`/ \ : ? & = % @ #` i reszta tego, czego
potrzebuje adres lub hasło) i dwie strony liter z akcentami, a w dolnym rzędzie Shift, klawisz strony,
Spacja, Backspace i Potwierdź. Kierunki przesuwają, Krzyżyk wpisuje, Trójkąt kasuje, Kwadrat to spacja,
**L1** to Shift (dwa razy: stałe duże litery), **R1** następna strona, **L2 / R2** przesuwają kursor, Start
zatwierdza, Kółko anuluje. Klawiatura USB pisze w każdej chwili: Enter zatwierdza, Esc anuluje.

![Klawiatura ekranowa](../images/pl/keyboard.jpg)

<!-- pagebreak -->

## 5. Na PC

### 5.1 UpdateRoms - odświeżanie pendrive'a konsoli

PlayStation Classic nie ma sieci, więc listy RetroArch i okładki jego pendrive'a powstają na PC:
**UpdateRoms** robi na PC to, co na Pi robi skan launchera - z siecią komputera i ze ścieżkami konsoli, tak
że konsola po włączeniu zastaje wszystko gotowe.

1. Skopiuj ROM-y na pendrive do `RetroArch/roms/<system>/` (punkt 3.9). Nazwy folderów muszą być nazwami
   baz RetroArch; instalator zakłada najpopularniejsze.
2. Uruchom **`UpdateRoms\UpdateRoms.exe` z pendrive'a** (instalator go tam położył). Program znajduje
   pendrive po tym, gdzie leży, pokazuje wiersz etapu, pasek postępu i log oraz:
   - pobiera paczkę baz RetroArch, gdy pendrive jej nie ma, i rozpoznaje po niej każdy ROM - gra znana
     bazie dostaje właściwą nazwę;
   - zapisuje po jednej liście na system do `RetroArch/bin/playlists/` ze ścieżkami konsoli, zachowując
     to, co dodał tam sam RetroArch;
   - pobiera okładki wszystkich ROM-ów bez okładki z serwerów miniatur libretro do
     `RetroArch/bin/thumbnails/`.
3. Bezpiecznie odłącz pendrive i włóż go z powrotem do konsoli. Zakładka RetroArch w wyborze zestawu
   pokazuje każdy system z grami.

Uruchamiaj go po każdej zmianie w folderach ROM-ów; folder, w którym nic się nie zmieniło, jest pomijany,
więc kolejne uruchomienie jest szybkie. Log to `System/Logs/updateroms.log`. Kartę Raspberry Pi w czytniku
można odświeżyć tak samo (`UpdateRoms.exe <dysk> --target rpi`), choć Pi robi to sam, gdy ma sieć.

### 5.2 LAN Share - własne gry i płyty na serwer w sieci

**LAN Share** (`LanShare.exe`, na stronie Sklepu w zakładce **LAN server**) wgrywa twoje gry PS1 na serwer
Sklepu w sieci domowej - `abstored` na Raspberry Pi, NAS-ie albo innym komputerze - i czyta płytę PS1 z napędu
CD/DVD komputera. Sklep na konsoli, Pi albo PC instaluje je potem stamtąd. Niczego nie trzeba instalować;
ustawienia są w `%LOCALAPPDATA%\AutoBleem LAN Share\`.

![Okno LAN Share](../images/pl/lanshare.jpg)

1. **Serwer**: wpisz jego adres (`http://<jego adres>:<port>`, jak w Sklepie) i naciśnij **Connect**. Po
   lewej pojawią się jego gry i problemy, które znalazł jego skan. Żeby wgrywać gry, podaj jedno z dwóch:
   - **Share** - folder gier serwera udostępniony w sieci (Samba), np. `\\raspberrypi\games`: LAN Share
     kopiuje tam gry i prosi serwer o skan. Serwer zostaje tylko do odczytu.
   - **Token** - gdy serwer uruchomiono z `--allow-uploads`: jego token (serwer wypisuje go przy starcie i
     trzyma w `<state>/upload-token`). LAN Share wysyła gry przez HTTP; przerwane wysyłanie jest wznawiane.
2. **Gry z tego PC**: wybierz folder z grami (jeden folder na grę), zaznacz gry i naciśnij **Publish the
   ticked games**. Kolumna **On the server** mówi, czy serwer już ma daną grę (po numerze seryjnym, inaczej
   po tytule); takiej gry LAN Share nie wysyła drugi raz. **Tick those not on the server** zaznacza resztę.
3. **Płyta**: włóż płytę PS1 i naciśnij **Read a disc and publish it**. Płyta jest czytana w całości do
   `.bin` + `.cue` (i `.sbi` dla gry z zabezpieczeniem LibCrypt, gdy napęd podaje podkanał), nazywana
   tytułem, sprawdzana ze znanym wzorcowym zrzutem (gdy wybrano bazy) i wgrywana. Dla gry na kilku płytach
   zaznacz **The game has more than one disc** - LAN Share poprosi o każdą kolejną i wgra całość jako jedną
   grę.
4. **Remove from the server...** zdejmuje zaznaczone gry z serwera. Nic nie jest kasowane: każda trafia do
   folderu `.removed` obok gier serwera, a przeniesienie jej z powrotem ją przywraca.

Bazy (**Databases**) - folder okładek AutoBleem (`coversU/P/J.db`) i `Sony - PlayStation.rdb` z RetroArch -
dają tytuły i sprawdzenie przeczytanej płyty; obie są opcjonalne. **Also share the games on this PC with the
Store** (domyślnie wyłączone) udostępnia Sklepowi folder z tego PC bezpośrednio. Przy pierwszym razie Windows
pyta o zaporę: zezwól tylko na sieci prywatne.

<!-- pagebreak -->

## 6. Narzędzia konsolowe (PlayStation Classic)

Dwa narzędzia na pendrivie PlayStation Classic. Oba rysują w motywie i języku launchera i oba obsługuje
się padem - a w kreatorze mapowania przednimi przyciskami konsoli. **PSC-Bios** to rozszerzenie dołączone do
pakietu konsoli: otwiera je *Informacje o sprzęcie* w menu systemowym i jest na liście rozszerzeń.
**ABFlashKit** to aplikacja w zestawie Aplikacje.

### 6.1 PSC-Bios

Rozszerzenie dołączone do pakietu konsoli, dostępne też na Raspberry Pi i pendrivie PC. Jest otwierane z
menu systemowego pozycji *Sieć i kontrolery* (albo z listy Rozszerzeń). Gdy to rozszerzenie jest zainstalowane, ale wyłączone, pozycja *Sieć i kontrolery* w szybkim menu i menu systemowym jest wyszarzona z notatką "włącz w Rozszerzeniach" - Krzyżyk tam otwiera Rozszerzenia.

Pierwszy ekran pokazuje fakty maszyny: czas, strefę czasową, adaptery sieciowe WiFi/Ethernet/Bluetooth z adresami, oraz
każdy podłączony kontroler z informacją, czy ma mapowanie przycisków. Część sieciowa wymaga kernela
AutoBleem na konsoli (punkt 6.2) lub narzędzi systemowych na Raspberry Pi / pendrivie PC; kreator
mapowania pada działa wszędzie.

![PSC-Bios: centrum Sieć i kontrolery](../images/pl/pscbios-main.jpg)

- **Select - Sieć WiFi** (tylko z kernelem lub NetworkManager): nazwa sieci (wpisana albo wybrana ze skanu),
  hasło, tryb sterownika, i *Zastosuj / Restartuj sieć*. Strefa czasowa ustawia się tutaj. Adres IP konsoli
  pokazuje się po połączeniu.
- **Kwadrat - Pady Bluetooth**: skan padów Bluetooth (DualShock 4, itd.) do sparowania albo usunięcia.
- **L1 - Parowanie DualShock 3**: połączenie tylko przez USB dla pierwszego DualShock 3, przez plugin sixaxis kernela.
- **R1 - Mapowanie pada**: kreator mapowania (poniżej).
- **Trójkąt - O programie**, **Kółko - powrót** do launchera.

**Kreator mapowania** pokazuje podłączony pad "na surowo" - każdą oś, przycisk i krzyżak jako liczby - oraz
rysunek DualShocka, który podświetla się przy naciskaniu. Ponieważ testowanemu padowi nie można ufać,
kreator obsługują **przednie przyciski** konsoli: **RESET** przełącza na następny pad, **OPEN** zaczyna
mapowanie (potem odpowiada na każde pytanie - naciśnij przycisk podświetlony na rysunku albo OPEN, gdy
pad takiego nie ma), **POWER** anuluje lub wychodzi. Przytrzymanie Kółka na padzie przez 2 sekundy kończy kreator (pasek się wypełnia, a podpowiedź w stopce mówi "Hold 2 s: Exit"). Gdy pad nie ma jeszcze żadnego mapowania, przytrzymanie jakiegokolwiek przycisku przez 2 sekundy to robi ("Hold any button 2 s: Exit"). Krótkie naciśnięcie mapuje się normalnie. Na klawiaturze Esc / Spacja / Enter zastępują POWER / RESET / OPEN. Na koniec nowe mapowanie jest dodane do testu, a OPEN
zapisuje je pod wybraną nazwą; od tej pory launcher je wczytuje.

![PSC-Bios: kreator mapowania pada](../images/pl/pscbios-wizard.jpg)

### 6.2 ABFlashKit - kernel AutoBleem

Kernel AutoBleem to opcjonalny zamiennik linuksowego kernela konsoli: daje działający zegar, adaptery USB
WiFi i Bluetooth (do PSC-Bios i padów Bluetooth) oraz obsługę przednich przycisków, z której emulator
korzysta przy punktach wznowienia. ABFlashKit go instaluje, najpierw robiąc kopię konsoli, i potrafi
przywrócić konsolę do stanu fabrycznego przez własne narzędzie odzyskiwania Sony.

> **To narzędzie zapisuje pamięć flash konsoli.** Przerwane wgrywanie - odcięte zasilanie, wyjęty pendrive
> - może uniemożliwić uruchomienie konsoli, a własny kernel unieważnia gwarancję. Nie odłączaj zasilania
> ani pendrive'a, dopóki konsola sama się nie uruchomi ponownie. ABFlashKit otwiera się na tym
> ostrzeżeniu; *Rozumiem* idzie dalej, *Koniec* wychodzi.

![Menu ABFlashKit](../images/pl/abflashkit-menu.jpg)

- **Wypal kernel**: robi na pendrivie kopię zapasową partycji konsoli (`LBOOT.EPB`), jeśli jeszcze jej nie
  ma, sprawdza ją i obraz kernela, wgrywa kernel i pliki systemowe AutoBleem, po czym restartuje konsolę.
  *Gotowe - gdy ekran zgaśnie, wepnij ponownie kabel zasilający*: odłącz zasilanie konsoli i podłącz je
  z powrotem.
- **Wykonaj kopię**: wszystkie cztery partycje do `LBOOT.EPB`, do późniejszego przywrócenia (poprzednia
  kopia jest nadpisywana po pytaniu).
- **Przywróć z kopii**: sprawdza, że kopia jest fabryczna, ustawia flagę odzyskiwania i restartuje konsolę
  do narzędzia odzyskiwania Sony, które przywraca konsolę z `LBOOT.EPB` na pendrivie - droga powrotu do
  fabrycznego oprogramowania.

Pasek postępu pod każdym krokiem pokazuje, jak daleko jest operacja. Narzędzie odmawia wgrania kernela na
konsolę z innym własnym oprogramowaniem (BleemSync, Project Eris): najpierw przywróć ją do stanu
fabrycznego.

<!-- pagebreak -->

## 7. Gdy coś nie działa

- **Logi**: AutoBleem trzyma logi w pamięci, żeby nie zapisywać pendrive'a bez przerwy - do `System/Logs/`
  na pendrivie, karcie albo w folderze danych trafiają tylko wtedy, gdy coś poszło nie tak: awaria
  launchera, gry PS1 albo RetroArch zapisuje je do `System/Logs/crash-<n>/` (zostają trzy ostatnie), a
  launcher mówi o tym raz, gdy wraca. Żeby zachowywać wszystkie logi, włącz *Opcje -> Diagnostyka ->
  Zachowuj logi na pendrivie* (od następnego uruchomienia) albo utwórz na PC pusty plik `System/Logs/keep`.
  Na Pi i PC strona *Informacje Sprzętowe* pokazuje, gdzie są logi, a Kwadrat zapisuje je do
  `System/Logs/saved-<n>/`. Pliki: `autobleem.log` (launcher), `launch.log` i `pcsx.log` (start gry PS1 i
  wyjście emulatora), `retroarch.log`, oraz - zawsze na pendrivie - `update.log` (aktualizacja z sieci) i
  `updateroms.log` (UpdateRoms).
- **Gry nie ma na półce**: sprawdź układ folderów (jeden folder na grę, formaty obrazów z punktu 3.9).
  *Menadżer gier* pokazuje za grami foldery, które skan odrzucił, z dopiskiem *Nie dodano* i przyczyną;
  Kwadrat usuwa taki folder. *Skanuj gry ponownie* w menu systemowym uruchamia skan jeszcze raz.
- **Brak okładek**: nie zainstalowano baz okładek (uruchom instalator ponownie z zaznaczonymi bazami)
  albo - dla gier RetroArch na konsoli - nie uruchomiono UpdateRoms na PC.
- **Pad nie działa albo ma pomieszane przyciski**: kreator mapowania w PSC-Bios (konsola) go mapuje; na
  Pi lub PC strona Informacje o sprzęcie pokazuje, co widzi SDL.
- **Konsola pokazuje czarny ekran po grze**: AutoBleem sam odbudowuje swoje okno (do trzech razy); jeśli
  zostaje czarny, przytrzymaj przycisk zasilania i włącz konsolę ponownie.
- **Raspberry Pi**: `Alt+F2` daje ekran logowania na drugiej konsoli; SSH jest włączone od pierwszego
  uruchomienia. `sudo journalctl -u autobleem` pokazuje usługę launchera; `sudo systemctl restart
  autobleem` uruchamia ją ponownie. Pierwsze uruchomienie, które nie mogło się zakończyć (brak sieci),
  ponawia się przy następnym starcie.
- **Windows**: `Esc` kończy launcher; folder danych to ten wybrany w instalatorze (domyślnie
  `Dokumenty\AutoBleem`), logi są w jego `System\Logs`.

AutoBleem to wolne oprogramowanie (GNU GPL v3 lub nowsza), bez gwarancji. Wsparcie i nowości: serwer
Discord podlinkowany na ekranie O programie oraz https://autobleem.retromenele.pl/.

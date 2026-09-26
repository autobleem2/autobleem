# Manuel utilisateur d'AutoBleem 2

AutoBleem 2 est un lanceur de jeux pour la **PlayStation Classic** - et, depuis la version 2, pour un **Raspberry Pi**,
un **PC démarrant sur une clé USB** et **Windows**. Il affiche vos jeux PS1 sous forme d'une étagère de jaquettes avec
leurs emboîtages et détails, les lance dans l'émulateur PCSX fourni, et, avec RetroArch installé à côté, peut jouer
les jeux d'autres systèmes. Ce manuel couvre l'installation sur chaque plateforme, l'utilisation quotidienne et les
outils fournis.

> Les téléchargements pour chaque plateforme se trouvent sur **https://autobleem.retromenele.pl/**. La page est
> organisée par plateforme : le panneau *Install* de chacune est ce que vous téléchargez ; les *Build inputs*
> en dessous sont ce que les installateurs téléchargent eux-mêmes.

## 1. Ce que vous obtenez

- **Le lanceur** - l'étagère de jaquettes, les listes (PlayStation, RetroArch, Applications), les détails du jeu,
  le menu système, les options, les outils de carte mémoire et de points de reprise. Le même programme sur
  toutes les plateformes.
- **Deux émulateurs PS1** - `pcsx-abnxt`, l'émulateur actuel (par défaut), et `pcsx-ab`, l'émulateur classique
  que AutoBleem a toujours fourni. Vous en choisissez un dans les options ; les deux utilisent les mêmes
  paramètres et cartes mémoire.
- **RetroArch** (optionnel sur toutes les plateformes) pour les autres systèmes : NES, SNES, Mega Drive, Game Boy,
  arcade et bien d'autres. AutoBleem crée ses listes RetroArch à partir des ROM que vous copiez et lance chaque
  jeu avec le bon cœur.
- **Les outils de console** (PlayStation Classic uniquement) : *PSC-Bios* pour WiFi, l'horloge et la configuration
  de la manette, et *ABFlashKit* pour installer le noyau AutoBleem.
- **UpdateRoms** pour Windows : actualise les listes RetroArch et les jaquettes d'une clé console sur un PC, car
  la console elle-même n'a pas de réseau.

![Le lanceur : l'étagère de jaquettes, les détails du jeu sélectionné, les indices de touches](../images/en/launcher.jpg)

<!-- pagebreak -->

## 2. Installation

### 2.1 PlayStation Classic

Vous avez besoin d'un PC Windows, une clé USB (USB 2.0, 8 Go ou plus ; l'installateur la formate si vous le
demandez) et la console d'origine. AutoBleem s'exécute à partir de la clé sans aucune modification de la console.
La clé doit être **FAT32** pour une console d'origine - son noyau ne peut pas lire l'exFAT. Seule une console
avec le noyau AutoBleem installé (ABFlashKit, chapitre 6) démarre également à partir d'une clé exFAT, ce qui
lève la limite de 4 Go de FAT32.

1. Téléchargez **AutoBleemInstaller-<version>.zip** du panneau PlayStation Classic du site et décompressez-le
   n'importe où. Il contient `AutoBleemInstaller.exe` et le paquet AutoBleem qu'il installe.
2. Branchez la clé et démarrez `AutoBleemInstaller.exe`. Sélectionnez le lecteur en haut. Cochez ce que vous
   voulez :
   - **Formater la clé** - seulement pour une clé neuve (tout ce qui s'y trouve est effacé). Sélectionnez FAT32,
     sauf si la console a le noyau AutoBleem.
   - **Bases de données de jaquettes** - les emboîtages et détails de la bibliothèque PS1 (cochée par défaut ;
     environ 300 Mo).
   - **RetroArch** - RetroArch avec ses cœurs, les applications supplémentaires (Doom, Quake, Amiga, ...) et les
     actifs libretro pour les jeux des autres systèmes. Désactivé par défaut ; peut être ajouté plus tard en
     réexécutant l'installateur.
   - **Fichiers BIOS** - les fichiers BIOS que les cœurs RetroArch nécessitent (nécessite RetroArch).
   - **Jeux exemple** - quelques jeux homebrew gratuits pour que l'étagère ne soit pas vide.
3. Appuyez sur **Installer** et attendez. Les barres de progression et le journal affichent chaque étape ; la clé
   est nommée `SONY` à la fin et `UpdateRoms` est mis dessus (voir chapitre 5).
4. Retirez la clé en toute sécurité, branchez-la dans le **second port USB** de la console (le droit, joueur 2)
   et mettez la console en marche. AutoBleem démarre à la place du menu d'origine.

**Allumage et extinction.** Avec la clé dedans, la console démarre, son voyant clignote quelques secondes
(AutoBleem est en cours de configuration) puis passe en veille avant que quoi que ce soit ne s'affiche - c'est
la propre façon de la console de mettre en scène une mise à jour, c'est comme cela qu'AutoBleem s'exécute. Appuyez
sur **Power** une fois et le lanceur s'affiche. *Éteindre* dans le menu système, ou le bouton Power de la console,
met la console en **veille d'AutoBleem** : la clé est d'abord déconnectée, puis le voyant devient **rouge** - le
signe qu'AutoBleem fonctionne comme prévu - et le prochain appui sur Power ramène le lanceur directement, en
quelques secondes. **Pendant que le voyant est rouge, la clé peut être retirée** et mise sur un PC sans que Windows
ne demande de la vérifier ; remettez-la avant d'appuyer sur Power. Débrancher la console passe à nouveau par la
veille de démarrage la prochaine fois.

Pour **mettre à jour** une clé, exécutez un nouvel installateur sur elle : vos jeux, sauvegardes, paramètres et
contenu RetroArch restent ; seuls les fichiers d'AutoBleem sont remplacés. Une clé créée avec AutoBleem 1.0 ou
AutoBleem-NG est automatiquement mise en page.

> La console d'origine n'a pas d'horloge et pas de réseau : les dates ne s'affichent qu'après l'installation du
> noyau AutoBleem (chapitre 6), et les jaquettes pour les jeux RetroArch proviennent d'UpdateRoms sur le PC
> (chapitre 5).

Les jeux vont dans le dossier `Games` de la clé, un dossier par jeu - voir la section 3.9 pour la mise en page.

### 2.2 Raspberry Pi

AutoBleem transforme un Pi en petite console : il démarre directement dans le lanceur, sans bureau. Deux images
prêtes sont sur le site - 32 bits et 64 bits - plus une archive pour un Raspberry Pi OS Lite existant.

| Modèle | Image 32 bits | Image 64 bits | Notes |
|---|---|---|---|
| Raspberry Pi 5 | oui | oui | |
| Raspberry Pi 4 Modèle B, Pi 400 | oui | oui | |
| Raspberry Pi 3 Modèle B / B+ / A+ | oui | oui | bon pour le lanceur et PS1 |
| Raspberry Pi Zero 2 W | oui | oui | 512 Mo de RAM : PS1 fonctionne, les cœurs RetroArch plus lourds non |
| Raspberry Pi 2 Modèle B | oui | v1.2 uniquement | lent pour tout en 3D |
| Raspberry Pi 1, Zero, Zero W | non | non | ARMv6 - aucune image ne fonctionne |

L'**image 32 bits est la recommandée** pour les jeux PS1 : le recompilateur ARM rapide de `pcsx-ab` est 32 bits
uniquement, donc la version 64 bits exécute les jeux PS1 plus lentement. L'image 64 bits a un ensemble plus
important de cœurs RetroArch.

**Avec Raspberry Pi Imager :**

1. Installez Raspberry Pi Imager (raspberrypi.com/software). Dans *Choose OS* sélectionnez *Use custom* et le
   `autobleem-<version>-rpi-armhf.img.xz` (32 bits) ou `-arm64.img.xz` (64 bits) que vous avez téléchargé - ou
   ajoutez l'URL du dépôt `https://autobleem.retromenele.pl/rpi-imager/os_list.json` dans les paramètres de
   l'application et sélectionnez AutoBleem dans la liste.
2. Utilisez l'écran de personnalisation d'Imager (l'engrenage, ou la question après *Suivant*) pour définir
   **le nom d'utilisateur et le mot de passe, le réseau WiFi et le pays, et activer SSH**. AutoBleem a besoin
   d'un réseau au premier démarrage.
3. Écrivez la carte, insérez-la dans le Pi avec un écran et un clavier ou une manette, et allumez-la.

**Le premier démarrage** prend 5 à 25 minutes et affiche ce qu'il fait à l'écran. Sans réseau, il vous en demande
un (une liste WiFi, le mot de passe), puis demande s'il faut installer RetroArch (une minute sans réponse signifie
oui), agrandit la partition système, crée la partition de données `AUTOBLEEM` à partir du reste de la carte,
installe RetroArch et ses cœurs, les paquets BIOS et les jeux exemple, et redémarre dans le lanceur.

Les réponses peuvent être fournies à l'avance dans **`autobleem.txt`** sur la partition de démarrage de la carte
(modifiable sur n'importe quel PC avant le premier démarrage) :

| Clé | Par défaut | Signification |
|---|---|---|
| `root_gib` | 8 | La taille de la partition système en Go ; le reste devient la partition de jeux. |
| `hdmi_mode` | 1920x1080@60 | Le mode d'écran pour tout le démarrage (`1280x720@60` pour une TV plus ancienne). |
| `retroarch` | (demandé) | `yes` / `no` - RetroArch et les autres systèmes, ou PS1 uniquement. |
| `thumbnails` | none | `boxarts` met en miroir l'ensemble complet des jaquettes pour les couvertures hors ligne (~9000 fichiers). |
| `bios`, `downloads`, `samples` | yes | Définissez à `no` pour ignorer les paquets BIOS, tous les téléchargements, ou les jeux exemple. |

**Sur un Raspberry Pi OS Lite existant** (Bookworm ou Trixie) : copiez `autobleem-rpi.tar.gz` (ou la version
arm64) sur le Pi, décompressez et exécutez `sudo bash install.sh`. Il pose les mêmes questions, crée la partition
de données en rétrécissant la racine au prochain démarrage (`--shrink-root <Go>`), et met le lanceur sur la
première console.

Après l'installation, la partition **`AUTOBLEEM`** de la carte (exFAT) est ce que vous remplissez : retirez la
carte et ouvrez-la sur n'importe quel PC, ou copiez via le réseau (SSH est activé). `Games/` pour les jeux PS1,
`RetroArch/roms/<système>/` pour les autres systèmes, `System/Bios/` pour le BIOS PS1 (section 3.10), `Themes/`
pour les thèmes.

### 2.3 Clé USB PC

La même appliance pour n'importe quel PC qui démarre sur USB - un système 32 bits, donc les anciennes machines
fonctionnent aussi :

1. Téléchargez `autobleem-<version>-pcusb-i386.img.xz` du panneau PC et écrivez-le sur une clé de 8 Go ou plus
   avec Raspberry Pi Imager (*Use custom*), balenaEtcher ou Rufus (mode DD).
2. Démarrez le PC depuis la clé (la touche du menu de démarrage de votre PC - F12, F8, Esc...). Les démarrages
   BIOS et UEFI fonctionnent ; **Secure Boot doit être désactivé**.
3. Le premier démarrage est celui du Pi : une question sur le réseau s'il n'y a pas de câble, la question RetroArch,
   puis l'installation - environ huit minutes avec un réseau câblé - et un redémarrage dans le lanceur.

La clé a alors une partition `AUTOBLEEM` pour vos jeux, visible sur Windows 10 (1903 et plus récent) comme un
second lecteur lorsque vous branchez la clé sur un PC en cours d'exécution. `autobleem.txt` est sur la première
partition, avec les mêmes clés que sur le Pi (pas `hdmi_mode` - le PC utilise le mode natif de l'écran).

### 2.4 Windows

AutoBleem en tant que programme Windows : plein écran, les émulateurs et RetroArch lancés en tant que programmes.

1. Téléchargez **AutoBleemSetup-<version>.exe** et exécutez-le. Il s'installe par utilisateur, sans droits
   administrateur : le programme sous `%LOCALAPPDATA%\Programs\AutoBleem`, les données (jeux, paramètres, thèmes,
   RetroArch) dans un dossier de votre choix - `Documents\AutoBleem` par défaut.
2. Cochez les composants - les bases de données de jaquettes, RetroArch (la version Windows officielle et ses
   cœurs), les fichiers BIOS, les jeux exemple - et laissez l'assistant d'installation les télécharger.
3. Démarrez AutoBleem à partir du menu Démarrer ou du Bureau. Sur un PC, le clavier fonctionne comme une
   manette (section 3.2).

L'exécution d'une nouvelle version du programme le met à jour et conserve le dossier de données. Le lanceur
vérifie également le site une fois par jour et propose une mise à jour s'il en existe une (section 3.11).

<!-- pagebreak -->

## 3. Utiliser AutoBleem

### 3.1 Le lanceur

Le lanceur s'ouvre sur l'étagère : les jaquettes de la liste actuelle, le jeu sélectionné au milieu, ses détails
à côté - éditeur, année, numéro de série, région, joueurs, quand il a été joué pour la dernière fois - et un
bouton de lecture. La barre en bas énumère ce que font les boutons. Un scan du dossier de jeux s'exécute en
arrière-plan à chaque démarrage ; pendant son exécution, une bulle en haut à droite affiche sa progression, et
les nouveaux jeux apparaissent sur l'étagère au fur et à mesure de leur découverte.

![Le sélecteur de liste : trois onglets et les groupes du courant avec leurs nombres de jeux](../images/en/set-picker.jpg)

### 3.2 Contrôles

| Bouton | Sur l'étagère |
|---|---|
| Gauche / Droite | Jeu précédent / suivant. Le maintien fait défiler. |
| L1 / R1 | Sauter à la première lettre précédente / suivante des titres. |
| Cross | Démarre le jeu sélectionné (un jeu PS1 dans l'émulateur PS1 ; un jeu RetroArch dans son cœur ; une app après son manuel). |
| Square | Démarre le jeu PS1 sélectionné dans RetroArch à la place. |
| Triangle | Le guide des touches. |
| Start | Un jeu aléatoire de la liste actuelle. |
| Select | Le sélecteur de liste : onglets PlayStation / RetroArch / Applications (L1 / R1), les groupes de l'onglet (Haut / Bas, L2 / R2 une page), Cross sélectionne. |
| Bas | Ouvre la rangée d'icônes sous le jeu (Paramètres, Jeu, Carte mémoire, Reprendre). Haut la ferme. |
| L2 + R2 | Le menu système (section 3.4). |

**Avec un clavier** (un PC sans manette ou un clavier USB sur la console, un Pi ou la clé PC) les touches
remplacent : **Flèches** = d-pad, **Entrée** = Cross, **Échap ou Retour arrière** = Circle, **Tabulation** =
Triangle, **Espace** = Square, **F1 / F2** = Select / Start, **Page Précédente / Page Suivante** = L1 / R1,
**Accueil / Fin** = L2 / R2, **F10** = le menu système. Sur une machine de développement, Échap ferme le
programme et Espace est Start.

Dans chaque liste et menu : Haut / Bas se déplacent, **L2 / R2 changent de page**, L1 / R1 vont à la première /
dernière ligne, **Cross sélectionne, Circle revient**. Un écran avec des paramètres les enregistre quand vous le
quittez avec Circle.

![La rangée d'icônes sous le jeu sélectionné](../images/en/launcher-icons.jpg)

### 3.3 Les listes

**Select** ouvre le sélecteur de liste. L'onglet PlayStation énumère *Tous les jeux*, *Jeux internes* (les vingt
intégrés de la console, sur une PlayStation Classic), chaque dossier que vous avez créé sous `Games/` (un jeu dans
un sous-dossier appartient à ce groupe), *Jeux favoris*, *Historique des jeux* et, le cas échéant, *Jeux de pistolet*.
L'onglet RetroArch énumère un groupe par système qui a des jeux, plus les favoris et l'historique propres de
RetroArch. L'onglet Applications regroupe les applications par type : *Toutes les applications*, puis *Jeux*,
*Émulateurs*, *Outils*, *Médias* et *Autres* (la catégorie est définie dans le fichier `app.ini` de chaque app).
Chaque ligne affiche combien d'éléments elle contient ; un groupe vide s'ouvre sur une étagère vide avec la rangée
d'icônes affichant seulement les paramètres.

### 3.4 Le menu rapide

**Haut** dans le lanceur, ou l'**icône d'engrenage** dans la rangée d'icônes (où se trouvent Paramètres / Jeu /
Carte mémoire / Reprendre) : le menu rapide pour les actions que vous atteignez depuis le carrousel. Une courte
liste : *Rescanner les jeux* (démarre un scan maintenant), *Store* (le Store AutoBleem pour télécharger des
extensions), *Réseau & Contrôleurs* (seulement où une extension installée fournit l'entrée `network` - PSC-Bios
sur la console, un Pi et la clé PC : WiFi, appairage Bluetooth, l'assistant de configuration de la manette - voir
section 6 ; grisé avec "activez-le dans Extensions" quand cette extension est désactivée - Cross ouvre la liste
des Extensions), et *Menu système...* (le menu complet ci-dessous). Haut / Bas se déplacent (boucle), Cross
sélectionne, Circle revient. Rien n'est unique ici - chaque élément est aussi dans le menu système.

### 3.5 Le menu système

**L2 + R2** (ensemble, dans n'importe quel ordre) ouvre le menu système sur l'étagère. Le menu est groupé en sections :

| Section | Élément | Ce qu'il fait |
|---|---|---|
| (haut) | Rescanner les jeux | Cherche les jeux nouveaux, modifiés ou supprimés maintenant (le scan regarde également le dossier lui-même). |
| | Extensions | Les extensions sur la clé - le Store AutoBleem et autres (section 3.12). |
| **Bibliothèque** | Gestionnaire de jeux | Les jeux PS1 sous forme de liste avec leurs dossiers : supprimer un jeu, vider les jaquettes. Désactivé pendant un scan. |
| | Cartes mémoire | Vos ensembles de cartes mémoire (section 3.7). |
| | Processeurs de scanner | Les programmes que chaque scan exécute en premier - leur ordre, activé ou désactivé (section 3.13). Désactivé pendant un scan. |
| **Système** | Options | Les paramètres d'AutoBleem (section 3.6). |
| | Réseau & Contrôleurs | Seulement où une extension installée fournit l'entrée `network` (`Provides=network` dans son `extension.ini` - PSC-Bios sur la console, un Pi et la clé PC) - WiFi, appairage de contrôleurs Bluetooth, configuration DualShock 3 et l'assistant de configuration de la manette - voir chapitre 6. Quand cette extension est installée mais désactivée, cet élément reste grisé avec une note "activez-le dans Extensions" - Cross ouvre la liste des Extensions. |
| | Informations matérielles | Les faits de la machine : système, CPU, stockage, interfaces réseau, fuseau horaire, affichage, les manettes et leurs configurations. Sur une console avec le noyau AutoBleem cela ouvre PSC-Bios (chapitre 6) ; sur d'autres machines cela affiche cette page d'information. |
| | Mise à jour logicielle | (Raspberry Pi et PC) Vérifiez le site pour un AutoBleem ou RetroArch plus récent maintenant. |
| | À propos | Crédits et licence. |
| **Quitter** | RetroArch | Quitte le lanceur pour le propre menu de RetroArch. La fermeture de RetroArch revient. |
| | Éteindre | Après une confirmation : sur la console la veille d'AutoBleem - la clé déconnectée, le voyant rouge, Power ramène le lanceur (section 2.1) ; sur un Pi ou un PC la machine s'éteint. |

![Le menu système](../images/en/system-menu.jpg)

### 3.6 Options

Les paramètres sont en groupes ; Haut / Bas se déplacent entre eux, Gauche / Droite change une valeur, Circle
quitte et enregistre. Chaque changement s'applique immédiatement.

| Groupe / Paramètre | Ce qu'il fait |
|---|---|
| **Interface** : Thème AutoBleem | L'apparence. Les thèmes vivent dans `Themes/` ; un fichier zip de thème y déposé est décompressé à la prochaine visite. Les thèmes qu'AutoBleem fournit sont actualisés à chaque mise à jour - pour en personnaliser un, copiez-le d'abord sous un nouveau nom. |
| Style de jaquette | Le cadre de coffret joaillier dessiné autour des jaquettes PS1. |
| Langue | La langue du lanceur, appliquée immédiatement (17 langues). |
| Utiliser la police du thème / Police | La police des écrans classiques : celle du thème, ou n'importe quel `.ttf`/`.otf` de `resources/fonts`, `RetroArch/fonts` ou le dossier du thème. |
| Délai d'affichage | Combien de temps la notification "Affichage : ..." reste, en secondes (0 = pour toujours). |
| **Son** : Musique, Musique de fond | Quelle piste joue sous le lanceur (celle du thème, ou un fichier de `resources/music`), et si une joue du tout. |
| **Émulation** : Émulateur PS1 | `pcsx-abnxt` (par défaut : PCSX-ReARMed courant avec les ajouts d'AutoBleem) ou `pcsx-ab` (le classique). Un point de reprise sauvegardé par un continue dans l'autre, sauf si le jeu s'exécutait sans fichier BIOS. |
| Écran large | La forme de l'image de l'émulateur PS1 pour chaque jeu. |
| Jouer tous les jeux PSX avec RA | Chaque jeu PS1 démarre dans le cœur PS1 de RetroArch. |
| Mettre à jour la configuration RA | AutoBleem écrit ses paramètres dans la configuration de RetroArch quand il lance un jeu là. |
| **Bibliothèque** : Afficher les jeux internes | Les jeux intégrés de la console dans les listes PlayStation (PlayStation Classic uniquement). |
| Récupérer les jaquettes en ligne | Le scan récupère les jaquettes manquantes depuis les serveurs de libretro (Raspberry Pi, PC, Windows). |
| **Mises à jour** | (Raspberry Pi, PC, Windows) `stable`, `latest` (également les pré-versions) ou `off`. |

![Les options, en groupes](../images/en/options.jpg)

### 3.7 Paramètres d'un jeu

Avec un jeu sélectionné, **Bas** ouvre sa rangée d'icônes : **Paramètres** (les options ci-dessus), **Jeu**
(les propres paramètres du jeu), **Carte mémoire** (sa carte mémoire) et **Reprendre** (ses points de reprise).
Cross ouvre celui sous le curseur.

L'**éditeur de jeu** affiche les détails du jeu à droite et ses paramètres à gauche, en trois groupes :

- **Jeu** : *Favori* (dans le groupe Jeux favoris), *Jeu de pistolet* (un jeu de pistolet - il rejoint le groupe
  Pistolets et s'exécute toujours dans RetroArch, dont le cœur PS1 a le GunCon), *Jouer avec RA* (ce jeu
  s'exécute dans RetroArch), *Verrouiller les données* (le scanner laisse le titre du jeu, le numéro de série
  et la liste des disques comme vous les avez définis).
- **Vidéo** : haute résolution, balayage et son niveau, saut de trames, le plugin GPU, et le *Filtre* - comment
  l'image est mise à l'échelle : Désactivé (pixels bruts), Linéaire (lissé) ou Net (pixels nets sans scintillement ;
  `pcsx-abnxt` uniquement - le classique `pcsx-ab` et RetroArch le montrent comme Désactivé).
- **Émulateur** : SpeedHack, la fréquence du processeur, l'interpolation SPU, le logo de démarrage (désactivé
  saute le shell BIOS - pour un disque homebrew dont le logo personnalisé casse le démarrage), et avec
  `pcsx-abnxt` le filtre *Lissage* et le basculement *Sony hacks*.

Triangle renomme le jeu, Square change sa carte mémoire, Start partage une nouvelle carte. Circle enregistre et
quitte.

**Paramètres enregistrés dans l'émulateur.** Le propre menu de l'émulateur a *Enregistrer les paramètres pour ce
jeu*. Une fois qu'un jeu a des paramètres enregistrés là, ce sont ceux avec lesquels il joue, et l'éditeur de jeu
affiche ses lignes Vidéo et Émulateur grises, avec ces valeurs, sous la titre *Enregistré dans l'émulateur*.
Pour revenir aux paramètres de l'éditeur de jeu, sélectionnez **Déverrouiller les paramètres** et confirmez :
cela supprime les paramètres que l'émulateur a enregistrés, et les lignes peuvent à nouveau être modifiées. Les
deux émulateurs, `pcsx-ab` et `pcsx-abnxt`, lisent et écrivent les mêmes paramètres enregistrés.

![L'éditeur de jeu](../images/en/game-editor.jpg)

### 3.7 Cartes mémoire et points de reprise

Chaque jeu PS1 a sa propre carte mémoire par défaut (conservée avec ses points de reprise dans `Games/!SaveStates/<dossier de jeu>/`).
**Cartes mémoire** dans le menu système gère les **ensembles partagés** - une carte que plusieurs jeux utilisent,
conservée dans `Games/!MemCards/` : en créer une (Square, avec le clavier à l'écran), la renommer (Cross), la
supprimer (Triangle). Un jeu est mis sur un ensemble avec *Changer la carte mémoire* dans son éditeur, ou depuis
son icône Carte mémoire.

L'**éditeur de carte mémoire** (l'icône Carte mémoire) affiche la carte du jeu et une deuxième carte côte à côte,
avec l'icône et le titre de chaque sauvegarde : copiez une sauvegarde entre les deux (Square), supprimez-en une
(Triangle), défragmentez une carte (Select). Start échange la carte à droite contre un autre ensemble.

![L'éditeur de carte mémoire](../images/en/memory-card-editor.jpg)

**Points de reprise** : quand vous quittez un jeu PS1 avec le bouton Reset de la console (ou le menu de l'émulateur
sur un Pi ou PC), AutoBleem conserve un point de reprise d'où vous étiez et l'offre sous l'icône **Reprendre** -
quatre emplacements, chacun avec une image du moment. Cross continue à partir de l'emplacement, Triangle le
supprime. Un jeu avec un point de reprise affiche une petite image sur son icône Reprendre.

### 3.8 Lancer des jeux, RetroArch et des applications

**Cross** lance le jeu sélectionné. Un jeu PS1 s'exécute dans l'émulateur PS1 choisi (section 3.5), plein écran,
jusqu'à ce que vous le quittiez - sur la console avec le bouton **Reset** avant (retour au lanceur avec un point
de reprise) ou **Power** (la console s'éteint) ; sur un Pi ou un PC via le menu de l'émulateur (Select + Start sur
la manette ou Échap sur un clavier). **Square** lance un jeu PS1 dans RetroArch à la place.

Un jeu **RetroArch** démarre dans RetroArch avec le cœur que le lanceur a choisi pour son système ; *Fermer le
contenu* ou *Quitter RetroArch* dans son menu revient au lanceur. L'élément RetroArch dans le menu système ouvre
le propre menu de RetroArch (XMB) sans rien de chargé, pour ses paramètres et ses propres listes de contenu.

Une **application** (l'ensemble Applications : les outils de console, et sur une console les applications
supplémentaires que le paquet de RetroArch apporte - Doom, Quake, Amiga, ...) affiche d'abord son manuel ; Cross
la lance, Circle revient.

![Le manuel d'une application avant son lancement](../images/en/app-start.jpg)

### 3.9 Ajouter des jeux

Les **jeux PS1** vont dans le dossier `Games`, **un dossier par jeu**, nommé d'après le jeu :

```
Games/
  Crash Bandicoot/            Crash Bandicoot.cue + Crash Bandicoot.bin
  Final Fantasy VII/          Final Fantasy VII (Disc 1).chd, (Disc 2).chd, (Disc 3).chd
  Platformers/                un dossier de jeux : son propre groupe dans le sélecteur de liste
    Klonoa/                   Klonoa.pbp
```

- Formats : `.cue` + `.bin` (ou `.img`), `.pbp`, `.chd` (aussi zstd), `.ecm` (décodé par le scan), `.iso`.
  Un jeu zippé fonctionne aussi : le processeur **Unzip** le décompresse avant le scan (section 3.13).
- Un jeu multi-disques est un dossier avec chaque disque dedans ; les dossiers nommés `Game (Disc 1)`, `Game (Disc 2)`
  ... sont fusionnés en un dossier `Game` par le scan.
- Les jeux directement dans `Games/` (fichiers libres) sont triés dans des dossiers par le scan.
- Une **jaquette** est un PNG à côté de l'image du jeu, nommé comme lui. Sans une, l'emboîtage vient des bases de
  données de jaquettes, ou - avec RetroArch installé - de l'ensemble de miniatures de libretro ; sur un Pi, un PC
  ou Windows une jaquette manquante est récupérée en ligne (Options → *Récupérer les jaquettes en ligne*).
- Le scan lit le numéro de série de chaque disque et prend le titre, l'éditeur, l'année, les joueurs et la région
  de la base de données PlayStation de RetroArch ou des bases de données de jaquettes. Changez n'importe quoi dans
  l'éditeur de jeu et cochez *Verrouiller les données* pour le conserver.

Les **autres systèmes** vont sous `RetroArch/roms/`, **un dossier par système, nommé comme les bases de données de
RetroArch sont** (le dossier est créé pour vous) : `Nintendo - Nintendo Entertainment System`, `Nintendo - Super
Nintendo Entertainment System`, `Sega - Mega Drive - Genesis`, `Nintendo - Game Boy Advance`, `FBNeo - Arcade Games`
(ou `Arcade`), ... Les ROM peuvent rester zippées. Sur un Pi, un PC ou Windows le scan les lit lui-même et écrit
les listes de jeux de RetroArch ; sur une clé de console, exécutez **UpdateRoms** sur le PC (chapitre 5).

Les **applications** vont sous `Apps/<nom>/` avec un `app.ini` (le nom, l'icône, ce qu'il faut exécuter) et une
`run.sh`.

Les **thèmes** vont sous `Themes/<nom>/` (`theme.json` et les images) - ou déposez le fichier zip du thème dans
`Themes/`.

### 3.10 Le BIOS PS1

Sur une **PlayStation Classic** l'émulateur utilise le propre BIOS de la console. Sur un **Raspberry Pi, un PC et
Windows** mettez votre propre BIOS PS1 dans `System/Bios/` : `romw.bin` (l'US/Européen SCPH-5501/5502) et
`romJP.bin` (le Japonais SCPH-5500). Les installateurs les remplissent à partir des paquets BIOS de RetroArch
sauf si vos propres fichiers y sont déjà. Sans eux l'émulateur s'exécute sur son BIOS HLE intégré, que beaucoup
de jeux tolèrent et certains non.

### 3.11 Mises à jour

- **Raspberry Pi, clé PC, Windows** : le lanceur vérifie le site au démarrage et une fois par jour (Options →
  *Mises à jour* est le canal ; *Mise à jour logicielle* dans le menu système vérifie maintenant). Quand il y a un
  AutoBleem ou RetroArch plus récent, il demande : *Mettre à jour maintenant* télécharge tout et réexécute
  l'installateur avec l'écran de progression du premier démarrage ; *Me le rappeler demain* et *Ignorer cette version*
  sont les autres réponses. Vos jeux et paramètres restent ; le lanceur scanne à nouveau une fois après une mise à jour.
- **PlayStation Classic** : exécutez un `AutoBleemInstaller.exe` plus récent sur la clé (section 2.1).

### 3.12 Extensions et le Store AutoBleem

Les **extensions** ajoutent leurs propres écrans au lanceur. Elles vivent dans `Extensions/<nom>/` sur la clé (sur
un Raspberry Pi sa partition de données, sur Windows le dossier de données) ; pour installer une, décompressez
son fichier zip là. **L2 + R2 → Extensions** les énumère : Cross en exécute une, Triangle la désactive ou la
réactive. Une extension qui a besoin du réseau ne démarre pas sans lui, et une qui a arrêté le lanceur est
désactivée - la liste le dit.

![La liste des extensions](../images/en/extensions.jpg)

Le **Store AutoBleem** est la première extension : applications et jeux à installer en un appui, sur chaque système
sur lequel AutoBleem s'exécute (une PlayStation Classic a besoin du WiFi du noyau AutoBleem). Ses quatre onglets,
L1 / R1 entre eux :

- **Applications** et **Jeux** : ce que les sources offrent, chacun avec son image, sa version, sa taille et le
  favicon de la source. Les éléments installés sont grisés. Cross installe (ou met à jour, ou réessaie après un
  échec), Triangle supprime ce que le Store a installé. L2 / R2 ou Gauche / Droite changent de page, **Select**
  affiche une source à la fois, **Start** recherche dans les titres. Les images d'élément sont cachées et peuvent
  être retentées si elles ne se chargent pas.
- **Téléchargements** : ce qui est en cours de téléchargement, en attente, échoué ou installé. La barre de
  progression se met à jour régulièrement. Les téléchargements se poursuivent en arrière-plan même après la
  fermeture du Store ; lancer un jeu ou éteindre les met seulement en pause, et un téléchargement arrêté reprend
  où il s'est arrêté. Un jeu installé apparaît sur l'étagère après le prochain scan, avec l'image du Store comme
  jaquette. Les téléchargements de plus de 2 Go fonctionnent sur toutes les plateformes, y compris les versions 32 bits.
- **Sources** : d'où viennent les listes - le propre catalogue d'AutoBleem, une liste TSV déposée dans
  `System/Extensions/store/sources/` et les adresses que vous ajoutez avec **Ajouter une URL source**. Chaque
  source affiche son favicon dans la liste. Cross sur une que vous avez ajoutée la renomme, change son adresse,
  bascule entre `http://` et `https://`, ou la supprime.

![L'onglet Applications du Store](../images/en/store-apps.jpg)

![Le menu d'une source](../images/en/store-source-menu.jpg)

Ce que le catalogue d'AutoBleem offre est aussi listé sur le site de téléchargement, `https://autobleem.retromenele.pl/store/`.
**Vous êtes responsable de ce que les sources que vous ajoutez contiennent.**

**Vos propres jeux sur votre réseau** : `abstored`, le serveur LAN du Store, sert un dossier de jeux PS1 au Store
sur le même réseau. Il s'exécute sur n'importe quelle machine Linux - un Raspberry Pi, un serveur domestique - et
lit seulement le dossier. Démarrez-le avec `abstored <dossier de jeux>`, ouvrez `http://<cette machine>:8124/`
dans un navigateur pour voir ce qu'il sert et les problèmes qu'il a trouvés, et ajoutez `http://<cette machine>:8124/store.tsv`
en tant que source. Les programmes prêts pour Linux et Windows sont sur la page du Store, dans son onglet
**Serveur LAN** ; le configurer en tant que service est `INSTALL-linux.md` (`ext_store/server/` dans la source).
**LAN Share** (section 5.2) met les jeux et disques d'un PC sur un tel serveur.

### 3.13 Processeurs de scanner

Les **processeurs de scanner** sont de petits programmes que chaque scan exécute avant de lire vos jeux. L'un peut
transformer un format qu'AutoBleem ne lit pas en un qu'il lit - un jeu zippé, par exemple - ou modifier les données
d'un jeu, comme un patch de traduction. Ils vivent dans `System/Processors/<nom>/` sur la clé (sur un Raspberry Pi
sa partition de données, sur Windows le dossier de données) ; pour installer un, décompressez son dossier là. Le
prochain scan l'exécute.

- **Unzip vient avec AutoBleem** : il décompresse les jeux PS1 zippés dans `Games/` avant que le scan les lise et les
  ROM zippées une à une (les jeux arcade restent zippés). La mise à jour d'AutoBleem le met à jour aussi et le laisse
  désactivé si vous l'aviez désactivé.
- Un processeur qui a déjà traité un jeu n'est pas réexécuté sur lui jusqu'à ce que le jeu change.
- Pendant qu'un processeur fonctionne, la bulle en haut à droite affiche ce qu'il fait ; un avertissement ou une
  erreur apparaît sur la ligne en dessous. `processors.log` dans le dossier des journaux a les détails.
- Lancer un jeu ou RetroArch arrête un processeur qui modifie les fichiers ; le prochain scan termine son travail.

**L2 + R2 → Processeurs de scanner** les affiche dans l'ordre qu'ils s'exécutent, un onglet pour les jeux PS1 et un
pour les ROM (L1 / R1). **Square** en prend un et Haut / Bas le déplace - l'ordre compte : un processeur qui
décompresse doit venir avant un qui corrige ce qui a été décompressé. **Cross** le bascule ou l'active,
**Triangle** le réexamine à chaque prochain scan, **Circle** revient et lance un scan si vous avez changé quelque
chose. Un processeur construit pour une autre machine reste sur la liste, grisé.

![Processeurs de scanner](../images/en/processors.jpg)

Écrire le vôtre : la page d'Unzip, `https://github.com/autobleem2/proc_unzip`, explique tout ce qu'un processeur
doit faire, et `tools/proc_check.py` dans la source d'AutoBleem en vérifie un avant de le partager.

<!-- pagebreak -->

## 4. Écrans

### 4.1 Gestionnaire de jeux

Les jeux PS1 sous forme de liste avec leurs dossiers et le jeu sélectionné avec sa jaquette et ses détails. Cross
ouvre l'éditeur de jeu, **Square supprime le jeu** (son dossier et, après une deuxième question, ses points de
reprise), Triangle supprime chaque PNG de jaquette à côté des jeux (le scan les reprend aux bases de données),
L2 / R2 page. L'espace libre du lecteur est en haut à droite. Le gestionnaire de jeux attend pendant qu'un scan
s'exécute.

![Le gestionnaire de jeux](../images/en/game-manager.jpg)

### 4.2 Informations matérielles

Les faits de la machine - système, matériel, stockage avec son espace libre, adresses réseau, les pilotes
d'affichage et audio, les manettes connectées - relus chaque seconde. Sur une PlayStation Classic avec le noyau
AutoBleem cet élément ouvre **PSC-Bios** à la place (chapitre 6).

Les deux premiers contrôleurs sont affichés comme Player 1 et Player 2 – les ports que l'émulateur PS1 leur attribue.
Tout contrôleur supplémentaire est affiché comme non utilisé par l'émulateur PS1. RetroArch attribue les contrôleurs
selon ses propres paramètres et peut les ordonner différemment. Quand un contrôleur est branché ou débranché, le lanceur
affiche brièvement quel pad est Player 1 et Player 2.

![Informations matérielles](../images/en/hardware-info.jpg)

### 4.3 Le guide des touches

Triangle sur l'étagère : chaque bouton de chaque écran sur une page. Quand un clavier USB est connecté ou a été
utilisé, une colonne Clavier affiche les touches à côté des boutons de la manette.

![Le guide des touches](../images/en/button-guide.jpg)

### 4.4 Le clavier à l'écran

Partout où du texte est saisi - un ensemble de carte mémoire, un titre de jeu, un mot de passe WiFi, l'adresse
d'une source - le même clavier, disposé comme celui d'un téléphone : des lettres, une page de symboles
(`/ \ : ? & = % @ #` et le reste qu'une adresse ou un mot de passe nécessite) et deux pages de lettres accentuées,
avec Maj, la touche de page, Espace, Retour arrière et Valider sur la rangée du bas. Les directions se déplacent,
Cross tape, Triangle supprime, Square est un espace, **L1** est Maj (deux fois pour les majuscules), **R1** la
page suivante, **L2 / R2** déplacent le curseur, Start confirme, Circle annule. Un clavier USB tape à tout moment :
Entrée confirme, Échap annule.

![Le clavier à l'écran](../images/en/keyboard.jpg)

<!-- pagebreak -->

## 5. Sur le PC

### 5.1 UpdateRoms - actualiser une clé de console

La PlayStation Classic n'a pas de réseau, donc les listes RetroArch et les jaquettes de sa clé sont créées sur le
PC : **UpdateRoms** fait sur le PC ce que le scan du lanceur fait sur un Pi, avec le réseau du PC et les chemins
de la console, donc la console démarre et trouve tout en place.

1. Copiez vos ROM sur la clé sous `RetroArch/roms/<système>/` (section 3.9). Les noms de dossiers doivent être
   les noms de base de données de RetroArch ; l'installateur crée les courants.
2. Démarrez **`UpdateRoms\UpdateRoms.exe` depuis la clé** (l'installateur l'a mis là). Il trouve la clé d'où elle
   est, affiche une ligne d'étape, une barre de progression et un journal, et :
   - télécharge le bundle de base de données de RetroArch quand la clé n'en a pas, et identifie chaque ROM à partir
     d'elle - un jeu que la base de données connaît obtient son nom correct ;
   - écrit une liste de jeux par système dans `RetroArch/bin/playlists/` avec les chemins de la console, en
     gardant tout ce que RetroArch lui-même y a ajouté ;
   - récupère les jaquettes de chaque ROM qui n'en a pas à partir des serveurs de miniatures de libretro dans
     `RetroArch/bin/thumbnails/`.
3. Retirez la clé en toute sécurité et remettez-la dans la console. L'onglet RetroArch du sélecteur de liste
   énumère chaque système qui a des jeux.

Réexécutez-le après chaque changement aux dossiers ROM ; un dossier auquel rien n'a changé est ignoré, donc une
réexécution est rapide. Le journal est `System/Logs/updateroms.log`. Une carte Raspberry Pi dans un lecteur de
cartes peut être actualisée de la même manière (`UpdateRoms.exe <lecteur> --target rpi`), bien qu'un Pi le fasse
lui-même s'il a un réseau.

### 5.2 LAN Share - vos jeux et disques sur le serveur sur votre réseau

**LAN Share** (`LanShare.exe`, sur la page du Store dans son onglet **Serveur LAN**) met vos jeux PS1 sur le
serveur du Store sur votre réseau domestique - un `abstored` sur un Raspberry Pi, un NAS ou un autre PC - et lit
un disque PS1 dans le lecteur CD/DVD du PC. Le Store sur la console, le Pi ou le PC l'installe ensuite de là.
Rien à installer ; les paramètres sont conservés dans `%LOCALAPPDATA%\AutoBleem LAN Share\`.

![La fenêtre LAN Share](../images/en/lanshare.jpg)

1. **Le serveur** : entrez son adresse (`http://<son adresse>:<port>`, comme le Store l'a) et appuyez sur
   **Connecter**. Ses jeux et tous les problèmes que son scan a trouvés sont énumérés à gauche. Pour mettre les
   jeux dessus, donnez l'un d'eux :
   - **Partage** - le dossier de jeux du serveur tel qu'il est partagé sur le réseau (Samba), par exemple
     `\\raspberrypi\games` : LAN Share y copie les jeux et demande au serveur de scanner. Le serveur lui-même reste
     en lecture seule.
   - **Jeton** - quand le serveur a été démarré avec `--allow-uploads` : son jeton (le serveur l'imprime au
     démarrage et le conserve dans `<state>/upload-token`). LAN Share télécharge via HTTP, et un téléchargement
     arrêté continue où il s'est arrêté.
2. **Jeux sur ce PC** : choisissez un dossier de jeux (un dossier par jeu), cochez des jeux et appuyez sur
   **Publier les jeux cochés**. **Sur le serveur** dit si le serveur a déjà un jeu (par son numéro de série, sinon
   par son titre) ; un tel jeu n'est jamais envoyé deux fois. **Cochez ceux non sur le serveur** coche les autres.
3. **Un disque** : insérez un disque PS1 dans le lecteur et appuyez sur **Lire un disque et le publier**. Le disque
   est lu en entier dans un `.bin` + `.cue` (et un `.sbi` pour un jeu LibCrypt, quand le lecteur donne le
   subchannel), nommé d'après son titre, vérifié par rapport au bon vidage connu (quand les bases de données sont
   choisies) et publié. Pour un jeu sur plusieurs disques, cochez **Le jeu a plus d'un disque** : LAN Share demande
   chaque disque suivant et les publie ensemble en tant qu'un jeu.
4. **Supprimer du serveur...** enlève les jeux sélectionnés du serveur. Rien n'est supprimé : chacun est déplacé dans
   un dossier `.removed` à côté des jeux du serveur, et le remettre le remet.

Les **bases de données** - le dossier de couvertures d'AutoBleem (`coversU/P/J.db`) et `Sony - PlayStation.rdb` de
RetroArch - donnent les titres et la vérification d'un disque lu ; les deux sont optionnels. **Partager aussi les
jeux sur ce PC avec le Store** (désactivé par défaut) sert le dossier sur ce PC au Store directement. La première
fois, Windows demande sur son firewall : autoriser les réseaux privés uniquement.

<!-- pagebreak -->

## 6. Les outils de console (PlayStation Classic)

Deux outils pour une clé PlayStation Classic. Les deux dessinent en thème et langue du lanceur et tous deux sont
pilotés par la manette - et, dans l'assistant de manette, par les boutons avant de la console. **PSC-Bios** est une
extension fournie avec le paquet de console : *Informations matérielles* dans le menu système l'ouvre, et c'est
dans la liste des extensions. **ABFlashKit** est une application dans l'ensemble Applications.

### 6.1 PSC-Bios

Une extension fournie avec le paquet de console, également disponible sur un Raspberry Pi et la clé PC. Elle
est ouverte depuis l'élément *Réseau & Contrôleurs* du menu système (ou de la liste des extensions). Quand cette
extension est installée mais désactivée, l'élément *Réseau & Contrôleurs* du menu rapide et du menu système
reste grisé avec une note "activez-le dans Extensions" - Cross y ouvre la liste des extensions.

L'écran d'ouverture affiche les faits de la machine : heure, fuseau horaire, adaptateurs réseau WiFi/Ethernet/
Bluetooth avec leurs adresses, et chaque contrôleur connecté avec s'il a une configuration. Les parties réseau
et Bluetooth ont besoin du noyau AutoBleem sur la console (section 6.2) ou des outils système sur un Raspberry Pi /
clé PC ; l'assistant de manette fonctionne sur tout système.

![PSC-Bios : le hub Réseau & Contrôleurs](../images/en/pscbios-main.jpg)

- **Select - Réseau WiFi** (noyau ou NetworkManager) : le nom du réseau (tapé ou choisi dans un scan), le mot
  de passe, le mode du pilote, et *Appliquer / Redémarrer le réseau*. Le fuseau horaire est également défini ici.
  L'adresse IP de la console s'affiche une fois connectée.
- **Square - Contrôleurs Bluetooth** : un scan pour les manettes Bluetooth (DualShock 4, etc.), pour appairer ou
  supprimer.
- **L1 - Appairage DualShock 3** : connexion USB uniquement pour le premier DualShock 3, via le plugin sixaxis
  du noyau.
- **R1 - Configuration de la manette** : l'assistant de configuration (ci-dessous).
- **Triangle - À propos**, **Circle - revenir** au lanceur.

**L'assistant de manette** affiche la manette connectée brute - chaque axe, bouton et chapeau sous forme de chiffres,
et une image DualShock qui s'illumine quand vous appuyez. Parce que la manette testée ne peut pas être fiable,
l'assistant est piloté par les **boutons avant de la console** : **RESET** passe à la manette suivante, **OPEN**
démarre la configuration (puis répond chaque question - appuyez sur le bouton allumé sur l'image, ou OPEN si la
manette n'a pas ce bouton), **POWER** annule ou quitte. Tenez Circle sur la manette pendant 2 secondes quitte
l'assistant (une barre se remplit et le hint du bas dit "Tenez 2 s : Quitter"). Pendant que la manette n'a pas de
configuration, tenir n'importe quel bouton 2 secondes le fait ("Tenez n'importe quel bouton 2 s : Quitter"). Un
appui court est configuré comme d'habitude. Sur un clavier, Échap / Espace / Entrée remplacent POWER / RESET / OPEN.
À la fin, la nouvelle configuration est ajoutée pour un test et OPEN la sauvegarde sous un nom de votre choix ; le
lanceur la charge à partir de maintenant.

![PSC-Bios : l'assistant de configuration de la manette](../images/en/pscbios-wizard.jpg)

### 6.2 ABFlashKit - le noyau AutoBleem

Le noyau AutoBleem est un remplacement optionnel du noyau Linux de la console : il apporte une horloge qui
fonctionne, des dongles USB WiFi et Bluetooth (pour PSC-Bios et des manettes Bluetooth) et le support des boutons
avant que l'émulateur utilise pour les points de reprise. ABFlashKit l'installe, fait d'abord une sauvegarde de la
console, et peut remettre la console au stock via la propre récupération de Sony.

> **Cet outil écrit dans la mémoire flash de la console.** Un flash qui est interrompu - l'alimentation coupée,
> la clé retirée - peut laisser la console incapable de démarrer, et l'installation d'un noyau personnalisé annule
> sa garantie. Gardez la console alimentée et la clé dedans jusqu'à ce qu'elle redémarre d'elle-même. ABFlashKit
> s'ouvre sur cet avertissement ; *Je comprends* continue, *Quitter* quitte.

![Le menu d'ABFlashKit](../images/en/abflashkit-menu.jpg)

- **Flash Kernel** : fait une sauvegarde de récupération des partitions de la console sur la clé (`LBOOT.EPB`)
  s'il n'en existe pas encore, la vérifie et vérifie l'image du noyau, écrit le noyau et les fichiers système
  d'AutoBleem, et redémarre. *Tout fait - quand l'écran devient noir remplacez le cordon d'alimentation* : tirez
  le cordon d'alimentation de la console et rebranchez-le.
- **Sauvegarde complète** : les quatre partitions à `LBOOT.EPB`, pour une restauration plus tard (la sauvegarde
  précédente est remplacée après une question).
- **Mode de restauration** : vérifie que la sauvegarde est du stock, définit le drapeau de récupération et redémarre
  dans la récupération de Sony, qui restaure la console à partir de `LBOOT.EPB` sur la clé - le chemin de retour au
  micrologiciel d'origine.

Une barre de progression sous chaque étape indique la progression de l'action. L'outil refuse d'écrire sur une
console qui exécute un autre micrologiciel personnalisé (BleemSync, Project Eris) : restaurez-la d'abord au stock.

<!-- pagebreak -->

## 7. Si quelque chose ne va pas

- **Journaux** : AutoBleem garde ses journaux en mémoire, donc la clé n'est pas écrite tout le temps - ils atteignent
  `System/Logs/` sur la clé, la carte ou le dossier de données seulement quand quelque chose ne va pas : un crash du
  lanceur, d'un jeu PS1 ou de RetroArch les enregistre dans `System/Logs/crash-<n>/` (les trois derniers sont gardés),
  et le lanceur le dit une fois quand il revient. Pour conserver chaque journal, activez *Options -> Diagnostic ->
  Conserver les journaux sur la clé* (à partir du prochain démarrage), ou créez un fichier vide `System/Logs/keep`
  sur un PC. Sur un Pi ou un PC, *Informations matérielles* affiche où sont les journaux et Square les enregistre
  dans `System/Logs/saved-<n>/`. Les fichiers : `autobleem.log` (le lanceur), `launch.log` et `pcsx.log` (le
  démarrage d'un jeu PS1 et la sortie de l'émulateur), `retroarch.log`, et - toujours sur la clé - `update.log`
  (une mise à jour en ligne) et `updateroms.log` (UpdateRoms).
- **Un jeu n'est pas sur l'étagère** : vérifiez la mise en page des dossiers (un dossier par jeu, les formats
  d'image de la section 3.9). Le *Gestionnaire de jeux* énumère les dossiers que le scan a refusés après les jeux,
  marqués *Non ajoutés*, avec la raison ; Square supprime un tel dossier. *Rescanner les jeux* dans le menu système
  réexécute le scan.
- **Pas de jaquettes** : les bases de données de jaquettes n'ont pas été installées (réexécutez l'installateur avec
  elles cochées), ou, pour les jeux RetroArch sur une console, UpdateRoms n'a pas été exécuté sur le PC.
- **Une manette ne fait rien ou a ses boutons mélangés** : l'assistant de manette de PSC-Bios (une console) la
  configure ; sur un Pi ou un PC la page Informations matérielles énumère ce que SDL voit.
- **La console affiche un écran noir après un jeu** : AutoBleem reconstruit sa fenêtre par lui-même (jusqu'à trois
  fois) ; s'il reste noir, maintenez le bouton Power et rallumez la console.
- **Raspberry Pi** : `Alt+F2` donne une invite de connexion sur la deuxième console ; SSH est activé à partir du
  premier démarrage. `sudo journalctl -u autobleem` affiche le service du lanceur ; `sudo systemctl restart autobleem`
  le redémarre. Un premier démarrage qui n'a pas pu terminer (pas de réseau) réessaie au prochain démarrage.
- **Windows** : `Échap` quitte le lanceur ; le dossier de données est celui choisi dans l'installation
  (`Documents\AutoBleem` par défaut), les journaux sont dans son `System\Logs`.

AutoBleem est un logiciel libre (GNU GPL v3 ou plus récent), sans garantie. Support et actualités : le serveur
Discord lié sur l'écran À propos, et https://autobleem.retromenele.pl/.

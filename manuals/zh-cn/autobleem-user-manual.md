# AutoBleem 2 用户手册

AutoBleem 2 是**PlayStation Classic**的游戏启动器 - 从第2个版本开始，也支持**树莓派(Raspberry Pi)**、**从USB闪存盘启动的PC**和**Windows**。它将您的PS1游戏显示为带有盒艺术和详细信息的书架式界面，在捆绑的PCSX模拟器中启动它们，如果安装了RetroArch，还可以玩其他系统的游戏。本手册涵盖在每个平台上的安装、日常使用和附带的工具。

> 每个平台的下载地址都在**https://autobleem.retromenele.pl/**。该页面按平台组织：每个平台的*Install*面板是您需要下载的内容；下面的*Build inputs*是安装程序自动获取的内容。

## 1. 您会获得什么

- **启动器** - 游戏盒艺术书架、游戏集合(PlayStation、RetroArch、应用)、游戏详情、系统菜单、选项、记忆卡和存档点工具。所有平台上的程序相同。
- **两个PS1模拟器** - `pcsx-abnxt`(当前版本，默认)和`pcsx-ab`(AutoBleem一直以来都内置的经典版本)。您可以在选项中选择其中一个；两者使用相同的设置和记忆卡。
- **RetroArch**(每个平台上都是可选的)用于其他系统：NES、SNES、Mega Drive、Game Boy、街机等。AutoBleem根据您复制的ROM构建RetroArch列表，并使用正确的内核启动每款游戏。
- **控制台工具**(仅PlayStation Classic)：*PSC-Bios*用于WiFi、时钟和手柄映射，*ABFlashKit*用于安装AutoBleem内核。
- **UpdateRoms** for Windows：刷新控制台闪存盘的RetroArch列表和盒艺术，因为控制台本身没有网络。

![启动器：游戏盒艺术书架、选中游戏的详情和按键提示](../images/en/launcher.jpg)

<!-- pagebreak -->

## 2. 安装

### 2.1 PlayStation Classic

您需要一台Windows PC、一个USB闪存盘(USB 2.0，8GB或更大；安装程序可以在您要求时格式化)和原始控制台。AutoBleem从闪存盘运行，不需要对控制台进行任何更改。对于原始控制台，闪存盘必须是**FAT32** - 其内核无法读取exFAT。只有安装了AutoBleem内核(ABFlashKit，第6章)的控制台才能从exFAT闪存盘启动，这消除了FAT32的4GB文件限制。

1. 从网站的PlayStation Classic面板下载**AutoBleemInstaller-<version>.zip**并将其解压到任何位置。它包含`AutoBleemInstaller.exe`和它安装的AutoBleem包。
2. 连接闪存盘并启动`AutoBleemInstaller.exe`。在顶部选择驱动器。勾选您想要的选项：
   - **格式化闪存盘** - 仅用于全新闪存盘(上面的所有内容都会被删除)。除非控制台有AutoBleem内核，否则选择FAT32。
   - **盖艺术数据库** - PS1库的盒艺术和详情(默认勾选；约300MB)。
   - **RetroArch** - RetroArch及其内核、额外应用程序(Doom、Quake、Amiga等)和libretro资源，用于其他系统的游戏。默认关闭；稍后可以再次运行安装程序来添加。
   - **BIOS文件** - RetroArch内核需要的BIOS文件(需要RetroArch)。
   - **示例游戏** - 一些免费的自制游戏，以便书架不是空的。
3. 按**Install**并等待。进度条和日志显示每一步；闪存盘最后被命名为`SONY`，并将`UpdateRoms`放在其上(见第5章)。
4. 安全弹出闪存盘，将其插入控制台的**第二个USB端口**(右侧，第2个玩家)并打开控制台电源。AutoBleem代替原始菜单启动。

**打开和关闭。**使用闪存盘时，控制台启动，其灯闪烁几秒钟(正在检测AutoBleem)，然后进入待机状态，然后才会显示任何内容 - 这是控制台自己的更新方式，AutoBleem由此得以运行。按一次**Power**，启动器出现。系统菜单中的*Power Off*或控制台的Power按钮将控制台置于**AutoBleem的待机状态**：先断开闪存盘的连接，然后灯变为**红色** - AutoBleem正常工作的标志 - 下一次按Power会在几秒钟内直接将启动器带回。**当灯为红色时可以拔出闪存盘**并将其放入PC，Windows不会要求检查它；在按Power之前将其插回。断开控制台的电源在下次启动时会经历启动待机。

要**更新**闪存盘，请在其上运行较新的安装程序：您的游戏、存档、设置和RetroArch内容保持不变；只有AutoBleem自己的文件被替换。使用AutoBleem 1.0或AutoBleem-NG制作的闪存盘会自动转换为新布局。

> 原始控制台没有时钟也没有网络：日期只在安装AutoBleem内核后显示(第6章)，RetroArch游戏的盒艺术来自PC上的UpdateRoms(第5章)。

游戏放在闪存盘的`Games`文件夹中，每个游戏一个文件夹 - 查看第3.9节了解布局。

### 2.2 树莓派

AutoBleem将Pi变成一个小型控制台：它直接启动到启动器中，没有桌面。网站上有两个现成的镜像 - 32位和64位 - 以及一个用于现有树莓派OS Lite的tarball。

| 型号 | 32位镜像 | 64位镜像 | 说明 |
|---|---|---|---|
| 树莓派 5 | 是 | 是 | |
| 树莓派 4 Model B、Pi 400 | 是 | 是 | |
| 树莓派 3 Model B / B+ / A+ | 是 | 是 | 适用于启动器和PS1 |
| 树莓派 Zero 2 W | 是 | 是 | 512MB RAM：PS1可运行，较重的RetroArch内核不行 |
| 树莓派 2 Model B | 是 | 仅v1.2 | 3D游戏运行缓慢 |
| 树莓派 1、Zero、Zero W | 否 | 否 | ARMv6 - 两个镜像都不支持 |

**32位镜像是PS1游戏推荐的** `pcsx-ab`的快速ARM动态编译器仅32位可用，所以64位构建运行PS1游戏较慢。64位镜像有更大的RetroArch内核集合。

**使用树莓派成像器：**

1. 安装树莓派成像器(raspberrypi.com/software)。在*Choose OS*中选择*Use custom*并选择您下载的`autobleem-<version>-rpi-armhf.img.xz`(32位)或`-arm64.img.xz`(64位) - 或在应用的设置中添加存储库URL`https://autobleem.retromenele.pl/rpi-imager/os_list.json`并从列表中选择AutoBleem。
2. 使用成像器的自定义屏幕(齿轮或*Next*后的问题)来设置**用户名和密码、WiFi网络和国家，并启用SSH**。AutoBleem在首次启动时需要网络。
3. 写入卡片，将其放入Pi(连接屏幕和键盘或手柄)，打开电源。

**首次启动**需要5到25分钟并在屏幕上显示执行内容。如果没有网络，它会要求您连接(WiFi列表、密码)，然后询问是否安装RetroArch(没有答案1分钟意味着是)，扩展系统分区，从其余部分制作`AUTOBLEEM`数据分区，安装RetroArch及其内核、BIOS包和示例游戏，然后重启进入启动器。

答案可以在首次启动前在卡片的引导分区上的**`autobleem.txt`**中预先提供(在任何PC上可编辑)：

| 关键字 | 默认值 | 意义 |
|---|---|---|
| `root_gib` | 8 | 系统分区的大小(单位GiB)；其余部分变成游戏分区。 |
| `hdmi_mode` | 1920x1080@60 | 整个启动过程中的屏幕模式(`1280x720@60`用于较旧的电视)。 |
| `retroarch` | (被询问) | `yes` / `no` - RetroArch和其他系统，或仅PS1。 |
| `thumbnails` | 无 | `boxarts`镜像整个盒艺术集供离线使用(~9000个文件)。 |
| `bios`、`downloads`、`samples` | 是 | 设置为`no`跳过BIOS包、所有下载或示例游戏。 |

**在现有树莓派OS Lite上**(Bookworm或Trixie)：将`autobleem-rpi.tar.gz`(或arm64版本)复制到Pi，解压并运行`sudo bash install.sh`。它会问相同的问题，通过在下次启动时缩小根来创建数据分区(`--shrink-root <GiB>`)，并在第一个控制台上放置启动器。

安装后，卡片的**`AUTOBLEEM`分区**(exFAT)是您要填充的：弹出卡片并在任何PC上打开，或通过网络复制(SSH已启用)。PS1游戏使用`Games/`，其他系统的ROM使用`RetroArch/roms/<system>/`，PS1 BIOS使用`System/Bios/`(第3.10节)，主题使用`Themes/`。

### 2.3 PC USB闪存盘

适用于任何可从USB启动的PC的相同设备 - 32位系统，所以旧机器也能工作：

1. 从PC面板下载`autobleem-<version>-pcusb-i386.img.xz`，使用树莓派成像器(*Use custom*)、balenaEtcher或Rufus(DD模式)将其写入8GB或更大的闪存盘。
2. 从闪存盘启动PC(您的PC的启动菜单按键 - F12、F8、Esc...)。BIOS和UEFI启动都支持；**Secure Boot必须关闭**。
3. 首次启动与Pi相同：如果没有网线则询问网络、RetroArch问题，然后安装 - 使用有线网络约八分钟 - 然后重启进入启动器。

闪存盘随后有一个`AUTOBLEEM`分区供您的游戏使用，在Windows 10(1903及更新版)中将闪存盘插入正在运行的PC时显示为第二个驱动器。`autobleem.txt`在第一个分区上，具有与Pi相同的键(没有`hdmi_mode` - PC使用屏幕的原生模式)。

### 2.4 Windows

AutoBleem作为Windows程序：全屏、模拟器和RetroArch启动为程序。

1. 下载**AutoBleemSetup-<version>.exe**并运行它。它按用户安装，不需要管理员权限：程序在`%LOCALAPPDATA%\Programs\AutoBleem`中，数据(游戏、设置、主题、RetroArch)在您选择的文件夹中 - 默认为`Documents\AutoBleem`。
2. 勾选组件 - 盖艺术数据库、RetroArch(官方Windows构建和其内核)、BIOS文件、示例游戏 - 并让设置助手下载它们。
3. 从开始菜单或桌面启动AutoBleem。在PC上，键盘用作手柄(第3.2节)。

在其上运行较新的设置会更新程序并保持数据文件夹。启动器还每天检查网站一次，当有新版本时提供更新(第3.11节)。

<!-- pagebreak -->

## 3. 使用AutoBleem

### 3.1 启动器

启动器在书架上打开：当前游戏集的盒艺术、中间选中的一个及其详情(发布者、年份、序列号、地区、玩家数、上次游玩时间)和播放按钮。底部的条形显示按钮的作用。游戏文件夹扫描在每次启动时在后台运行；运行时，右上角的气泡显示其进度，新游戏在被找到时出现在书架上。

![游戏集选择器：三个选项卡和当前选项卡的分组及其游戏数量](../images/en/set-picker.jpg)

### 3.2 控制

| 按钮 | 在书架上 |
|---|---|
| 左 / 右 | 上一款 / 下一款游戏。长按继续滚动。 |
| L1 / R1 | 跳转到标题的前一个 / 下一个首字母。 |
| Cross | 启动选定的游戏(PS1游戏在PS1模拟器中；RetroArch游戏在其内核中；应用在读我文件之后)。 |
| Square | 在RetroArch中启动选定的PS1游戏。 |
| Triangle | 按键指南。 |
| Start | 当前游戏集中的随机游戏。 |
| Select | 游戏集选择器：PlayStation / RetroArch / 应用选项卡(L1 / R1)、选项卡的分组(上 / 下、L2 / R2分页)、Cross选择。 |
| 下 | 打开游戏下的图标行(设置、游戏、记忆卡、恢复)。上关闭。 |
| L2 + R2 | 系统菜单(第3.4节)。 |

**使用键盘**(没有手柄的PC，或控制台、Pi或PC闪存盘上的USB键盘)，按键代替：**箭头键** = d-pad、**Enter** = Cross、**Esc或Backspace** = Circle、**Tab** = Triangle、**Space** = Square、**F1 / F2** = Select / Start、**Page Up / Page Down** = L1 / R1、**Home / End** = L2 / R2、**F10** = 系统菜单。在开发机器上Esc关闭程序，Space是Start。

在每个列表和菜单中：上 / 下移动、**L2 / R2转页**、L1 / R1跳转到首 / 末行、**Cross选择、Circle返回**。带有设置的屏幕在您用Circle离开时保存设置。

![选定游戏下的图标行](../images/en/launcher-icons.jpg)

### 3.3 游戏集合

**Select**打开游戏集选择器。PlayStation选项卡列出*所有游戏*、*内置游戏*(PlayStation Classic上控制台的内置20款)、您在`Games/`下创建的每个文件夹(子文件夹中的游戏属于该组)、*收藏游戏*、*游戏历史*以及任何被标记为此的游戏的*光枪游戏*。RetroArch选项卡列出每个有游戏的系统的一组，加上RetroArch自己的收藏和历史。应用选项卡按类型对应用进行分组：*所有应用*，然后*游戏*、*模拟器*、*工具*、*媒体*和*其他*(类别在每个应用的`app.ini`文件中设置)。每行显示其包含的项目数；一个没有项目的分组在空书架上打开，图标行仅显示设置。

### 3.4 快速菜单

在启动器中**上**，或图标行中的**齿轮图标**(设置 / 游戏 / 记忆卡 / 恢复所在的地方)：从旋转木马快速到达的操作的快速菜单。简短列表：*重新扫描游戏*(立即开始扫描)、*商店*(AutoBleem商店下载扩展)、*网络和控制器*(仅在已安装扩展提供`network`条目的地方 - PlayStation Classic上的PSC-Bios、Pi和PC闪存盘：WiFi、蓝牙配对、手柄映射向导 - 见第6章；当该扩展被禁用时显示灰色并提示"在扩展中启用" - Cross打开扩展列表)和*系统菜单...*(下面的完整菜单)。上 / 下移动(循环)，Cross选择，Circle返回。这里没有独特的东西 - 每一项都在系统菜单中。

### 3.5 系统菜单

**L2 + R2**(一起，任意顺序)在书架上打开系统菜单。菜单分为几个部分：

| 部分 | 项目 | 作用 |
|---|---|---|
| (顶部) | 重新扫描游戏 | 立即查找新的、更改的或删除的游戏(扫描也会自己监视该文件夹)。 |
| | 扩展 | 闪存盘上的扩展 - AutoBleem商店和其他(第3.12节)。 |
| **库** | 游戏管理器 | PS1游戏作为列表及其文件夹：删除游戏、刷新盖艺术。在扫描运行时被禁用。 |
| | 记忆卡 | 您的记忆卡集合(第3.7节)。 |
| | 扫描仪处理器 | 每次扫描首先运行的程序 - 其顺序、打开或关闭(第3.13节)。在扫描运行时被禁用。 |
| **系统** | 选项 | AutoBleem的设置(第3.6节)。 |
| | 网络和控制器 | 仅在已安装扩展提供`network`条目的地方(其`extension.ini`中有`Provides=network` - PlayStation Classic上的PSC-Bios、Pi和PC闪存盘) - WiFi、蓝牙控制器配对、DualShock 3设置以及手柄映射向导 - 见第6章。当该扩展已安装但禁用时，此项保持灰色并带有注释"在扩展中启用" - Cross打开扩展列表。 |
| | 硬件信息 | 机器的事实：系统、CPU、存储、网络接口、时区、显示、手柄及其映射。在具有AutoBleem内核的PlayStation Classic上打开PSC-Bios(第6章)；在其他机器上显示此信息页面。 |
| | 软件更新 | (树莓派和PC)立即检查网站是否有较新的AutoBleem或RetroArch。 |
| | 关于 | 信用和许可。 |
| **离开** | RetroArch | 离开启动器进入RetroArch自己的菜单。关闭RetroArch会返回。 |
| | 关机 | 确认后：在控制台上AutoBleem待机 - 闪存盘断开、灯变红、Power将启动器带回(第2.1节)；在Pi或PC上机器关闭。 |

![系统菜单](../images/en/system-menu.jpg)

### 3.6 选项

设置分组；上 / 下在组间移动，左 / 右改变值，Circle离开并保存。每次更改立即应用。

| 分组 / 设置 | 作用 |
|---|---|
| **界面**：AutoBleem主题 | 外观。主题在`Themes/`中；放在那里的主题zip在下次访问时解压。AutoBleem附带的主题随每次更新刷新 - 要自定义一个，首先复制它并使用新名称。 |
| 盖艺术样式 | 围绕PS1盖艺术绘制的珠宝盒框架。 |
| 语言 | 启动器的语言，立即应用(17种语言)。 |
| 使用主题中的字体 / 字体 | 经典屏幕的字体：主题的，或来自`resources/fonts`、`RetroArch/fonts`或主题文件夹的任何`.ttf`/`.otf`。 |
| 显示超时 | "显示：..."通知保留多长时间，单位秒(0 =永远)。 |
| **声音**：音乐、背景音乐 | 在启动器下播放哪个曲目(主题的或用户自己的来自`resources/music`)，以及是否播放。 |
| **模拟**：PS1模拟器 | `pcsx-abnxt`(默认：带有AutoBleem附加功能的当前PCSX-ReARMed)或`pcsx-ab`(经典)。一个保存的恢复点在另一个中继续，除非游戏在没有BIOS文件的情况下运行。 |
| 宽屏 | PS1模拟器为每款游戏的图像形状。 |
| 使用RA播放所有PSX游戏 | 每款PS1游戏都在RetroArch的PS1内核中启动。 |
| 更新RA配置 | AutoBleem在从那里启动游戏时将其设置写入RetroArch的配置。 |
| **库**：显示内置游戏 | PlayStation列表中控制台的内置游戏(仅PlayStation Classic)。 |
| 在线获取盖艺术 | 扫描从libretro服务器获取缺失的封面(树莓派、PC、Windows)。 |
| **更新** | (树莓派、PC、Windows)`stable`、`latest`(预发布版本也包含)或`off`。 |

![选项分组显示](../images/en/options.jpg)

### 3.7 游戏的设置

选中游戏后，**下**打开其图标行：**设置**(上面的选项)、**游戏**(游戏自己的设置)、**记忆卡**(其记忆卡)和**恢复**(其存档点)。Cross在光标下打开当前的。

**游戏编辑器**在右边显示游戏的详情，在左边显示其设置，分为三组：

- **游戏**：*收藏*(在收藏游戏分组中)、*光枪游戏*(光枪游戏 - 它加入光枪分组并始终在RetroArch中运行，其PS1内核有GunCon)、*使用RA播放*(该游戏在RetroArch中运行)、*锁定数据*(扫描器按照您设置的方式保持游戏的标题、序列号和光盘列表)。
- **视频**：高分辨率、扫描线及其等级、帧跳、GPU插件和*过滤器* - 图像缩放方式：关闭(纯像素)、线性(平滑)或锐利(清晰像素无闪烁；仅`pcsx-abnxt` - 经典`pcsx-ab`和RetroArch显示为关闭)。
- **模拟器**：SpeedHack、CPU时钟、SPU插值、启动徽标(关闭跳过BIOS外壳 - 用于自定义徽标破坏启动的自制光盘)以及使用`pcsx-abnxt`的*平滑*过滤和*Sony黑客*切换。

Triangle重命名游戏，Square更改其记忆卡，Start分享新卡。Circle保存并离开。

**在模拟器中保存的设置。** 模拟器自己的菜单有*为此游戏保存设置*。一旦游戏在那里保存了设置，这些就是它播放时使用的，游戏编辑器在标题*在模拟器中保存*下显示其视频和模拟器行灰显，带这些值。要回到游戏编辑器的设置，选择**解锁设置**并确认：这删除模拟器保存的设置，这些行可以再次更改。两个模拟器`pcsx-ab`和`pcsx-abnxt`读写相同的保存设置。

![游戏编辑器](../images/en/game-editor.jpg)

### 3.7 记忆卡和存档点

每款PS1游戏默认有其自己的记忆卡(与其存档点一起保存在`Games/!SaveStates/<game folder>/`)。系统菜单中的**记忆卡**管理**共享集** - 多个游戏使用、保存在`Games/!MemCards/`中的卡：创建一个(Square，使用屏幕键盘)、重命名(Cross)、删除(Triangle)。游戏通过其编辑器中的*更改记忆卡*或从其记忆卡图标放在集上。

**记忆卡编辑器**(记忆卡图标)并排显示游戏的卡和第二个卡，每个存档的图标和标题：在两者之间复制存档(Square)、删除一个(Triangle)、碎片整理卡(Select)。Start将右侧的卡换为另一集。

![记忆卡编辑器](../images/en/memory-card-editor.jpg)

**恢复点**：当您通过控制台的Reset按钮(或Pi或PC上模拟器的菜单)离开PS1游戏时，AutoBleem保存您所在位置的状态并在**恢复**图标下提供 - 四个槽位，每个都有该时刻的图像。Cross从槽位继续，Triangle删除它。带有恢复点的游戏在其恢复图标上显示一个小图像。

### 3.8 启动游戏、RetroArch和应用

**Cross**启动选定的游戏。PS1游戏在选定的PS1模拟器中运行(第3.5节)、全屏、直到您离开它 - 在控制台上用前面的**Reset**按钮(返回启动器及恢复点)或**Power**(控制台关闭)；在Pi或PC上通过模拟器的游戏菜单(手柄上Select + Start或键盘上Esc)。**Square**在RetroArch中启动PS1游戏。

**RetroArch**游戏在RetroArch中启动，启动器为其系统选择的内核；其菜单中的*关闭内容*或*退出RetroArch*返回启动器。系统菜单中的RetroArch项打开RetroArch自己的菜单(XMB)，什么都没加载，用于其设置和其自己的内容列表。

**应用**(应用集：控制台工具，以及在控制台上RetroArch包提供的额外应用 - Doom、Quake、Amiga等)首先显示其读我文件；Cross启动它，Circle返回。

![应用在启动前的读我](../images/en/app-start.jpg)

### 3.9 添加游戏

**PS1游戏**进入`Games`文件夹，**每款游戏一个文件夹**，以游戏命名：

```
Games/
  Crash Bandicoot/            Crash Bandicoot.cue + Crash Bandicoot.bin
  Final Fantasy VII/          Final Fantasy VII (Disc 1).chd, (Disc 2).chd, (Disc 3).chd
  Platformers/                游戏文件夹：游戏选择器中的自有分组
    Klonoa/                   Klonoa.pbp
```

- 格式：`.cue` + `.bin`(或`.img`)、`.pbp`、`.chd`(也包括zstd)、`.ecm`(由扫描解码)、`.iso`。压缩的游戏也能工作：**解压**处理器在扫描前解压它(第3.13节)。
- 多光盘游戏是一个包含每个光盘的文件夹；名为`Game (Disc 1)`、`Game (Disc 2)`...的文件夹由扫描合并为一个`Game`文件夹。
- 直接放入`Games/`的游戏(松散文件)由扫描排序为文件夹。
- **盖艺术**是游戏图像旁的PNG，同名。没有的话，盒艺术来自盖艺术数据库，或 - 安装了RetroArch - 来自libretro的缩略图集；在Pi、PC或Windows上缺失的是在线获取(选项 → *在线获取盖艺术*)。
- 扫描读取每个光盘的序列号，并从RetroArch的PlayStation数据库或盖艺术数据库获取标题、发布者、年份、玩家和地区。在游戏编辑器中更改任何内容并勾选*锁定数据*来保持它。

**其他系统**进入`RetroArch/roms/`，**每个系统一个文件夹，按RetroArch的数据库命名**(文件夹为您创建)：`Nintendo - Nintendo Entertainment System`、`Nintendo - Super Nintendo Entertainment System`、`Sega - Mega Drive - Genesis`、`Nintendo - Game Boy Advance`、`FBNeo - Arcade Games`(或`Arcade`)... ROM可以保持压缩。在Pi、PC或Windows上扫描自己读取它们并写入RetroArch的播放列表；在控制台闪存盘上，在PC上运行**UpdateRoms**(第5章)。

**应用**进入`Apps/<name>/`，带有`app.ini`(名称、图标、运行内容)和`run.sh`。

**主题**进入`Themes/<name>/`(`theme.json`和图像) - 或将主题zip拖放到`Themes/`中。

### 3.10 PS1 BIOS

在**PlayStation Classic**上，模拟器使用控制台自己的BIOS。在**树莓派、PC和Windows**上，将您自己的PS1 BIOS放入`System/Bios/`：`romw.bin`(美国/欧洲SCPH-5501/5502)和`romJP.bin`(日本SCPH-5500)。除非您自己的文件已经在那里，否则安装程序从RetroArch BIOS包中填充它们。没有它们，模拟器在其内置HLE BIOS上运行，许多游戏容忍并且某些则不容忍。

### 3.11 更新

- **树莓派、PC闪存盘、Windows**：启动器在启动时检查网站并每天一次(选项 → *更新*是频道；系统菜单中的*软件更新*立即检查)。当有较新的AutoBleem或RetroArch时它会询问：*立即更新*下载所有内容并使用首次启动进度屏幕重新运行安装程序；*明天提醒我*和*跳过此版本*是其他答案。您的游戏和设置保持；启动器在更新后重新扫描一次。
- **PlayStation Classic**：在闪存盘上运行较新的`AutoBleemInstaller.exe`(第2.1节)。

### 3.12 扩展和AutoBleem商店

**扩展**将其自己的屏幕添加到启动器。它们在闪存盘上的`Extensions/<name>/`中(在树莓派上的数据分区、Windows上的数据文件夹)；要安装一个，在那里解压其zip。**L2 + R2 → 扩展**列出它们：Cross运行一个，Triangle关闭或再次打开它。需要网络的扩展不能在没有网络的情况下启动，停止启动器的扩展被关闭 - 列表说明。

![扩展列表](../images/en/extensions.jpg)

**AutoBleem商店**是第一个扩展：应用和游戏，仅按一次就能安装，在AutoBleem运行的每个系统上(PlayStation Classic需要AutoBleem内核的WiFi)。其四个选项卡，L1 / R1在它们之间：

- **应用**和**游戏**：源提供的内容，每个都带其图像、版本、大小和源favicon。已安装的项目显示为灰色。Cross安装(或更新或在失败后重试)，Triangle删除商店安装的内容。L2 / R2或左 / 右转页，**Select**一次显示一个源，**Start**搜索标题。项目图像被缓存并在加载失败时可以重试。
- **下载**：正在下载、等待、失败或已安装的内容。进度条稳定更新。下载在后台继续，即使您离开商店；启动游戏或关机仅暂停它们，已停止的下载从停止处继续。安装的游戏在下次扫描后出现在书架上，以商店的图像作为其盖艺术。所有平台上都支持超过2GB的下载，包括32位构建。
- **源**：列表来自哪里 - AutoBleem自己的目录、删除到`System/Extensions/store/sources/`的TSV列表以及使用**添加源URL**添加的地址。每个源在列表中显示其favicon。Cross在您添加的源上重命名、改变其地址、在`http://`和`https://`之间切换或删除它。

![商店的应用选项卡](../images/en/store-apps.jpg)

![源的菜单](../images/en/store-source-menu.jpg)

AutoBleem的目录提供的也在下载站点列出，`https://autobleem.retromenele.pl/store/`。**您对您添加的源包含的内容负责。**

**您网络上的您自己的游戏**：`abstored`，商店的LAN服务器，在同一网络上向商店提供PS1游戏文件夹。它在任何Linux机器上运行 - 树莓派、家用服务器 - 并仅读取文件夹。使用`abstored <games folder>`启动它，在浏览器中打开`http://<that machine>:8124/`查看它提供的内容以及找到的任何问题，添加`http://<that machine>:8124/store.tsv`作为源。用于Linux和Windows的现成程序在商店的页面上，在其**LAN服务器**选项卡中；将其设置为服务是`INSTALL-linux.md`(源中的`ext_store/server/`)。**LAN分享**(第5.2节)从PC将游戏和光盘放在这样的服务器上。

### 3.13 扫描仪处理器

**扫描仪处理器**是每次扫描在读取您的游戏前运行的小程序。一个可以将AutoBleem不读的格式转换为它读的格式 - 例如压缩游戏 - 或改变游戏的数据，例如翻译补丁。它们在闪存盘上的`System/Processors/<name>/`中(在树莓派上的数据分区、Windows上的数据文件夹)；要安装一个，在那里解压其文件夹。下次扫描运行它。

- **解压包含在AutoBleem中**：它在扫描读取之前解压`Games/`中的压缩PS1游戏，一次一个地解压压缩ROM(街机集保持压缩)。更新AutoBleem也会更新它，如果您关闭它会保持关闭状态。
- 已处理游戏的处理器在游戏更改之前不再在其上运行。
- 处理器工作时，右上角的气泡显示其在做什么；警告或失败出现在其下面的行上。日志文件夹中的`processors.log`有详情。
- 启动游戏或RetroArch停止改变文件的处理器；下次扫描完成其工作。

**L2 + R2 → 扫描仪处理器**按其运行顺序显示它们，PS1游戏的一个选项卡，ROM的一个(L1 / R1)。**Square**捡起处理器并上 / 下移动它 - 顺序重要：解压的处理器必须来到补丁所解压内容的一个之前。**Cross**关闭或打开一个，**Triangle**让它在下次扫描时查看每款游戏，**Circle**返回并在您改变某些内容时启动扫描。为另一台机器构建的处理器保持在列表中，灰显。

![扫描仪处理器](../images/en/processors.jpg)

编写您自己的：解压的页面`https://github.com/autobleem2/proc_unzip`解释处理器必须做的一切，以及AutoBleem源中的`tools/proc_check.py`在您分享前检查一个。

<!-- pagebreak -->

## 4. 屏幕

### 4.1 游戏管理器

PS1游戏作为列表及其文件夹和选定游戏的盖艺术和详情。Cross打开游戏编辑器，**Square删除游戏**(其文件夹及在第二个问题后其存档点)，Triangle删除游戏旁边的每个盖艺术PNG(扫描再次从数据库获取它们)，L2 / R2分页。驱动器的空闲空间在右上角。游戏管理器在扫描运行时等待。

![游戏管理器](../images/en/game-manager.jpg)

### 4.2 硬件信息

机器的事实 - 系统、硬件、带其空闲空间的存储、网络地址、显示和音频驱动、连接的手柄 - 每秒重新读取。在具有AutoBleem内核的PlayStation Classic上此项打开**PSC-Bios**(第6章)；在其他机器上显示此信息页面。

前两个手柄分别显示为Player 1和Player 2 — PS1模拟器分配给它们的端口。任何额外的手柄显示为PS1模拟器未使用。RetroArch根据自己的设置分配手柄,可能会以不同的顺序排列它们。当手柄连接或断开连接时,启动器会简要显示哪个手柄是Player 1和Player 2。

![硬件信息](../images/en/hardware-info.jpg)

### 4.3 按键指南

书架上的Triangle：每个屏幕的每个按钮在一页上。当USB键盘连接或已使用时，键盘列显示键盘键与手柄按钮一起。

![按键指南](../images/en/button-guide.jpg)

### 4.4 屏幕键盘

任何输入文本的地方 - 记忆卡集、游戏标题、WiFi密码、源地址 - 相同的键盘，像电话那样布局：字母、符号页(`/ \ : ? & = % @ #`以及地址或密码需要的其余部分)和两页带重音字母，Shift、页键、Space、Backspace和底行上的确认。方向移动，Cross输入，Triangle删除，Square是空格，**L1**是Shift(两次是大写锁定)，**R1**下一页，**L2 / R2**移动光标，Start确认，Circle取消。USB键盘任何时间输入：Enter确认，Esc取消。

![屏幕键盘](../images/en/keyboard.jpg)

<!-- pagebreak -->

## 5. 在PC上

### 5.1 UpdateRoms - 刷新控制台闪存盘

PlayStation Classic没有网络，所以它闪存盘的RetroArch列表和盒艺术在PC上制作：**UpdateRoms**在PC上做启动器的扫描在Pi上做的，具有PC的网络和控制台的路径，所以控制台启动并在原位找到所有内容。

1. 复制您的ROM到闪存盘下的`RetroArch/roms/<system>/`(第3.9节)。文件夹名称必须是RetroArch的数据库名称；安装程序制作常见的。
2. 从闪存盘启动**`UpdateRoms\UpdateRoms.exe`**(安装程序放在那里)。它从其所在的地方找到闪存盘、显示阶段行、进度条和日志，并：
   - 当闪存盘没有的时候下载RetroArch的数据库包，并通过它识别每个ROM - 数据库知道的游戏获得其适当名称；
   - 写一个播放列表到`RetroArch/bin/playlists/`中，带控制台的路径，保持RetroArch自己在那里添加的任何内容；
   - 从libretro的缩略图服务器获取没有的每个ROM的盒艺术到`RetroArch/bin/thumbnails/`。
3. 安全弹出闪存盘并将其放回控制台。游戏选择器的RetroArch选项卡列出每个有游戏的系统。

在ROM文件夹每次更改后再次运行它；一个没有更改的文件夹被跳过，所以重新运行是快的。日志是`System/Logs/updateroms.log`。卡读卡器中的树莓派卡可以用相同方式刷新(`UpdateRoms.exe <drive> --target rpi`)，不过Pi有网络时自己做。

### 5.2 LAN分享 - 您的游戏和光盘在您网络上的服务器上

**LAN分享**(`LanShare.exe`，在商店的页面上在其**LAN服务器**选项卡中)将您的PS1游戏放在您家庭网络上的商店服务器上 - 树莓派上的`abstored`、NAS或另一台PC - 并读取PC的CD/DVD驱动器中的PS1光盘。您的控制台、Pi或PC上的商店随后从那里安装它们。什么都不安装；设置保持在`%LOCALAPPDATA%\AutoBleem LAN Share\`。

![LAN分享窗口](../images/en/lanshare.jpg)

1. **服务器**：输入其地址(`http://<its address>:<port>`，如商店有的)并按**连接**。其游戏和找到的任何问题在左侧列出。要将游戏放在其上，给出其中之一：
   - **分享** - 服务器的游戏文件夹如网络上共享的(Samba)，例如`\\raspberrypi\games`：LAN分享复制游戏到那里并要求服务器扫描。服务器本身保持只读。
   - **令牌** - 当服务器启动时带`--allow-uploads`：其令牌(服务器在启动时打印并保持在`<state>/upload-token`)。LAN分享通过HTTP上传，已停止的上传从停止处继续。
2. **这台PC上的游戏**：选择游戏文件夹(每个游戏一个文件夹)、勾选游戏并按**发布勾选的游戏**。**在服务器上**说服务器是否已有游戏(通过其序列号，否则通过其标题)；这样的游戏从不发送两次。**勾选那些不在服务器上的**勾选其余的。
3. **光盘**：将PS1光盘放在驱动器中并按**读光盘并发布它**。光盘全部读入`.bin` + `.cue`(对于LibCrypt游戏，当驱动器给出子通道时还有`.sbi`)，以其标题命名、针对已知良好转储检查(当选择数据库时)并发布。用于多个光盘上的游戏勾选**游戏在多个光盘上**：LAN分享请求每个下一光盘并作为一个游戏一起发布它们。
4. **从服务器删除...**取出选定的游戏从服务器。什么都不删除：每个被移到服务器游戏旁的`.removed`文件夹，移回把它放回。

**数据库** - AutoBleem的覆盖文件夹(`coversU/P/J.db`)和RetroArch的`Sony - PlayStation.rdb` - 给出标题和读光盘的检查；两者都是可选的。**也与商店分享这台PC上的游戏**(默认关闭)直接向商店服务此PC上的文件夹。第一次Windows会询问其防火墙：仅允许专用网络。

<!-- pagebreak -->

## 6. 控制台工具(PlayStation Classic)

PlayStation Classic闪存盘的两个工具。两者用启动器的主题和语言绘制，两者都由手柄驱动 - 以及在手柄映射向导中由控制台的前按钮。**PSC-Bios**是随控制台包提供的扩展：系统菜单中的*硬件信息*打开它，它在扩展列表中。**ABFlashKit**是应用集中的应用。

### 6.1 PSC-Bios

随控制台包提供的扩展，也在树莓派和PC闪存盘上可用。它从系统菜单的*网络和控制器*项打开(或从扩展列表)。当该扩展已安装但被禁用时，快速菜单和系统菜单中的*网络和控制器*项显示为灰色并注有"在扩展中启用" - Cross那里打开在它的扩展列表。

打开屏幕显示机器事实：时间、时区、带其地址的WiFi/以太网/蓝牙网络适配器，以及每个连接的控制器是否有按钮映射。网络和蓝牙部分需要控制台上的AutoBleem内核(第6.2节)或树莓派 / PC闪存盘上的系统工具；手柄映射向导在任何系统上工作。

![PSC-Bios：网络和控制器中心](../images/en/pscbios-main.jpg)

- **Select - WiFi网络**(内核或NetworkManager)：网络名称(输入或从扫描挑选)、密码、驱动模式和*应用 / 重启网络*。时区也在这里设置。连接后显示控制台的IP地址。
- **Square - 蓝牙控制器**：扫描蓝牙手柄(DualShock 4等)以配对或删除。
- **L1 - DualShock 3配对**：通过内核的sixaxis插件的第一个DualShock 3的仅USB连接。
- **R1 - 控制器映射**：映射向导(下面)。
- **Triangle - 关于**，**Circle - 返回**启动器。

**手柄映射向导**显示连接的手柄原始 - 每个轴、按钮和hat作为数字，以及一个当您按下时点亮的DualShock图像。因为测试下的手柄不能被信任，向导由控制台的**前按钮**驱动：**RESET**切换到下一个手柄，**OPEN**启动映射(随后回答每个问题 - 按图像上点亮的按钮，或当手柄没有那个按钮时按OPEN)，**POWER**取消或离开。在手柄上长按Circle 2秒钟离开向导(一个条填充并页脚提示说"保持2秒：退出")。当手柄还没有映射时，长按任何按钮2秒钟做它("保持任何按钮2秒：退出")。短按照常规映射。在键盘上，Esc / Space / Enter代表POWER / RESET / OPEN。最后新映射为测试添加，OPEN用您选择的名称保存它；启动器从此加载它。

![PSC-Bios：控制器映射向导](../images/en/pscbios-wizard.jpg)

### 6.2 ABFlashKit - AutoBleem内核

AutoBleem内核是控制台Linux内核的可选替代品：它带一个工作时钟、USB WiFi和蓝牙插件(用于PSC-Bios和蓝牙手柄)以及模拟器为恢复点使用的前按钮支持。ABFlashKit安装它，首先备份控制台，并可以通过Sony自己的恢复将控制台恢复到库存。

> **此工具写入控制台的闪存。** 被中断的闪存 - 电源切断、闪存盘拔出 - 可能使控制台无法启动，安装自定义内核使其保修无效。保持控制台通电并闪存盘插入直到它自己重启。ABFlashKit在此警告上打开；*我理解*继续，*退出*离开。

![ABFlashKit的菜单](../images/en/abflashkit-menu.jpg)

- **Flash内核**：在闪存盘上制作控制台分区的恢复备份(`LBOOT.EPB`)如果还没有、检查它和内核映像、写内核和AutoBleem系统文件、并重启。*完成 - 屏幕变黑时更换电源线*：拔出控制台的电源并插回。
- **完整备份**：所有四个分区到`LBOOT.EPB`用于稍后恢复(之前的备份在问题后被覆盖)。
- **恢复模式**：检查备份是库存的、设置恢复标志并重启进入Sony的恢复，它从闪存盘上的`LBOOT.EPB`恢复控制台 - 返回库存固件的方式。

每个步骤下的进度条显示操作有多远。工具拒绝flash运行另一个自定义固件的控制台(BleemSync、Project Eris)：首先恢复它到库存。

<!-- pagebreak -->

## 7. 如果出了问题

- **日志**：AutoBleem在内存中保持其日志，所以闪存盘不一直被写入 - 它们仅在出错时到达闪存盘、卡或数据文件夹中的`System/Logs/`：启动器崩溃、PS1游戏或RetroArch崩溃将它们保存到`System/Logs/crash-<n>/`(保持最后三个)，启动器返回时说一次。要保持每个日志，打开*选项 -> 诊断 -> 在闪存盘上保持日志*(从下次启动)或在PC上创建空文件`System/Logs/keep`。在Pi或PC上，*硬件信息*显示日志在哪里并Square将它们保存到`System/Logs/saved-<n>/`。文件：`autobleem.log`(启动器)、`launch.log`和`pcsx.log`(PS1游戏的启动和模拟器的输出)、`retroarch.log`以及 - 总是在闪存盘上 - `update.log`(在线更新)和`updateroms.log`(UpdateRoms)。
- **游戏不在书架上**：检查文件夹布局(每个游戏一个文件夹，第3.9节的图像格式)。*游戏管理器*在游戏后列出扫描拒绝的文件夹、标记*未添加*、带原因；Square删除这样的文件夹。系统菜单中的重新扫描再次运行扫描。
- **没有封面**：盖艺术数据库未安装(再次运行安装程序勾选它们)或用于控制台上的RetroArch游戏，PC上未运行UpdateRoms。
- **手柄不工作或按钮混淆**：PSC-Bios的手柄映射向导(控制台)映射它；在Pi或PC上硬件信息页面列出SDL看到的。
- **游戏后控制台显示黑屏**：AutoBleem自己重建其窗口(最多三次)；如果保持黑色，按住电源按钮并再次打开控制台。
- **树莓派**：`Alt+F2`在第二个控制台给出登录提示；SSH从首次启动启用。`sudo journalctl -u autobleem`显示启动器的服务；`sudo systemctl restart autobleem`重启它。无法完成的首次启动(无网络)在下次启动时重试。
- **Windows**：`Esc`离开启动器；数据文件夹是设置中选择的(默认`Documents\AutoBleem`)，日志在其`System\Logs`中。

AutoBleem是自由软件(GNU GPL v3或更高版本)，没有保修。支持和新闻：About屏幕上链接的Discord服务器以及https://autobleem.retromenele.pl/。

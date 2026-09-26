# Manual do usuário do AutoBleem 2

AutoBleem 2 é um lançador de jogos para **PlayStation Classic** - e, desde a versão 2, para **Raspberry Pi**,
um **PC iniciado por um pen drive USB** e **Windows**. Ele mostra seus jogos PS1 como uma prateleira de capas com
suas artes de capa e detalhes, os inicia no emulador PCSX incluído, e, com o RetroArch instalado ao lado, pode
reproduzir jogos de outros sistemas. Este manual abrange a instalação em cada plataforma, o uso diário e as
ferramentas incluídas.

> Os downloads para cada plataforma estão em **https://autobleem.retromenele.pl/**. A página é organizada por
> plataforma: o painel *Install* de cada uma é o que você baixa; os *Build inputs* abaixo são o que os
> instaladores baixam sozinhos.

## 1. O que você obtém

- **O lançador** - a prateleira de capas, os conjuntos (PlayStation, RetroArch, Aplicativos), os detalhes do jogo,
  o menu do sistema, as opções, as ferramentas de cartão de memória e pontos de salvamento. O mesmo programa em
  todas as plataformas.
- **Dois emuladores PS1** - `pcsx-abnxt`, o atual (padrão), e `pcsx-ab`, o clássico que o AutoBleem sempre forneceu.
  Você escolhe um nas opções; ambos usam as mesmas configurações e cartões de memória.
- **RetroArch** (opcional em todas as plataformas) para outros sistemas: NES, SNES, Mega Drive, Game Boy, arcade e
  muitos mais. AutoBleem constrói suas listas de RetroArch a partir dos ROMs que você copia e inicia cada jogo com
  o núcleo correto.
- **As ferramentas de console** (apenas PlayStation Classic): *PSC-Bios* para WiFi, relógio e mapeamento de
  controle, e *ABFlashKit* para instalar o kernel AutoBleem.
- **UpdateRoms** para Windows: atualiza as listas de RetroArch e as capas de um pen drive de console em um PC, pois
  o console em si não tem rede.

![O lançador: a prateleira de capas, os detalhes do jogo selecionado, os indicadores de botão](../images/en/launcher.jpg)

<!-- pagebreak -->

## 2. Instalação

### 2.1 PlayStation Classic

Você precisa de um PC Windows, um pen drive USB (USB 2.0, 8 GB ou mais; o instalador o formata se solicitado) e o
console original. AutoBleem roda a partir do pen drive sem nenhuma alteração no console. O pen drive deve ser **FAT32**
para um console original - seu kernel não pode ler exFAT. Apenas um console com o kernel AutoBleem instalado
(ABFlashKit, capítulo 6) também inicializa a partir de um pen drive exFAT, o que levanta o limite de 4 GB do FAT32.

1. Baixe **AutoBleemInstaller-<version>.zip** do painel PlayStation Classic do site e descompacte em qualquer lugar.
   Ele contém `AutoBleemInstaller.exe` e o pacote AutoBleem que instala.
2. Conecte o pen drive e inicie `AutoBleemInstaller.exe`. Selecione o drive no topo. Marque o que deseja:
   - **Formatar o pen drive** - apenas para um novo pen drive (tudo nele é apagado). Selecione FAT32 a menos que o
     console tenha o kernel AutoBleem.
   - **Bancos de dados de capas** - as artes de capa e detalhes da biblioteca PS1 (marcado por padrão; cerca de 300 MB).
   - **RetroArch** - RetroArch com seus núcleos, aplicativos adicionais (Doom, Quake, Amiga, ...) e ativos libretro
     para jogos de outros sistemas. Desabilitado por padrão; pode ser adicionado mais tarde executando o instalador
     novamente.
   - **Arquivos BIOS** - os arquivos BIOS que os núcleos do RetroArch precisam (requer RetroArch).
   - **Jogos de exemplo** - alguns jogos homebrew gratuitos para que a prateleira não fique vazia.
3. Pressione **Instalar** e aguarde. As barras de progresso e o log mostram cada etapa; o pen drive é nomeado `SONY`
   ao final e `UpdateRoms` é colocado nele (veja capítulo 5).
4. Remova o pen drive com segurança, conecte-o à **segunda porta USB** do console (a direita, jogador 2) e ligue o
   console. AutoBleem é iniciado em vez do menu original.

**Ligando e desligando.** Com o pen drive dentro, o console inicializa, sua luz pisca por alguns segundos (AutoBleem
está sendo configurado) e depois entra em espera antes de nada ser exibido - é assim que o console encena uma
atualização, é assim que o AutoBleem é executado. Pressione **Power** uma vez e o lançador aparece. *Desligar* no
menu do sistema, ou o botão Power do console, coloca o console em **espera do AutoBleem**: o pen drive é
desconectado primeiro, depois a luz fica **vermelha** - o sinal de que o AutoBleem está funcionando corretamente - e
o próximo pressionamento de Power traz o lançador de volta direto, em alguns segundos. **Enquanto a luz está vermelha,
o pen drive pode ser retirado** e colocado em um PC sem o Windows pedir para verificá-lo; coloque-o de volta antes de
pressionar Power. Desconectar a alimentação do console passa pela espera de inicialização novamente da próxima vez.

Para **atualizar** um pen drive, execute um instalador mais recente nele: seus jogos, salvamentos, configurações e
conteúdo do RetroArch permanecem; apenas os arquivos do próprio AutoBleem são substituídos. Um pen drive criado com
AutoBleem 1.0 ou AutoBleem-NG é atualizado automaticamente para o novo layout.

> O console original não tem relógio nem rede: as datas são exibidas apenas após a instalação do kernel AutoBleem
> (capítulo 6), e as capas para jogos de RetroArch vêm do UpdateRoms no PC (capítulo 5).

Os jogos vão para a pasta `Games` do pen drive, uma pasta por jogo - veja a seção 3.9 para o layout.

### 2.2 Raspberry Pi

AutoBleem transforma um Pi em um pequeno console: ele inicializa diretamente no lançador, sem desktop. Duas imagens
prontas estão no site - 32 bits e 64 bits - além de um tarball para um Raspberry Pi OS Lite existente.

| Modelo | Imagem de 32 bits | Imagem de 64 bits | Notas |
|---|---|---|---|
| Raspberry Pi 5 | sim | sim | |
| Raspberry Pi 4 Modelo B, Pi 400 | sim | sim | |
| Raspberry Pi 3 Modelo B / B+ / A+ | sim | sim | bom para o lançador e PS1 |
| Raspberry Pi Zero 2 W | sim | sim | 512 MB de RAM: PS1 funciona, núcleos RetroArch mais pesados não |
| Raspberry Pi 2 Modelo B | sim | apenas v1.2 | lento para qualquer coisa em 3D |
| Raspberry Pi 1, Zero, Zero W | não | não | ARMv6 - nenhuma imagem funciona |

A **imagem de 32 bits é a recomendada** para jogos PS1: o recompilador ARM rápido de `pcsx-ab` é apenas 32 bits,
então a compilação de 64 bits executa jogos PS1 mais lentamente. A imagem de 64 bits tem um conjunto maior de núcleos
RetroArch.

**Com o Raspberry Pi Imager:**

1. Instale o Raspberry Pi Imager (raspberrypi.com/software). Em *Choose OS* selecione *Use custom* e o
   `autobleem-<version>-rpi-armhf.img.xz` (32 bits) ou `-arm64.img.xz` (64 bits) que você baixou - ou adicione a URL
   do repositório `https://autobleem.retromenele.pl/rpi-imager/os_list.json` nas configurações do aplicativo e
   selecione AutoBleem na lista.
2. Use a tela de personalização do Imager (a engrenagem, ou a pergunta após *Avançar*) para definir **nome de usuário
   e senha, rede WiFi e país, e habilitar SSH**. AutoBleem precisa de uma rede no primeiro inicialização.
3. Escreva o cartão, insira no Pi com uma tela e teclado ou controle, e ligue-o.

**O primeiro inicialização** leva de 5 a 25 minutos e mostra o que está fazendo na tela. Sem rede, ele pede uma (uma
lista WiFi, a senha), depois pergunta se instalar RetroArch (um minuto sem resposta significa sim), amplia a partição
do sistema, cria a partição de dados `AUTOBLEEM` do resto do cartão, instala RetroArch e seus núcleos, os pacotes BIOS
e os jogos de exemplo, e reinicializa no lançador.

As respostas podem ser fornecidas antecipadamente em **`autobleem.txt`** na partição de inicialização do cartão
(editável em qualquer PC antes do primeiro inicialização):

| Chave | Padrão | Significado |
|---|---|---|
| `root_gib` | 8 | O tamanho da partição do sistema em GiB; o resto se torna a partição de jogos. |
| `hdmi_mode` | 1920x1080@60 | O modo de tela para todo o inicialização (`1280x720@60` para uma TV mais antiga). |
| `retroarch` | (solicitado) | `yes` / `no` - RetroArch e outros sistemas, ou apenas PS1. |
| `thumbnails` | none | `boxarts` espelha o conjunto completo de capas para capas offline (~9000 arquivos). |
| `bios`, `downloads`, `samples` | yes | Defina para `no` para pular pacotes BIOS, todos os downloads ou jogos de exemplo. |

**Em um Raspberry Pi OS Lite existente** (Bookworm ou Trixie): copie `autobleem-rpi.tar.gz` (ou a versão arm64)
para o Pi, descompacte e execute `sudo bash install.sh`. Ele faz as mesmas perguntas, cria a partição de dados
encolhendo a raiz na próxima inicialização (`--shrink-root <GiB>`), e coloca o lançador no primeiro console.

Após a instalação, a partição **`AUTOBLEEM`** do cartão (exFAT) é o que você preenche: remova o cartão e abra-o em
qualquer PC, ou copie via rede (SSH está habilitado). `Games/` para jogos PS1, `RetroArch/roms/<sistema>/` para
outros sistemas, `System/Bios/` para BIOS PS1 (seção 3.10), `Themes/` para temas.

### 2.3 Pen drive USB para PC

O mesmo dispositivo para qualquer PC que inicializa por USB - um sistema de 32 bits, então máquinas antigas também
funcionam:

1. Baixe `autobleem-<version>-pcusb-i386.img.xz` do painel PC e escreva-o em um pen drive de 8 GB ou mais com
   Raspberry Pi Imager (*Use custom*), balenaEtcher ou Rufus (modo DD).
2. Inicialize o PC a partir do pen drive (a tecla do menu de inicialização do seu PC - F12, F8, Esc...). Tanto
   inicialização BIOS quanto UEFI funcionam; **Secure Boot deve estar desabilitado**.
3. O primeiro inicialização é o do Pi: uma pergunta sobre rede se não houver cabo, a pergunta do RetroArch, depois
   a instalação - cerca de oito minutos com uma rede com fio - e um reinicialização no lançador.

O pen drive tem uma partição `AUTOBLEEM` para seus jogos, visível no Windows 10 (1903 e mais recente) como uma
segunda unidade quando você conecta o pen drive a um PC em execução. `autobleem.txt` está na primeira partição, com
as mesmas chaves que no Pi (sem `hdmi_mode` - o PC usa o modo nativo da tela).

### 2.4 Windows

AutoBleem como um programa Windows: tela cheia, emuladores e RetroArch iniciados como programas.

1. Baixe **AutoBleemSetup-<version>.exe** e execute-o. Ele instala por usuário, sem direitos de administrador: o
   programa em `%LOCALAPPDATA%\Programs\AutoBleem`, os dados (jogos, configurações, temas, RetroArch) em uma pasta
   que você escolhe - `Documents\AutoBleem` por padrão.
2. Marque os componentes - os bancos de dados de capas, RetroArch (a versão Windows oficial e seus núcleos), os
   arquivos BIOS, os jogos de exemplo - e deixe o assistente de configuração baixá-los.
3. Inicie AutoBleem no menu Iniciar ou na Área de Trabalho. Em um PC, o teclado funciona como um controle (seção 3.2).

Executar uma versão mais recente do programa sobre ela a atualiza e mantém a pasta de dados. O lançador também
verifica o site uma vez por dia e oferece uma atualização se houver uma (seção 3.11).

<!-- pagebreak -->

## 3. Usando o AutoBleem

### 3.1 O lançador

O lançador abre na prateleira: as capas do conjunto atual, a selecionada no meio, seus detalhes ao lado - editora,
ano, número de série, região, jogadores, quando foi jogado pela última vez - e um botão de reprodução. A barra na
parte inferior lista o que os botões fazem. Uma verificação da pasta de jogos é executada em segundo plano a cada
inicialização; enquanto é executada, uma bolha no canto superior direito mostra seu progresso, e novos jogos aparecem
na prateleira conforme são encontrados.

![O seletor de conjuntos: três abas e os grupos do presente com seus números de jogos](../images/en/set-picker.jpg)

### 3.2 Controles

| Botão | Na prateleira |
|---|---|
| Esquerda / Direita | Jogo anterior / próximo. Mantenha pressionado para rolar. |
| L1 / R1 | Pule para a primeira letra anterior / próxima dos títulos. |
| Cross | Inicia o jogo selecionado (um jogo PS1 no emulador PS1; um jogo RetroArch em seu núcleo; um aplicativo após seu manual). |
| Square | Inicia o jogo PS1 selecionado no RetroArch em vez disso. |
| Triangle | O guia de botões. |
| Start | Um jogo aleatório do conjunto atual. |
| Select | O seletor de conjuntos: abas PlayStation / RetroArch / Aplicativos (L1 / R1), os grupos da aba (Cima / Baixo, L2 / R2 uma página), Cross seleciona. |
| Baixo | Abre a linha de ícones sob o jogo (Configurações, Jogo, Cartão de Memória, Retomar). Cima a fecha. |
| L2 + R2 | O menu do sistema (seção 3.4). |

**Com um teclado** (um PC sem controle ou um teclado USB no console, um Pi ou o pen drive PC) as teclas
substituem: **Setas** = d-pad, **Enter** = Cross, **Esc ou Backspace** = Circle, **Tab** = Triangle,
**Espaço** = Square, **F1 / F2** = Select / Start, **Page Up / Page Down** = L1 / R1, **Home / End** =
L2 / R2, **F10** = o menu do sistema. Em uma máquina de desenvolvimento, Esc fecha o programa e Espaço é Start.

Em cada lista e menu: Cima / Baixo se movem, **L2 / R2 mudam de página**, L1 / R1 pulam para a primeira / última
linha, **Cross seleciona, Circle volta**. Uma tela com configurações as salva quando você a sai com Circle.

![A linha de ícones sob o jogo selecionado](../images/en/launcher-icons.jpg)

### 3.3 Os conjuntos

**Select** abre o seletor de conjuntos. A aba PlayStation lista *Todos os Jogos*, *Jogos Internos* (os vinte
incorporados do console, em um PlayStation Classic), cada pasta que você criou em `Games/` (um jogo em uma
subpasta pertence a esse grupo), *Jogos Favoritos*, *Histórico de Jogos* e, se houver, *Jogos de Pistola*. A aba
RetroArch lista um grupo por sistema que tem jogos, mais Favoritos e Histórico próprios do RetroArch. A aba
Aplicativos agrupa aplicativos por tipo: *Todos os Aplicativos*, depois *Jogos*, *Emuladores*, *Ferramentas*,
*Mídia* e *Outro* (a categoria é definida no arquivo `app.ini` de cada aplicativo). Cada linha mostra quantos
itens ela contém; um grupo vazio abre em uma prateleira vazia com a linha de ícones mostrando apenas Configurações.

### 3.4 O menu rápido

**Cima** no lançador, ou o **ícone de engrenagem** na linha de ícones (onde estão Configurações / Jogo / Cartão de
Memória / Retomar): o menu rápido para ações que você alcança no carrossel. Uma lista breve: *Verificar Jogos* (inicia
uma verificação agora), *Loja* (a Loja AutoBleem para baixar extensões), *Rede e Controles* (apenas onde uma extensão
instalada fornece a entrada `network` - PSC-Bios no console, um Pi e o pen drive PC: WiFi, emparelhamento Bluetooth,
o assistente de mapeamento de controle - veja a seção 6; acinzentado com "ative-o nas Extensões" quando essa extensão
está desabilitada - Cross abre a lista de Extensões), e *Menu do Sistema...* (o menu completo abaixo). Cima / Baixo se
movem (envolvente), Cross seleciona, Circle volta. Nada é único aqui - cada item também está no menu do sistema.

### 3.5 O menu do sistema

**L2 + R2** (juntos, em qualquer ordem) abre o menu do sistema sobre a prateleira. O menu é agrupado em seções:

| Seção | Item | O que faz |
|---|---|---|
| (topo) | Verificar Jogos | Procura por jogos novos, modificados ou removidos agora (a verificação também observa a pasta em si). |
| | Extensões | As extensões no pen drive - a Loja AutoBleem e outras (seção 3.12). |
| **Biblioteca** | Gerenciador de Jogos | Os jogos PS1 como uma lista com suas pastas: delete um jogo, limpe as capas. Desabilitado durante uma verificação. |
| | Cartões de Memória | Seus conjuntos de cartões de memória (seção 3.7). |
| | Processadores de Scanner | Os programas que cada verificação executa primeiro - sua ordem, habilitados ou desabilitados (seção 3.13). Desabilitado durante uma verificação. |
| **Sistema** | Opções | As configurações do AutoBleem (seção 3.6). |
| | Rede e Controles | Apenas onde uma extensão instalada fornece a entrada `network` (`Provides=network` em seu `extension.ini` - PSC-Bios no console, um Pi e o pen drive PC) - WiFi, emparelhamento de controle Bluetooth, configuração DualShock 3 e o assistente de mapeamento de controle - veja o capítulo 6. Quando essa extensão está instalada mas desabilitada, este item permanece acinzentado com uma nota "ative-o nas Extensões" - Cross abre a lista de Extensões. |
| | Informações de Hardware | Os fatos da máquina: sistema, CPU, armazenamento, interfaces de rede, fuso horário, display, os controles e seus mapeamentos. Em um console com o kernel AutoBleem, isto abre PSC-Bios (capítulo 6); em outras máquinas, mostra esta página de informações. |
| | Atualização de Software | (Raspberry Pi e PC) Verifique o site para uma versão mais recente do AutoBleem ou RetroArch agora. |
| | Sobre | Créditos e licença. |
| **Sair** | RetroArch | Sai do lançador para o menu próprio do RetroArch. Fechar RetroArch volta. |
| | Desligar | Após uma confirmação: no console a espera do AutoBleem - o pen drive desconectado, a luz vermelha, Power traz o lançador (seção 2.1); em um Pi ou um PC a máquina desliga. |

![O menu do sistema](../images/en/system-menu.jpg)

### 3.6 Opções

As configurações estão em grupos; Cima / Baixo se move entre eles, Esquerda / Direita muda um valor, Circle sai e
salva. Cada mudança é aplicada imediatamente.

| Grupo / Configuração | O que faz |
|---|---|
| **Interface**: Tema de AutoBleem | A aparência. Os temas vivem em `Themes/`; um arquivo zip de tema depositado lá é extraído na próxima visita. Os temas que o AutoBleem fornece são atualizados a cada atualização - para personalizar um, copie-o primeiro com um novo nome. |
| Estilo de Capa | O moldura de caixa de jóias desenhada ao redor das capas PS1. |
| Idioma | O idioma do lançador, aplicado imediatamente (17 idiomas). |
| Usar fonte do tema / Fonte | A fonte das telas clássicas: a do tema, ou qualquer `.ttf`/`.otf` de `resources/fonts`, `RetroArch/fonts` ou a pasta do tema. |
| Tempo de Exibição | Quanto tempo a notificação "Exibindo: ..." permanece, em segundos (0 = para sempre). |
| **Som**: Música, Música de Fundo | Qual faixa toca sob o lançador (a do tema, ou um arquivo de `resources/music`), e se uma toca. |
| **Emulação**: Emulador PS1 | `pcsx-abnxt` (padrão: PCSX-ReARMed atual com adições do AutoBleem) ou `pcsx-ab` (o clássico). Um ponto de salvamento salvo por um continua no outro, a menos que o jogo tenha sido executado sem um arquivo BIOS. |
| Widescreen | A forma da imagem do emulador PS1 para cada jogo. |
| Reproduzir todos os jogos PSX com RA | Cada jogo PS1 inicia no núcleo PS1 do RetroArch. |
| Atualizar Config RA | AutoBleem escreve suas configurações na configuração do RetroArch quando inicia um jogo lá. |
| **Biblioteca**: Mostrar Jogos Internos | Os jogos incorporados do console nas listas PlayStation (apenas PlayStation Classic). |
| Buscar capas online | A verificação baixa capas faltantes dos servidores libretro (Raspberry Pi, PC, Windows). |
| **Atualizações** | (Raspberry Pi, PC, Windows) `stable`, `latest` (também pré-lançamentos) ou `off`. |

![As opções, em grupos](../images/en/options.jpg)

### 3.7 Configurações de um jogo

Com um jogo selecionado, **Baixo** abre sua linha de ícones: **Configurações** (as opções acima), **Jogo**
(as configurações próprias do jogo), **Cartão de Memória** (seu cartão de memória) e **Retomar** (seus pontos de
salvamento). Cross abre o que está sob o cursor.

O **editor de jogos** mostra os detalhes do jogo à direita e suas configurações à esquerda, em três grupos:

- **Jogo**: *Favorito* (no grupo Jogos Favoritos), *Jogo de Pistola* (um jogo de pistola - ele se junta ao grupo
  pistola e sempre é executado no RetroArch, cujo núcleo PS1 tem o GunCon), *Reproduzir com RA* (este jogo é
  executado no RetroArch), *Bloquear dados* (o scanner deixa o título do jogo, o número de série e a lista de discos
  como você os definiu).
- **Vídeo**: alta resolução, linhas de varredura e seu nível, salta de quadro, o plugin GPU, e o *Filtro* - como a
  imagem é escalada: Desabilitado (pixels brutos), Linear (suavizado) ou Nítido (pixels nítidos sem cintilação; apenas
  `pcsx-abnxt` - o clássico `pcsx-ab` e RetroArch o mostram como Desabilitado).
- **Emulador**: SpeedHack, a frequência da CPU, interpolação SPU, o logo de inicialização (desabilitado pula o shell
  BIOS - para um disco homebrew cujo logo personalizado quebra a inicialização), e com `pcsx-abnxt` o filtro
  *Suavização* e a alternância *Hacks Sony*.

Triangle renomeia o jogo, Square muda seu cartão de memória, Start compartilha um novo cartão. Circle salva e sai.

**Configurações salvas no emulador.** O menu próprio do emulador tem *Salvar configurações para este jogo*. Uma vez
que um jogo tem configurações salvas lá, são as que ele joga, e o editor de jogos mostra suas linhas Vídeo e Emulador
acinzentadas, com aqueles valores, sob o título *Salvo no Emulador*. Para voltar às configurações do editor de jogos,
selecione **Desbloquear configurações** e confirme: isto remove as configurações que o emulador salvou, e as linhas podem
ser alteradas novamente. Ambos os emuladores, `pcsx-ab` e `pcsx-abnxt`, leem e escrevem as mesmas configurações salvas.

![O editor de jogos](../images/en/game-editor.jpg)

### 3.7 Cartões de Memória e Pontos de Salvamento

Cada jogo PS1 tem seu próprio cartão de memória por padrão (mantido com seus pontos de salvamento em
`Games/!SaveStates/<pasta de jogo>/`). **Cartões de Memória** no menu do sistema gerencia **conjuntos compartilhados**
- um cartão que vários jogos usam, mantido em `Games/!MemCards/`: crie um (Square, com o teclado na tela),
renomeie (Cross), delete (Triangle). Um jogo é colocado em um conjunto com *Mudar cartão de memória* em seu editor,
ou a partir de seu ícone de Cartão de Memória.

O **editor de cartão de memória** (o ícone de Cartão de Memória) mostra o cartão do jogo e um segundo cartão lado a
lado, com o ícone e título de cada salvamento: copie um salvamento entre os dois (Square), delete um (Triangle),
desfragmente um cartão (Select). Start troca o cartão à direita por outro conjunto.

![O editor de cartão de memória](../images/en/memory-card-editor.jpg)

**Pontos de salvamento**: quando você sai de um jogo PS1 com o botão Reset do console (ou o menu do emulador em um Pi
ou PC), AutoBleem mantém um ponto de salvamento de onde você estava e o oferece sob o ícone **Retomar** - quatro slots,
cada um com uma imagem do momento. Cross continua a partir do slot, Triangle o delete. Um jogo com um ponto de salvamento
mostra uma pequena imagem em seu ícone Retomar.

### 3.8 Iniciando Jogos, RetroArch e Aplicativos

**Cross** inicia o jogo selecionado. Um jogo PS1 é executado no emulador PS1 escolhido (seção 3.5), tela cheia, até você
sair - no console com o botão **Reset** anterior (volta ao lançador com um ponto de salvamento) ou **Power** (o console
desliga); em um Pi ou um PC via o menu do emulador (Select + Start no controle ou Esc no teclado). **Square** inicia um
jogo PS1 no RetroArch em vez disso.

Um jogo **RetroArch** é iniciado no RetroArch com o núcleo que o lançador escolheu para seu sistema; *Fechar conteúdo*
ou *Sair do RetroArch* em seu menu volta ao lançador. O item RetroArch no menu do sistema abre o menu próprio do RetroArch
(XMB) sem nada carregado, para suas configurações e suas próprias listas de conteúdo.

Um **aplicativo** (o conjunto de Aplicativos: as ferramentas de console, e em um console os aplicativos adicionais que o
pacote RetroArch traz - Doom, Quake, Amiga, ...) mostra primeiro seu manual; Cross o inicia, Circle volta.

![O manual de um aplicativo antes de iniciar](../images/en/app-start.jpg)

### 3.9 Adicionando Jogos

Os **jogos PS1** vão para a pasta `Games`, **uma pasta por jogo**, nomeada após o jogo:

```
Games/
  Crash Bandicoot/            Crash Bandicoot.cue + Crash Bandicoot.bin
  Final Fantasy VII/          Final Fantasy VII (Disc 1).chd, (Disc 2).chd, (Disc 3).chd
  Platformers/                uma pasta de jogos: seu próprio grupo no seletor de conjuntos
    Klonoa/                   Klonoa.pbp
```

- Formatos: `.cue` + `.bin` (ou `.img`), `.pbp`, `.chd` (também zstd), `.ecm` (decodificado pela verificação),
  `.iso`. Um jogo compactado também funciona: o processador **Unzip** o extrai antes da verificação (seção 3.13).
- Um jogo multi-disco é uma pasta com cada disco dentro; as pastas nomeadas `Game (Disc 1)`, `Game (Disc 2)`
  ... são mescladas em uma pasta `Game` pela verificação.
- Jogos diretamente em `Games/` (arquivos soltos) são classificados em pastas pela verificação.
- Uma **capa** é um PNG ao lado da imagem do jogo, nomeado como ela. Sem uma, a arte vem dos bancos de dados de
  capas, ou - com RetroArch instalado - do conjunto de miniaturas libretro; em um Pi, um PC ou Windows uma capa
  faltante é baixada online (Opções → *Buscar capas online*).
- A verificação lê o número de série de cada disco e pega o título, editora, ano, jogadores e região do banco de dados
  PlayStation de RetroArch ou dos bancos de dados de capas. Mude qualquer coisa no editor de jogos e marque *Bloquear
  dados* para mantê-lo.

Os **outros sistemas** vão sob `RetroArch/roms/`, **uma pasta por sistema, nomeada como são os bancos de dados de
RetroArch** (a pasta é criada para você): `Nintendo - Nintendo Entertainment System`, `Nintendo - Super Nintendo
Entertainment System`, `Sega - Mega Drive - Genesis`, `Nintendo - Game Boy Advance`, `FBNeo - Arcade Games`
(ou `Arcade`), ... Os ROMs podem permanecer compactados. Em um Pi, um PC ou Windows a verificação os lê por conta própria
e escreve as listas de jogos de RetroArch; em um pen drive de console, execute **UpdateRoms** em um PC (capítulo 5).

Os **aplicativos** vão sob `Apps/<nome>/` com um `app.ini` (o nome, o ícone, o que executar) e uma `run.sh`.

Os **temas** vão sob `Themes/<nome>/` (`theme.json` e as imagens) - ou solte o arquivo zip do tema em `Themes/`.

### 3.10 O BIOS PS1

Em um **PlayStation Classic** o emulador usa o BIOS próprio do console. Em um **Raspberry Pi, um PC e
Windows** coloque seu próprio BIOS PS1 em `System/Bios/`: `romw.bin` (o US/Europeu SCPH-5501/5502) e
`romJP.bin` (o Japonês SCPH-5500). Os instaladores os preenchem a partir dos pacotes BIOS de RetroArch a menos que
seus próprios arquivos já estejam lá. Sem eles o emulador é executado no seu BIOS HLE incorporado, que muitos jogos
toleram e alguns não.

### 3.11 Atualizações

- **Raspberry Pi, pen drive PC, Windows**: o lançador verifica o site na inicialização e uma vez por dia (Opções →
  *Atualizações* é o canal; *Atualização de Software* no menu do sistema verifica agora). Quando há uma versão mais
  recente do AutoBleem ou RetroArch, ele pergunta: *Atualizar agora* baixa tudo e rexecuta o instalador com a tela
  de progresso do primeiro inicialização; *Lembrar-me amanhã* e *Pular esta versão* são as outras respostas. Seus
  jogos e configurações permanecem; o lançador reverifica uma vez após uma atualização.
- **PlayStation Classic**: execute um `AutoBleemInstaller.exe` mais recente no pen drive (seção 2.1).

### 3.12 Extensões e a Loja AutoBleem

As **extensões** adicionam suas próprias telas ao lançador. Elas vivem em `Extensions/<nome>/` no pen drive (em um
Raspberry Pi sua partição de dados, em Windows a pasta de dados); para instalar uma, descompacte seu zip lá. **L2 + R2
→ Extensões** as lista: Cross executa uma, Triangle a desabilita ou reabilita. Uma extensão que precisa da rede não é
iniciada sem ela, e uma que parou o lançador é desabilitada - a lista diz.

![A lista de extensões](../images/en/extensions.jpg)

A **Loja AutoBleem** é a primeira extensão: aplicativos e jogos para instalar em um toque, em cada sistema que o AutoBleem
é executado (um PlayStation Classic precisa do WiFi do kernel AutoBleem). Suas quatro abas, L1 / R1 entre elas:

- **Aplicativos** e **Jogos**: o que as fontes oferecem, cada um com sua imagem, versão, tamanho e ícone da fonte. Os itens
  instalados são acinzentados. Cross instala (ou atualiza, ou tenta novamente após uma falha), Triangle remove o que a
  Loja instalou. L2 / R2 ou Esquerda / Direita mudam de página, **Select** mostra uma fonte de cada vez, **Start** pesquisa
  nos títulos. As imagens dos itens são cacheadas e podem ser retentadas se não carregarem.
- **Downloads**: o que está sendo baixado, aguardando, falhou ou está instalado. A barra de progresso é atualizada constantemente.
  Os downloads continuam em segundo plano mesmo depois de sair da Loja; iniciar um jogo ou desligar apenas as pausa, e um
  download interrompido continua de onde parou. Um jogo instalado aparece na prateleira após a próxima verificação, com a
  imagem da Loja como capa. Downloads maiores que 2 GB funcionam em todas as plataformas, incluindo compilações de 32 bits.
- **Fontes**: de onde as listas vêm - o catálogo próprio do AutoBleem, uma lista TSV depositada em
  `System/Extensions/store/sources/` e os endereços que você adiciona com **Adicionar uma URL de fonte**. Cada fonte mostra
  seu ícone na lista. Cross em uma que você adicionou a renomeia, muda seu endereço, alterna entre `http://` e `https://`,
  ou a remove.

![A aba Aplicativos da Loja](../images/en/store-apps.jpg)

![O menu de uma fonte](../images/en/store-source-menu.jpg)

O que o catálogo do AutoBleem oferece também é listado no site de download, `https://autobleem.retromenele.pl/store/`.
**Você é responsável pelo que as fontes que você adiciona contêm.**

**Seus próprios jogos em sua rede**: `abstored`, o servidor LAN da Loja, serve uma pasta de jogos PS1 para a Loja na
mesma rede. Ele é executado em qualquer máquina Linux - um Raspberry Pi, um servidor doméstico - e apenas lê a pasta.
Inicie-o com `abstored <pasta de jogos>`, abra `http://<essa máquina>:8124/` em um navegador para ver o que ele serve e
quaisquer problemas que encontrou, e adicione `http://<essa máquina>:8124/store.tsv` como uma fonte. Programas prontos para
Linux e Windows estão na página da Loja, em sua aba **Servidor LAN**; configurá-lo como um serviço é `INSTALL-linux.md`
(`ext_store/server/` na fonte). **LAN Share** (seção 5.2) coloca jogos e discos de um PC em tal servidor.

### 3.13 Processadores de Scanner

Os **processadores de scanner** são pequenos programas que cada verificação executa antes de ler seus jogos. Um pode
transformar um formato que o AutoBleem não lê em um que ele lê - um jogo compactado, por exemplo - ou alterar os dados
de um jogo, como um patch de tradução. Eles vivem em `System/Processors/<nome>/` no pen drive (em um Raspberry Pi sua
partição de dados, em Windows a pasta de dados); para instalar um, descompacte sua pasta lá. A próxima verificação o executa.

- **Unzip vem com o AutoBleem**: ele extrai os jogos PS1 compactados em `Games/` antes que a verificação os leia e os
  ROMs compactados um de cada vez (os jogos arcade permanecem compactados). Atualizar o AutoBleem também o atualiza e o
  deixa desabilitado se você o desabilitou.
- Um processador que já tratou um jogo não é reexecutado nele até o jogo mudar.
- Enquanto um processador funciona, a bolha no canto superior direito mostra o que está fazendo; um aviso ou erro aparece
  na linha abaixo. `processors.log` na pasta de logs tem os detalhes.
- Iniciar um jogo ou RetroArch para um processador que modifica arquivos; a próxima verificação termina seu trabalho.

**L2 + R2 → Processadores de Scanner** os mostra na ordem em que são executados, uma aba para jogos PS1 e uma para ROMs
(L1 / R1). **Square** pega um e Cima / Baixo o move - a ordem importa: um processador que extrai deve vir antes de um que
aplica patch no que foi extraído. **Cross** o alterna ou o ativa, **Triangle** o reexamina em cada próxima verificação,
**Circle** volta e inicia uma verificação se você mudou algo. Um processador construído para outra máquina permanece na
lista, acinzentado.

![Processadores de Scanner](../images/en/processors.jpg)

Escreva o seu: a página de Unzip, `https://github.com/autobleem2/proc_unzip`, explica tudo o que um processador precisa fazer,
e `tools/proc_check.py` no fonte do AutoBleem verifica um antes de compartilhá-lo.

<!-- pagebreak -->

## 4. Telas

### 4.1 Gerenciador de Jogos

Os jogos PS1 como uma lista com suas pastas e o jogo selecionado com sua capa e detalhes. Cross abre o editor de jogos,
**Square delete o jogo** (sua pasta e, após uma segunda pergunta, seus pontos de salvamento), Triangle delete cada PNG
de capa ao lado dos jogos (a verificação os recupera dos bancos de dados), L2 / R2 página. O espaço livre da unidade está
na parte superior direita. O gerenciador de jogos aguarda enquanto uma verificação é executada.

![O gerenciador de jogos](../images/en/game-manager.jpg)

### 4.2 Informações de Hardware

Os fatos da máquina - sistema, hardware, armazenamento com seu espaço livre, endereços de rede, os drivers de display e
áudio, os controles conectados - relido a cada segundo. Em um PlayStation Classic com o kernel AutoBleem este item abre
**PSC-Bios** em vez disso (capítulo 6).

![Informações de Hardware](../images/en/hardware-info.jpg)

### 4.3 O Guia de Botões

Triangle na prateleira: cada botão de cada tela em uma página. Quando um teclado USB está conectado ou foi usado, uma coluna
de Teclado mostra as teclas ao lado dos botões do controle.

![O Guia de Botões](../images/en/button-guide.jpg)

### 4.4 O Teclado na Tela

Onde quer que o texto seja digitado - um conjunto de cartão de memória, um título de jogo, uma senha WiFi, o endereço de
uma fonte - o mesmo teclado, disposto como o de um telefone: letras, uma página de símbolos (`/ \ : ? & = % @ #` e o
resto que um endereço ou senha precisa) e duas páginas de letras acentuadas, com Shift, a tecla de página, Espaço,
Backspace e Confirmar na linha inferior. As direções se movem, Cross digita, Triangle delete, Square é um espaço, **L1**
é Shift (duas vezes para caps lock), **R1** a próxima página, **L2 / R2** movem o cursor, Start confirma, Circle cancela.
Um teclado USB digita a qualquer momento: Enter confirma, Esc cancela.

![O Teclado na Tela](../images/en/keyboard.jpg)

<!-- pagebreak -->

## 5. No PC

### 5.1 UpdateRoms - Atualizando um Pen drive de Console

O PlayStation Classic não tem rede, então as listas de RetroArch e as capas de seu pen drive são criadas em um PC:
**UpdateRoms** faz em um PC o que a verificação do lançador faz em um Pi, com a rede do PC e os caminhos do console,
para que o console inicialize e encontre tudo em posição.

1. Copie seus ROMs no pen drive sob `RetroArch/roms/<sistema>/` (seção 3.9). Os nomes das pastas devem ser os nomes dos
   bancos de dados de RetroArch; o instalador cria os comuns.
2. Inicie **`UpdateRoms\UpdateRoms.exe` a partir do pen drive** (o instalador o colocou lá). Ele encontra o pen drive de
   onde está, mostra uma linha de estágio, uma barra de progresso e um log, e:
   - baixa o pacote de banco de dados de RetroArch quando o pen drive não tem um, e identifica cada ROM a partir dele -
     um jogo que o banco de dados conhece obtém seu nome correto;
   - escreve uma lista de jogos por sistema em `RetroArch/bin/playlists/` com os caminhos do console, mantendo tudo o que
     o RetroArch adicionou lá;
   - busca as capas de cada ROM que não tem uma dos servidores de miniaturas libretro em `RetroArch/bin/thumbnails/`.
3. Remova o pen drive com segurança e coloque-o de volta no console. A aba RetroArch do seletor de conjuntos lista cada
   sistema que tem jogos.

Rexecute após cada alteração nas pastas de ROM; uma pasta a qual nada mudou é pulada, portanto uma reexecução é rápida.
O log é `System/Logs/updateroms.log`. Um cartão Raspberry Pi em um leitor de cartão pode ser atualizado da mesma forma
(`UpdateRoms.exe <unidade> --target rpi`), embora um Pi o faça por conta própria se tiver uma rede.

### 5.2 LAN Share - Seus Jogos e Discos no Servidor em Sua Rede

**LAN Share** (`LanShare.exe`, na página da Loja em sua aba **Servidor LAN**) coloca seus jogos PS1 no servidor da Loja
em sua rede doméstica - um `abstored` em um Raspberry Pi, um NAS ou outro PC - e lê um disco PS1 na unidade de CD/DVD do
PC. A Loja no console, Pi ou PC o instala então de lá. Nada para instalar; as configurações são mantidas em
`%LOCALAPPDATA%\AutoBleem LAN Share\`.

![A janela do LAN Share](../images/en/lanshare.jpg)

1. **O servidor**: digite seu endereço (`http://<seu endereço>:<porta>`, como a Loja tem) e pressione **Conectar**. Seus
   jogos e quaisquer problemas que sua verificação encontrou são listados à esquerda. Para colocar jogos nele, dê um deles:
   - **Compartilhar** - a pasta de jogos do servidor conforme compartilhada na rede (Samba), por exemplo `\\raspberrypi\games`:
     LAN Share copia os jogos lá e pede ao servidor para verificar. O servidor em si permanece somente leitura.
   - **Token** - quando o servidor foi iniciado com `--allow-uploads`: seu token (o servidor o imprime na inicialização e o
     mantém em `<state>/upload-token`). LAN Share faz upload via HTTP, e um download interrompido continua de onde parou.
2. **Jogos neste PC**: escolha uma pasta de jogos (uma pasta por jogo), marque os jogos e pressione **Publique os jogos
   marcados**. **No servidor** diz se o servidor já tem um jogo (por seu número de série, senão por seu título); tal jogo
   nunca é enviado duas vezes. **Marque os não no servidor** marca o resto.
3. **Um disco**: coloque um disco PS1 na unidade e pressione **Leia um disco e publique-o**. O disco é lido completamente
   em um `.bin` + `.cue` (e um `.sbi` para um jogo LibCrypt, quando a unidade dá o subcanal), nomeado após seu título,
   verificado contra o bom dump conhecido (quando os bancos de dados são escolhidos) e publicado. Para um jogo em vários
   discos, marque **O jogo tem mais de um disco**: LAN Share pede cada disco seguinte e os publica juntos como um jogo.
4. **Remova do servidor...** tira os jogos selecionados do servidor. Nada é deletado: cada um é movido para uma pasta
   `.removed` ao lado dos jogos do servidor, e movê-lo de volta o recupera.

Os **bancos de dados** - a pasta de capas do AutoBleem (`coversU/P/J.db`) e `Sony - PlayStation.rdb` do RetroArch - dão os
títulos e a verificação de um disco lido; ambos são opcionais. **Também compartilhe os jogos neste PC com a Loja** (desabilitado
por padrão) serve a pasta neste PC para a Loja diretamente. A primeira vez, o Windows pergunta sobre seu firewall: permita
apenas redes privadas.

<!-- pagebreak -->

## 6. As Ferramentas de Console (PlayStation Classic)

Duas ferramentas para um pen drive de PlayStation Classic. Ambas desenham no tema e idioma do lançador e ambas são dirigidas
pelo controle - e, no assistente de controle, pelos botões anteriores do console. **PSC-Bios** é uma extensão fornecida com
o pacote de console: *Informações de Hardware* no menu do sistema a abre, e está na lista de extensões. **ABFlashKit** é um
aplicativo no conjunto de Aplicativos.

### 6.1 PSC-Bios

Uma extensão fornecida com o pacote de console, também disponível em um Raspberry Pi e pen drive PC. Ela é aberta do item
*Rede e Controles* do menu do sistema (ou da lista de extensões). Quando esta extensão está instalada mas desabilitada, o
item *Rede e Controles* no menu rápido e menu do sistema permanece acinzentado com uma nota "ative-o nas Extensões" - Cross
lá abre a lista de Extensões.

A tela de abertura mostra os fatos da máquina: hora, fuso horário, adaptadores de rede WiFi/Ethernet/Bluetooth com seus
endereços, e cada controle conectado com se tem um mapeamento. As partes de rede e Bluetooth precisam do kernel AutoBleem no
console (seção 6.2) ou ferramentas de sistema em um Raspberry Pi / pen drive PC; o assistente de controle funciona em qualquer
sistema.

![PSC-Bios: o hub de Rede e Controles](../images/en/pscbios-main.jpg)

- **Select - Rede WiFi** (kernel ou NetworkManager): o nome da rede (digitado ou escolhido de uma verificação), a senha, o
  modo do driver, e *Aplicar / Reiniciar Rede*. O fuso horário também é definido aqui. O endereço IP do console é mostrado
  uma vez conectado.
- **Square - Controles Bluetooth**: uma verificação para gamepads Bluetooth (DualShock 4, etc.), para emparelhar ou remover.
- **L1 - Emparelhamento DualShock 3**: conexão apenas USB para o primeiro DualShock 3, via o plugin sixaxis do kernel.
- **R1 - Mapeamento de Controle**: o assistente de mapeamento (abaixo).
- **Triangle - Sobre**, **Circle - voltar** ao lançador.

**O assistente de controle** mostra o controle conectado bruto - cada eixo, botão e chapéu como números, e uma imagem DualShock
que se ilumina quando você pressiona. Como o controle testado não pode ser confiável, o assistente é dirigido pelos **botões
anteriores do console**: **RESET** passa para o próximo controle, **OPEN** inicia o mapeamento (então responde cada pergunta -
pressione o botão iluminado na imagem, ou OPEN se o controle não tiver esse botão), **POWER** cancela ou sai. Manter Circle no
controle por 2 segundos sai do assistente (uma barra se preenche e a dica do rodapé diz "Mantenha pressionado por 2 s: Sair").
Enquanto o controle não tem um mapeamento, manter qualquer botão por 2 segundos o faz ("Mantenha qualquer botão pressionado por 2 s:
Sair"). Uma pressão breve é mapeada como de costume. Em um teclado, Esc / Espaço / Enter substituem POWER / RESET / OPEN. Ao
final, o novo mapeamento é adicionado para um teste e OPEN o salva com um nome de sua escolha; o lançador o carrega daí em diante.

![PSC-Bios: o assistente de mapeamento de controle](../images/en/pscbios-wizard.jpg)

### 6.2 ABFlashKit - o Kernel AutoBleem

O kernel AutoBleem é uma substituição opcional do kernel Linux do console: traz um relógio que funciona, dongles USB WiFi e
Bluetooth (para PSC-Bios e controles Bluetooth) e suporte aos botões anteriores que o emulador usa para pontos de salvamento.
ABFlashKit o instala, faz um backup do console primeiro, e pode colocar o console de volta ao stock via a recuperação própria
de Sony.

> **Esta ferramenta escreve na memória flash do console.** Um flash que é interrompido - a energia é cortada, o pen drive é
> removido - pode deixar o console incapaz de iniciar, e instalar um kernel personalizado anula sua garantia. Mantenha o console
> ligado e o pen drive dentro até que ele se reinicie sozinho. ABFlashKit abre neste aviso; *Entendo* continua, *Sair* sai.

![O menu do ABFlashKit](../images/en/abflashkit-menu.jpg)

- **Flash Kernel**: faz um backup de recuperação das partições do console no pen drive (`LBOOT.EPB`) se não existe ainda, o
  verifica e verifica a imagem do kernel, escreve o kernel e os arquivos de sistema do AutoBleem, e reinicializa. *Tudo pronto
  - quando a tela fica preta substitua o cabo de alimentação*: puxe o cabo de alimentação do console e plugue novamente.
- **Backup Completo**: todas as quatro partições para `LBOOT.EPB`, para uma recuperação depois (o backup anterior é substituído após
  uma pergunta).
- **Modo de Recuperação**: verifica que o backup é do stock, define a bandeira de recuperação e reinicializa na recuperação de
  Sony, que recupera o console de `LBOOT.EPB` no pen drive - o caminho de volta ao firmware original.

Uma barra de progresso sob cada etapa mostra quanto a ação avançou. A ferramenta se recusa a piscar um console que está executando
outro firmware personalizado (BleemSync, Project Eris): recupere-o para o stock primeiro.

<!-- pagebreak -->

## 7. Se Algo Correr Mal

- **Logs**: AutoBleem mantém seus logs em memória, então o pen drive não é escrito o tempo todo - eles alcançam `System/Logs/` no
  pen drive, cartão ou pasta de dados apenas quando algo correr mal: um crash do lançador, de um jogo PS1 ou do RetroArch os salva
  em `System/Logs/crash-<n>/` (os últimos três são mantidos), e o lançador diz uma vez quando volta. Para manter cada log, ative
  *Opções -> Diagnóstico -> Manter logs no pen drive* (a partir do próximo inicialização), ou crie um arquivo vazio `System/Logs/keep`
  em um PC. Em um Pi ou um PC, *Informações de Hardware* mostra onde os logs estão e Square os salva em `System/Logs/saved-<n>/`.
  Os arquivos: `autobleem.log` (o lançador), `launch.log` e `pcsx.log` (o inicialização de um jogo PS1 e a saída do emulador),
  `retroarch.log`, e - sempre no pen drive - `update.log` (uma atualização online) e `updateroms.log` (UpdateRoms).
- **Um jogo não está na prateleira**: verifique o layout da pasta (uma pasta por jogo, os formatos de imagem da seção 3.9). O
  *Gerenciador de Jogos* lista as pastas que a verificação recusou após os jogos, marcadas *Não adicionado*, com a razão; Square
  delete tal pasta. *Verificar Jogos* no menu do sistema reexecuta a verificação.
- **Sem capas**: os bancos de dados de capas não foram instalados (reexecute o instalador com eles marcados), ou, para jogos de
  RetroArch em um console, UpdateRoms não foi executado em um PC.
- **Um controle não faz nada ou tem seus botões misturados**: o assistente de controle de PSC-Bios (um console) o mapeia; em um Pi
  ou um PC a página de Informações de Hardware lista o que SDL vê.
- **O console mostra uma tela preta após um jogo**: AutoBleem reconstrói sua janela por conta própria (até três vezes); se ficar preta,
  mantenha o botão Power pressionado e ligue o console novamente.
- **Raspberry Pi**: `Alt+F2` dá um prompt no segundo console; SSH está habilitado desde o primeiro inicialização. `sudo journalctl -u
  autobleem` mostra o serviço do lançador; `sudo systemctl restart autobleem` o reinicializa. Um primeiro inicialização que não pôde
  terminar (sem rede) tenta novamente no próximo inicialização.
- **Windows**: `Esc` sai do lançador; a pasta de dados é a escolhida na instalação
  (`Documents\AutoBleem` por padrão), os logs estão em seu `System\Logs`.

AutoBleem é software livre (GNU GPL v3 ou posterior), sem garantia. Suporte e notícias: o servidor Discord ligado na tela Sobre, e
https://autobleem.retromenele.pl/.

# Interface — Menus e configurações

## Direção visual atual — aprovada em 2026-10-01

**Tático minimalista nos menus e no HUD**. A revisão v4 substitui somente a
direção Industrial refinado dos menus da v3, por pedido explícito do usuário.
Contrato atual: `visual/proposals/interface_visual_proposal_v4.md`;
referência da direção Tático: `visual/proposals/interface_visual_v3.html`.
Logo claro, texto branco/cinza, seleção por sublinhado e fundo translúcido.
Não há molduras ornamentais nem cartão grande de descrição à direita.
O título fica à esquerda nas telas largas; submenus ficam centralizados.
O nome exibido no título é **IRON SEED**, em ambos os idiomas. Os identificadores
técnicos GDP (executável, diretório de preferências e saves) permanecem iguais;
esta mudança de nome visual não exige migração nem altera a gameplay.
Validação do nome: modelo, navegação SDL e pixels OpenGL passaram (3/3,
Release); o título completo cabe em 1280×720, 960×540, 640×360 e 320×200.
Capturas em `visual/validation/interface_ptbr_title_1280x720.png` e
`visual/validation/iron_seed_title_320x200.png` registram a revisão.

Em 1280×720, cada linha do título mede **348×30 px**, antes 608×50 px.
A escala da tipografia/linhas é limitada a 2× também em Full HD/ultrawide;
páginas densas e telas pequenas adaptam a escala e altura para manter todas
as opções visíveis. Pausa, resultado e confirmação usam painéis translúcidos.
O fallback compacto inclui setas e colchetes legíveis. `ListMenuLayout`, com
flag de título compartilhada, continua sendo a única fonte da geometria tanto
para renderização quanto para cliques, incluindo `<` / `>`.
Não muda ordem das opções, estados, remapeamento, saves nem ações de teclado/mouse.
Todas as páginas possuem regressão de renderização nas quatro resoluções-base;
16:10 e ultrawide também têm validação estrutural dos layouts.

O atlas da fonte é incorporado ao executável: builds portáteis não dependem
de uma fonte no computador do usuário. A licença acompanha o pacote Windows.
O gerador offline em `tools/generate_ui_font.py` não é dependência do build.

### Fundo animado do título

`MenuBackdrop` amostra uma cena 3D de demonstração no renderer real do jogo,
com sala/material, props, câmera lenta e dois inimigos em patrulha visual.
Reutiliza os modelos e materiais já existentes; não adiciona nova textura.
Não é vídeo nem partida automática: seu relógio e trajetórias de apresentação
são independentes de `SessionSystem`, entidades, dungeon, seed, saves e IAs.
Portanto não treina/avalia cérebros nem gera progressão ou dano ao jogador.
A dungeon continua sendo criada somente ao confirmar um modo de jogo.

O relógio avança no título e seus submenus. Em configurações abertas a partir
da pausa, a demonstração fica estática; a sessão real continua pausada conforme
as regras existentes (inclusive o cronômetro específico de Speedrun).
O fundo respeita escala interna e materiais do perfil gráfico selecionado;
usa somente duas luzes, sem sombras. Interface e cliques ficam na resolução
nativa. Não promete um ganho de FPS sem medição no hardware do usuário.

## Fluxo atual — revisão de 2026-10-02

Esta revisão substitui o fluxo histórico descrito abaixo. `MenuFlow` é o dono
das páginas e comandos; `Application` aplica as ações ao jogo. O renderer recebe
somente um snapshot de textos, seleção e linhas habilitadas. Desenho e hit test
usam o mesmo `ListMenuLayout`, com conversão de coordenadas high-DPI.

O menu principal contém `TEST MODE`, `CAMPAIGN / STORY`, `SPEEDRUN`, `SETTINGS`
e `QUIT GAME`. WASD/setas, Enter, Esc, hover e clique funcionam em todos os
submenus. Esc volta uma página; no título, abre a confirmação de saída.
As teclas dos menus são fixas, independentemente dos bindings da gameplay.

### Test Mode

Arena única dedicada, de 32 × 40 m, com os cinco tipos de inimigo, coberturas
altas, plataformas baixas, munição, saúde, pistola, carabina, faca, sniper,
escopeta e SMG. As três novas armas estão dispostas na arena; use G para
liberar a primária antes de recolher outra. Sniper e escopeta têm modelos próprios
Used v1 na mão e no chão, em validação visual; SMG ainda reutiliza a carabina
aprovada até sua proposta. O HUD identifica cada tipo. Não gera
eventos de campanha nem termina a run quando o robô morre. A pausa permite
reiniciar a área ou voltar ao título. A arena usa as IAs ativas do mesmo jogo.
`--test-mode` abre diretamente a área, também aceitando seed e limite de frames.
`GDP_ENABLE_TEST_MODE=OFF` exclui o código da arena, o atalho CLI e sua entrada
do menu. A opção permanece ON nos builds de desenvolvimento atuais; deve ser
OFF no build de distribuição final.

### Campanha / História

Primeiro escolher um dos três slots. Cada slot oferece `CONTINUE`, `NEW GAME`
e `BACK`; Continue só fica ativo para checkpoint válido. Novo jogo pede
Easy/Normal/Hard e exige confirmação antes de substituir um slot ocupado,
inclusive quando seu arquivo está inválido ou incompatível. Cancel é a opção
inicial; nenhum arquivo é apagado durante a seleção.

São 10 andares com carry-over de HP, armor, munição, nível e progressão.
Cada andar gera de 7 a 10 salas e zero ou uma rota alternativa, conforme sua
seed; o mesmo slot/seed reproduz o mesmo layout. A missão do andar pede limpar
o evento da Arena, recuperar a chave da sala Secret ou ativar o terminal da START
e eliminar sua emboscada. O primeiro andar sempre
usa a Arena como introdução; nos seguintes, a escolha é determinística.
O save representa **a entrada do andar**, não o ponto exato de saída. Repetir
ou continuar restaura essa entrada. Matar o boss e cumprir a missão libera o
terminal da sala Boss, mas nenhuma das ações isolada encerra a partida nem
troca de andar. O objetivo passa a `ACTIVATE EXIT`
(`ATIVE A SAÍDA`). Aproximar-se do console existente e usar Interagir (E por
padrão, respeitando remapeamentos) sobe diretamente nos nove primeiros andares,
com os recursos atuais e sem menu de resultado intermediário. No décimo, usar
o terminal mostra a vitória. Concluir apaga somente o checkpoint
do slot ativo. Abrir configurações, mudar de modo ou voltar ao título não troca
nem apaga os outros saves.

O terminal fica bloqueado enquanto o boss estiver vivo ou a missão não estiver
concluída. O HUD apresenta a tarefa pendente em EN/PT-BR e, se o boss já morreu,
mostra `COMPLETE MISSION`/`CONCLUA A MISSÃO` ao mirar o console. A interação exige
estar na mesma sala, mirar o console, distância máxima de 2,25 m e ausência
de parede/cobertura entre o jogador e ele. O HUD informa bloqueio, subida ou
conclusão, em EN/PT-BR. Matar o boss pelo corredor conta para liberar esse terminal
se a missão também estiver concluída; o jogador ainda precisa alcançá-lo.
Standard/Speedrun continuam concluindo na
morte do boss; Test Mode permanece uma área sem vitória automática.

Dificuldade inicial: Easy 0,75×, Normal 1×, Hard 1,25×. Cada andar acrescenta
0,08 ao multiplicador; no andar 10 resulta em 1,47× / 1,72× / 1,97×.
O multiplicador alimenta população, comportamento, eventos e orçamento de loot.
Esses valores são uma base para playtest, não balanceamento final.
Salas elegíveis não iniciais/finais/secretas têm 25% de chance de um pickup
extra de arma; os pickups de campanha escolhem pistola (15%), carabina (40%),
SMG (20%), escopeta (17%) ou sniper (8%). Secret garante uma primária,
normalizando os pesos sem pistola. Duplicatas procedurais oferecem munição
à reserva da arma compatível, sem trocar seleção ou encher o carregador.
As armas usam as zonas e a separação de loot existentes, e a mesma seed reproduz
o conteúdo. Slot 1 seleciona a primária possuída, 2 a pistola e 3 a faca;
4/5 continuam reservados. O inventário aceita uma primária e uma pistola:
G permite trocar de primária sem apagar silenciosamente arma ou munição.
Campanha/História ainda reutiliza a progressão jogável: esta revisão não adiciona
roteiro, diálogos ou cenas narrativas.

### Speedrun

Submenu com Start Run, Leaderboard, Best Seed e Back. Toda run usa Hard; não
existe seletor de dificuldade e o cronômetro continua durante pausa e settings.
Leaderboard mantém os 10 menores tempos de vitória, em ordem, **neste computador**.
Não existe backend online nesta etapa. Best Seed usa a seed do primeiro colocado
e fica inativo sem recorde. Derrotas e runs de outros modos/dificuldades não
entram no placar. Regras de Speedrun 36 isolam os novos tempos Hard dos anteriores.

### Configurações e remapeamento

Settings abre Display / Audio / Controls / Language, no título ou na pausa, preservando
a origem ao voltar. Display conserva FOV, perfis gráficos, resolução, fullscreen
e VSync; Audio conserva Master/Effects/Ambient/UI. Controls conserva sensibilidade
e Invert Y e adiciona duas páginas de bindings.

Controls agora separa Keyboard / Mouse de Controller. O primeiro conserva
sensibilidade, inversão de Y e as duas páginas de teclas; o segundo oferece
duas páginas de botões remapeáveis com troca ao reutilizar um botão, reset
independente, toggle de vibração e intensidade de 0% a 100%. O analógico de
movimento pode ser esquerdo (padrão) ou direito (Southpaw), trocando junto o
analógico da câmera. Há sensibilidade horizontal/vertical e ao mirar,
inversão vertical e zonas mortas independentes para movimento/câmera.
Options permanece reservado para pausa/cancelamento da captura; a navegação
do menu continua no analógico esquerdo. As duas famílias de bindings e as
preferências de câmera são persistidas separadamente.

Teclado, cinco botões do mouse e os dois sentidos do scroll são aceitos.
Uma tecla já usada troca as duas ações, evitando bindings duplicados ou ações
sem comando. Shift/Ctrl/Alt direitos são aliases dos esquerdos. Esc cancela
captura e nunca é remapeável; F3 permanece reservado ao painel de desempenho.
Reset Defaults restaura os comandos originais (Ctrl agacha, Shift caminha
silenciosamente). Os slots 4 e 5 aparecem explicitamente como reservados,
sem adicionar equipamento ou item especial funcional nesta etapa.

As setas `<` / `>` clicáveis reduzem/aumentam configurações. Clicar no nome ou
valor só seleciona. A/D e setas laterais também ajustam, sem mudança no contrato
aprovado anteriormente. Alterações são aplicadas e gravadas imediatamente;
Esc/Back e shutdown repetem uma gravação que tenha ficado pendente.

Validação automatizada inclui modelos, persistência, teclas, mouse, slots,
retorno à pausa, placar e arena em uma aplicação SDL/OpenGL real offscreen.
Layouts/pixels também são exercitados em 1280×720, 960×540, 640×360 e 320×200.
A aprovação de ergonomia e balanceamento continua sendo feita em gameplay.

### Idioma: English / Português (BR)

Em `SETTINGS → LANGUAGE` / `CONFIGURAÇÕES → IDIOMA`, A/D, setas laterais ou
as setas clicáveis `<` / `>` alternam os dois idiomas nos dois sentidos.
A troca é imediata, inclusive na página aberta, e fica salva em `settings.toml`
(schema 10; introduzida no schema 8). Ao reabrir o jogo, a preferência é restaurada. Instalações novas
e configurações anteriores mantêm inglês até o usuário escolher PT-BR.

O catálogo em `src/game/Localization.cpp` cobre menus, controles, pausa,
confirmação, resultados, HUD, objetivos, avisos e barra do boss. Os comandos
dos menus são identificadores, nunca comparações com o texto traduzido.
Números, seeds, nomes literais das teclas e identificadores persistentes não
mudam. CLI e logs continuam em inglês; métricas de diagnóstico preservam
suas siglas, unidades e identificadores técnicos.

O atlas incorporado conserva a mesma fonte aprovada e agora inclui os acentos
do português. Medição e truncamento usam caracteres UTF-8 completos; células
do atlas reservam espaço para os acentos sem invadir o glifo vizinho.
As dimensões compactas e o fundo 3D aprovado são preservados nos dois idiomas.
Detalhes e evidências: [localization.md](localization.md).

## Histórico anterior à revisão

> **Estado:** menu principal funcional implementado na Fase 21, pausa in-game
> funcional desde a Fase 19 e menu de resultado integrado na Fase 64.
> Configurações runtime foram integradas na Fase 66, sua persistência segura na
> Fase 67, invert Y na Fase 68, fullscreen na Fase 69 e resolução na Fase 70.
> Feedback sonoro de navegação, confirmação e retorno foi integrado na Fase 62.
> Na Fase 63, todos os menus funcionais possuem paridade entre teclado e mouse.
> A Fase 74 adicionou o cronômetro determinístico da run ao HUD e ao resultado;
> a Fase 75 tornou Speedrun selecionável e a Fase 76 adicionou o melhor tempo
> local ao HUD e ao resultado. A Fase 77 separou próxima seed e repetição, e a
> Fase 78 adicionou acesso direto à seed do recorde. A Fase 79 integrou a
> Campanha de três andares ao título, HUD e resultado.
> A Fase 80 passou a preservar recursos e progressão em `NEXT FLOOR`.
> A Fase 81 adicionou `CONTINUE` persistente no início do andar atual.
> A Fase 111 aplicou uma linguagem industrial única ao título, pausa, resultado,
> configurações e confirmação, preservando os layouts usados pelo mouse.
> A tela `CONTROLS` foi adicionada ao título e à pausa como referência funcional
> dos comandos atuais, sem alterar bindings nem a linguagem visual aprovada.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Menu principal funcional

Todos os menus compartilham `UiVisualTheme` e o mesmo compositor de painel:
sombra, aro técnico, faixa semântica superior e cantos marcados. A seleção usa
fundo ciano escuro e barras nas duas extremidades, mantendo leitura clara tanto
por teclado quanto por hover. Perigo e sucesso substituem apenas a cor de
ênfase; posição e dimensões continuam vindo dos layouts funcionais existentes.

O bootstrap gráfico mostra `START RUN`, `CONTINUE`, `CAMPAIGN`, `SPEEDRUN`,
`BEST SEED`, `SETTINGS`, `CONTROLS` e `EXIT GAME` antes de gerar a dungeon.
`W/S` ou setas navegam com wrap, `Enter` confirma, o mouse seleciona e confirma
por clique, e `Esc` abre a confirmação de saída. A seleção inicial segura é
sempre `START RUN`.
Alterar efetivamente a seleção toca `UiNavigate`; confirmar por teclado ou mouse
toca `UiConfirm`. Movimento do ponteiro sem troca de opção não repete áudio.

Enquanto o menu está aberto, o mouse permanece livre e Player/Enemy não recebem
fixed update. Confirmar `START RUN` cria mundo, entidades, eventos, iluminação e
recursos gráficos da run e então captura o mouse. `EXIT GAME` fecha a aplicação
somente após confirmação, sem criar o mundo. `--headless` inicia diretamente
porque não possui interação.

`START RUN` inicia o modo Standard; `SPEEDRUN` inicia a mesma seed e gameplay,
mas mantém o cronômetro correndo durante pausa e configurações. `BEST SEED`
mostra o tempo do recorde e inicia um Speedrun na seed correspondente. Sem
recorde, mostra `--`, permanece visualmente inativo e não fecha o menu quando
confirmado. `CAMPAIGN` inicia três andares determinísticos: o HUD identifica
`FLOOR n/3`, os resultados intermediários oferecem `NEXT FLOOR` e o último
mostra `CAMPAIGN CLEAR`. Ranking continua como expansão futura do mesmo estado.
Ao avançar, os valores já apresentados de HP, Armor, munição, nível e XP são
restaurados no novo andar; posição e estados transitórios pertencem ao novo
mundo.

`CONTINUE Fn/3` aparece ativo quando existe um checkpoint de Campaign válido.
Ele restaura a entrada do andar indicado e só então gera seu mundo; sem arquivo,
com corrupção, schema futuro ou regras incompatíveis, exibe `CONTINUE --` e a
confirmação mantém o menu aberto. O checkpoint permanece disponível ao voltar
ao título ou jogar outro modo e é removido após `CAMPAIGN CLEAR`.

## Pausa in-game

`Esc` abre e fecha a pausa, sempre selecionando `RESUME` como opção segura.
`W/S` ou setas navegam com wrap e `Enter` confirma `RESUME`, `RESTART RUN`,
`SETTINGS`, `CONTROLS`, `MAIN MENU` ou `EXIT GAME`. Hover também seleciona qualquer opção e o botão primário confirma;
cliques fora dos botões não executam comandos. O mouse relativo é liberado no
menu e restaurado no retorno.
Enquanto o overlay está aberto, fixed update, look e comandos pendentes não
avançam; a UI apenas converte a seleção em um comando para `Application`.
O bus `UI` continua avançando para reproduzir `UiNavigate`, `UiConfirm` e
`UiBack`, enquanto os cursores de Effects/Ambient permanecem congelados.
`RESTART RUN` encerra explicitamente a sessão ativa antes de repetir a seed
atual, portanto o restart não é rejeitado por ainda estar em `Playing`. Em
Campaign, ele também restaura o snapshot da entrada do andar.

O desenho e o hit test usam o mesmo `PauseMenuLayout`. A posição lógica do
ponteiro é convertida para as dimensões reais do framebuffer antes da consulta,
mantendo os botões alinhados em escalas high-DPI e após redimensionamento.

`CONTROLS` abre uma página de consulta a partir do título ou da pausa. O jogo
permanece congelado quando a origem é a pausa; `Esc`, `Enter` ou clique em
`BACK` devolvem exatamente ao menu de origem. A página mostra os controles
efetivamente implementados, sem listar os slots 4/5 ainda reservados.
O painel rápido `Tab` também apresenta a seed exata da run para facilitar
reprodução de mapas e relatos de bugs.

## Menu de resultado

Victory e Defeat bloqueiam a simulação, alteram a iluminação da cena e abrem um
menu sobre o viewport. A opção inicial é `NEW RUN` após vitória ou `NEXT SEED`
após derrota; `REPLAY SEED` repete o mesmo mapa, `MAIN MENU` retorna ao título e
`EXIT GAME` abre a confirmação segura. `W/S`, setas, `Enter`, hover e clique
seguem o mesmo contrato dos demais menus, enquanto `Esc` abre a mesma confirmação.
Na Campaign, uma vitória antes do terceiro andar troca a ação inicial por
`NEXT FLOOR` e a repetição por `REPLAY FLOOR`; replay restaura os recursos com
que o jogador entrou naquele andar.

O mouse relativo é liberado na transição terminal. `ResultMenuLayout` é usado
simultaneamente pelo renderer e pelo hit test high-DPI. Os comandos de próxima
seed e repetição são distintos; `SessionSystem` continua sendo o dono da fase e
dos índices de tentativa/seed.

## Retorno ao título

`MAIN MENU` na pausa encerra primeiro a run ativa; no resultado, reutiliza o
estado terminal já registrado. Nos dois casos, `SessionSystem::restart` prepara
a próxima seed antes da limpeza. Mundo, entidades, eventos, colisão, feedback,
vozes e recursos gráficos da run são descartados, mas janela, contexto OpenGL e
processo permanecem ativos. O título reaparece com `START RUN` selecionado e
mouse livre.

## Navegação

Cima/baixo, esquerda/direita, confirmar e voltar continuam sendo ações lógicas
independentes do dispositivo. Teclado e mouse já mapeiam para os mesmos comandos
e cues; gamepad permanece como expansão futura.

## Cronômetro da run

Durante uma run Standard, o canto superior direito mostra `TIME MM:SS.cc`; em
Speedrun, mostra `SPEEDRUN MM:SS.cc`, sempre abaixo do
objetivo. O valor vem dos ticks fixos da sessão e não do tempo de renderização;
por isso permanece estável com qualquer FPS. Pausa e configurações congelam a
contagem Standard, mas não interrompem Speedrun. Victory e Defeat preservam o
tempo final visível, e a próxima run recomeça em zero. Runs acima de uma hora
usam `HH:MM:SS.cc`.

Quando existe um recorde local e o modo ativo é Speedrun, uma segunda linha
`BEST MM:SS.cc` aparece sob o cronômetro e o título da janela repete esse valor.
Victory mostra `NEW RECORD` no painel central somente quando o primeiro recorde
foi criado ou o tempo anterior foi superado; resultados piores continuam
mostrando `BEST`. A informação não aparece em Standard.

## Configurações runtime

`SETTINGS` pode ser aberto pelo título ou pela pausa. Os valores atuais são:

- Master, Effects, Ambient e UI: 0–100%, passos de 10%;
- sensibilidade: 30–200% da base de 0,12, passos de 10%;
- Invert Y: `OFF/ON`, com esquerda/direita escolhendo o estado;
- FOV vertical: 60–100 graus, passos de 2 graus;
- qualidade: `POTATO`, `LOW`, `MEDIUM`, `HIGH` ou `ULTRA`;
- resolução: 960×540, 1280×720, 1600×900 ou 1920×1080;
- Fullscreen: `OFF/ON`, aplicado à janela sem reiniciar a run;
- VSync: `OFF/ON`, aplicado ao contexto OpenGL ativo sem reiniciar a run.

W/S e setas verticais escolhem a linha. A/D e setas laterais ajustam. No mouse,
cada linha possui botões `<` e `>` independentes: o primeiro reduz e o segundo
aumenta; clicar no nome ou no valor somente seleciona a linha. `BACK`, `Enter`
sobre `BACK` ou `Esc` retorna exatamente ao título ou à pausa de origem. Look e
fixed update permanecem congelados enquanto a tela está aberta.

Os valores são aplicados imediatamente e preservados entre runs. Ao retornar por
`BACK` ou `Esc`, eles são gravados no `settings.toml` do diretório de preferências
da plataforma. Se a aplicação for encerrada diretamente, o shutdown também tenta
salvar qualquer alteração pendente. Na próxima execução, o carregamento acontece
antes da configuração de áudio, Player e câmera. Invert Y é aplicado somente ao
delta vertical do mouse. Fullscreen compara a preferência com o estado real da
janela e solicita a troca ao SDL; se o backend rejeitar, o jogo permanece aberto
e restaura/persiste o estado real antes de registrar o problema. Resolução e
fullscreen são independentes: fullscreen usa a resolução do desktop e preserva
o preset escolhido para aplicá-lo ao voltar ao modo janela. A troca de preset
redimensiona e recentraliza a janela; o renderer consulta o framebuffer real a
cada quadro. VSync solicita intervalo de troca 1 quando ligado e 0 quando
desligado; se o backend não oferecer o modo pedido, a falha é registrada sem
encerrar a aplicação. Qualidade altera imediatamente somente o orçamento visual
do mundo; HUD, menus, simulação, colisão e geração permanecem iguais. `POTATO` e
`LOW` desativam sombras e materiais detalhados; os demais perfis aumentam escala,
luzes e sombras progressivamente. Os dois perfis mínimos ainda desenham a sala
vizinha direta, preservando a continuidade ao atravessar portas. Remapeamento
continua para uma fase posterior.

## Confirmações

`EXIT GAME` no título, pausa ou resultado abre o mesmo diálogo de confirmação,
sem destruir o estado do menu de origem. `CANCEL` é sempre a opção inicial;
WASD, setas, `Enter`, `Esc`, hover e clique estão disponíveis, e somente
confirmar `EXIT` encerra o processo. `Esc` e `CANCEL` fecham o diálogo e devolvem
o controle ao menu preservado. O evento de fechamento emitido diretamente pelo
sistema operacional continua sendo atendido sem uma segunda confirmação.

Sobrescrever save e apagar dados deverão usar o mesmo princípio quando esses
fluxos forem implementados: confirmação com opção segura selecionada inicialmente.

## Separação

Estados de menu bloqueiam updates de Player/Enemy. UI não possui regras de gameplay.

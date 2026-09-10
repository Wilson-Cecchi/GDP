# Interface — Menus e configurações

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
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Menu principal funcional

Todos os menus compartilham `UiVisualTheme` e o mesmo compositor de painel:
sombra, aro técnico, faixa semântica superior e cantos marcados. A seleção usa
fundo ciano escuro e barras nas duas extremidades, mantendo leitura clara tanto
por teclado quanto por hover. Perigo e sucesso substituem apenas a cor de
ênfase; posição e dimensões continuam vindo dos layouts funcionais existentes.

O bootstrap gráfico mostra `START RUN`, `CONTINUE`, `CAMPAIGN`, `SPEEDRUN`,
`BEST SEED`, `SETTINGS` e `EXIT GAME` antes de gerar a dungeon.
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
`SETTINGS`, `MAIN MENU` ou `EXIT GAME`. Hover também seleciona qualquer opção e o botão primário confirma;
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

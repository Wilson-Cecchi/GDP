# Interface — Menus e configurações

> **Estado:** menu principal funcional implementado na Fase 21, pausa in-game
> funcional desde a Fase 19 e menu de resultado integrado na Fase 64.
> Configurações permanecem especificadas.
> Feedback sonoro de navegação, confirmação e retorno foi integrado na Fase 62.
> Na Fase 63, todos os menus funcionais possuem paridade entre teclado e mouse.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Menu principal funcional

O bootstrap gráfico mostra `START RUN` e `EXIT GAME` antes de gerar a dungeon.
`W/S` ou setas navegam com wrap, `Enter` confirma, o mouse seleciona e confirma
por clique, e `Esc` encerra. A seleção inicial segura é sempre `START RUN`.
Alterar efetivamente a seleção toca `UiNavigate`; confirmar por teclado ou mouse
toca `UiConfirm`. Movimento do ponteiro sem troca de opção não repete áudio.

Enquanto o menu está aberto, o mouse permanece livre e Player/Enemy não recebem
fixed update. Confirmar `START RUN` cria mundo, entidades, eventos, iluminação e
recursos gráficos da run e então captura o mouse. `EXIT GAME` fecha a aplicação
sem criar o mundo. `--headless` inicia diretamente porque não possui interação.

Campanha, Speedrun, Ranking e Configurações continuam como expansão futura do
mesmo estado, sem antecipar sistemas ainda não implementados.

## Pausa in-game

`Esc` abre e fecha a pausa, sempre selecionando `RESUME` como opção segura.
`W/S` ou setas navegam com wrap e `Enter` confirma `RESUME`, `RESTART RUN`,
`MAIN MENU` ou `EXIT GAME`. Hover também seleciona qualquer opção e o botão primário confirma;
cliques fora dos botões não executam comandos. O mouse relativo é liberado no
menu e restaurado no retorno.
Enquanto o overlay está aberto, fixed update, look e comandos pendentes não
avançam; a UI apenas converte a seleção em um comando para `Application`.
O bus `UI` continua avançando para reproduzir `UiNavigate`, `UiConfirm` e
`UiBack`, enquanto os cursores de Effects/Ambient permanecem congelados.
`RESTART RUN` encerra explicitamente a sessão ativa antes de pedir a próxima
seed, portanto o restart não é rejeitado por ainda estar em `Playing`.

O desenho e o hit test usam o mesmo `PauseMenuLayout`. A posição lógica do
ponteiro é convertida para as dimensões reais do framebuffer antes da consulta,
mantendo os botões alinhados em escalas high-DPI e após redimensionamento.

## Menu de resultado

Victory e Defeat bloqueiam a simulação, alteram a iluminação da cena e abrem um
menu sobre o viewport. A opção segura é `NEW RUN` após vitória ou `RETRY` após
derrota; `MAIN MENU` retorna ao título e `EXIT GAME` encerra a aplicação. `W/S`,
setas, `Enter`, hover e clique seguem o mesmo contrato dos demais menus, enquanto
`Esc` preserva a saída direta.

O mouse relativo é liberado na transição terminal. `ResultMenuLayout` é usado
simultaneamente pelo renderer e pelo hit test high-DPI. Confirmar a primeira
opção apenas solicita o reinício existente; `SessionSystem` continua sendo o
dono da fase e da derivação da próxima seed.

## Retorno ao título

`MAIN MENU` na pausa encerra primeiro a run ativa; no resultado, reutiliza o
estado terminal já registrado. Nos dois casos, `SessionSystem::restart` prepara
a próxima seed antes da limpeza. Mundo, entidades, eventos, colisão, feedback,
vozes e recursos gráficos da run são descartados, mas janela, contexto OpenGL e
processo permanecem ativos. O título reaparece com `START RUN` selecionado e
mouse livre.

## Navegação

Cima/baixo, confirmar e voltar continuam sendo ações lógicas independentes do
dispositivo. Teclado e mouse já mapeiam para os mesmos comandos e cues; gamepad
permanece como expansão futura.

## Configurações

FOV, sensibilidade, invert Y, esquema de controles, resolução, fullscreen e
volumes. Master, Effects, Ambient e UI já existem como buses independentes; a
tela de configuração permanece planejada.

## Confirmações

Sobrescrever save, apagar dados ou sair deve exigir confirmação com opção segura selecionada inicialmente.

## Separação

Estados de menu bloqueiam updates de Player/Enemy. UI não possui regras de gameplay.

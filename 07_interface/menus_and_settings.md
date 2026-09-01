# Interface — Menus e configurações

> **Estado:** menu principal funcional implementado na Fase 21 e pausa in-game
> funcional desde a Fase 19. Configurações permanecem especificadas;
> Victory/Defeat já usam banner no viewport e `Enter` para reconstruir a run.
> Feedback sonoro de navegação, confirmação e retorno foi integrado na Fase 62.
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
`W/S` ou setas navegam com wrap e `Enter` confirma `RESUME`, `RESTART RUN` ou
`EXIT GAME`. O mouse relativo é liberado no menu e restaurado no retorno.
Enquanto o overlay está aberto, fixed update, look e comandos pendentes não
avançam; a UI apenas converte a seleção em um comando para `Application`.
O bus `UI` continua avançando para reproduzir `UiNavigate`, `UiConfirm` e
`UiBack`, enquanto os cursores de Effects/Ambient permanecem congelados.
`RESTART RUN` encerra explicitamente a sessão ativa antes de pedir a próxima
seed, portanto o restart não é rejeitado por ainda estar em `Playing`.

## Resultado provisório

Victory e Defeat bloqueiam a simulação, alteram a iluminação da cena e mostram
`ENTER NEW RUN/RETRY` no viewport. `Enter` reconstrói uma run com seed derivada;
`Esc` encerra. A futura tela de resultado deve representar o mesmo
`SessionPhase`, sem duplicar a regra de vitória ou restart na UI.

## Navegação

Cima/baixo, confirmar, voltar; gamepad e mouse podem mapear para as mesmas ações
lógicas e os mesmos cues, sem criar regras sonoras específicas do dispositivo.

## Configurações

FOV, sensibilidade, invert Y, esquema de controles, resolução, fullscreen e
volumes. Master, Effects, Ambient e UI já existem como buses independentes; a
tela de configuração permanece planejada.

## Confirmações

Sobrescrever save, apagar dados ou sair deve exigir confirmação com opção segura selecionada inicialmente.

## Separação

Estados de menu bloqueiam updates de Player/Enemy. UI não possui regras de gameplay.

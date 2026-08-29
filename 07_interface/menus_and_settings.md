# Interface — Menus e configurações

> **Estado:** pausa in-game funcional implementada na Fase 19. Menu principal e
> configurações permanecem especificados; Victory/Defeat já usam banner no
> viewport e `Enter` para reconstruir a run.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Menu principal

Campanha, Speedrun, Ranking, Configurações e Sair.

## Pausa in-game

`Esc` abre e fecha a pausa, sempre selecionando `RESUME` como opção segura.
`W/S` ou setas navegam com wrap e `Enter` confirma `RESUME`, `RESTART RUN` ou
`EXIT GAME`. O mouse relativo é liberado no menu e restaurado no retorno.
Enquanto o overlay está aberto, fixed update, look e comandos pendentes não
avançam; a UI apenas converte a seleção em um comando para `Application`.

## Resultado provisório

Victory e Defeat bloqueiam a simulação, alteram a iluminação da cena e mostram
`ENTER NEW RUN/RETRY` no viewport. `Enter` reconstrói uma run com seed derivada;
`Esc` encerra. A futura tela de resultado deve representar o mesmo
`SessionPhase`, sem duplicar a regra de vitória ou restart na UI.

## Navegação

Cima/baixo, confirmar, voltar; gamepad e mouse podem mapear para as mesmas ações lógicas.

## Configurações

FOV, sensibilidade, invert Y, esquema de controles, resolução, fullscreen e volumes.

## Confirmações

Sobrescrever save, apagar dados ou sair deve exigir confirmação com opção segura selecionada inicialmente.

## Separação

Estados de menu bloqueiam updates de Player/Enemy. UI não possui regras de gameplay.

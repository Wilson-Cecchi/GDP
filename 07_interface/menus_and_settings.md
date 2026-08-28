# Interface — Menus e configurações

> **Estado:** menu principal e configurações permanecem especificados; o estado
> terminal funcional de Victory/Defeat usa o título da janela e `Enter` na Fase
> 13 enquanto a tela dedicada ainda não existe.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Menu principal

Campanha, Speedrun, Ranking, Configurações e Sair.

## Resultado provisório

Victory e Defeat bloqueiam a simulação, alteram a iluminação da cena e mostram
`ENTER: NEW RUN/RETRY` no título. `Enter` reconstrói uma run com seed derivada;
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

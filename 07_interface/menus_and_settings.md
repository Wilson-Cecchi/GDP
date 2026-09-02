# Interface — Menus e configurações

> **Estado:** menu principal funcional implementado na Fase 21, pausa in-game
> funcional desde a Fase 19 e menu de resultado integrado na Fase 64.
> Configurações runtime foram integradas na Fase 66, sua persistência segura na
> Fase 67, invert Y na Fase 68, fullscreen na Fase 69 e resolução na Fase 70.
> Feedback sonoro de navegação, confirmação e retorno foi integrado na Fase 62.
> Na Fase 63, todos os menus funcionais possuem paridade entre teclado e mouse.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Menu principal funcional

O bootstrap gráfico mostra `START RUN`, `SETTINGS` e `EXIT GAME` antes de gerar a dungeon.
`W/S` ou setas navegam com wrap, `Enter` confirma, o mouse seleciona e confirma
por clique, e `Esc` encerra. A seleção inicial segura é sempre `START RUN`.
Alterar efetivamente a seleção toca `UiNavigate`; confirmar por teclado ou mouse
toca `UiConfirm`. Movimento do ponteiro sem troca de opção não repete áudio.

Enquanto o menu está aberto, o mouse permanece livre e Player/Enemy não recebem
fixed update. Confirmar `START RUN` cria mundo, entidades, eventos, iluminação e
recursos gráficos da run e então captura o mouse. `EXIT GAME` fecha a aplicação
sem criar o mundo. `--headless` inicia diretamente porque não possui interação.

Campanha, Speedrun e Ranking continuam como expansões futuras do mesmo estado.

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

Cima/baixo, esquerda/direita, confirmar e voltar continuam sendo ações lógicas
independentes do dispositivo. Teclado e mouse já mapeiam para os mesmos comandos
e cues; gamepad permanece como expansão futura.

## Configurações runtime

`SETTINGS` pode ser aberto pelo título ou pela pausa. Os valores atuais são:

- Master, Effects, Ambient e UI: 0–100%, passos de 10%;
- sensibilidade: 30–200% da base de 0,12, passos de 10%;
- Invert Y: `OFF/ON`, com esquerda/direita escolhendo o estado;
- FOV vertical: 60–100 graus, passos de 2 graus;
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
encerrar a aplicação. Remapeamento continua para uma fase posterior.

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

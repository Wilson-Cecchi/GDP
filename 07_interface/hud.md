# Interface — HUD

> **Estado:** HUD funcional implementado na Fase 17. HP, munição, chaves, nível,
> XP, objetivo, evento, resultado e prompts contextuais aparecem no viewport;
> a Fase 18 acrescenta feedback temporário de combate e eventos e a Fase 58
> integra Armor. A Fase 82 expõe a pressão do andar da Campanha, a Fase 84
> identifica Pistol ou Carbine e a Fase 85 indica Overdrive ativo. O título permanece somente
> como apoio de diagnóstico. A Fase 111 unificou o acabamento industrial do HUD
> e dos menus sem alterar sua geometria funcional. A Fase 112 acrescentou o
> viewmodel metálico da arma sem ocupar a camada de hit tests da interface.
> A Fase 117 acrescenta um overlay técnico opcional de desempenho, alternado
> por `F3`, sem interferir no HUD normal ou na simulação. Na Fase 118, o painel
> também identifica o perfil gráfico e a escala interna ativos. Na Fase 120,
> ele separa submissão da CPU, execução da GPU e espera de apresentação.
> A Fase 145 implementou a variante Rail v1 aprovada para o HUD de gameplay;
> a aceitação visual final em partida ainda depende do usuário.
> O minimapa funcional usa a mesma moldura Rail, sem criar textura ou receita
> artística nova.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Direção

Interface militar/industrial. Em widescreen, o centro preserva uma área de gameplay
conceitualmente 4:3 e as laterais recebem informação.

O painel esquerdo apresenta HP e Armor em linhas e barras independentes. Armor
usa azul técnico, é exibida mesmo vazia e permite perceber exatamente quando o
dano começou a alcançar o HP.

## Layout

```text
┌──────────────┬──────────────────────────────┬──────────────┐
│ HP / ARMOR   │                              │ SETOR        │
│ AMMO / ARMA  │         VIEWPORT 4:3         │ OBJETIVO     │
│ STATUS       │           GAMEPLAY           │ SCANNER      │
│              │                              │ ALERTAS      │
└──────────────┴──────────────────────────────┴──────────────┘
```

## Regra

O viewport domina. Painéis organizam informação e não competem com combate. Alertas
contextuais são temporários.

O passe atual ocorre após o blit e não usa a projeção 3D. HP, munição e chaves
ocupam o canto inferior esquerdo; nível e XP, o inferior direito; objetivo e
evento, o superior direito. `VICTORY`/`DEFEAT` preservam o banner central. Os
prompts `E OPEN DOOR`, `DOOR LOCKED`, `E USE TERMINAL` e `R RELOAD` aparecem
somente quando a mesma ação é válida ou necessária na simulação.

`UiVisualTheme` define uma única hierarquia: fundo quase preto, painel azul
grafite, aro técnico, texto principal, texto secundário e ciano de interação.
Aviso, perigo e sucesso têm laranja, vermelho e verde exclusivos. Menus continuam
com o painel industrial da Fase 111; o HUD de gameplay usa painéis Rail finos,
sem sombra pesada nem textura de tela, conforme a
[proposta A aprovada](../visual/proposals/hud_visual_proposal_v1.md). O grupo
inferior esquerdo tem quatro módulos em 2×2: HP e Armor em cima, arma/munição e
chaves embaixo. Cada recurso mantém texto e barra independentes, sem sobrepor
um ao outro. O canto inferior direito conserva nível e XP. Objetivo e tempo
compartilham um painel superior direito; o evento aparece logo abaixo, com
espaçamento explícito. Prompts e avisos temporários usam a mesma moldura leve.
Nada disso amplia hit targets nem ocupa o centro do combate.

## Minimap

Um painel no canto superior esquerdo mostra o setor em coordenadas reais do
mundo: footprint proporcional ao tamanho real de cada sala, ciano para a sala
atual, marcação clara para salas visitadas,
marcação discreta para saídas vizinhas descobertas e linhas que copiam os
pontos reais de cada corredor, inclusive curvas, em vez de traçar um L entre
centros de salas. O ponto do jogador e seu indicador de direção acompanham o
deslocamento dentro de salas e corredores. Isso mantém o marcador dentro do
contorno da sala enquanto o jogador estiver nela, em vez de aparentar um
deslocamento causado por ícones de tamanho fixo. Salas além das saídas conhecidas
ficam ocultas; a sala Secret não recebe ícone especial antes de ser visitada.

A lista de salas visitadas pertence à run, é inicializada com START e zerada ao
reiniciar, sair ao menu ou avançar de andar. O renderer recebe apenas um snapshot
limitado a 32 salas e 64 conexões, sem consultar o mundo nem alterar a geração.
Em telas pequenas, o mapa se recolhe quando o painel técnico `F3` ocupa o
canto superior esquerdo. A composição reutiliza as cores e os painéis já
aprovados da HUD Rail; não revisa materiais ou texturas.

A linha de munição usa `PISTOL atual/reserva` ou `CARBINE atual/reserva`. Essa
identidade vem pronta no snapshot visual e muda no mesmo fixed update em que o
pickup ou uma seleção por `1`, `2` ou roda é aceita. A entrada visual por baixo
é repetida na troca, sem fabricar disparo ou alterar os hit tests do HUD. Desde
o início da Fase 139, sua força vem do timer autoritativo de `Weapon`: `0,34 s`
para Pistol e `0,48 s` para Carbine. A arma só aceita disparo ou recarga quando
o mesmo movimento visual termina.

Na Fase 140, os tempos atuais caem para `0,24 s` na Pistol e `0,32 s` na
Carbine. Se teclado ou roda escolherem o slot oposto durante a entrada, o HUD e
o viewmodel mudam de identidade no mesmo fixed update e começam o novo movimento
por baixo; não esperam a animação anterior terminar.

Durante Overdrive, a mesma linha acrescenta `OD` entre a identidade da arma e a
munição. O título diagnóstico mostra também os segundos inteiros restantes; o
renderer recebe apenas o booleano de apresentação e nunca avança o timer.

O viewmodel é composto em perspectiva antes deste passe 2D: Pistol e Carbine
possuem silhuetas próprias, recuo e flash acompanham disparos consumidos, recarga
faz um arco curto e a troca de arma entra por baixo. O detalhe ciano passa a
magenta durante Overdrive. A retícula e os painéis permanecem por cima, e menus
pausam a animação junto com a simulação.

Desde a Fase 137, o arco de recarga recebe o progresso real de `Weapon`, com
duração própria para Pistol e Carbine. O prompt `R RELOAD` desaparece durante a
ação já iniciada, e a munição exibida só muda quando o fixed update conclui a
transferência.

A retícula abre conforme o estado transitório de spread. Pistol continua
semiautomática; Carbine pode manter o botão primário pressionado. A interface
somente apresenta bloom e munição: cadência, trigger mode e dry fire permanecem
autoritativos no gameplay.

Durante a Campanha, a linha do cronômetro inclui `FLOOR n/3` e `THREAT 100%`,
`110%` ou `120%`. O renderer recebe o percentual pronto em `HudRenderData`; ele
não calcula dificuldade nem altera a simulação.

## Diagnóstico de desempenho

`F3` alterna um painel no canto superior esquerdo com FPS estimado, média, P95,
pior frame, percentual fora do budget de 60 Hz, update, render, espera de
apresentação, tempo real da GPU, draw calls, triângulos, entidades visíveis,
luzes, sombras, caches e tempos de carga. Verde indica P95 de até 17 ms;
laranja, até 34 ms; vermelho, acima disso. `GPU --` indica driver sem suporte ou
os quatro frames iniciais de aquecimento. O overlay recebe valores agregados
prontos e não mede, consulta ou altera o gameplay.

## Feedback temporário

Dano recebido acende as bordas sem cobrir a área central. Acertos usam quatro
marcas claras ao redor da mira e eliminações mudam o marcador para vermelho.
Coleta, início de evento, nova onda, setor limpo e level-up usam mensagens
curtas com duração controlada pelo fixed update; a UI apenas apresenta o
snapshot e não controla a simulação.
`G` mostra `WEAPON DROPPED` ao soltar uma arma ou
`KEEP ONE GUN / NEED SPACE` quando a ação não é válida. Esses avisos não
alteram o inventário; a simulação decide se houve descarte.

## Painel rápido

Segurar `Tab` mostra placar, abates, eventos, loot e tempo da run. Os números
vêm do snapshot de progressão; abrir o painel não pausa nem altera o jogador.
Ele é separado do diagnóstico `F3` e não aparece sobre menus.
Com a lanterna ligada, uma pequena indicação `F LIGHT ON` fica na borda
inferior em resoluções normais, sem ocupar o centro da mira.

## Escala

Layout deve funcionar em 16:9, 16:10 e ultrawide sem depender de pixels fixos.

## Confronto do boss

Após `SessionState::bossEngaged`, um painel próprio no topo central recebe
uma barra de HP, o percentual de vida e a fase do IndustrialRobot. As fases são
calculadas pela mesma função do combate, `industrialRobotPhase`: `RANGE` acima
de 66%, `PRESSURE` acima de 33% até 66% e `PURSUIT` até 33%. O percentual usa a
vida máxima daquela instância, inclusive a escala de andar da Campaign.

`buildBossHud` transforma apenas o estado autoritativo em um snapshot visual.
O boss pode ser engajado no corredor e atravessar portas abertas; mudar sua
sala lógica não remove a indicação. Antes do engajamento, inimigos inativos,
boss morto ou estado terminal não revelam uma barra. Pausa conserva o mesmo
valor e reiniciar exige um novo engajamento. Saúde inválida é rejeitada antes
de chegar ao renderer.

O bloco usa a moldura, texto e cor de alerta da HUD Rail existente. Sua largura
permanece fixa entre as fases, com barra centralizada e preenchimento da
esquerda para a direita. Em telas compactas, título e fase usam duas linhas.
Os painéis de objetivo, evento, mapa e desempenho reservam espaço abaixo dessa
faixa durante o confronto. Se faltar espaço para o mapa sem cobrir os recursos,
ele se recolhe. Não introduz textura, material, regra de dano ou cue novo.
As verificações de pixels e capturas de fixture cobrem 1280×720, 960×540,
640×360 e 320×200; a avaliação em partida continua no playtest.

# Progresso de Chave + Arena na Campanha

Data: 2026-10-12. Estado: **implementado; testes de frontend e pixels OpenGL aprovados**.
A aceitação visual em partida permanece pendente.

## Contexto

`RecoverAndSecure` guarda `keyRecovered` e `arenaSecured` separadamente e conclui
a missão quando ambos são verdadeiros. Antes desta mudança, a linha da missão
no painel `Tab` mostrava apenas `MISSION: KEY + ARENA` /
`MISSÃO: CHAVE + ARENA` até a conclusão. O objetivo breve da HUD e o
marcador do mapa mudam segundo a etapa pendente. As linhas de boss e saída
continuam independentes.

## Texto implementado

Somente a linha da missão do painel `Tab` muda em `RecoverAndSecure`.
A ordem é fixa, Chave depois Arena, em todos os estados; os números
explicam qualquer ordem de execução. `1/1` significa etapa concluída, não
quantidade de chaves do inventário. O total corresponde à soma dos dois estados.

| Estado real | English | Português (BR) |
| --- | --- | --- |
| Nenhum, 0/2 | `MISSION 0/2: KEY 0/1 ARENA 0/1` | `MISSÃO 0/2: CHAVE 0/1 ARENA 0/1` |
| Chave primeiro, 1/2 | `MISSION 1/2: KEY 1/1 ARENA 0/1` | `MISSÃO 1/2: CHAVE 1/1 ARENA 0/1` |
| Arena primeiro, 1/2 | `MISSION 1/2: KEY 0/1 ARENA 1/1` | `MISSÃO 1/2: CHAVE 0/1 ARENA 1/1` |
| Ambos, 2/2 | `MISSION 2/2: KEY 1/1 ARENA 1/1` | `MISSÃO 2/2: CHAVE 1/1 ARENA 1/1` |

Manter a linha `2/2` após concluir a missão até a troca de andar, inclusive
durante o combate com o boss. O objetivo breve pode então dizer `DEFEAT BOSS` /
`DERROTE O CHEFE`, ou `ACTIVATE EXIT` / `ATIVE A SAÍDA`, conforme o estado
autoritativo. As linhas existentes `BOSS ALIVE` / `BOSS DOWN` e `EXIT LOCKED` /
`EXIT READY` continuam abaixo, traduzidas como hoje. A morte antecipada do boss
não altera os números da missão; a saída só muda quando a simulação a libera.

## Local e geometria

- Exibir ao segurar `Tab`, dentro da linha da missão já existente. Não acrescentar
  uma linha, painel ou indicador persistente à HUD. Assim, o centro de combate e
  as áreas de objetivo, minimapa e boss mantêm a composição tática minimalista.
- Em 320×200, `hudScale` e `panelScale` são 1. O texto PT-BR mais longo tem
  31 caracteres visíveis: 186 px na fonte bitmap de avanço 6 px. Somando 16 px
  de largura para o painel, ocupa aproximadamente 202 px e cabe nos 308 px
  disponíveis entre as margens. A altura atual da lista de 11 linhas permanece
  96 px; posição e regra de recuo do mapa permanecem as atuais.
- Em 1280×720, `panelScale` é 2; a mesma linha ocupa 372 px de texto mais 32 px
  de painel, cerca de 404 px. A largura continua calculada pelo texto traduzido
  antes de posicionar o painel. Não fazer truncamento independente de bytes;
  usar o comprimento UTF-8 em caracteres que o renderer já aplica.
- Tratar números como informação principal e cor apenas como reforço opcional.
  Se houver cor de sucesso na implementação, manter contraste e leitura completa
  em monocromia; não usar animação contínua ou ícones que dependam de FOV.

## Implementação

O snapshot `HudRenderData` contém `campaignKeyRecovered` e
`campaignArenaSecured` (booleans, falsos por padrão), preenchidos em
`Application.cpp` diretamente de `CampaignMissionState` no modo Campanha.
`campaignRouteMission` identifica o tipo e evita mostrar a linha em outras
missões. A primeira etapa não é derivada de `campaignMission`, da
contagem geral `keys`, do marcador de mapa ou do estado do boss: após Arena
primeiro, o objetivo breve pode continuar `RECOVER KEY` e não indica que a Arena
já foi cumprida. O renderer soma os booleans apenas para apresentação. O catálogo
de `Localization.cpp` contém as quatro frases completas, traduzidas e medidas no
idioma selecionado. Save, regras de missão, objetivo breve, terminal e critério
de saída permanecem com seus contratos atuais.

## Validação

O teste de frontend verifica os quatro textos exatos em EN e PT-BR e a largura
de 31 caracteres UTF-8 em português, inclusive `Ã` em `MISSÃO`.
`./build/debug/gdp_frontend_tests` terminou com código 0; o CTest
`frontend_menu_systems` passou em 0,04 s. `campaign_systems`,
`campaign_checkpoint_persistence_systems` e `architecture_boundaries` também
passaram na integração de 2026-10-12.

O fixture OpenGL passou com `gdp_renderer_gl_tests --campaign-hud-only`. Ele
cobre o painel `Tab` em 320×200, 640×360, 960×540 e 1280×720, nos dois
idiomas, com 0/2, ambas as variantes de 1/2 e 2/2. Compara os pixels entre os
quatro estados e confirma que as flags não mudam a imagem com `Tab` fechado.
As capturas estão em `build/validation/campaign-route-20261012/`. A inspeção
das amostras PT-BR 320×200 e 1280×720 e EN 1280×720 não encontrou corte da
linha da missão nem sobreposição com os recursos de canto. A leitura em partida
e a aceitação visual final pertencem ao usuário; os testes técnicos não as
substituem.

Decisão visual aprovada: progresso detalhado apenas no `Tab`.

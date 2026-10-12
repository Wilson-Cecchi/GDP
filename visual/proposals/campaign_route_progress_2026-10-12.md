# Proposta — progresso de Chave + Arena na Campanha

Data: 2026-10-12. Estado: **proposta; não implementada**.

## Situação atual

`RecoverAndSecure` guarda `keyRecovered` e `arenaSecured` separadamente e conclui
a missão quando ambos são verdadeiros. A linha da missão no painel `Tab` hoje
mostra apenas `MISSION: KEY + ARENA` / `MISSÃO: CHAVE + ARENA` até a conclusão;
ela não informa qual etapa já foi cumprida. O objetivo breve da HUD e o
marcador do mapa mudam segundo a etapa pendente. As linhas de boss e saída
continuam independentes.

## Texto proposto

Substituir **somente a linha da missão** do painel `Tab` em `RecoverAndSecure`.
Usar a mesma ordem fixa, Chave depois Arena, em todos os estados; os números
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

## Dados mínimos e implementação posterior

No snapshot `HudRenderData`, adicionar apenas `campaignKeyRecovered` e
`campaignArenaSecured` (booleans, falsos por padrão). Preenchê-los em
`Application.cpp` diretamente de `CampaignMissionState` quando o modo for
Campanha. `campaignRouteMission` já identifica o tipo e evita mostrar a linha
em outras missões. Não derivar a primeira etapa de `campaignMission`, da
contagem geral `keys`, do marcador de mapa ou do estado do boss: após Arena
primeiro, o objetivo breve pode continuar `RECOVER KEY` e não indica que a Arena
já foi cumprida. O renderer soma os booleans apenas para apresentação. Adicionar
ao catálogo de `Localization.cpp` rótulos ou frases estáveis para que a linha
seja traduzida integralmente e medida no idioma selecionado. Não mudar save,
regras de missão, objetivo breve, terminal ou critério de saída nesta entrega.

## Verificações para a fase de implementação

1. Teste de snapshot para os quatro pares de flags e ambas as ordens de
   progresso. Confirmar que boss morto antes da missão, terminal bloqueado e
   retomada de checkpoint preservam os estados individuais corretos.
2. Teste de localização com os quatro textos exatos acima em EN e PT-BR; medir
   por caracteres UTF-8, inclusive `Ã` em `MISSÃO`, e alternar idioma em partida.
3. Teste de pixels OpenGL do `Tab` em 320×200, 640×360, 960×540 e 1280×720 nos
   dois idiomas. Capturar 0/2, ambas as variantes de 1/2 e 2/2; verificar
   diferenças entre estados e ausência de corte, sobreposição com recursos,
   boss, minimapa e painel `F3`. Testar boss vivo/morto e saída bloqueada/liberada.
4. Conferir em partida a leitura no tamanho real de cada resolução. A aprovação
   visual em jogo permanece uma decisão do usuário após as capturas; os testes
   técnicos não a substituem.

Decisão visual proposta: progresso detalhado apenas no `Tab`. Se o usuário
quiser acompanhamento persistente durante combate, definir esse requisito
antes de reservar edições no HUD principal.

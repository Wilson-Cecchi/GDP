# Plano de formatos das salas

## Critério de sala pronta

Uma **variante de sala** só recebe o estado **pronta** quando seu formato,
disposição e colisão dos objetos, texturas das paredes, piso e teto, e
aparência dos objetos presentes foram implementados e aprovados em partida.
Aprovação de uma proposta para implementação, captura técnica ou existência
da sala na campanha não fecha esse critério. A aprovação vale para a variante
testada, sem se estender automaticamente às outras salas do mesmo tema.

| Variante | Estado pelo critério completo | Falta para fechar |
| --- | --- | --- |
| Storage Hall, 18 × 20 m | **Pronta** | — |
| Central Hub octogonal | Em validação | Conexões em partida e aprovação final dos objetos/materiais aplicados. |
| Manutenção em L | **Pronta** | — |
| Cafeteria alongada, 18 × 24 m | **Pronta** | — |
| Segurança em T | **Pronta na prévia isolada** | Integração na campanha é etapa separada. |
| Arena do Boss com colunas | Em validação | Playtest da revisão com colunas e aceite visual completo da variante. |
| Laboratório redesenhado | Em validação na campanha | Playtest do formato, materiais, móveis e colunas na sala do Terminal da semente 2. |
| Arena de combate redesenhada | Prévia | Playtest do formato e materiais finais dos objetos. |
| Office deslocado | Planejada | Formato, objetos e materiais da variante. |

Estado em 02/11/2026: Central Hub octogonal e Manutenção em L estão na geração
da campanha para playtest das conexões. A Cafeteria alongada foi aprovada em
partida como variante pronta, mas ainda está disponível apenas na prévia
isolada; sua entrada na campanha é uma etapa de integração distinta.
Em 14/11/2026, o usuário aprovou a variante em L da Manutenção com seus móveis,
materiais e teto revisado. A campanha na semente 1793, a travessia e a geração
de mundo passaram pelos testes; a variante passa a **pronta**.
Segurança em T foi aprovada pelo usuário em 03/11/2026, após o teste da
sequência de indicadores. Continua em prévia isolada; a campanha ainda não
recebeu esse formato.

## Biblioteca de formatos

| Sala | Estado | Formato e medida de referência | Uso proposto |
| --- | --- | --- | --- |
| Central Hub | Campanha em teste | Octógono regular, limite 20 × 20 m, 6 m de altura | Módulo Hub com portas nas quatro faces cardeais e marco central. |
| Manutenção | Campanha em teste | L dentro de 16 × 20 m, 5,5 m de altura | Variante normal grande junto da versão retangular. |
| Cafeteria | Prévia aprovada | Retângulo 18 × 24 m, 6 m de altura | Variante longa pronta na prévia; `CafeteriaHall_01` da campanha continua em 18 × 18 m. |
| Storage | Manter por ora | Hall 18 × 20 m | Já oferece um eixo longo e estantes centrais; revisar após playtest conjunto. |
| Laboratory | Em playtest | Hall 18 × 20 m | Geometria da prévia integrada à variante grande da campanha; aprovação visual pendente. |
| Security | Prévia isolada em teste | Corpo em T, limite 20 × 20 m, 6 m de altura | Galeria larga com duas rotas laterais e aproximação central de 8 m. |
| Office | Ideia posterior | Planta deslocada, com um recuo lateral | Diferenciar a circulação da Manutenção em L sem transformar toda sala em corredor. |
| Boss | Colunas em playtest | 28 × 32 m, 7 m de altura | Duas colunas no flanco direito complementam as barricadas existentes; desníveis caminháveis ficam para outra etapa. |

## Detalhes de fixação para próximas salas

Os parafusos 3D dos bancos da Cafeteria servem de referência para mesas,
estantes, armários e colunas de outras salas. Colocá-los em encontros reais
entre peças, como tampo e suporte, prateleira e montante, porta e dobradiça,
ou placa e coluna. Variar quantidade e posição conforme o objeto, mantendo
escala legível sem repetir uma grade decorativa em toda superfície. Verificar
de perto se a cabeça está apoiada na peça e se não cria faces sobrepostas.
A Cafeteria aprovada não precisa de nova intervenção para aplicar essa regra.

Para a Cafeteria, manter o balcão e sua passagem lateral. Distribuir as mesas
em dois trechos de refeição ao longo do eixo de 24 m, com uma linha de visão
central, portas livres e espaço para contornar os bancos. Alongar a sala sem
alongar o desenho dos materiais: paredes, piso, teto e balcão devem repetir
texturas conforme metros reais. A proporção de 18 × 24 m foi aprovada na
prévia isolada com Sniper.

## Prévia da Segurança em T

A sala tem uma aproximação sul de 8 m de largura que se abre numa galeria de
20 m. Duas estações de controle ficam na parede norte; armários ficam nas
paredes da aproximação e duas coberturas baixas marcam as rotas laterais.
O corredor central e o encontro entre as três partes do T permanecem livres.
A prévia usa os materiais atuais da Segurança para avaliar apenas formato,
circulação e colisão. Ainda não é uma variante da campanha.

```bash
cmake --build build/playtest --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/playtest/gdp --security-t-preview --seed 20261102
```

O teste começa com Sniper e sem inimigos. Percorrer os dois braços da galeria,
contornar as coberturas e verificar a junção com o corredor estreito. A decisão
sobre o formato antecede a revisão das texturas e a integração na campanha.

```bash
cmake --build build/playtest --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/playtest/gdp --cafeteria-long-preview --seed 20261029
```

Para testar as conexões do Hub e da Manutenção em L na campanha, iniciar uma
partida normal com `--seed 0` após o mesmo build. Essa semente contém a variante
em L. As 1.000 sementes de 0 a 999 passaram pela validação estrutural.

## Ordem de integração

1. **Conexões de salas não retangulares.** Colocar o contorno no `RoomModule`
   em coordenadas locais e girá-lo junto com módulo, conectores e zonas. Validar
   cada porta contra uma face real do contorno, não somente contra o retângulo
   envolvente. Abrir o vão da porta no mesh da parede. Aplicar a colisão do
   contorno somente dentro da sala correspondente, preservando corredor e sala
   vizinha quando a porta estiver aberta.
2. **Central Hub.** Integrar o octógono aprovado como módulo Hub, mantendo as
   quatro portas cardeais e o marco central. Verificar posições de inimigos,
   loot, luzes e circulação ao redor da coluna em todas as rotações e em mapas
   gerados por sementes diferentes.
3. **Manutenção em L.** Integrar como variante normal grande. Ajustar a porta
   da face curta para ter largura e folga para o maior inimigo. Remover apenas
   móveis que cruzem o recorte; verificar bancadas, caixas quebráveis, spawn e
   luzes no corredor interno do L.
4. **Cafeteria longa.** A prévia isolada de 18 × 24 m passou à biblioteca da
   campanha para o playtest com portas e encontros reais. Revisar em partida
   a distribuição das mesas e do balcão, as luzes e o ritmo do trajeto;
   consulte [o roteiro de campanha](campaign_room_playtest_2026-11-04.md).

## Critérios de validação

- Todas as portas ligadas abrem um vão visível e transitável; nenhuma parede,
  coluna, bancada ou caixa ocupa o vão ou a chegada do corredor.
- Jogador e inimigos conseguem entrar, sair, contornar móveis e atravessar
  corredores em ambos os sentidos, inclusive com portas bloqueadas/desbloqueadas.
- Piso, teto, paredes inclinadas, tiros, explosões e iluminação respeitam o
  mesmo contorno. Texturas mantêm escala física, sem esticamento.
- A geração segue determinística, válida em quatro rotações e em uma amostra
  de sementes; checkpoints carregam a mesma geometria. Atualizar o fingerprint
  de referência somente após comparar a mudança esperada na geração.
- Executar os testes de módulos/mundo, travessia, combate, props e renderização
  OpenGL. Fazer capturas em FOV 60/100, POTATO/ULTRA e playtest NVIDIA em mapa
  de campanha com conexões reais. Captura técnica não substitui aprovação em
  partida.

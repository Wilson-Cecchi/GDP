# Plano de formatos das salas

Após concluir e integrar as salas à campanha, seguir para o
[plano visual do arsenal e do Player](../visual/weapon_visual_roadmap.md).

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
| Central Hub octogonal | **Pronta** | — |
| Manutenção em L | **Pronta** | — |
| Laboratório grande, 18 × 20 m | **Pronta** | — |
| Cafeteria alongada, 18 × 24 m | **Pronta na prévia isolada** | Validar a variante já incluída na biblioteca em partidas de campanha. |
| Segurança em T | **Pronta na prévia isolada** | Validar a variante já incluída na biblioteca em partidas de campanha. |
| Arena do Boss com colunas | Em validação | Playtest da revisão com colunas e aceite visual completo da variante. |
| Segurança quadrada | **Pronta na prévia isolada** | Validar a variante agora integrada à biblioteca em partida de campanha. |
| Arena de combate redesenhada | Prancha aprovada; revisão integrada em validação | Validar parede, piso, teto, galerias, escadas, colunas e coberturas em partida. A primeira prévia foi reprovada. |
| Office deslocado | Planejada | Formato, objetos e materiais da variante. |

Em 20/11/2026, a `CafeteriaHall_01` recebeu mesas, bancos e balcão para a
variante de 18 × 18 m; os módulos Office receberam estação e acabamento de
metal próprios. Variantes compactas de Cafeteria, Segurança, Laboratório e
Manutenção receberam uma estação pequena coerente com o tema. Essas mudanças
estão integradas para [playtest de campanha](campaign_visual_playtest_2026-11-20.md)
e ainda não alteram o estado de aprovação das variantes acima.

Estado em 02/11/2026: Central Hub octogonal e Manutenção em L estavam na geração
da campanha para playtest das conexões. A Cafeteria alongada foi aprovada em
partida na prévia isolada. Em 15/11/2026, o código já inclui a variante longa
na biblioteca da campanha; falta validar suas conexões e encontros em partida.
Em 14/11/2026, o usuário aprovou a variante em L da Manutenção com seus móveis,
materiais e teto revisado. A campanha na semente 1793, a travessia e a geração
de mundo passaram pelos testes; a variante passa a **pronta**.
Segurança em T foi aprovada pelo usuário em 03/11/2026, após o teste da
sequência de indicadores. Em 15/11/2026, o código já inclui a variante na
biblioteca da campanha; falta validar suas conexões e encontros em partida.
As travessas de teto de Storage, Cafeteria, Segurança, Laboratório e Hub foram
aprovadas em 15/11/2026; esse aceite não fecha outras pendências dessas salas.
Em 15/11/2026, o usuário aprovou a Segurança quadrada depois dos dutos no
teto. A composição da prévia foi integrada ao módulo `SecurityHall_01` da
campanha; a semente `1` coloca essa variante no primeiro andar para validar
portas, encontros e circulação em partida real.
Em 15/11/2026, o usuário aprovou o Laboratório grande após testar vasos
quebráveis e a correção das marcas de tiro nas paredes; essa variante está pronta.
O usuário já havia aprovado o Central Hub octogonal após os acabamentos de
piso, paredes, coluna e objetos; a revisão das travessas preserva esse aceite.
Em 15/11/2026, a prancha da Arena de Combate foi aprovada. A primeira prévia
foi reprovada por se afastar da composição. A revisão reúne materiais de
parede, piso e teto e arquitetura 3D na prévia e na variante da campanha;
a avaliação visual e de combate ainda depende de playtest.

## Biblioteca de formatos

| Sala | Estado | Formato e medida de referência | Uso proposto |
| --- | --- | --- | --- |
| Central Hub | Pronta na campanha | Octógono regular, limite 20 × 20 m, 6 m de altura | Módulo Hub com portas nas quatro faces cardeais e marco central. |
| Manutenção | Campanha em teste | L dentro de 16 × 20 m, 5,5 m de altura | Variante normal grande junto da versão retangular. |
| Cafeteria | Biblioteca da campanha; playtest pendente | Retângulo 18 × 24 m, 6 m de altura | Variante longa aprovada na prévia; `CafeteriaHall_01` de 18 × 18 m também permanece na campanha. |
| Storage | Manter por ora | Hall 18 × 20 m | Já oferece um eixo longo e estantes centrais; revisar após playtest conjunto. |
| Laboratory | Pronta na campanha | Hall 18 × 20 m | Geometria e visual da variante grande aprovados em partida. |
| Security | Biblioteca da campanha; playtest pendente | Corpo em T, limite 20 × 20 m, 6 m de altura | Galeria larga com duas rotas laterais e aproximação central de 8 m. |
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
A prévia permite avaliar formato, circulação e colisão sem inimigos; a variante
equivalente também está na biblioteca da campanha para teste em encontros reais.

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

# Environment — Laboratory

> **Estado:** materiais de parede, piso e teto implementados e validados na
> Fase 10. A variante grande com bancadas, contenção e colunas físicas também
> está na campanha para playtest; a revisão visual ainda aguarda aprovação.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Função

Pesquisa e risco.

## Identidade

- equipamentos
- tanques
- vidro/polímero
- luz fria
- contaminação localizada

## Gameplay

Engineer; experimental; interação.

## Geração

Geometria principal vem do `RoomModule`. Props, materiais, desgaste, luzes e estados variam
com sub-seeds independentes e respeitam zonas reservadas/connectors.

## Implementação atual

### Prévia da geometria

A prévia `Laboratory` mede 18 × 20 m com teto a 6 m e segue a composição de
`laboratorio.png`: instalações de pesquisa nas duas laterais e eixo central
amplo para circulação e combate. Bancadas e unidades de contenção encostam nas
paredes; não há bolsões estreitos atrás delas. Duas bancadas baixas ficam
desencontradas no miolo, oferecendo cobertura sem fechar o corredor central
nem a passagem pelos flancos. O acesso às quatro possíveis portas, ao terminal
e às posições reservadas para inimigos permanece livre.
As peças sólidas usam a mesma geometria para desenho e colisão. A prévia
começa sem inimigos; o módulo `LaboratoryHall_01` reproduz essa composição na
campanha, com encontros e portas reais.

```bash
cmake --build build/playtest --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/playtest/gdp --laboratory-preview --seed 20261029
```

As unidades de contenção permanecem opacas nesta fase. O vidro funcional
depende de uma revisão posterior.
As [capturas técnicas](../visual/validation/laboratory_geometry_preview/README.md)
mostram FOV 60 e 100 em POTATO e ULTRA; a aprovação em partida está pendente.

`generateLaboratoryRoomMaterials(roomSeed)` seleciona `Sterile`, `Operational`
ou `Contained`. A condição usa o ID local 400; Wall/Floor/Ceiling usam 401–403:

- **Wall:** Ceramic branco-frio, 2–6 × 2 painéis precisamente alinhados e um
  módulo científico azul-escuro com apenas dois indicadores verdes;
- **Floor:** Polymer azul-acinzentado, grade 2×2 ou 4×4, travas nos cantos,
  tampa técnica central e uma única mancha dessaturada somente em `Contained`;
- **Ceiling:** Aluminum claro e simétrico, com filtros simples e luminárias
  brancas intensas ocupando entre 20% e 40% dos módulos.

O perfil evita ferrugem, óleo, redes caóticas e indicadores excessivos. Receitas
e conjunto possuem hashes canônicos isolados e são preparados pelo renderer via
`prepareLaboratoryRoomMaterials`. Salas com tema Laboratory usam este perfil
no jogo; a regressão OpenGL offscreen também verifica a saída visual.

### Prévia de materiais e móveis v1

Em 03/11/2026, a prévia isolada passou a usar as três amostras Operational
aprovadas: parede cerâmica, piso azul e teto de alumínio. Cada imagem cobre
2 × 2 m em coordenadas físicas. O espelhamento das bordas evita a emenda
abrupta da amostra original sem esticar riscos, parafusos ou módulos.
O módulo grande `LaboratoryHall_01` usa essas amostras para playtest na campanha;
as outras salas Laboratory mantêm os materiais procedurais.

As bancadas e unidades de contenção usam esmalte metálico claro em repetição
de 0,75 m. Portas, puxadores, molduras, telas, grades e fixadores são volumes
3D; as peças principais mantêm a colisão existente. As janelas de contenção
continuam opacas. As [capturas técnicas](../visual/validation/laboratory_material_v1/README.md)
incluem vista geral e detalhes dos móveis; o aceite em partida está pendente.

### Rodapé, extração e espécime de teste

A prévia agora inclui rodapé físico com juntas nos cantos, interrompido nas
portas, e dois dutos suspensos de extração com suportes e grelhas inferiores.
Uma planta fica no canto posterior esquerdo, dentro de uma cúpula de vidro
translúcido sobre base metálica. A base sólida encosta na parede e na unidade
de contenção; o vidro é apenas visual. A circulação central, os flancos e as
quatro portas continuam livres. Esses detalhes ainda aguardam aceite em
partida antes de serem considerados prontos.

Cada uma das duas bancadas centrais recebeu dois vasos pequenos de espécime,
também cobertos por domos translúcidos. As bases encostam no tampo e ocupam
apenas as extremidades das bancadas; a área entre elas permanece livre.

### Revisão da parede e colunas físicas

Em 04/11/2026, a faixa azul repetida na textura da parede foi removida da
prévia. A nova amostra `wall_operational_v2.png` apresenta um painel cerâmico
claro de 2 × 2 m, com desgaste discreto e quatro fixadores. A faixa azul da
prancha de referência representa uma coluna, portanto passou a ser modelada
como volume físico na parede. Cada coluna tem corpo azul escuro com colisão,
acabamento na base e no topo, bordas claras e indicador verde luminoso em 3D.
As colunas respeitam as quatro rotações da sala e são omitidas onde
encontrariam móveis, zonas reservadas ou portas.

Esta revisão é uma prévia para playtest. A textura anterior continua no
repositório, e o piso, teto e móveis permanecem como na versão anterior.

No primeiro andar da campanha com a semente `2`, a sala do Terminal é o
`LaboratoryHall_01`. Inicie um slot novo com o comando documentado em
[Playtest das salas no modo Campanha](campaign_room_playtest_2026-11-04.md).

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate rasteriza 1.000 room seeds, cobre as três condições, exige hashes únicos,
dois indicadores organizados, juntas finas, contaminação localizada e ocupação
real de 20–40% dos módulos de teto por luzes. A regressão OpenGL confirma uma
saída visual distinta dos demais ambientes.

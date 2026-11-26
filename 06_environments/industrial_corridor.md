# Environment — Industrial Corridor

> **Estado em 20/11/2026:** material próprio v2 de parede, piso e teto
> integrado à prévia e à campanha, com seed independente das salas nas pontas.
> Captura técnica passou; falta aceite em partida.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Função

Conectar setores.

## Identidade

- placas metálicas grandes
- iluminação linear
- pouco detalhe
- rodapé amarelo/preto somente na base

## Gameplay

Worker ocasional; navegação clara.

## Geração

Geometria principal vem do `RoomModule`. Props, materiais, desgaste, luzes e estados variam
com sub-seeds independentes e respeitam zonas reservadas/connectors.

## Implementação atual

### Prévia da travessia

A prévia isolada conecta dois cômodos compactos por um corredor reto de cerca
de 10,3 m, com portas de 2,4 × 2,8 m. Ela permite avaliar a travessia real em
ambos os sentidos sem inimigos. Colunas físicas espaçadas junto às paredes
substituem os suportes decorativos da prévia. A prévia e os corredores gerados
da campanha usam a mesma geometria e colisão; as colunas preservam a largura
central para o maior inimigo e deixam livres as portas e curvas. Trechos curtos
podem ficar sem colunas. Abrir diretamente com a build de desenvolvimento:

Em 03/11/2026, a prévia falhava ao iniciar porque o cômodo de destino usa
papel `Boss` apenas para satisfazer o grafo de teste e herdava colunas da arena
do Boss, maiores que o próprio cômodo. O destino recebeu um ID exclusivo de
prévia, sem essa arquitetura. O executável abriu com duas salas e um corredor;
a travessia visual em partida ainda aguarda novo playtest.

```bash
cmake --build build/playtest --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/playtest/gdp --industrial-corridor-preview --seed 20261031
```

As [capturas técnicas](../visual/validation/industrial_corridor_geometry_preview/README.md)
registram os suportes decorativos anteriores. As
[capturas das colunas físicas](../visual/validation/corridor_columns_geometry_preview/README.md)
mostram a geometria atual em FOV 60 e 100, ULTRA e POTATO. O teste visual
jogando está pendente.

A [proposta v2 de material próprio](../visual/proposals/industrial_corridor_dedicated_material_v2/README.md)
foi implementada para playtest. O corredor mantém a mesma aparência nos dois
sentidos e não herda a textura de qualquer cômodo adjacente.

`generateIndustrialRoomMaterials(roomSeed)` seleciona deterministicamente
`Maintained`, `Used` ou `Worn` e deriva sub-receitas independentes para:

- **Wall:** a receita procedural conserva a grade de placas e a variação por
  sala, mas o renderer usa a parede v2 HD aprovada: painéis de aço pintado,
  juntas recuadas, fixadores, desgaste localizado e sinalização âmbar/preta
  somente na base. Os mapas compartilhados de albedo, normal e propriedades
  têm 1024×1024 px para cada condição;
- **Floor:** a receita BareSteel original continua disponível no gerador
  procedural. As salas Industrial da campanha agora usam placas largas de aço
  grafite com juntas deslocadas, normal e mapa de propriedades em 256×256 px.
  Essa revisão remove a repetição miúda do piso anterior e aguarda aceite
  visual em partida; o corredor mantém seu piso exclusivo;
- **Ceiling:** Aluminum, grade 2 × 2–4 e sequência regular de luminárias lineares
  com intensidade emissiva própria.

Wear/dirt vêm da condição global, mas cada canal usa sua própria stream. O
conjunto e cada receita possuem hashes canônicos usados pelo cache. A sala
inicial e as salas com tema Industrial usam este perfil no jogo. Office agora
tem paleta própria de aço neutro com acentos ciano.
A descrição das oito paredes e o formato para enviar novas referências estão
em [walls_current_reference.md](walls_current_reference.md).

### Gabaritos da campanha (25/11/2026)

A campanha usa os dois tamanhos pedidos. Corredores comuns e ramificações
continuam em **1,2 × 2,2 m**. Uma transferência da rota principal por andar,
preferindo ligação com Hub ou Arena e depois a aproximação do Boss, tenta o
gabarito **2,4 × 2,8 m**. O gerador amplia as duas portas da aresta junto com o
corredor. Se o trajeto largo interceptar uma sala, exceder o comprimento
permitido ou invadir a arquitetura dos cômodos, essa ligação volta ao tamanho
estreito e o gerador tenta a próxima ligação elegível. Em mapas apertados,
todos os trechos podem permanecer estreitos para preservar a rota válida.
Bibliotecas de módulos
fornecidas por testes ou ferramentas conservam seus conectores originais.

O trecho largo usa um pequeno afastamento adicional entre a porta e o primeiro
segmento interno; sem isso, a faixa de 2,4 m falsamente intercepta a própria
sala durante a validação. A posição das salas e a regra de abertura das portas
continuam determinadas pelos módulos e pelo gameplay.

## Validação visual

As colunas físicas do corredor usam o albedo estrutural repetível de 0,8 m.
O desgaste acompanha a condição industrial, sem alterar os volumes de colisão.
As [capturas técnicas](../visual/validation/structural_columns_v1/README.md)
em FOV 60/100 e POTATO/ULTRA aguardam aceitação visual em partida.

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate automatizado gera e rasteriza as três superfícies para 1.000 room seeds,
exige todas as condições, hashes únicos no lote, rodapé apenas na base, faixa de
tráfego legível e contraste das luminárias.

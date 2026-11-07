# Environment — Industrial Corridor

> **Estado:** primeiro conjunto de materiais de parede, piso e teto implementado
> e validado na Fase 10; geometria modular e detalhes ocasionais continuam em
> expansão.
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
substitui a v1 após o pedido por uma textura realmente nova. Ela trata a
dependência atual do material do cômodo de origem e aguarda decisão visual.

`generateIndustrialRoomMaterials(roomSeed)` seleciona deterministicamente
`Maintained`, `Used` ou `Worn` e deriva sub-receitas independentes para:

- **Wall:** a receita procedural conserva a grade de placas e a variação por
  sala, mas o renderer usa a parede v2 HD aprovada: painéis de aço pintado,
  juntas recuadas, fixadores, desgaste localizado e sinalização âmbar/preta
  somente na base. Os mapas compartilhados de albedo, normal e propriedades
  têm 1024×1024 px para cada condição;
- **Floor:** BareSteel, grade 2/4 × 4, detalhes antiderrapantes, bordas mais
  escuras e centro de tráfego discretamente polido;
- **Ceiling:** Aluminum, grade 2 × 2–4 e sequência regular de luminárias lineares
  com intensidade emissiva própria.

Wear/dirt vêm da condição global, mas cada canal usa sua própria stream. O
conjunto e cada receita possuem hashes canônicos usados pelo cache. A sala
inicial e as salas com tema Industrial ou Office usam este perfil no jogo.
A descrição das oito paredes e o formato para enviar novas referências estão
em [walls_current_reference.md](walls_current_reference.md).

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

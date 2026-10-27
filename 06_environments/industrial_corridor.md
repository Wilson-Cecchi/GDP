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

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate automatizado gera e rasteriza as três superfícies para 1.000 room seeds,
exige todas as condições, hashes únicos no lote, rodapé apenas na base, faixa de
tráfego legível e contraste das luminárias.

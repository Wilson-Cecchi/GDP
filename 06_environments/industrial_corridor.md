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

- **Wall:** PaintedSteel, grade de 2–4 × 2 placas, juntas/rims e rodapé
  amarelo/preto restrito aos 4 pixels inferiores da textura 64×64;
- **Floor:** BareSteel, grade 2/4 × 4, detalhes antiderrapantes, bordas mais
  escuras e centro de tráfego discretamente polido;
- **Ceiling:** Aluminum, grade 2 × 2–4 e sequência regular de luminárias lineares
  com intensidade emissiva própria.

Wear/dirt vêm da condição global, mas cada canal usa sua própria stream. O
conjunto e cada receita possuem hashes canônicos usados pelo cache. A sala START
fixa usa atualmente esse perfil visual do Industrial Corridor.

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate automatizado gera e rasteriza as três superfícies para 1.000 room seeds,
exige todas as condições, hashes únicos no lote, rodapé apenas na base, faixa de
tráfego legível e contraste das luminárias.

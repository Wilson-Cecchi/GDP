# Environment — Cafeteria

> **Estado:** materiais de parede, piso e teto implementados e validados na
> Fase 10; mesas, bancos, balcão e navegação entre salas continuam em expansão.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Função

Contraste humano.

## Identidade

- mesas/bancos em grupos
- balcão
- cozinha industrial
- iluminação difusa

## Gameplay

Encontros moderados; leitura de espaço social.

## Geração

Geometria principal vem do `RoomModule`. Props, materiais, desgaste, luzes e estados variam
com sub-seeds independentes e respeitam zonas reservadas/connectors.

## Implementação atual

`generateCafeteriaRoomMaterials(roomSeed)` escolhe `Clean`, `InUse` ou `Faded`
e deriva receitas independentes pelos IDs locais 301–303; a condição usa 300:

- **Wall:** Ceramic representando painéis civis claros, grade de 2–4 × 2 e
  faixa vermelha contínua com 8–12% da altura, mais desgaste discreto;
- **Floor:** Ceramic representando piso industrial lavável, grade 2×2 ou 4×4,
  faixa central polida, manchas largas suaves e quatro contatos circulares de
  mobiliário;
- **Ceiling:** Aluminum claro em módulos regulares, pouco desgaste e
  luminárias largas, neutras e uniformemente espaçadas.

Cada perfil preserva mais base clara e limpa que Storage sem eliminar marcas de
uso humano. Receitas e conjunto possuem hashes canônicos próprios; o renderer
os prepara e cacheia antes do draw por `prepareCafeteriaRoomMaterials`. Salas
com tema Cafeteria usam este perfil no jogo; a regressão OpenGL offscreen
também verifica a saída visual.

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate rasteriza as três superfícies para 1.000 room seeds, cobre todas as
condições, exige hashes únicos, faixa vermelha confinada/alinhada, circulação
polida, quatro contatos de mobiliário e contraste regular das luminárias. A
regressão OpenGL comprova que o perfil permanece distinto dos demais.

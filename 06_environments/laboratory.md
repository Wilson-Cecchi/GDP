# Environment — Laboratory

> **Estado:** materiais de parede, piso e teto implementados e validados na
> Fase 10; equipamentos, tanques e navegação entre salas continuam em expansão.
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
`prepareLaboratoryRoomMaterials`. Enquanto a cena navegável permanece na START
industrial, a variante é exercitada pela regressão OpenGL offscreen.

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate rasteriza 1.000 room seeds, cobre as três condições, exige hashes únicos,
dois indicadores organizados, juntas finas, contaminação localizada e ocupação
real de 20–40% dos módulos de teto por luzes. A regressão OpenGL confirma uma
saída visual distinta dos demais ambientes.

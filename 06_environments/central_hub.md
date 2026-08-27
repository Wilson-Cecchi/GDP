# Environment — Central Hub

> **Estado:** materiais de parede, piso e teto implementados e validados na
> Fase 10; portas 3D, landmark geométrico e navegação entre salas continuam em
> expansão.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Função

Orientação entre rotas.

## Identidade

- espaço central
- várias portas legíveis
- landmark
- poucas linhas de navegação contínuas

## Gameplay

Decisão de rota; encontros opcionais.

## Geração

Geometria principal vem do `RoomModule`. Props, materiais, desgaste, luzes e estados variam
com sub-seeds independentes e respeitam zonas reservadas/connectors.

## Implementação atual

`generateCentralHubRoomMaterials(roomSeed)` seleciona `Open`, `Active` ou
`Rerouted` e um eixo `Horizontal`, `Vertical` ou `Cross`. Condição/eixo usam o
ID local 700; Wall/Floor/Ceiling usam 701–703:

- **Wall:** PaintedSteel com 1–3 divisões amplas, uma coluna arquitetônica e uma
  única placa ciano compacta; nenhuma faixa amarelo/preto decorativa;
- **Floor:** BareSteel em grade 2×2 ou 2×4, com uma rota ciano contínua de 2 px
  ou um único cruzamento controlado que alcança as bordas da textura;
- **Ceiling:** BareSteel em quatro módulos amplos, com duas luminárias lineares
  ou um par central alinhado ao mesmo eixo de circulação.

`CentralHubRoomMaterialSet` preserva o eixo compartilhado nas três receitas,
mas deriva cada superfície por sub-seed independente. Validação rejeita
parâmetros pertencentes à superfície errada, acento amarelo, divergência de eixo
e corrupção do hash canônico. O renderer prepara/cacheia o conjunto com
`prepareCentralHubRoomMaterials`. Como a cena navegável permanece na START
industrial, o perfil é exercitado offscreen.

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate rasteriza 1.000 room seeds, cobre os três estados e os três eixos, exige
hashes únicos, exatamente uma sinalização ciano na parede, continuidade das
rotas do piso e 135–144 pixels de luminária alinhada no teto. A regressão OpenGL
confirma a oitava saída visual distinta.

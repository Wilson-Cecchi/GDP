# Environment — Technical / Maintenance

> **Estado:** materiais de parede, piso e teto implementados e validados na
> Fase 10; máquinas, bancadas e navegação entre salas continuam em expansão.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Função

Infraestrutura operacional.

## Identidade

- tubos funcionais
- máquinas
- cabos
- bancadas
- óleo/calor localizados

## Gameplay

Worker/engineer; sistemas.

## Geração

Geometria principal vem do `RoomModule`. Props, materiais, desgaste, luzes e estados variam
com sub-seeds independentes e respeitam zonas reservadas/connectors.

## Implementação atual

`generateTechnicalRoomMaterials(roomSeed)` seleciona `Serviced`, `Working` ou
`Overloaded`. A condição usa o ID local 500; as superfícies usam 501–503:

- **Wall:** PaintedSteel sobre base simples 2×2, 1–2 tubos verticais com sombra,
  corpo e highlight, além de caixa técnica e válvula sempre conectadas;
- **Floor:** BareSteel pesado 2×2 com juntas escuras, parafusos, uma grade
  opcional emoldurada e uma mancha de óleo controlada/não totalmente preta;
- **Ceiling:** BareSteel dividido em categorias de painel, tubo, duto e cabo,
  garantindo exatamente uma luminária protegida para navegação.

Wear, dirt e oil crescem com carga sem transformar todas as superfícies em
detalhe. Receitas e conjunto usam hashes próprios e substreams 500–503. O
renderer prepara/cacheia o perfil via `prepareTechnicalRoomMaterials`. Salas
com tema Maintenance usam este perfil no jogo; ele também é exercitado pela
regressão OpenGL offscreen.

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate rasteriza 1.000 room seeds, cobre as três cargas, exige hashes únicos,
tubos com três tons, óleo limitado, grade única e uma luminária protegida. A
regressão OpenGL confirma uma saída visual distinta para o perfil.

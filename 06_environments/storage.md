# Environment — Storage

> **Estado:** materiais de parede, piso e teto implementados e validados na Fase
> 10; pallets, estantes e caixas físicas permanecem na expansão de props.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Função

Estoque, cobertura e loot.

## Identidade

- pallets, estantes, caixas em clusters
- luz irregular
- marcação logística

## Gameplay

Worker; loot; caminhos entre pilhas.

## Geração

Geometria principal vem do `RoomModule`. Props, materiais, desgaste, luzes e estados variam
com sub-seeds independentes e respeitam zonas reservadas/connectors.

## Implementação atual

`generateStorageRoomMaterials(roomSeed)` escolhe `Orderly`, `Dusty` ou
`Neglected` e deriva streams isoladas a partir dos IDs 100–103 do domínio
`RoomMaterial`, separados do Industrial Corridor.

- **Wall:** PaintedSteel amarronzado, 2–3 × 2 placas grandes, variação de placa
  substituída, oxidação localizada em juntas/base, riscos baixos e uma marcação
  logística curta;
- **Floor:** Ceramic representa concreto industrial, com manchas grandes em
  células de 8 px, duas marcas controladas de arrasto e no máximo uma rachadura
  fina condicionada ao budget;
- **Ceiling:** BareSteel escuro, painéis/vigas grandes e luminárias espaçadas com
  intensidade inferior à industrial; cada fixture pode falhar por seed.

Cada receita e o conjunto completo possuem hash canônico. O cache do renderer
usa esse hash como chave, portanto a mesma room seed pode existir em Industrial
e Storage sem colisão.

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate rasteriza as três superfícies para 1.000 room seeds, exige as três
condições, hashes únicos, concreto sem oxidação, marcação curta, arrasto legível
e contraste mínimo do teto. A regressão OpenGL alterna Industrial, Storage,
Combat e Cafeteria e confirma que cada perfil muda os pixels renderizados.

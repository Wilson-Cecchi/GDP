# Environment — Storage

> **Estado:** materiais de parede, piso e teto implementados e validados na Fase
> 10. A sala grande tem estantes físicas e materiais v3; o usuário aprovou a
> prévia em partida em 29/10/2026 e as caixas texturizadas em 02/11/2026.
> A variante StorageHall_01 atende ao critério de sala pronta.
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

A `StorageHall_01` (`18 × 6 × 20 m`) tem estantes nas paredes laterais e duas
ilhas internas desencontradas. A linha central e os caminhos ao redor das ilhas
permanecem livres. As peças visíveis das estantes, cargas, pilares e vigas usam
os mesmos volumes para renderização e colisão. A validação rejeita peças que
invadam portas ou áreas reservadas para inimigos, loot e props; o Hall reserva
uma faixa própria para os props gerados.

Para testar exatamente essa sala na GPU NVIDIA, use o build local de playtest:

```bash
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia ./build/playtest/gdp --storage-room-preview --seed 20261027
```

A prévia começa sem inimigos para inspecionar formato e colisão. O usuário
aprovou a estante v3 com tom mais verde em partida.

Em 03/11/2026, o Hall recebeu rodapé físico verde grafite com juntas e
parafusos, recortado nas portas, e um duto 3D suspenso no eixo central. O
rodapé usa repetição em escala física; o duto passa entre as linhas de luz.
Esta nova camada de detalhes tem validação técnica e aguarda playtest visual.
Capturas NVIDIA em POTATO/ULTRA e FOV 60/100 estão em
`visual/validation/ready_rooms_details_20261103/storage/`.

A parede do Hall usa seis variações de chapa baseadas na prancha `storage.png`:
`Orderly`, `Dusty` e `Neglected`, cada uma com e sem marcação. Os trechos têm
até 6 × 6 m, sem esticar a imagem, e somente um trecho marcado é escolhido por
parede; a faixa de segurança fica na base. A prévia técnica na Quadro K2100M
está em `visual/validation/storage_wall_v1/preview_six_variants_orientation_1849x1011.png`.
A sala grande usa também três variações de piso e teto (`Orderly`, `Dusty`,
`Neglected`), aprovadas para implementação e aplicadas em trechos de até
6 × 6 m. As amostras estão em
`visual/proposals/storage_material_v1/review_floor_ceiling.html`.
As caixas sobre as estantes são objetos quebráveis de 35 HP: tiros, explosões
e disparos do Boss podem destruí-las; a estrutura da estante permanece sólida.
As caixas alternam os albedos oliva e carvão aprovados em
`visual/proposals/storage_box_v1/`, aplicados uma vez por face.
O usuário aprovou o resultado das caixas na sala em 02/11/2026.
A armação fixa das estantes usa a direção visual v3 em
`visual/proposals/storage_rack_v3/review.html`, aprovada para implementação em
29/10/2026. Colunas laranja, tampos grafite, bordas e suportes recebem materiais
nas condições `Orderly`, `Dusty` ou `Neglected`. O desgaste é projetado em
escala física, com lascas extras nas quinas laranja; os furos sombreados e as
nervuras mantêm passo de 18 cm, sem esticar o albedo. Os furos não atravessam
a malha de colisão. As caixas
quebráveis, vigas altas e pilares mantêm seus materiais próprios.
As prateleiras terminam na borda dos montantes, sem interseção nem fresta nos
volumes de colisão. A v1 foi devolvida por parecer lisa em jogo; a v2 foi
substituída pela v3 antes da integração. A v3 foi aprovada em partida.
Após o playtest de 03/11/2026, as colunas das estantes passaram a usar um tom
verde grafite próximo ao rodapé, preservando os detalhes de desgaste da v3.
Essa nova cor aguarda aceite em partida.
O rodapé recebeu um escurecimento leve após o feedback seguinte, mantendo
visíveis as juntas, tampas e cabeças de parafuso.

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

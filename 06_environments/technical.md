# Environment — Technical / Maintenance

> **Estado:** materiais de parede, piso e teto implementados e validados na
> Fase 10. Uma prévia isolada da geometria e dos móveis está pronta para
> playtest. A variante em L está integrada como sala normal grande na campanha;
> suas conexões aguardam playtest.
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

### Prévia da geometria

A prévia `Technical/Maintenance` mede 16 × 20 m e tem teto a 5,5 m. Segue a
composição de [`maintence.png`](maintence.png): bancadas de manutenção,
gabinetes e geradores ocupam as laterais; tubulações ficam acima da passagem.
Duas estações de manutenção desencontradas ficam no miolo da sala, cada uma
com base, tampo e uma caixa quebrável. Elas deixam 4,4 m livres entre os tampos e
passagens laterais para contorná-las. As quatro posições possíveis de porta e
as áreas reservadas para inimigos, objetos e loot ficam livres. As caixas têm
35 HP, recebem dano de tiros e explosões e perdem a colisão ao serem destruídas;
as bancadas permanecem no lugar. As peças sólidas compartilham a mesma
geometria para desenho e colisão.

```bash
cmake --build build/playtest --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/playtest/gdp --technical-preview --seed 20261029
```

A prévia abre sem inimigos para avaliar formato e circulação. Bancadas,
gabinetes e geradores usam material genérico nesta fase; as duas caixas usam
as variações oliva e carvão já existentes. As
[capturas técnicas](../visual/validation/technical_geometry_preview/README.md)
mostram FOV 60 e 100 em POTATO e ULTRA; a aprovação em partida está pendente.

### Variante em L

A nova prévia `--technical-l-preview` usa o mesmo limite de 16 × 20 m e altura
de 5,5 m, retirando o canto frontal direito. O piso, o teto, as paredes e a
colisão seguem esse recorte; bancadas que cruzariam o vazio são omitidas. A
prévia original `--technical-preview` continua disponível. Ambas começam com
Sniper e sem inimigos.

```bash
cmake --build build/playtest --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/playtest/gdp --technical-l-preview --seed 20261029
```

As [capturas técnicas](../visual/validation/shaped_rooms_v1/README.md) cobrem
FOV 60/100 e POTATO/ULTRA. O formato em L foi aprovado em partida para
implementação; as conexões reais com corredores ainda precisam de validação.
Na campanha, o formato em L aparece como variante de Manutenção grande, com
as quatro rotações possíveis. A sala retangular permanece na biblioteca.
Consulte a [ordem de integração](room_shape_plan.md).

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

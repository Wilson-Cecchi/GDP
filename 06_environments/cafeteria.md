# Environment — Cafeteria

> **Estado:** a variante alongada de 18 × 24 m foi aprovada em partida em
> 02/11/2026 como sala pronta: formato, circulação, móveis e seus materiais,
> paredes, piso e teto. A variante longa também consta na biblioteca da campanha,
> junto de `CafeteriaHall_01` de 18 × 18 m. Esta última recebeu duas mesas com
> bancos e um balcão ajustado à área livre em 20/11/2026; falta validar ambas
> em encontro real de campanha.
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

### Prévia da geometria

A prévia isolada mede 18 × 20 m e tem teto a 6 m. Quatro conjuntos de mesa e
bancos ocupam as laterais; o balcão de serviço fica na parede direita, com
uma passagem larga no meio alinhada à porta lateral. Os armários encontram o
balcão e a parede para não formar um corredor sem saída atrás dele. A faixa central e os
corredores em volta dos móveis ficam livres. Mesas, bancos e balcão usam a
mesma geometria para desenho e colisão; a validação reserva pontos de
inimigos e rejeita mobiliário nas quatro possíveis portas.

Uma variante de 18 × 24 m já está disponível na prévia isolada. Ela distribui
seis conjuntos de mesa ao longo da sala e alonga o balcão, preservando sua
passagem lateral. A `CafeteriaHall_01` da campanha continua em 18 × 18 m; a
variante longa também entrou na biblioteca procedural e aguarda playtest com
conexões e encontros reais.

```sh
./build/playtest/gdp --cafeteria-preview --seed 20261029
```

```bash
cmake --build build/playtest --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/playtest/gdp --cafeteria-long-preview --seed 20261029
```

As proporções e a circulação da prévia foram aprovadas. Parede, piso e teto
usam os PNGs v1 aprovados, e a luz branco quente foi suavizada. Mesas, bancos,
suportes e balcão usam os albedos metálicos v1 aprovados para implementação,
com condições Clean/InUse/Faded. A aparência aplicada foi aprovada em partida
para esta variante. A aprovação visual ocorreu na prévia isolada; a versão da
campanha ainda precisa de validação específica.

Em 03/11/2026, a variante longa recebeu rodapé físico em tom quente, duto
3D suspenso sobre o balcão de serviço e dois vasos nos cantos do salão. As
plantas têm folhas largas e baixas, diferentes das plantas da Security T;
somente os vasos bloqueiam a passagem. Esta nova camada de detalhes aguarda
playtest visual. Capturas NVIDIA em POTATO/ULTRA e FOV 60/100 estão em
`visual/validation/ready_rooms_details_20261103/cafeteria/`.

Na prévia NVIDIA/ULTRA, a primeira captura saturou parede e piso. O renderer
reduz a intensidade percebida desses dois canais apenas na Cafeteria; as
capturas comparáveis estão em `visual/validation/cafeteria_material_v1/`.
A versão final mais escura foi aprovada pelo usuário em 28/10/2026.

Em 01/11/2026, uma revisão da prévia detalhou os balcões de serviço com portas,
puxadores, rodapé metálico, faixa vermelha, bandeja rasa e nicho aberto com
prateleira. A interrupção dos balcões diante da porta lateral continua igual.
As [capturas técnicas](../visual/validation/cafeteria_counter_v3/README.md)
mostram a revisão em POTATO/ULTRA e FOV 60/100; a revisão final foi aprovada
posteriormente em partida.

As colunas físicas usam uma textura estrutural própria com repetição de 0,8 m,
três níveis de desgaste e tom ajustado para a Cafeteria. A geometria e a
colisão seguem iguais. [Capturas técnicas](../visual/validation/structural_columns_v1/README.md)
em FOV 60/100 e POTATO/ULTRA documentam a validação técnica anterior ao
aceite em partida.

Em 02/11/2026, após o teste da variante longa, o usuário pediu mais detalhes
nos móveis e colunas menos frias. Cada banco da prévia recebeu duas conexões
elevadas alinhadas aos suportes: arruela, cabeça e fenda em geometria 3D.
Travessas discretas conectam as pernas aos tampos e aos bancos. A
textura estrutural das colunas foi preservada; o tom agora é um cinza quente
próximo ao das paredes. As dimensões, rotas e texturas de parede, piso e teto
não mudaram. As [capturas da revisão](../visual/validation/cafeteria_furniture_v2/README.md)
são técnicas; o resultado foi aprovado posteriormente em partida.

No teste seguinte, o usuário encontrou faces serrilhadas no topo do balcão.
O armário traseiro invadia o volume da cobertura e as bordas da bandeja
invadiam seu fundo. Esses volumes agora apenas se encontram nas bordas, sem
superfícies concorrentes. A regressão verifica que as peças superiores não se
intersectam. As [capturas do topo e da vista rasante](../visual/validation/cafeteria_counter_top_fix/README.md)
em POTATO/ULTRA e FOV 60/100 são técnicas; a correção aguarda nova avaliação
visual em partida. O usuário aprovou a sala na sequência em 02/11/2026.

A direção visual aprovada e a simulação conceitual estão em
`visual/proposals/cafeteria_material_v1/review.html`. Os nove PNGs são
carregados de `assets/visual/rooms/cafeteria/`. A captura NVIDIA/ULTRA da
versão final foi aprovada; MEDIUM e POTATO não foram capturados nesta revisão.

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
usa a condição determinada pela receita para selecionar os PNGs v1 aprovados,
e os cacheia antes do draw por `prepareCafeteriaRoomMaterials`. Salas com tema
Cafeteria usam este perfil no jogo.

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate rasteriza as três superfícies para 1.000 room seeds, cobre todas as
condições, exige hashes únicos, faixa vermelha confinada/alinhada, circulação
polida, quatro contatos de mobiliário e contraste regular das luminárias. A
regressão OpenGL comprova que o perfil permanece distinto dos demais.

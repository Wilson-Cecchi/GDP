# Environment — Central Hub

> **Estado:** variante octogonal aprovada em partida após revisão de paredes,
> piso, teto, coluna e objetos. Integrada à campanha; as travessas adicionais
> do teto também foram aprovadas em 15/11/2026.
> Em 07/11/2026, a quinta luminária física de preenchimento ao lado leste
> serviu de piloto aprovado para estender o preenchimento às demais salas.
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

### Prévia da geometria

A prévia `Central Hub` ocupa um limite de 20 × 20 m, com teto a 6 m. A câmara
tem oito faces iguais: quatro faces cardeais retas mantêm espaço para as
futuras portas, intercaladas com quatro diagonais. Segue a composição de
[`centralHub.png`](centralHub.png): uma coluna central serve de marco, enquanto
quatro rotas permitem contorná-la. As posições reservadas
para inimigos, objetos e loot permanecem livres. As peças sólidas usam o mesmo
volume para desenho e colisão; o limite caminhável segue as paredes inclinadas.
A prévia abre sem inimigos para revisar formato e navegação. Na campanha, o
mesmo contorno octogonal substitui o antigo módulo Hub; as portas cardeais
abrem vãos nas paredes e os corredores conservam sua própria área caminhável.

```bash
cmake --build build/combat-preview --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/combat-preview/gdp --central-hub-preview --seed 20261030
```

As [capturas técnicas da coluna](../visual/validation/central_hub_column_v1/README.md)
mostram FOV 60 e 100 em POTATO e ULTRA. O formato foi aprovado em partida;
o novo acabamento da coluna aguarda playtest.
O corpo central agora projeta sombra no piso nos perfis com sombras dinâmicas;
as [capturas da sombra](../visual/validation/central_hub_column_shadow_v1/README.md)
registram a mudança em ULTRA. POTATO não habilita sombras dinâmicas.
O [ajuste da sombra da coluna](../visual/validation/central_hub_column_shadow_v2/README.md)
usa o contorno octogonal do eixo em vez da caixa de colisão quadrada. A base
também bloqueia luz direta e acrescenta uma sombra de contato curta no piso,
visível inclusive em POTATO. O ajuste aguarda avaliação visual em partida.
Após o feedback em partida, a coluna deixou de usar iluminação fixa no corpo.
A prévia passou a receber as quatro luminárias e o alcance do Hub amplo, como
na campanha. As [capturas com luz, apagão e lanterna](../visual/validation/central_hub_blackout_fix_v1/README.md)
mostram a chapa sob iluminação e o corpo escurecendo quando as lâmpadas apagam.
Uma [passagem de detalhes 3D](../visual/validation/central_hub_details_v1/README.md)
acrescenta uma escotilha e uma grelha de piso em quadrantes entre as rotas,
duas grelhas no teto e dutos presos às quatro faces diagonais. As peças do piso
são rasas e caminháveis; os dutos ficam acima da área de combate e não passam
pelas portas. Esta passagem não substitui os materiais de piso e teto ainda
em proposta.
Nas paredes diagonais, a
[passagem de detalhes localizados](../visual/validation/central_hub_wall_details_v1/README.md)
inclui dois painéis de manutenção com fechos e duas grelhas com lâminas 3D.
Molduras e parafusos são peças separadas; os quatro vãos cardeais continuam livres.
Consulte a [ordem de integração](room_shape_plan.md).

A revisão visual da parede segue a
[proposta Central Hub/Wall v1](../visual/proposals/central_hub_wall_v1/README.md).
A parede v1 está aplicada no Hub octogonal e aguarda aprovação visual em partida.
O tile de 2 × 2 m repete por distância real nas faces retas e diagonais. O rodapé
grafite, filete laranja, juntas, parafusos e conduítes são peças 3D separadas.
As [capturas técnicas](../visual/validation/central_hub_wall_v1/README.md)
incluem FOV 60/100 em POTATO/ULTRA na Quadro K2100M.
Após o feedback de que as paredes estavam claras demais, a prévia de 14/11/2026
escurece apenas o tom das chapas e aumenta sua resposta metálica às luminárias.
Uma segunda revisão aproxima o tom das paredes do aço grafite quente da coluna
central, mantendo a textura e o desgaste próprios das paredes. O reflexo também
aparece em POTATO; esta revisão aguarda aprovação em partida.
Para harmonizar o restante da sala com a coluna e as paredes, o rodapé,
pilastras, conduítes, painéis, dutos, grelhas, escotilha e carcaças das
luminárias receberam grafite quente e aço de borda menos azulado. A sinalização
das rotas passou de ciano para laranja na revisão seguinte. A textura principal
do piso, já aprovada,
e a posição/intensidade das luminárias foram preservadas. As
[capturas técnicas da revisão](../visual/validation/central_hub_harmony_v1/README.md)
aguardam confirmação visual em partida.
Na prévia seguinte, o piso ganhou um tom mais quente e reflexo metálico mais
suave, mantendo o desenho e a escala das chapas. As faixas do piso, filetes da
parede, indicadores e LEDs da coluna compartilham o laranja. As
[capturas desta passagem](../visual/validation/central_hub_floor_orange_v1/README.md)
foram aprovadas pelo usuário. A passagem seguinte acrescenta pares de baias
de serviço rasas e trilhos de status nas quatro paredes diagonais, sem ocupar
as portas. O Hub agora usa apenas sua decoração fixa; o gerador deixa de
colocar a caixa ou barricada aleatória nessa sala. As demais salas conservam
seus props funcionais até a substituição por famílias próprias, descritas em
[props variáveis e drops](random_props_plan.md). As
[capturas técnicas do Hub e dos drops](../visual/validation/central_hub_decor_pickups_v1/README.md)
registram a passagem; a aprovação visual em partida ainda está pendente.

`generateCentralHubRoomMaterials(roomSeed)` seleciona `Open`, `Active` ou
`Rerouted` e um eixo `Horizontal`, `Vertical` ou `Cross`. Condição/eixo usam o
ID local 700; Wall/Floor/Ceiling usam 701–703:

- **Wall:** a receita PaintedSteel ainda define a condição e o eixo, mas no
  Hub octogonal o renderer usa o tile `wall_plate_v1.png` para o albedo.
  A coluna central usa o acabamento visual próprio descrito abaixo;
- **Floor:** a receita BareSteel ainda produz variações para o gate de geração;
  no Hub octogonal, o renderer usa `floor_plate_v1.png` em tile de 2 × 2 m,
  enquanto quatro linhas laranja são geometria localizada;
- **Ceiling:** chapa grafite `ceiling_plate_v1.png` em tile de 2 × 2 m no
  octógono. Quatro luminárias lineares ocupam quadrantes simétricos, suspensas
  por duas hastes cada uma; uma quinta, mais fraca e de menor alcance, fica
  entre as duas do lado leste. Suas fontes de luz acompanham os modelos e
  apagam quando as luminárias são destruídas.

A [coluna de navegação v1](../visual/proposals/central_hub_column_v1/README.md)
tem corpo octogonal com chapa de 1 × 1 m repetida na escala física, quatro
telas de direção, luminárias verticais, fixadores e cabos 3D. A base e as
quatro áreas das telas continuam sólidas para colisão. As telas indicam rotas,
sem interação nova nesta fase. A [proposta de piso e teto v1](../visual/proposals/central_hub_floor_ceiling_v1/README.md)
é a meta visual apontada pelo usuário. A
[primeira passagem no jogo](../visual/validation/central_hub_material_light_stage1/README.md)
aplica a chapa grafite ao piso em escala física de 2 m, separa as linhas de
navegação em peças 3D e aquece as quatro luzes do Hub. A textura principal
desse piso foi aprovada pelo usuário em 06/11/2026. A
[passagem seguinte](../visual/validation/central_hub_floor_details_ceiling_v2/README.md)
aplica metal escovado de 0,75 m aos detalhes da escotilha e grelhas, substitui
o albedo do teto por uma chapa de 2 m e suspende as quatro luminárias.

`CentralHubRoomMaterialSet` preserva o eixo compartilhado nas três receitas,
mas deriva cada superfície por sub-seed independente. Validação rejeita
parâmetros pertencentes à superfície errada, acento amarelo, divergência de eixo
e corrupção do hash canônico. O renderer prepara/cacheia o conjunto com
`prepareCentralHubRoomMaterials`. Salas com papel Hub usam este perfil no
jogo, independentemente do tema do módulo; a regressão OpenGL offscreen
também verifica a saída visual.

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate das receitas rasteriza 1.000 room seeds, cobre os três estados e os três
eixos, exige hashes únicos, exatamente uma sinalização ciano na parede,
continuidade das rotas do piso procedural e 135–144 pixels de luminária
alinhada no teto. No Hub octogonal, o piso carregado em jogo é a chapa v1 com
rotas 3D. A regressão OpenGL confirma a saída visual distinta.

## Piloto de sombras por geometria

O piloto jogável fica isolado em `--central-hub-shadow-preview`. Ele cria um
Worker e uma barricada quebrável para comparar as sombras dinâmicas da coluna,
do inimigo e do objeto. Use `--central-hub-shadow-legacy-preview` para abrir a
mesma cena com o método anterior. Ambos começam com a Sniper. A implementação,
os limites atuais e as medições estão em [dynamic_shadows_plan.md](dynamic_shadows_plan.md).
As [capturas da quinta luminária](../visual/validation/global_shadows_v3/README.md)
comparam a mesma barricada com cinco, quatro, duas, uma e nenhuma luz acesa.

```bash
cmake --build build/combat-preview --target gdp
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/combat-preview/gdp --central-hub-shadow-preview --seed 20261106
```

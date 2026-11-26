# Environment — Security

> **Estado:** materiais de parede, piso e teto implementados e validados na
> Fase 10. A prévia isolada em T com consoles e cobertura física foi aprovada
> em partida em 03/11/2026. A variante quadrada foi aprovada na prévia isolada
> em 15/11/2026 e integrada ao módulo `SecurityHall_01` da campanha. Sua
> circulação e seus encontros ainda precisam de playtest na campanha. A
> variante em T também consta na biblioteca e aguarda esse teste específico.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Função

Controle e restrição.

## Identidade

- monitores pontuais
- consoles
- vermelho de alerta
- portas reforçadas
- barricadas

## Gameplay

Guard/officer; terminais; objetivos.

## Geração

Geometria principal vem do `RoomModule`. Props, materiais, desgaste, luzes e estados variam
com sub-seeds independentes e respeitam zonas reservadas/connectors.

## Implementação atual

### Prévia da geometria

A prévia `Security` mede 18 × 18 m, com teto a 6 m, e segue a composição de
[`seguranca.png`](seguranca.png): área de operação e monitores ao fundo,
um nicho de observação opaco na lateral direita, gabinete à esquerda e duas
barricadas baixas desencontradas. A abertura central atrás da bancada tem
6,8 m, e o eixo entre barricadas mantém 6,9 m livres. Centro, flancos e quatro
posições possíveis de porta permanecem transitáveis. O nicho tem um painel de
monitoramento embutido; não é uma janela transparente nem um terminal interativo.
As peças sólidas usam a mesma geometria para desenho e colisão; a sala abre
sem inimigos para avaliar o formato.

```bash
cmake --build build/playtest --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/playtest/gdp --security-preview --seed 20261029
```

As [capturas técnicas da geometria inicial](../visual/validation/security_geometry_preview/README.md)
mostram FOV 60 e 100 em POTATO e ULTRA. O mobiliário inicial foi substituído
pela revisão abaixo, aprovada na prévia isolada; falta o teste na campanha.

### Revisão da Segurança quadrada

A prévia quadrada agora usa a mesma família de metal, fixadores, painéis e
indicadores da Segurança em T. Duas estações de controle ficam na parede ao
fundo da vista inicial,
voltadas para o centro, sem fechar a posição de porta central. O armário da
parede oeste e as duas coberturas baixas receberam os respectivos detalhes 3D.
O nicho opaco da parede leste tem três monitores embutidos com indicadores que
acompanham o estado da sala. A carcaça externa é metal de equipamento escuro,
com laterais mais estreitas e base em três painéis ventilados, distinta das
colunas estruturais. Dois vasos ocupam os
cantos sul sem bloquear as portas. As peças principais continuam com colisão;
o centro e os acessos laterais ficam livres. A composição foi aprovada
visualmente na prévia isolada em 15/11/2026 e agora
integra também a variante quadrada da campanha.

Dois dutos suspensos de 0,7 m de largura percorrem o teto em L: o trecho
principal vai de uma parede à outra perto do nicho, e o ramal segue até a
parede oposta. Eles se encontram sem sobrepor seus volumes e não cruzam as
luminárias. A altura livre sob eles é 5,18 m.
As [capturas técnicas dos dutos](../visual/validation/security_square_duct_v1/README.md)
mostram a vista geral e o detalhe em ULTRA e POTATO.

O usuário aprovou a carcaça externa do nicho. As colunas de parede da variante
quadrada recebiam o material estrutural genérico, esverdeado, enquanto a
variante em T usa o acabamento de parede de Segurança. As colunas quadradas
agora usam esse mesmo acabamento; geometria e colisão permanecem próprias da
variante quadrada. A comparação da variante quadrada em campanha ainda está pendente.

As [capturas do nicho de monitoramento](../visual/validation/security_observation_v1/README.md)
mostram o detalhe em ULTRA e POTATO; o conjunto foi aprovado no playtest da
prévia isolada.

Para validar a integração em uma partida de campanha, inicie um slot novo no
primeiro andar com a semente `1`. Essa semente contém `SecurityHall_01` com
as quatro portas, zonas de inimigos e objetos, consoles, nicho, vasos e dutos.
Confira a circulação ao redor das coberturas, tiros pelas portas e a ocupação
dos inimigos. Um checkpoint existente preserva sua própria semente.

```bash
cd /mnt/projetos/GDP/GDP-3
cmake --build build/technical-ceiling-build --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/technical-ceiling-build/gdp --seed 1
```

### Variante em T para playtest de formato

A prévia isolada `--security-t-preview --seed 20261102` mede 20 × 20 m e 6 m
de altura. O acesso sul tem 8 m de largura e desemboca numa galeria transversal
de 20 m. Há duas estações de controle na parede norte, armários junto às paredes
do acesso e duas coberturas baixas nas alas. O jogo inicia com Sniper, sem
inimigos. O material da Segurança existente permanece aplicado nesta avaliação;
há uma luminária sobre o acesso estreito e duas sobre a galeria larga.
O formato também consta na biblioteca da campanha; essa prévia segue útil para
inspeção sem inimigos. Detalhes e comando de execução estão no
[plano de formatos](room_shape_plan.md).

Na prévia em T, as três luminárias de Segurança agora ficam suspensas por duas
hastes com bases no teto. O arquivo 3D e o fonte Blender estão em
[`ceiling_fixture_family_v2`](../visual/proposals/ceiling_fixture_family_v2/README.md).
Para testar esta alteração em partida na Nvidia:

```bash
cd /home/wilson/GDP/GDP-3
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/playtest/gdp --security-t-preview --seed 20261102
```

Em 02/11/2026 começou a revisão das texturas da variante em T. A
[proposta v1](../visual/proposals/security_material_v1/README.md) apresenta
parede, piso e teto de aço/alumínio escuro em três condições, com repetição
de 2 × 2 m e conceito em perspectiva. Em 03/11/2026, a pedido do usuário,
as nove variantes PNG foram aplicadas às salas de Segurança. A prévia em T
repete as três superfícies a cada 2 m, incluindo suas faces recortadas. As
[capturas técnicas](../visual/validation/security_material_v1/README.md)
documentam a aplicação; a aparência da variante em T foi aprovada em partida
em 03/11/2026.
Consoles, armários e coberturas receberam a revisão de mobiliário v1 abaixo.

Em 03/11/2026, a revisão v2 do piso da Segurança em T trocou as manchas
grandes repetidas por um albedo metálico mais neutro, mantendo a escala de
2 × 2 m. O renderer espelha alguns tiles e aplica desgaste discreto por região
do mundo; tampas, grelhas e reparos aparecem esparsamente, fora do albedo.
Um rodapé metálico com fixações acompanha o contorno da sala e deixa os vãos
das portas livres. Pequenos painéis de manutenção decoram trechos desocupados
das paredes. Os detalhes rasos não alteram a colisão. Plantas foram reservadas
para um objeto visual próprio com colisão, ainda não implementado.
Veja as [capturas v2](../visual/validation/security_floor_details_v2/README.md).
Essa revisão integra a variante em T aprovada em partida em 03/11/2026.

### Prévia de decoração v3

Após o teste da v2, o rodapé recebeu junções físicas nas quinas. As grelhas
do piso agora têm abertura escura, moldura e barras elevadas. Dois vasos
metálicos com folhagem 3D ocupam os cantos externos da galeria; o recipiente
tem colisão, enquanto as folhas são apenas visuais. Dois trechos de duto 3D
formam um L próximo ao teto, com abraçadeiras, grelhas inferiores e suportes.
Os dutos ficam afastados das três luminárias da prévia e têm colisão na caixa
principal, acima da altura do jogador.

O tipo de peça de duto é reutilizável, mas nesta fase foi posicionado apenas
na Segurança em T. Outras salas exigem arranjos próprios para preservar luzes,
portas e leitura do teto. As [capturas v3](../visual/validation/security_details_v3/README.md)
permitem inspecionar os detalhes; a variante em T foi aprovada em partida em
03/11/2026.

`generateSecurityRoomMaterials(roomSeed)` ainda seleciona `Secured`,
`Monitored` ou `Alert` e mantém os hashes determinísticos. O renderer usa a
condição para escolher os três PNGs correspondentes. O antigo rasterizador
procedural e seus testes permanecem como baseline técnico, mas sua parede com
console pintado, piso com tampa e teto com luminárias pintadas não são mais o
albedo exibido. Consoles e luminárias devem permanecer elementos físicos
separados das superfícies repetíveis.

### Mobiliário v1 da Segurança em T

Com a [proposta aprovada](../visual/proposals/security_furniture_v1/README.md),
as duas estações ganharam três telas com indicadores, painéis de controle,
portas e fixações. Os armários laterais têm portas, dobradiças, puxadores e
ventilação; as duas coberturas têm placas, grelhas e faixa de segurança.
Os cascos usam um albedo grafite em escala física de 0,75 m, com acentos
âmbar ou vermelhos segundo o estado da sala. A geometria de colisão acompanha
os armários até 2,2 m; detalhes rasos são só visuais. As
[capturas técnicas](../visual/validation/security_furniture_v1/README.md)
registram a etapa técnica; o mobiliário da variante em T foi aprovado em
partida em 03/11/2026.

Após o relato de sobreposição na cobertura, as travessas laterais foram
afastadas dos planos do casco para eliminar a cintilação de faces na vista de
canto. O painel amarelo grande no topo foi trocado por três indicadores
estreitos. As [capturas da correção](../visual/validation/security_furniture_cover_fix/README.md)
mostram a cobertura em POTATO e ULTRA, FOV 60 e 100. A correção foi incluída na
aprovação em partida de 03/11/2026.

Os indicadores dos consoles, armários e coberturas agora acompanham o evento
real da sala. Começam âmbar; durante o bloqueio com iluminação vermelha, ficam
vermelhos por um segundo e apagados no segundo seguinte, repetindo o ciclo.
Quando o evento termina após a eliminação dos inimigos, ficam amarelos por três
segundos e depois verdes. O tempo usa o relógio da partida e pausa junto com
ela. A [sequência em capturas](../visual/validation/security_indicator_sequence/README.md)
é técnica: a prévia isolada em T não tem inimigos nem evento para acionar essas
transições por combate. Para conferir a animação na sala sem alterar a campanha,
use a demonstração visual: ela mantém o alerta por 10 segundos, mostra o amarelo
por 3 segundos e o verde por 10 segundos antes de repetir.

```bash
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/playtest/gdp --security-t-preview --security-indicator-demo --seed 20261102
```

O usuário aprovou a sala em partida em 03/11/2026. A aprovação cobre a prévia
isolada com formato, móveis, materiais e indicadores; o playtest da variante
da campanha permanece uma etapa separada.

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate legado rasteriza 1.000 room seeds e cobre os três estados. A regressão
OpenGL e as capturas da Segurança em T verificam o caminho atual com PNGs;
a aprovação visual final depende de playtest.

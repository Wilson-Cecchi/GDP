# Environment — Combat Room / Arena

> **Estado:** materiais de parede, piso e teto implementados e validados na
> Fase 10; geometria modular, coberturas e navegação entre salas continuam em
> expansão.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Função

Encontro controlado.

## Identidade

- espaço aberto
- cobertura planejada
- linhas de visão
- iluminação contrastada

## Gameplay

Combate; spawn seguro; rota de movimento.

## Geração

Geometria principal vem do `RoomModule`. Props, materiais, desgaste, luzes e estados variam
com sub-seeds independentes e respeitam zonas reservadas/connectors.

## Implementação atual

### Arena com galerias — composição completa em revisão

A [proposta visual v1](../visual/proposals/combat_arena_material_v1/README.md)
foi aprovada como direção visual. A primeira prévia, que aplicava apenas a
parede e peças simples, foi reprovada. A revisão integra paredes oliva,
piso grafite, teto escuro, galerias, escadas, corrimãos, coberturas, colunas,
vigas, dutos e grelhas em 3D. As superfícies usam tile de 2 × 2 m e desgaste
Ready, Active e Scarred. O Boss tem seu próprio material metálico. A geometria
visível tem colisão correspondente; falta aceite visual em partida.

### Arena do Boss

A campanha mantém a sala de 28 × 32 m e 7 m de altura. Duas barricadas
quebráveis no flanco esquerdo continuam no encontro. O flanco direito agora
tem duas colunas estruturais fixas, centradas nas posições locais `(7, -8)` e
`(7, 8)`, com núcleo de 1,6 × 1,6 m, base e topo de 2 × 2 m e faixa de
contraste. A composição gira com a sala e preserva as quatro posições de porta
e os spawns do Boss e dos ajudantes. Os blocos de colisão da pequena sala de
testes foram removidos; a cobertura central atual corresponde somente aos
objetos visíveis. Os objetos
gerados usam duas zonas reservadas entre o centro e as colunas para não
atravessá-las.

O acabamento atual da arena usa painéis metálicos próprios no piso, nas
paredes e no teto, com cor, relevo e propriedades de superfície. Vigas,
dutos, grelhas, luminárias e quatro armários elétricos de parede completam a
arquitetura; parafusos e faixas laranja marcam juntas e rotas. As barricadas
do Boss têm carcaça grafite com faixas de alerta. O terminal A ocupa a mesma
posição central na campanha e na prévia, com colisão nas colunas, tela e
travessas visíveis; o vão abaixo da tela permanece livre para tiros. Na arena,
o metal usa tons grafite quentes e as luzes da moldura são âmbar. A [proposta e as capturas técnicas](../visual/proposals/boss_arena_v1/README.md)
registram a comparação visual ainda pendente em partida.

Para avaliar o combate diretamente, a prévia abre essa arena com o Boss, um
Officer, um Guard, as barricadas e Sniper para o jogador. Ela concede 1,25 s
de reação inicial. O piso permanece plano nesta fase; uma plataforma acessível
exigirá navegação dos inimigos e validação separada.

```bash
cmake --build build/technical-ceiling-build --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/technical-ceiling-build/gdp --boss-room-preview --seed 20261101
```

O playtest deve conferir os dois flancos, tiro bloqueado pelas colunas,
deslocamento do Boss e dos ajudantes após perder visão, e acesso ao centro.

### Prévia da geometria

A variante isolada da Arena mede 20 × 22 m, com teto a 6 m. Seis muretas
assimétricas deixam uma passagem central de 4 m e flancos largos; galerias
laterais elevadas, oito colunas, cinco vigas e dois dutos físicos marcam a altura sem fechar a
linha de visão. Duas escadas laterais de 14 degraus finos, apoiados por
suportes na parede, seguem o alinhamento das galerias e chegam à ponta delas.
Seis pilares adicionais sustentam as bordas internas das galerias, três por
lateral, com bases no piso e capitéis sob a plataforma. Eles terminam antes da
escada e deixam livres as portas e as passagens centrais. O pilar mais próximo
de cada porta lateral fica em `z=2,0 m` local: a aproximação direta às portas
foi verificada com o collider do jogador nas quatro rotações do módulo de
campanha. A posição anterior em `z=0,3 m` deixava o volume da porta livre,
mas bloqueava essa aproximação.
As galerias ficam junto às paredes laterais, e o espaço sob os degraus
superiores permanece aberto. A
geometria visível também é usada pela colisão de navegação, e a validação
rejeita peças dentro de zonas de inimigos, objetos ou portas. A prévia isolada
não gera inimigos para permitir examinar a sala. A variante com a mesma escada
e galeria agora integra a campanha para playtest com encontros e portas reais;
consulte [o roteiro de campanha](campaign_room_playtest_2026-11-04.md).

Com a build de playtest validada nesta fase, abrir diretamente esta sala:

```sh
cmake --build build/technical-ceiling-build --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/technical-ceiling-build/gdp --combat-room-preview --seed 20261028
```

`generateCombatRoomMaterials(roomSeed, true)` seleciona o conjunto novo de
parede, piso e teto na prévia e em `MaintenanceGalleryArena_01`; o perfil sem
o segundo argumento continua na sala do Boss.
As placas das paredes da galeria têm 3 × 3 m; cada chapa de 6 × 6 m recebe
orientação e variação sutil de tinta por posição mundial. O piso agora tem
uma chapa de 2 × 2 m por tile e o teto mantém painéis escuros em escala de
2 × 2 m. As capturas desta revisão estão em
`visual/validation/combat_arena_floor_ceiling_v5/`. Os obstáculos usam
um material exclusivo em escala de 1,2 m, com faces recuadas, nervuras e
fixadores 3D; já não recebem a imagem da parede. Travessas nas colunas
encontram vigas perimetrais no teto. Os mezaninos ganharam mãos francesas,
chapas de ancoragem e fixadores sem bloquear a circulação. As colunas agora
têm metal grafite próprio, rebites e reflexo controlado. Uma barra emissiva
duplicada à frente de cada luminária foi removida, mantendo a lente 3D e as
cinco luzes quebráveis. O acabamento dos corrimãos agora é metal pintado
laranja com escovação fina, riscos e reflexo próprio, sem juntas impressas
sobre as emendas 3D. Os pilares das galerias usam o acabamento escuro das
colunas. A aparência final ainda aguarda playtest.
As capturas dos novos pilares e corrimãos ficam em
`visual/validation/combat_arena_rails_pillars_v7/`.
As cinco luminárias agora usam um branco quente suave `(1,00; 0,88; 0,76)`
na luz e na lente, com intensidade e alcance preservados. As capturas da
temperatura revisada estão em `visual/validation/combat_arena_warm_lamps_v8/`.

O perfil original de `generateCombatRoomMaterials(roomSeed)` escolhe deterministicamente `Ready`,
`Active` ou `Scarred` e deriva três receitas independentes:

- **Wall:** PaintedSteel verde-oliva, placas grandes 1–4 × 2, moldura reforçada,
  sinalização amarela curta e 1–3 impactos localizados conforme o desgaste;
- **Floor:** Ceramic representando concreto/metal escuro em quatro grandes
  lajes, perímetro de arena amarelo dessaturado e marcas de passagem esparsas
  (somente no perfil original do Boss);
- **Ceiling:** BareSteel escuro, vigas espessas e luminárias espaçadas que
  preservam a leitura de navegação.

Condição, desgaste e impactos variam por seed sem transformar a arena em espaço
abandonado. IDs locais 200–203 do domínio `RoomMaterial` isolam o perfil dos
outros ambientes. Cada receita e o conjunto completo possuem hashes canônicos,
usados pelo cache do renderer antes do draw. O Boss e a Arena com galerias
selecionam este perfil no jogo. As arenas compactas de Manutenção (503) e
Segurança (603) usam os materiais atuais dos respectivos temas, inclusive piso
e teto; a regressão OpenGL offscreen também verifica a saída visual.

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate gera e rasteriza as três superfícies para 1.000 room seeds, cobre as três
condições, exige hashes únicos, 1–3 núcleos de impacto, perímetro contínuo no
piso e contraste mínimo entre vigas e luminárias. A regressão OpenGL confirma
que o perfil altera o framebuffer em relação aos demais ambientes implementados.

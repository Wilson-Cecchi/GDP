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

### Arena do Boss

A campanha mantém a sala de 28 × 32 m e 7 m de altura. Duas barricadas
quebráveis no flanco esquerdo continuam no encontro. O flanco direito agora
tem duas colunas estruturais fixas, centradas nas posições locais `(7, -8)` e
`(7, 8)`, com núcleo de 1,6 × 1,6 m, base e topo de 2 × 2 m e faixa de
contraste. A composição gira com a sala, mantém a cobertura central anterior
e preserva as quatro posições de porta e os spawns do Boss e dos ajudantes. Os objetos
gerados usam duas zonas reservadas entre o centro e as colunas para não
atravessá-las.

Para avaliar o combate diretamente, a prévia abre essa arena com o Boss, um
Officer, um Guard, as barricadas e Sniper para o jogador. Ela concede 1,25 s
de reação inicial. O piso permanece plano nesta fase; uma plataforma acessível
exigirá navegação dos inimigos e validação separada.

```bash
cmake --build build/playtest --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/playtest/gdp --boss-room-preview --seed 20261101
```

O playtest deve conferir os dois flancos, tiro bloqueado pelas colunas,
deslocamento do Boss e dos ajudantes após perder visão, e acesso ao centro.

### Prévia da geometria

A variante isolada da Arena mede 20 × 22 m, com teto a 6 m. Quatro coberturas
assimétricas deixam uma passagem central de 4 m e flancos largos; galerias
laterais elevadas, pilares e duas vigas altas marcam a altura sem fechar a
linha de visão. Duas escadas laterais de 14 degraus finos, apoiados por
suportes na parede, seguem o alinhamento das galerias e chegam à ponta delas.
As galerias ficam junto às paredes laterais, e o espaço sob os degraus
superiores permanece aberto. A
geometria visível também é usada pela colisão de navegação, e a validação
rejeita peças dentro de zonas de inimigos, objetos ou portas. A prévia isolada
não gera inimigos para permitir examinar a sala. A variante com a mesma escada
e galeria agora integra a campanha para playtest com encontros e portas reais;
consulte [o roteiro de campanha](campaign_room_playtest_2026-11-04.md).

Com a build de playtest validada nesta fase, abrir diretamente esta sala:

```sh
cmake --build build/combat-preview --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/combat-preview/gdp --combat-room-preview --seed 20261028
```

Os materiais usados na prévia são os do perfil Combat Room já existente. O
trabalho de texturas de parede, piso e teto vem após a aprovação do espaço.

`generateCombatRoomMaterials(roomSeed)` escolhe deterministicamente `Ready`,
`Active` ou `Scarred` e deriva três receitas independentes:

- **Wall:** PaintedSteel verde-oliva, placas grandes 1–4 × 2, moldura reforçada,
  sinalização amarela curta e 1–3 impactos localizados conforme o desgaste;
- **Floor:** Ceramic representando concreto/metal escuro em quatro grandes
  lajes, perímetro de arena amarelo dessaturado e marcas de passagem esparsas;
- **Ceiling:** BareSteel escuro, vigas espessas e luminárias espaçadas que
  preservam a leitura de navegação.

Condição, desgaste e impactos variam por seed sem transformar a arena em espaço
abandonado. IDs locais 200–203 do domínio `RoomMaterial` isolam o perfil dos
outros ambientes. Cada receita e o conjunto completo possuem hashes canônicos,
usados pelo cache do renderer antes do draw. Salas com papel Arena ou Boss
selecionam este perfil no jogo, independentemente do tema do módulo; a
regressão OpenGL offscreen também verifica a saída visual.

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate gera e rasteriza as três superfícies para 1.000 room seeds, cobre as três
condições, exige hashes únicos, 1–3 núcleos de impacto, perímetro contínuo no
piso e contraste mínimo entre vigas e luminárias. A regressão OpenGL confirma
que o perfil altera o framebuffer em relação aos demais ambientes implementados.

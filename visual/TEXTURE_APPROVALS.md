# Registro de aprovação visual de texturas

Este arquivo é o gate humano obrigatório para a revisão visual do GDP. Os
materiais e texturas existentes são um **baseline técnico funcional**: podem
continuar sendo usados durante o desenvolvimento, mas não representam arte
final aprovada.

Pranchas e capturas PNG/GIF em `visual/` são artefatos locais de revisão, não
assets carregados pelo jogo. Elas não acompanham novos clones do repositório;
os caminhos registrados abaixo documentam o histórico da aprovação e podem
não estar disponíveis fora da máquina onde foram geradas. Para novas aprovações,
anexe a proposta e as capturas ao pedido de revisão visual.

## Regra obrigatória

Nenhuma textura ou receita visual revisada pode ser implementada no renderer,
gerador procedural ou HUD antes de uma aprovação visual explícita do usuário.
Alterações de gameplay, correções de cobertura e diagnósticos podem continuar,
desde que não sejam apresentados como aprovação artística da textura.

Toda proposta nova de superfície ou objeto deve declarar o tamanho físico do
tile e como o UV preserva a mesma densidade em peças curtas e compridas. Não
aplicar um único PNG esticado para preencher uma face maior. Se o desenho não
for repetível (números, placas, juntas ou dano localizado), separar esses
elementos em geometria, faixa de acabamento ou decal. O contrato e a
verificação estão em [Escala física das texturas](TEXTURE_SCALE.md).

Texturas de teto devem representar somente a superfície arquitetônica neutra:
painéis, juntas, vigas, dutos e desgaste podem existir, mas nenhuma lâmpada,
lente luminosa, faixa emissiva ou brilho de luminária pode ser desenhado ou
gravado no albedo. A luminária é sempre a unidade independente
`Lighting/CeilingFixture`, aplicada ao modelo próprio e com material emissivo
separado. Essa restrição deve aparecer literalmente em todo prompt futuro de
teto e impede a sobreposição entre iluminação pintada e geometria real.

Como cada receita procedural pode gerar infinitas variantes, a unidade de
aprovação é uma superfície ou família visual identificável, por exemplo
`Storage/Wall`, `Security/Ceiling`, `Worker/PaintedSteel` ou `Door/Panel`.
A aprovação usa uma prancha com seeds representativas; não exige aprovar cada
imagem gerada, mas não se estende a outra superfície ou família.

## Fluxo de trabalho

1. Escolher exatamente uma unidade visual e registrar objetivo, referências,
   paleta, material, densidade de detalhe, desgaste e função no gameplay.
2. Produzir uma proposta visual antes de editar a implementação definitiva. A
   prancha deve mostrar pelo menos três variantes procedurais, vista próxima,
   vista de gameplay e iluminação neutra suficiente para julgar o material.
3. Marcar a proposta como `AGUARDANDO APROVAÇÃO` e apresentar o artefato ao
   usuário.
4. Se houver pedido de revisão, manter o código atual e gerar uma nova versão
   da proposta. Aprovação parcial vale apenas para os itens citados.
5. Somente após a resposta explícita `APROVADA PARA IMPLEMENTAÇÃO` alterar
   recipes, shaders, paletas, mapas ou assets daquela unidade.
6. Depois da implementação, gerar capturas dentro do jogo, executar regressões
   técnicas e solicitar a aceitação visual final. Divergência relevante em
   relação à proposta aprovada retorna para revisão.

## Estados permitidos

```text
NÃO REVISADA
    ↓
PROPOSTA EM PREPARAÇÃO
    ↓
AGUARDANDO APROVAÇÃO
    ├──→ REVISÃO SOLICITADA ──→ PROPOSTA EM PREPARAÇÃO
    └──→ APROVADA PARA IMPLEMENTAÇÃO
                              ↓
                    IMPLEMENTADA EM VALIDAÇÃO
                              ↓
                         ACEITA NO JOGO
```

`APROVADA PARA IMPLEMENTAÇÃO` precisa registrar o artefato e a versão exatos.
Comentários genéricos não aprovam outras unidades visuais nem uma futura
variante da mesma proposta.

## Registro mínimo de cada aprovação

```text
unidade visual:
versão da proposta:
arquivo/prancha:
seeds mostradas:
tile físico / mapeamento UV:
decisão do usuário:
data:
commit de implementação:
capturas no jogo:
resultado final:
```

## Propostas ativas

### Central Hub/Column — proposta v1

```text
unidade visual: Central Hub/Column
versão: v1
data: 2026-11-05
proposta: visual/proposals/central_hub_column_v1/README.md
conceito: visual/proposals/central_hub_column_v1/concept.png
decisão do usuário: "gostei da proposta, pode implementar"
estado: IMPLEMENTADA EM VALIDAÇÃO
asset do jogo: assets/visual/rooms/central_hub/column_plate_v1.png
capturas técnicas: visual/validation/central_hub_column_v1/README.md; visual/validation/central_hub_blackout_fix_v1/README.md
aceite visual em partida: pendente
```

### Central Hub/Floor e Central Hub/Ceiling — proposta v1

```text
unidades visuais: Central Hub/Floor; Central Hub/Ceiling
versão: v1
data: 2026-11-05
proposta: visual/proposals/central_hub_floor_ceiling_v1/README.md
conceito: visual/proposals/central_hub_floor_ceiling_v1/concept.png
decisão do usuário: imagem conceitual apontada como meta de detalhes, reflexos e iluminação em 2026-11-06
estado: FLOOR/TEXTURA PRINCIPAL ACEITA NO JOGO EM 06/11/2026; DETALHES E CEILING IMPLEMENTADOS EM VALIDAÇÃO
asset do jogo: assets/visual/rooms/central_hub/floor_plate_v1.png; floor_detail_metal_v1.png; ceiling_plate_v1.png
capturas técnicas: visual/validation/central_hub_material_light_stage1/README.md
detalhes 3D localizados para playtest: visual/validation/central_hub_details_v1/README.md
nova passagem técnica: visual/validation/central_hub_floor_details_ceiling_v2/README.md
aceite dos novos detalhes e teto em partida: pendente
```

### Central Hub/Wall — proposta v1

```text
unidade visual: Central Hub/Wall
versão: v1
data: 2026-11-04
proposta: visual/proposals/central_hub_wall_v1/README.md
prancha: visual/proposals/central_hub_wall_v1/contact_sheet.png
referências: 06_environments/centralHub.png e captura nativa do Hub
tile físico proposto: 2 × 2 m; UV por metros reais ao longo de cada face
estado: APROVADA PARA IMPLEMENTAÇÃO; implementada em validação
asset do jogo: assets/visual/rooms/central_hub/wall_plate_v1.png
aceite final em partida: pendente
```

As três condições compartilham o mesmo painel de aço pintado; conduítes,
rodapé, filete ciano e placas de direção são geometria localizada. A prancha
é direção de arte; o tile aplicado foi preparado a partir dela. Capturas
técnicas: visual/validation/central_hub_wall_v1/README.md e
visual/validation/central_hub_wall_details_v1/README.md. Os painéis de
manutenção e grelhas 3D aguardam aceite visual em partida.

### Security/Wall, Security/Floor, Security/Ceiling — v1

```text
unidades visuais: Security/Wall; Security/Floor; Security/Ceiling
versão: v1
data: 2026-11-02
proposta: visual/proposals/security_material_v1/README.md
prancha: visual/proposals/security_material_v1/contact_sheet.png
conceito em perspectiva: visual/proposals/security_material_v1/scene_concept.png
condições: Secured, Monitored, Alert
tile físico proposto: 2 × 2 m por superfície; UV contínuo por metros reais
decisão do usuário: "aplica as novas texturas" em 03/11/2026
estado: IMPLEMENTADA EM VALIDAÇÃO
assets do jogo: assets/visual/rooms/security/{wall,floor,ceiling}_{secured,monitored,alert}.png
capturas técnicas: visual/validation/security_material_v1/
aceite final em partida: pendente
```

O teto proposto contém apenas albedo arquitetônico neutro; luminárias 3D
permanecem separadas. Tampa de piso e ventilação serão detalhes localizados,
fora dos tiles repetíveis. Antes da integração, validar emendas e densidade
em faces longas. A imagem em perspectiva é conceito, não captura nativa.
A implementação repete as texturas a cada 2 m nas faces da sala em T e
preserva as luminárias como geometria separada.

### Security/Floor e detalhes de parede — revisão v2

```text
unidades visuais: Security/Floor; Security/WallTrim
versão: v2
data: 2026-11-03
pedido do usuário: reduzir repetição do piso e decorar paredes com rodapé
estado: IMPLEMENTADA EM VALIDAÇÃO
asset do jogo: assets/visual/rooms/security/floor_neutral_v2.png
fonte e processo: visual/proposals/security_floor_v2/README.md
capturas técnicas: visual/validation/security_floor_details_v2/
aceite final em partida: pendente
```

As paredes e tetos da v1 continuam como superfície base. Rodapé, fixações e
painéis são geometria localizada e deixam as aberturas livres. Plantas ainda
não integram esta revisão.

### Security/Decoration — revisão v3

```text
unidades visuais: Security/WallTrim; Security/FloorVent; Security/Planter; Security/CeilingDuct
versão: v3
data: 2026-11-03
pedido do usuário: unir rodapé nas quinas, destacar grelhas, incluir plantas e dutos 3D
estado: IMPLEMENTADA EM VALIDAÇÃO
capturas técnicas: visual/validation/security_details_v3/
aceite final em partida: pendente
```

Os dois vasos e os dois trechos de duto estão na prévia isolada da Segurança
em T. A geometria de duto pode ser reutilizada em outras salas após revisão
de cada teto e luminária.

### Security/Furniture — mobiliário v1

```text
unidade visual: Security/Furniture (consoles, armários e coberturas da prévia em T)
versão: v1
data: 2026-11-03
proposta: visual/proposals/security_furniture_v1/concept.png
tile físico: 0,75 × 0,75 m, mapeamento por posição no mundo com espelhamento nas emendas
decisão do usuário: "proposta visual aceita, pode iniciar a implementação baseada no modelo que vc criou"
estado: APROVADA NA PRÉVIA ISOLADA em 03/11/2026
asset do jogo: assets/visual/rooms/security/furniture_graphite_v1.png
capturas técnicas: visual/validation/security_furniture_v1/
aceite final em partida: sala Segurança em T aprovada em 03/11/2026
```

O conceito guia forma e acabamento; as ferragens, telas, ventilações e
faixas de segurança são volumes localizados. Os estados Secured, Monitored
e Alert compartilham o albedo base e ajustam a tonalidade do casco. Os
indicadores começam âmbar e seguem o alerta real: vermelho piscante, amarelo
por três segundos após o encontro e verde em seguida. A sequência foi
registrada em `visual/validation/security_indicator_sequence/`.
O defeito de faces sobrepostas na cobertura, relatado em 03/11/2026, foi
corrigido no renderer e registrado em
`visual/validation/security_furniture_cover_fix/`. A sala Segurança em T,
incluindo o mobiliário e a sequência de indicadores, foi aprovada em partida
em 03/11/2026.

### Cafeteria/Furniture — materiais dos móveis v1

```text
unidade visual: Cafeteria/Furniture (mesa, bancos, suportes e balcão)
versão: v1
data: 2026-10-31
pranchas: visual/proposals/cafeteria_furniture_v1/{clean,inuse,faded}_board.png
variantes: Clean, InUse, Faded
decisão do usuário: "aprovados com essa textura metalica" em 31/10/2026
estado: IMPLEMENTADA E APROVADA NA CAFETERIA ALONGADA
assets do jogo: assets/visual/rooms/cafeteria/furniture_{steel,seat,support,cabinet}_{clean,inuse,faded}.png
capturas técnicas: visual/validation/cafeteria_furniture_v1/;
                  visual/validation/cafeteria_furniture_v1_uv_fix/;
                  visual/validation/cafeteria_furniture_metal_v1/
```

As pranchas mostram detalhe e distância aproximada de gameplay. O
[contrato de aplicação](proposals/cafeteria_furniture_v1/README.md) separa
materiais de geometria e exige escala física do detalhe, especialmente no
balcão. O renderer agora usa os 12 albedos e junta/puxador em escala física;
o playtest humano da variante alongada foi aprovado em 02/11/2026. A revisão
posterior removeu faixas longas dos albedos e corrigiu a repetição no shader. O aço
agora reflete luminárias e lanterna; os acabamentos pintados recebem brilho
mais difuso. As capturas dessa resposta à luz são técnicas; o aceite final
foi dado em partida para a Cafeteria alongada.

Em 02/11/2026, após testar a Cafeteria alongada, o usuário pediu fixadores 3D
nos bancos e colunas mais coerentes com a paleta da sala. A prévia recebeu
fixadores geométricos sobre os apoios, travessas sob mesas/bancos e um tom de
coluna mais quente, sem
alterar os PNGs aprovados de parede, piso, teto ou móveis. As
[capturas técnicas](validation/cafeteria_furniture_v2/README.md) registram a
revisão. O playtest posterior revelou faces irregulares no topo do balcão.
As peças sobrepostas foram ajustadas, com [capturas técnicas de topo e vista
rasante](validation/cafeteria_counter_top_fix/README.md); a nova correção
foi aprovada em partida em 02/11/2026. O usuário sugeriu usar parafusos 3D
com mais frequência em futuras mesas, estantes, armários e colunas, sem
reabrir esta sala.

### Lighting/CeilingFixture — família v2 por sala

```text
unidade visual: Lighting/CeilingFixture
versão: família v2
data: 2026-10-31
prancha: visual/proposals/ceiling_fixture_family_v2/family_board.png
variantes: sete temas de sala, Boss/Arena e Hub; carcaça Maintained/Used/Worn
decisão do usuário: aprovada em 31/10/2026
estado: IMPLEMENTADA EM VALIDAÇÃO
assets do jogo: assets/visual/models/ceiling_fixture_family_v2/*.isemesh
capturas técnicas: visual/validation/ceiling_fixture_family_v2/
```

A [proposta editável no Blender](proposals/ceiling_fixture_family_v2/README.md)
mantém uma base de luminária comum e apresenta adaptações de geometria por
ambiente. As pranchas distinguem condição da carcaça dos estados ON/OFF/ALERT.
As nove variantes foram aprovadas para integração. A validação técnica no jogo
e o playtest da aparência são registrados separadamente.
O apagão dinâmico foi implementado em 31/10/2026: ao quebrar as luminárias,
a luz ambiente rebatida da sala diminui; luzes de salas vizinhas deixam de
atravessar paredes distantes. As [capturas técnicas](validation/room_blackout_v1/README.md)
mostram o resultado com luzes intactas, destruídas e lanterna ligada na
Cafeteria e no Storage. A aparência do apagão em partida aguarda validação.
A revisão de 01/11/2026 também mantém a intensidade ambiente própria de cada
sala visível ao fundo, inclusive quando observada pela porta de outra sala.
O renderer carrega as nove malhas; há capturas nativas de Storage,
Maintenance, Laboratory, Security e Hub na Quadro K2100M, POTATO/ULTRA,
FOV 60/100. O playtest humano das demais variantes integradas está pendente.
Em 31/10/2026, o usuário relatou ter gostado da prévia Security aberta na
build de teste com sniper. O comentário confirma essa prévia; não estende a
aprovação em partida às outras oito variantes da família.

### Industrial Corridor / conjunto próprio de materiais — v2

```text
unidade visual: Industrial Corridor / parede, piso e teto exclusivos do corredor
versão: v2
data: 2026-10-31
prancha: visual/proposals/industrial_corridor_dedicated_material_v2/board.png
variantes: Maintained, Used, Worn
decisão do usuário: aguardando
estado: AGUARDANDO APROVAÇÃO
```

A v1 recebeu pedido de revisão por se parecer demais com o material atual.
Contrato e limites da v2 em
[`proposals/industrial_corridor_dedicated_material_v2/README.md`](proposals/industrial_corridor_dedicated_material_v2/README.md).

### Industrial Corridor / conjunto próprio de materiais — v1

```text
unidade visual: Industrial Corridor / parede, piso e teto exclusivos do corredor
versão: v1
data: 2026-10-31
prancha: visual/proposals/industrial_corridor_dedicated_material_v1/board.png
variantes: Maintained, Used, Worn
decisão do usuário: "faz uma textura realmente nova"
estado: REVISÃO SOLICITADA
```

Contrato e limites em
[`proposals/industrial_corridor_dedicated_material_v1/README.md`](proposals/industrial_corridor_dedicated_material_v1/README.md).

### Sniper — luneta v2 e visão de mira v1

```text
unidades visuais: Weapon/Sniper optic v2; HUD/Sniper scoped view v1
data: 2026-10-02
prancha: visual/proposals/sniper_scope_proposal_v2.png
contrato/prompt: visual/proposals/sniper_scope_proposal_v2.md
variantes da luneta: Mantido / Usado / Gasto (Usado recomendada)
vistas: close da luneta; SEM MIRA e MOUSE2 ilustrativos
seeds: não aplicável à prancha conceitual
pedido: luneta mais visível e retículo na visão de mira
resposta funcional: "As duas coisas"
decisão visual desta prancha: "aprovado pode implementar" — Usado v2 / mira v1
commit de implementação: feat: implement approved sniper optic and scoped aim
capturas nativas: visual/validation/sniper_optic_v2_gameplay.png;
                 visual/validation/sniper_optic_v2_ground.png;
                 visual/validation/sniper_scope_v2_medium_1280x720.png;
                 visual/validation/sniper_scope_v2_medium_960x600.png;
                 visual/validation/sniper_scope_v2_medium_320x200.png;
                 visual/validation/sniper_scope_v2_potato_1280x720.png
estado: IMPLEMENTADA EM VALIDAÇÃO — aceitação no jogo pendente
```

Preserva a identidade da sniper v1 fora da luneta e o HUD Tático fora da visão
de mira. Aprovações da sniper v1 e do player Usado não se estendem a esta prancha.
Zoom e balanceamento preservados. A abertura é inteira na build, diferente do
corte do painel ilustrativo explicitado ao aprovar. Capturas da fixture OpenGL
offscreen não equivalem a playtest manual. Evidências e roteiro de teste estão
em `04_gameplay/sniper_scope_2026-10-02.md`. Na etapa da luneta, o player ainda
não estava implementado; seu estado atual está nas unidades seguintes.

### Player presentation/Hands + Reload contacts — v2

```text
unidade visual: mãos, luvas/punhos e contatos nas recargas do player
versão: v2 — anatomia e material refinados; poses próprias por fase/arma
data: 2026-10-03
prancha: visual/proposals/player_hands_proposal_v2.png
contrato/prompts: visual/proposals/player_hands_proposal_v2.md
variantes: Mantido / Usado / Gasto; Usado recomendada
pedido: "prepara um visual melhor para as mãos e ajuste as mãos para
         quando tiver que recarregar as armas"
decisão do usuário: "visual simulado aprovado para implementação" — Usado v2
commit de implementação v2: entrega mãos Usado v2 (histórico Git)
capturas nativas: visual/validation/player_hands_v2_* (fixture OpenGL offscreen)
implementação/testes: 04_gameplay/player_hands_2026-10-03.md
estado: REVISÃO SOLICITADA — implementação v2 rejeitada no playtest
```

Prancha entregue: detalhe palma/dorso, três desgastes, empunhadura da pistola
e entrada do pente da carabina. Storyboard extra das recargas não foi gerado
(ferramenta rejeitou); o contrato documenta as poses propostas para todas as
armas, sem apresentá-las como animações executadas na etapa de proposta.
Implementação agora possui capturas/GIFs nativos das cinco recargas, contatos
com orientação/flexão por dedo e mão direita no ferrolho da sniper. Custo:
três draws, 4016 triângulos, 36 juntas e aproximadamente 9 MiB de mapas com
mipmaps; sem benchmark comparativo de FPS. Preserva FOV próximo de
68°, identidade das armas e regras de munição/tempo. Torso/cinto é unidade
separada e continua pendente. Aceitação visual final desta v2 depende do jogo.

Feedback de playtest: mãos/mangas finas e longas, proporção diferente da
prancha, faca com duas mãos e punhos dobrando excessivamente nas recargas.
O commit `88c5f87` não recebeu aceitação visual. A referência Usado v2
permanece aprovada como direção; correção local da implementação em revisão,
com evidências nativas e contrato em `04_gameplay/player_hands_2026-10-04.md`.
Revisão posterior dos contatos das seis armas e encurtamento dos braços:
`04_gameplay/player_weapon_contacts_2026-10-04.md`. O usuário aprovou
movimentação e visuais **por enquanto** em 2026-10-05, após testar o jogo:
aceitação provisória da revisão posterior, não da implementação rejeitada em
`88c5f87`. Texturas e identidade das armas não mudaram; torso/cinto seguem
fora desta aprovação. Testes técnicos isolados não constituem aprovação visual.

### Player presentation/Torso + Belt — v4

```text
unidade visual: torso, cintura, cinto e suportes do Player
versão: v4 — uniforme grafite e locais de saque, incluindo granada no cinto esquerdo dianteiro
data: 2026-10-18
prancha: visual/proposals/player_body_proposal_v4.png
contrato: visual/proposals/player_body_proposal_v4.md
decisão do usuário sobre a prancha v4: usar como referência para modelagem no Blender em 2026-10-20
implementação: blockout Blender criado; prévia opt-in GDP_PLAYER_RIG_PREVIEW=1 na build local; export ISPLYR03 com torso, pernas e braços separados; corpo v1 permanece padrão
estado: REFERÊNCIA DE MODELAGEM APROVADA; VALIDAÇÃO VISUAL EM JOGO PENDENTE
```

O pedido de animação definiu locais de guarda e saque. O usuário autorizou a
prancha v4 como referência para o modelo e rig em Blender. O relatório técnico
está em `visual/validation/player_blender_rig/REPORT.md`. A integração na build
está disponível como prévia de teste, com capturas em
`visual/validation/player_blender_game/`. A aceitação da aparência animada em
partida continua pendente; esta prévia não promove a v4 como visual padrão.

### Player presentation/Torso + Belt — v3 (histórico)

```text
unidade visual: torso, cintura, cinto e continuidade do uniforme do Player
versão: v3 — silhueta humana e uniforme grafite com mangas até as luvas
data: 2026-10-18
prancha: visual/proposals/player_body_proposal_v3_sleeved.png
contrato: visual/proposals/player_body_proposal_v3.md
decisão do usuário sobre a prancha v3: pendente
implementação: nenhuma; a build mantém o corpo v1
estado: SUBSTITUÍDA PELA V4 ANTES DA APROVAÇÃO
```

A prancha v3 é conceito, não captura nativa nem mesh. A aprovação anterior dos
inimigos não se estende ao Player. Animações do corpo inferior sem mudança de
receita visual podem ser verificadas em paralelo; modelagem e materiais desta
v3 exigem aprovação explícita da prancha identificada acima.

### Player presentation/Torso + Belt — v2

```text
unidade visual: torso/abdômen/cintura e cinto com coldre/bainha do player
versão: v2 — uniforme grafite preservado; definição anatômica e equipamentos
data: 2026-10-02
prancha: visual/proposals/player_body_proposal_v2.png
contrato/prompts: visual/proposals/player_body_proposal_v2.md
variantes: Mantido / Usado / Gasto; Usado recomendada
pedido: corpo parado estranho; acrescentar torso e cinto para pistola/faca
decisão do usuário sobre a prancha v2: nenhuma ainda
commit de implementação: nenhum; renderer permanece na v1
capturas nativas da v2: nenhuma; prancha é conceito, não screenshot
estado: AGUARDANDO APROVAÇÃO — Usado v2 recomendada
```

Não amplia o inventário nem redesenha pistola/faca. Equipamento no cinto
segue propriedade e arma ativa: arma em uso ou largada não reaparece guardada.
V1 não aprova automaticamente a geometria/materiais novos da v2. Braços/mãos
têm aprovação própria e implementação v2 em validação; torso/cinto aguardam
aprovação explícita desta nova prancha antes de qualquer alteração na build.

### Player presentation — v1

```text
unidade visual: Player presentation — corpo e mãos em primeira pessoa
versão: v1 — uniforme industrial grafite
data: 2026-10-02
prancha: visual/proposals/player_body_proposal_v1.png
contrato/prompt: visual/proposals/player_body_proposal_v1.md
variantes: Mantido / Usado / Gasto (Usado recomendada)
referência: captura nativa da carabina Used, somente contexto de estilo
seeds: não aplicável à prancha conceitual; não é saída do gerador do jogo
pedido: pernas visíveis e animadas ao andar/pular/agachar; mãos nas recargas
decisão do usuário: "visual aprovado" — Usado APROVADA PARA IMPLEMENTAÇÃO
commit de implementação: entrega corpo inferior v1 (histórico Git)
capturas no jogo: visual/validation/player_body_v1_* (fixture OpenGL offscreen)
estado: PARCIALMENTE IMPLEMENTADA; REVISÃO SOLICITADA da postura parada/cintura
```

Não aprova redesenho das armas, inimigos, HUD ou ambientes. Corpo proposto
é humano, com tecido/couro/borracha foscos, não metálico ou robótico.
Corpo inferior acompanha simulação e distância real, com skinning e oclusão.
Testes/capturas e orçamento estão em `04_gameplay/player_body_2026-10-02.md`;
não equivalem a playtest humano nem benchmark de FPS. Braços, mãos e contatos
nas recargas tiveram candidato v1 local, com revisão de proporções solicitada
("essas mãos estão fora de proporção"). Esse histórico permanece em
`04_gameplay/player_hands_2026-10-02.md`. Novo refinamento em 2026-10-03
originou Hands + Reload contacts v2 acima, aprovado para implementação e agora
em validação. Isso não aceita automaticamente o resultado no jogo nem aprova
torso/cinto v2.

Feedback após teste do corpo: "notei que o corpo parado fica meio estranho
talvez seja melhor colocar um torso, talvez até um cinto para segurar a
pistola e faca enquanto não estiverem em uso". V1 não recebeu aceitação
visual final; revisão v2 registrada acima, sem alteração do código atual.

### Weapon/reload — descarte e carregador novo v2

Pedido confirmado: "faça isso no jogo por favor" após proposta de descartar
balas restantes e contar pentes completos. Estado: IMPLEMENTADA EM VALIDAÇÃO.
O pente velho cai sem retornar e outro entra; escopeta mantém cartuchos.
Nesta etapa, meshes e materiais aprovados foram reutilizados, sem novas
texturas ou mãos. Mãos implementadas posteriormente em unidade própria acima.
Contrato: `04_gameplay/weapon_reload_animation_2026-10-02.md` (seção v2).
O playtest ainda decide aceitação visual; testes técnicos não substituem essa
aprovação. O registro v1 seguinte permanece como histórico.

### Weapon/reload — animações rígidas v1 (histórico)

```text
unidade visual: animação de recarga por arma; sem revisão de texturas
pedido do usuário: "podemos colocar animação de recando as armas para cada arma diferente?"
data: 2026-10-02
contrato: 04_gameplay/weapon_reload_animation_2026-10-02.md
escopo: pistola, carabina, sniper, escopeta e SMG provisória; faca sem recarga
implementação: perfis próprios e peças rígidas móveis; sem mãos/IK
capturas: visual/validation/weapon_reload_v1_pistol.gif;
          visual/validation/weapon_reload_v1_carbine.gif;
          visual/validation/weapon_reload_v1_smg.gif;
          visual/validation/weapon_reload_v1_sniper.gif;
          visual/validation/weapon_reload_v1_shotgun.gif
estado: IMPLEMENTADA EM VALIDAÇÃO; aceitação visual final pendente
```

As sequências são da fixture nativa offscreen, seed 1717, 31 poses/arma,
640×360. As cores/texturas e as silhuetas externas não foram redesenhadas;
somente particionadas para articulação. A SMG não ganhou arte própria nem
aprovação de modelo por receber um perfil de animação.

### Weapon/Sniper e Weapon/Shotgun — v1

```text
unidades visuais: Weapon/Sniper; Weapon/Shotgun (aprovações independentes)
versão: v1 — equipamento metálico industrial, direção Used apresentada
pranchas: visual/proposals/weapon_sniper_proposal_v1.png;
          visual/proposals/weapon_shotgun_proposal_v1.png
variantes: Maintained / Used / Worn, vista próxima e gameplay ilustrativo
contrato/prompts: visual/proposals/new_firearms_proposal_v1.md
decisão do usuário: "aprovadas a parte visual e funcional"
escopo: pranchas v1 de sniper e escopeta apresentadas; direção Used
data: 2026-10-02
commit de implementação artística: feat: approved sniper and shotgun Used v1 models
capturas nativas: visual/validation/weapon_sniper_v1_used_gameplay.png;
                 visual/validation/weapon_sniper_v1_used_ground.png;
                 visual/validation/weapon_sniper_v1_used_potato.png;
                 visual/validation/weapon_shotgun_v1_used_gameplay.png;
                 visual/validation/weapon_shotgun_v1_used_ground.png;
                 visual/validation/weapon_shotgun_v1_used_potato.png
estado: IMPLEMENTADA EM VALIDAÇÃO; aceitação visual final pendente
```

### Weapon/SMG — v1

```text
unidade visual: Weapon/SMG
versão: v1 — SMG compacta de aço metálico, variante Used recomendada
contrato/prompt: visual/proposals/weapon_smg_proposal_v1.md
prancha: visual/proposals/weapon_smg_proposal_v1.png
SHA-256: 5c65ae82b20ec2f728440f96db3cb1f32f83e059c74671f8b75e0af4c409f1a0
variantes: Maintained / Used / Worn
vistas: perfil, close, primeira pessoa ilustrativa e arma no chão
pedido do usuário: visual próprio para a SMG aprovado como objetivo
decisão visual desta prancha: "visual da arma aprovada" (2026-10-08) — Used v1
data: 2026-10-08
commit de implementação artística: entrega SMG Used v1 (histórico Git)
capturas nativas: visual/validation/weapon_smg_v1_used_gameplay.png;
                 visual/validation/weapon_smg_v1_used_ground.png;
                 visual/validation/weapon_smg_v1_used_potato.png
estado: IMPLEMENTADA EM VALIDAÇÃO — aceitação no jogo pendente
```

Funcionalidade das três aprovada pelo usuário em 2026-10-02. Sniper e escopeta
v1 têm geometrias próprias reutilizadas na mão e no chão, com materiais Used
metálicos existentes, sem alterar seus mapas ou as armas anteriores. As seis
capturas vêm do renderer nativo em uma fixture offscreen, não de um playtest
manual nem da prancha ilustrativa. FOV 60/100, contato com chão e visibilidade
em MEDIUM/POTATO têm gates técnicos; aceitação visual final cabe ao usuário.
A SMG agora tem silhueta própria no viewmodel e no pickup, com carregador e
mecanismo articulados nas recargas. A geometria reutiliza a família de
materiais metálicos Used já existente; não foram geradas texturas novas. As
capturas são da fixture nativa OpenGL offscreen, não de uma partida manual.
A prancha continua sendo referência conceitual, não textura pronta. O usuário
ainda precisa validar o resultado jogando antes da aceitação visual final ou
de uma nova versão pública.

### Interface/menu — v4 (refinamento solicitado)

```text
unidade visual: menus (HUD Tático v3 preservado)
versão: v4 — Tático minimalista compacto + cena de demonstração
referência da direção: visual/proposals/interface_visual_v3.html (Tático minimalista)
contrato: visual/proposals/interface_visual_proposal_v4.md
decisão do usuário: "deixa o menu no modo Tatico minimanista tambem"
                   botões menores e tela do jogo rodando de fundo
data: 2026-10-01
commit de implementação: feat: compact tactical menus with live game backdrop
capturas: visual/validation/interface_v4_menu_1280x720.png;
          visual/validation/interface_v4_menu_640x360.png;
          visual/validation/interface_v4_bindings_320x200.png;
          visual/validation/interface_v4_display_1280x720.png
resultado final: título ACEITO NO JOGO via captura nativa aprovada
                 "eu amei essa tela" (2026-10-01)
                 submenus IMPLEMENTADOS EM VALIDAÇÃO
```

Substitui a direção Industrial refinado dos menus da v3. Reutiliza a direção
Tático já mostrada e os materiais/modelos existentes. Não aprova novas texturas,
armas, modelos nem mudanças na gameplay ou treinamento das IAs.

### Interface/menu e HUD — v3

Direção dos menus substituída pela v4 acima; HUD Tático desta versão preservado.

```text
unidade visual: menus e HUD de gameplay
versão da proposta: v3 (combinação de duas direções)
arquivo: visual/proposals/interface_visual_v3.html
contrato: visual/proposals/interface_visual_proposal_v3.md
variantes mostradas: Industrial refinado; Tático minimalista; Terminal da instalação
decisão do usuário: HUD Tático minimalista + menus Industrial refinado
                    "use o Tatico minimalista para a hud do jogo e o Industrial Refinado para o menu"
data: 2026-10-01
commit de implementação: feat: apply minimal tactical HUD and refined industrial menus
capturas: visual/validation/interface_v3_menu_1280x720.png;
          visual/validation/interface_v3_hud_1280x720.png;
          visual/validation/interface_v3_bindings_320x200.png;
          visual/validation/interface_v3_boss_320x200.png
resultado final: IMPLEMENTADA EM VALIDAÇÃO (aceitação em partida pendente)
```

Não aprova a variante Terminal da instalação, novas texturas de ambiente,
modelos, armas ou mudanças de gameplay. Substitui apenas a composição visual
do HUD Rail/Precision e acabamento anterior dos menus.

### HUD/map visual language — v1 (somente HUD de gameplay)

```text
unidade visual: HUD/map visual language — HUD de gameplay
versão da proposta: v1
arquivo/pranchas: visual/proposals/hud_visual_v1_a_rail.png;
                 visual/proposals/hud_visual_v1_b_plate.png;
                 visual/proposals/hud_visual_v1_c_stencil.png
variantes mostradas: A Rail; B Plate; C Stencil
decisão do usuário: APROVADA PARA IMPLEMENTAÇÃO somente A Rail ("a")
data: 2026-09-26
commit de implementação: feat: implement approved Rail gameplay HUD
capturas no jogo: visual/validation/hud_visual_v1_a_comparison.png;
                  visual/validation/hud_visual_v1_a_after_960x540_neutral.png;
                  visual/validation/hud_visual_v1_a_after_1280x720_dim.png
resultado final: IMPLEMENTADA EM VALIDAÇÃO (aceitação visual pendente)
```

Contrato e limites em
[`proposals/hud_visual_proposal_v1.md`](proposals/hud_visual_proposal_v1.md).

### HUD/map visual language — v2 (somente HUD de gameplay)

```text
unidade visual: HUD/map visual language — HUD de gameplay
versão da proposta: v2
arquivo/pranchas: visual/proposals/hud_visual_v2_a_precision.png;
                 visual/proposals/hud_visual_v2_b_telemetry.png;
                 visual/proposals/hud_visual_v2_c_instrument.png;
                 visual/proposals/hud_visual_v2_close_comparison.png
variantes mostradas: A Precision; B Telemetry; C Instrument
decisão do usuário: A Precision APROVADA PARA IMPLEMENTAÇÃO
                    ("eu gostei da hud a"; "pode implementar")
data: 2026-09-27
commit de implementação: a registrar após validação técnica
capturas no jogo: visual/validation/hud_visual_v2_a_implemented_1280x720_neutral.png;
                  visual/validation/hud_visual_v2_a_implemented_960x540_neutral.png;
                  visual/validation/hud_visual_v2_a_implemented_960x600_neutral.png;
                  visual/validation/hud_visual_v2_a_implemented_1280x720_dim.png
resultado final: IMPLEMENTADA EM VALIDAÇÃO (aceitação visual em partida pendente)
```

Contrato e comparação em
[`proposals/hud_visual_proposal_v2.md`](proposals/hud_visual_proposal_v2.md).

### Weapon/first-person presentation — v1

```text
unidade visual: Weapon/first-person presentation
versão da proposta: v1
arquivo/pranchas: visual/proposals/weapon_first_person_v1_maintained.png;
                 visual/proposals/weapon_first_person_v1_used.png;
                 visual/proposals/weapon_first_person_v1_worn.png
variantes mostradas: Maintained; Used; Worn
decisão do usuário: APROVADA PARA IMPLEMENTAÇÃO somente Used
                    ("aprovada para implementação da usada")
data: 2026-09-26
commit de implementação: feat: remodel used first-person weapons
capturas no jogo: visual/validation/weapon_first_person_v1_used_pistol_gameplay.png;
                  visual/validation/weapon_first_person_v1_used_pistol_close.png;
                  visual/validation/weapon_first_person_v1_used_carbine_gameplay.png;
                  visual/validation/weapon_first_person_v1_used_carbine_close.png;
                  visual/validation/weapon_first_person_v1_used_knife_gameplay.png;
                  visual/validation/weapon_first_person_v1_used_knife_close.png;
                  visual/validation/weapon_first_person_v1_used_carbine_potato.png;
                  visual/validation/weapon_first_person_v1_used_comparison.png;
                  visual/validation/weapon_first_person_v1_used_pose_correction_comparison.png
resultado final: IMPLEMENTADA EM VALIDAÇÃO (aceitação visual pendente)
```

Contrato e limites estão em
[`proposals/weapon_first_person_proposal_v1.md`](proposals/weapon_first_person_proposal_v1.md).

### Weapon/ground pickup — v1

```text
unidade visual: Weapon/ground pickup — Pistol e Carbine
versão da proposta: v1
arquivo/pranchas: visual/proposals/ground_weapon_pickup_v1_close.png;
                 visual/proposals/ground_weapon_pickup_v1_gameplay.png
variantes mostradas: três distribuições de desgaste Used
decisão do usuário: REVISÃO SOLICITADA; arma deve parecer jogada no chão,
                    como se tivesse caído, usando a aparência atual
data: 2026-09-27
commit de implementação: nenhum
resultado final: SUBSTITUÍDA PELA V2; bloco violeta continua na build
```

Contrato e limites em
[`proposals/ground_weapon_pickup_proposal_v1.md`](proposals/ground_weapon_pickup_proposal_v1.md).

### Weapon/ground pickup — v2

```text
unidade visual: Weapon/ground pickup — Pistol e Carbine
versão da proposta: v2
arquivo/pranchas: visual/proposals/ground_weapon_pickup_v2_close.png;
                 visual/proposals/ground_weapon_pickup_v2_gameplay.png
variantes mostradas: três poses de repouso para os modelos Used existentes
decisão do usuário: APROVADA PARA IMPLEMENTAÇÃO ("pode implementar")
data: 2026-09-27
commit de implementação: a registrar após validação técnica
capturas no jogo: visual/validation/ground_weapon_pickup_v2_close.png;
                  visual/validation/ground_weapon_pickup_v2_gameplay.png;
                  visual/validation/ground_weapon_pickup_v2_dim.png;
                  visual/validation/ground_weapon_pickup_v2_potato.png;
                  visual/validation/ground_weapon_pickup_v2_fall_mid.png
resultado final: IMPLEMENTADA EM VALIDAÇÃO (aceitação visual em partida pendente)
```

Contrato e limites em
[`proposals/ground_weapon_pickup_proposal_v2.md`](proposals/ground_weapon_pickup_proposal_v2.md).

### Industrial Corridor/Ceiling — v1 HD

```text
unidade visual: Industrial Corridor/Ceiling
versão da proposta: v1 HD
arquivo/prancha: visual/proposals/industrial_corridor_ceiling_proposal_v1_hd.png
seeds mostradas: 2 Maintained; 3 Used; 1 Worn
decisão do usuário: APROVADA PARA IMPLEMENTAÇÃO, com aparência metálica
                    obrigatória ("aprovado com a observação de textura que tem
                    que estar metalica")
data: 2026-08-30
commit de implementação: `feat: implement metallic HD Industrial ceiling`
capturas no jogo: visual/validation/industrial_corridor_ceiling_v1_hd_maintained_seed2_gameplay.png;
                  visual/validation/industrial_corridor_ceiling_v1_hd_maintained_seed2_close.png;
                  visual/validation/industrial_corridor_ceiling_v1_hd_used_seed3_gameplay.png;
                  visual/validation/industrial_corridor_ceiling_v1_hd_used_seed3_close.png;
                  visual/validation/industrial_corridor_ceiling_v1_hd_worn_seed1_gameplay.png;
                  visual/validation/industrial_corridor_ceiling_v1_hd_worn_seed1_close.png;
                  visual/validation/industrial_corridor_ceiling_v1_hd_comparison.png
resultado final: ACEITA NO JOGO ("pode fazer o commit que eu faço o push")
```

A implementação usa material `1024×1024`, Albedo/Normal/RMA, UV por dimensão
física e três níveis de desgaste. As chapas, vigas, bordas e fixadores têm
metalness explícito e testado. O albedo é deliberadamente neutro e não contém
lâmpada, brilho ou emissão; `Lighting/CeilingFixture` continua sendo geometria
independente. Contrato e prompt estão em
[`proposals/industrial_corridor_ceiling_proposal_v1_hd.md`](proposals/industrial_corridor_ceiling_proposal_v1_hd.md).

### Lighting/CeilingFixture — v1 HD

```text
unidade visual: Lighting/CeilingFixture
versão da proposta: v1 HD
arquivo/prancha: visual/proposals/ceiling_fixture_proposal_v1_hd.png
seeds mostradas: não aplicável; uma base com estados ON, OFF e ALERT
decisão do usuário: APROVADA PARA IMPLEMENTAÇÃO ("pode prosseguir")
data: 2026-08-30
commit de implementação: `feat: separate HD ceiling fixtures from room materials`
capturas no jogo: visual/validation/ceiling_fixture_v1_hd_on.png;
                  visual/validation/ceiling_fixture_v1_hd_off.png;
                  visual/validation/ceiling_fixture_v1_hd_alert.png
resultado final: ACEITA NO JOGO ("aprovado")
```

Os oito albedos de teto permanecem neutros. A implementação v1 HD substitui o
provisório por carcaça, aro, junta, lente e fixadores físicos, com atlas
`1024×1024` de Albedo, Normal, RMA e Emission; somente a lente recebe emissão.
Contrato e prompt estão em
[`proposals/ceiling_fixture_proposal_v1_hd.md`](proposals/ceiling_fixture_proposal_v1_hd.md).

### Industrial Corridor/Wall — v2 HD

```text
unidade visual: Industrial Corridor/Wall
versão da proposta: v2 HD
arquivo/prancha: visual/proposals/industrial_corridor_wall_proposal_v2_hd.png
seeds mostradas: 2 Maintained; 3 Used; 1 Worn
decisão do usuário: APROVADA PARA IMPLEMENTAÇÃO CONDICIONAL
                    ("se a textura for parecida com a que vc me mostrou pode prosseguir")
data: 2026-08-30
commit de implementação: `feat: implement Industrial Corridor wall v2 HD`
capturas no jogo: visual/validation/industrial_corridor_wall_v2_hd_maintained_seed2_gameplay.png;
                  visual/validation/industrial_corridor_wall_v2_hd_maintained_seed2_close.png;
                  visual/validation/industrial_corridor_wall_v2_hd_used_seed3_gameplay.png;
                  visual/validation/industrial_corridor_wall_v2_hd_used_seed3_close.png;
                  visual/validation/industrial_corridor_wall_v2_hd_worn_seed1_gameplay.png;
                  visual/validation/industrial_corridor_wall_v2_hd_worn_seed1_close.png
resultado final: ACEITA NO JOGO
                 ("As texturas estão muito bonitas")
```

A v2 propõe material `1024×1024`, densidade UV física, filtragem trilinear e
mapas de Albedo, Normal e Roughness/Metalness/AO. O contrato completo está em
[`proposals/industrial_corridor_wall_proposal_v2_hd.md`](proposals/industrial_corridor_wall_proposal_v2_hd.md).

## Histórico de revisões

### Industrial Corridor/Wall — v1

```text
unidade visual: Industrial Corridor/Wall
versão da proposta: v1
arquivo/prancha: visual/proposals/industrial_corridor_wall_proposal_v1.png
seeds mostradas: 2 Maintained; 3 Used; 1 Worn
decisão do usuário: APROVADA PARA IMPLEMENTAÇÃO (resposta: "aprovado")
data: 2026-08-30
commit de implementação: —
capturas no jogo: visual/validation/industrial_corridor_wall_v1_maintained_seed2.png;
                  visual/validation/industrial_corridor_wall_v1_used_seed3.png;
                  visual/validation/industrial_corridor_wall_v1_worn_seed1.png
resultado final: REVISÃO SOLICITADA; capturas consideradas simples e de baixa
                 qualidade, com pedido de solução mais complexa e ao menos HD
```

Objetivo, direção, paleta, regras e prompt reproduzível estão no arquivo
[`proposals/industrial_corridor_wall_proposal_v1.md`](proposals/industrial_corridor_wall_proposal_v1.md).
A eventual aprovação desta proposta não se estende a Floor, Ceiling, Door ou
outro ambiente.

## Worker — variação de braços e pernas v1

```text
unidade visual: Worker / silhueta dos membros v1
versão da proposta: v1
arquivo/prancha: visual/proposals/worker_limb_variation_v1.svg
contrato: visual/proposals/worker_limb_variation_v1.md
seeds mostradas: 101, 202 e 303 (ilustrativas; não são capturas do gerador)
decisão do usuário: APROVADA PARA IMPLEMENTAÇÃO (resposta: "aprovado")
data: 2026-10-15
implementação técnica: 2026-10-16
commit de implementação: feat(visual): vary Worker limb proportions by seed
capturas do renderer: visual/validation/medium_worker_seed{101,202,303}_{near,far}_idle.png;
                      visual/validation/medium_worker_seed{101,202,303}_near_{walk,attack}.png;
                      visual/validation/potato_worker_seed{101,202,303}_{near,far}_idle.png
capturas em partida: pendentes
resultado final: IMPLEMENTADA EM VALIDAÇÃO; aceitação no jogo pendente
```

A aprovação cobre proporções e variação dos membros do Worker. Outros papéis,
materiais e texturas exigem propostas próprias.

## Worker — modelo e materiais v2

```text
unidade visual: Worker/modelo e família de materiais v2
versão da proposta: v2
contrato/pranchas: visual/proposals/worker_model_material_v2.md;
                  visual/proposals/worker_model_material_v2_*.png
variantes conceituais: segurança industrial (laranja), manutenção de campo
                      (azul petróleo), unidade recuperada (marfim/vermelho)
feedback do usuário: modelo conceitual considerado bonito; cores livres
decisão para implementação: APROVADA PARA IMPLEMENTAÇÃO — variante A laranja
                             ("gostei desse laranja no final das contas,
                              pode implementar", 16/10/2026)
assets carregados: assets/visual/worker_painted_steel_v2.png;
                  assets/visual/worker_front_panel_source_v2.png
capturas técnicas: visual/validation/worker_model_material_v2_review/
                   visual/validation/worker_eye_glow_clear/
                   visual/validation/worker_eye_glow_dark/
capturas em partida: test_arena_native.png e test_arena_worker_native.png
                     (Worker distante/oculto; insuficientes para aceitação)
feedback em partida: usuário testou o Worker e disse "até o momento esse ai
                    esta muito bom"; centro emissivo/halo ainda sem aprovação
                    visual específica em partida
estado: IMPLEMENTADA EM VALIDAÇÃO — Worker bem recebido; olho pendente em jogo
```

As pranchas v2 são edições geradas a partir de uma captura offscreen e **não**
representam o renderer atual. A geração modificou a geometria apesar da
referência; a aprovação de 16/10 cobre esse modelo e a paleta A, mantendo a
variação de membros v1. As paletas B/C e outros papéis não foram aprovados.
Após a primeira captura técnica, o usuário apontou divergência com a prancha
e exigiu textura, sombras entre peças, parafusos e desgaste nas bordas. A
implementação foi refeita com chapa frontal amostrada da própria prancha,
textura de aço laranja, juntas grafite, fixadores escuros e desgaste nas
arestas. Capturas offscreen e testes não constituem aceitação em partida.
O centro âmbar do visor recebeu emissão própria e um halo discreto, verificados
em capturas técnicas com luz normal e baixa na Quadro K2100M. O feedback positivo
do usuário sobre o Worker não é uma validação específica desse novo brilho.

## Worker — serra circular visual v3

```text
unidade visual: Worker/ferramenta-serra v3
referência: visual/Proposta_visual_separada/Worker.md e Worker.png
decisão: usuário gostou da ideia de trocar a ferramenta pela serra
implementação: disco dentado de aço escuro, carcaça laranja, giro no eixo X
capturas técnicas: visual/validation/worker_saw_v2/ (frente e lateral,
                   MEDIUM e POTATO, seeds 101, 202 e 303)
estado: PROTÓTIPO TÉCNICO — não corresponde à prancha aprovada
```

A prancha propõe também golpes e efeitos de faísca; esta etapa altera apenas a
ferramenta visual e seu giro. Alcance, dano, colisão e comportamento permanecem
iguais. A comparação das capturas com a prancha mostrou diferenças grandes na
silhueta, na integração com o braço e nos detalhes mecânicos. O usuário
esclareceu que deseja seguir exatamente as propostas visuais. Esta serra
procedural não é uma entrega visual aprovada nem deve ser integrada a um
download como representação final da proposta.

## Referências visuais individuais dos cinco inimigos

```text
unidades: Worker, Guard, Officer, Engineer e IndustrialRobot
artefatos: visual/Proposta_visual_separada/{Worker,Guard,Officer,
           Engineer,IndustrialRobot}.{md,png}
decisão: usuário quer seguir exatamente as propostas visuais
estado: ACEITAS NO JOGO em 19/10/2026 para as cinco reconstruções testadas
```

Cada PNG mostra frente, lados e costas. São referências de forma, peças,
equipamentos, cor, desgaste e luz. Não há arquivos 3D originais nem texturas
separadas; as superfícies entre vistas exigem reconstrução. Existem capturas
técnicas das quatro vistas, e o usuário testou a prévia em partida antes de
aprovar as reconstruções atuais. As sugestões de habilidades e IA nos arquivos
Markdown não estão aprovadas como mudanças de gameplay por esta decisão visual.

Em 17/10/2026, foram preparadas reconstruções editáveis em
`assets/visual/models/` e capturas técnicas em
`visual/validation/enemy_reference_game/`. O preview original exigia
`GDP_ENEMY_REFERENCE_MODELS=1`. Na integração da candidata 0.1.7, as
reconstruções e as hitboxes correspondentes passaram a ser padrão;
`GDP_ENEMY_REFERENCE_MODELS=0` recupera o conjunto anterior para comparação.
As geometrias e texturas ainda divergem das
pranchas em densidade de peças, desgaste localizado e detalhes das armas. O
usuário aprovou em partida as reconstruções atuais, sem declarar equivalência
exata com as pranchas. A revisão das hitboxes está em
`visual/validation/enemy_hitbox_alignment.md`.

Em 18/10/2026, a pedido do usuário, o preview recebeu zonas de dano alinhadas
às cinco reconstruções: o sensor alto do Boss agora integra a cabeça fraca.
Guard, Officer, Engineer e Boss disparam a partir das armas; o Worker usa a
mão livre no ataque próximo, e o Boss usa a garra na fase de perseguição quando
alcança o jogador. As poses de mira, recuo e golpe têm capturas técnicas em
`visual/validation/enemy_attack_game/`. A mudança é específica deste preview;
em 19/10/2026, após testar a prévia com Worker280, o usuário aprovou os
visuais dos cinco inimigos e a gameplay. Essa decisão cobre a aparência e as
colisões do preview, separadamente dos testes de distribuição ainda necessários.

## Inventário inicial de ambientes

Todas as superfícies abaixo começam como `NÃO REVISADA`.

### Storage/Wall — v1 Dusty e variações de desgaste

```text
unidade visual: Storage/Wall na sala grande StorageHall_01 e sua prévia
versão da proposta: v1 Dusty com cinco variações derivadas
referência: 06_environments/storage.png
amostra aprovada: visual/proposals/storage_material_v1/wall_dusty.png
asset do jogo: assets/visual/rooms/storage/wall_dusty_v1.png
decisão do usuário: "gostei das texturas, podemos usar elas nas paredes";
                   "assim temos 6 variações para as paredes do Storage, por mim esta tudo bem,
                    contanto que por parede apenas uma das chapas seja numerada"
data: 2026-10-28
escopo: paredes da Storage grande; Floor e Ceiling não incluídos
capturas no jogo: visual/validation/storage_wall_v1/preview_1920x1080_ultra.png (mapeamento anterior);
                 visual/validation/storage_wall_v1/preview_uv_corrected_1920x1080_ultra.png (mapeamento quadrado)
variações: Orderly, Dusty, Neglected; cada uma com e sem marcação
regra: no máximo uma chapa numerada por parede; faixa de segurança na base
capturas atuais: visual/validation/storage_wall_v1/preview_six_variants_orientation_1849x1011.png;
                 visual/validation/storage_wall_v1/preview_side1_1920x1080.png
resultado final: ACEITA NO PLAYTEST da sala Storage em 2026-10-28
```

A amostra foi gerada com `imagegen` a partir da prancha Storage. A autorização
direta do usuário se refere à amostra de parede mostrada nesta conversa; não
aprova mudanças futuras de paleta ou a aplicação das amostras ao piso e teto.

### Storage/Floor e Storage/Ceiling — propostas v1

```text
unidades visuais: Storage/Floor e Storage/Ceiling
proposta: visual/proposals/storage_material_v1/review_floor_ceiling.html
amostras: floor_{orderly,dusty,neglected}.png;
          ceiling_{orderly,dusty,neglected}.png
pedido do usuário: "sobre tetos e pisos, vamos criar variações para eles também"
data: 2026-10-28
decisão do usuário: "aprovados" (piso e teto)
estado: IMPLEMENTADAS NO HALL; playtest aprovado em 2026-10-28
assets do jogo: assets/visual/rooms/storage/floor_*_v1.png;
               assets/visual/rooms/storage/ceiling_*_v1.png
```

As seis amostras são ortogonais, com luz neutra; nenhuma luminária está pintada
nos tetos. O usuário aprovou o playtest da sala em 2026-10-28.

### Storage/Boxes — proposta v1

```text
unidade visual: caixas quebráveis sobre as estantes da Storage
amostras: visual/proposals/storage_box_v1/box_olive_v1.png;
          visual/proposals/storage_box_v1/box_charcoal_v1.png
decisão do usuário: "textura das caixas aprovadas" em 2026-10-28
decisão final do usuário: "as caixas foram aprovadas" em 2026-11-02,
                          após solicitar conferência da aplicação no jogo
estado: ACEITA NO JOGO para a Storage Hall
assets do jogo: assets/visual/rooms/storage/box_olive_v1.png;
               assets/visual/rooms/storage/box_charcoal_v1.png
```

As caixas usam colisão e dano dinâmicos. O cubo padrão mapeia a amostra uma
vez por face. A aprovação final cobre a aplicação na Storage Hall.

### Storage/Rack — proposta v1

```text
unidade visual: armação fixa das estantes da Storage
proposta: visual/proposals/storage_rack_v1/review.html
amostras: rack_{orderly,dusty,neglected}.png
pedido do usuário: estantes ainda sem textura, observado em 29/10/2026
decisão do usuário: "pode implementar" em 29/10/2026
estado: REVISÃO SOLICITADA em 29/10/2026; v1 permanece no jogo
escopo: postes e prateleiras; caixas quebráveis não incluídas
assets do jogo: assets/visual/rooms/storage/rack_{orderly,dusty,neglected}_v1.png
```

A estrutura usava `RoomMaterialChannel::Generic` e o checker de fallback. A
montagem na proposta é conceitual e não substitui captura nativa após a
implementação. O desgaste da estante acompanha a condição da sala.

### Storage/Rack — proposta v2

```text
unidade visual: armação fixa das estantes da Storage
proposta: visual/proposals/storage_rack_v2/review.html
amostras: rack_{orderly,dusty,neglected}.png
pedido: material parece liso e apenas pintado; prateleiras atravessam montantes
estado: SUBSTITUÍDA PELA PROPOSTA V3 antes da integração
escopo: abrasão legível e UV 1,0; sem alterar caixas
```

A interseção geométrica das prateleiras foi corrigida independentemente da
aprovação do material. As amostras v2 são proposta local e ainda não são
carregadas pelo jogo.

### Storage/Rack — proposta v3

```text
unidade visual: estrutura fixa das estantes da Storage
proposta: visual/proposals/storage_rack_v3/review.html
prancha: visual/proposals/storage_rack_v3/rack_family_v3.png
vista de jogo conceitual: visual/proposals/storage_rack_v3/gameplay_distance_concept.png
variantes: conservada, usada, negligenciada
pedido: textura com identidade mais marcante e sem esticar na aplicação
resposta: usuário gostou da direção visual, exigiu UV sem esticar e disse
          "pode implementar a v3" em 29/10/2026; depois pediu um pouco mais
          de desgaste nas colunas laranja e sugeriu testar tom mais verde;
          após testar a build de playtest, disse "aprovado, podemos prosseguir
          para a proxima sala" em 29/10/2026
estado: ACEITA NO JOGO para a estante v3 com tom mais verde
escopo: colunas, tampos, bordas e junções; caixas excluídas
assets do jogo: assets/visual/rooms/storage/rack_v3_{upright,shelf}_{orderly,dusty,neglected}.png
```

O mapeamento antigo `vUv * 3.0` não mantinha a densidade da textura em faces
de dimensões diferentes. A v3 projeta o desgaste em metros reais e espelha o
albedo nas emendas; furos visuais e nervuras usam passo físico de 18 cm no
shader. As quinas das colunas recebem lascas adicionais em escala física. Os
furos são recessos sombreados, sem vazamento físico da malha de colisão.
As capturas técnicas da revisão estão em
`visual/validation/storage_rack_v3/`. O usuário testou a build de playtest e
aprovou a estante com o tom mais verde antes de seguir à próxima sala.

### Cafeteria/Wall, Floor, Ceiling e iluminação — proposta v1

```text
unidades visuais: Cafeteria/Wall; Cafeteria/Floor; Cafeteria/Ceiling;
                 direção de iluminação Cafeteria
proposta: visual/proposals/cafeteria_material_v1/review.html
amostras: wall_{clean,inuse,faded}.png;
          floor_{clean,inuse,faded}.png;
          ceiling_{clean,inuse,faded}.png
vista de sala: gameplay_concept_soft_warm.png (conceito, não captura nativa)
pedido: texturas da Cafeteria após aprovação do layout;
        iluminação um pouco mais confortável
estado: APROVADAS pelo usuário em 28/10/2026; integradas e aceitas na captura
        nativa NVIDIA/ULTRA após o ajuste final de brilho
assets do jogo: assets/visual/rooms/cafeteria/{wall,floor,ceiling}_{clean,inuse,faded}.png
capturas nativas: visual/validation/cafeteria_material_v1/before_nvidia_ultra_seed20261029.png;
                 visual/validation/cafeteria_material_v1/adjusted_nvidia_ultra_seed20261029.png;
                 visual/validation/cafeteria_material_v1/darker_nvidia_ultra_seed20261029.png (aprovada)
```

As amostras são albedos sob luz neutra, e o teto não contém luminárias
pintadas. O usuário aprovou as três superfícies e a iluminação em 28/10/2026;
a captura nativa em ULTRA confirmou a correção da saturação e o usuário aprovou
a versão final mais escura. MEDIUM e POTATO permanecem sem captura própria.

### Cafeteria/ServiceCounter — detalhe v2 em prévia

```text
pedido: balcões menos simples, com mais decoração e detalhes
implementação: portas, puxadores, rodapé, faixa, bandeja e nicho de serviço
capturas: visual/validation/cafeteria_counter_v3/README.md
estado: IMPLEMENTADA NA PRÉVIA; aguardando aceite visual em partida
```

As peças ficam dentro da linha de serviço existente e deixam a porta lateral
livre. A geometria nova é sólida e compartilhada com a colisão. A densidade
das texturas dos móveis continua baseada em metros, sem alongar o albedo.

### Laboratory/Wall, Floor e Ceiling — proposta v1

```text
proposta: visual/proposals/laboratory_material_v1/review.html
amostras: wall_operational_v2.png; floor_operational.png;
          ceiling_operational.png
estado: IMPLEMENTADA NA PRÉVIA ISOLADA; aceite em partida pendente
decisão: aprovada para implementação pelo usuário em 01/11/2026
assets: assets/visual/rooms/laboratory/{wall,floor,ceiling}_operational_v1.png
capturas técnicas: visual/validation/laboratory_material_v1/
```

Amostras criadas a partir da prancha `06_environments/laboratorio.png`, com
repetição planejada de 2 × 2 m por imagem. A versão inicial da parede mantinha
uma faixa azul na borda e foi substituída por uma revisão com faixa central;
essa revisão usa espelhamento de tiles a cada 2 m para evitar emendas abruptas.
Os móveis da prévia usam `furniture_enamel_v1.png` em escala de 0,75 m, com
portas, molduras e fixadores 3D. A campanha mantém o perfil procedural.

### Laboratory/Wall — revisão v2

```text
proposta e prompt: visual/proposals/laboratory_material_v2/README.md
asset da prévia: assets/visual/rooms/laboratory/wall_operational_v2.png
estado: IMPLEMENTADA NA PRÉVIA ISOLADA; aceite em partida pendente
```

O feedback de 03/11/2026 apontou excesso de informação visual na parede. A
faixa azul foi removida do bitmap e representada por colunas físicas com
colisão, acabamento e indicador luminoso. O piso, teto e mobiliário v1
permanecem na prévia.

| Ambiente | Wall | Floor | Ceiling |
|---|---|---|---|
| Industrial Corridor | V2 HD aceita no jogo | Não revisada | V1 HD aceita no jogo |
| Storage | V1 com seis variações aprovada no playtest | Três variantes aprovadas no playtest | Três variantes aprovadas no playtest |
| Combat Room | Não revisada | Não revisada | Não revisada |
| Cafeteria | V1 aprovada na captura NVIDIA/ULTRA | V1 aprovada na captura NVIDIA/ULTRA | V1 aprovada na captura NVIDIA/ULTRA |
| Laboratory | V2 em validação na prévia | V1 em validação na prévia | V1 em validação na prévia |
| Technical Area | Não revisada | Não revisada | Não revisada |
| Security | Não revisada | Não revisada | Não revisada |
| Central Hub | Wall v1 implementada, em validação visual | Chapa v1 em prévia, aceite em partida pendente | Proposta v1 aguardando implementação |

## Outros pacotes visuais

| Unidade | Estado inicial |
|---|---|
| Door/Frame | Não revisada |
| Door/Panel | Não revisada |
| Worker/silhueta dos membros | Reconstrução atual aceita em partida em 19/10/2026; variação v1 é histórico separado |
| Worker/modelo e material v2 | Histórico da direção laranja; a reconstrução individual atual foi aceita em jogo em 19/10/2026 |
| Cinco referências individuais de inimigos | Reconstruções atuais dos cinco papéis aceitas em partida em 19/10/2026 e ativadas por padrão na candidata 0.1.7 |
| Worker/material families adicionais | Não revisada |
| Guard/material families | Não revisada |
| Officer/material families | Não revisada |
| Engineer/material families | Não revisada |
| IndustrialRobot/material families | Não revisada |
| Weapon/first-person presentation | V1 Used implementada em validação |
| Player presentation | V1 corpo/cintura em revisão; V2 torso/cinto aguardando aprovação; mãos v2 publicadas rejeitadas, revisão posterior de mãos/contatos aprovada provisoriamente |
| Weapon/Sniper optic | V2 Usado implementada em validação |
| HUD/Sniper scoped view | V1 implementada em validação |
| HUD/map visual language | V2 A Precision implementada em validação; mapa não revisado |
| Weapon/ground pickup | V2 modelos Used caídos implementada em validação; V1 substituída |
| Grenade/first-person and thrown | Aparência e efeito da 0.1.6 parcialmente aprovados em 14/10/2026; revisão geral de texturas planejada para fase posterior, sem aprovação automática de nova versão |
| Lighting/CeilingFixture | V1 HD aceita no jogo; família v2 implementada em validação |
| Architecture/StructuralColumn | V1 repetível de 0,8 m, três estados de desgaste, implementada em validação; aprovação visual em partida pendente |

## Relação com os testes

A aprovação humana decide direção artística, legibilidade e coerência. Os gates
automatizados continuam responsáveis por determinismo, canais válidos, ausência
de NaN, cobertura, hashes, cache, desempenho e estabilidade do framebuffer.
Um teste aprovado não substitui a aprovação visual; uma aprovação visual não
permite ignorar uma regressão técnica.

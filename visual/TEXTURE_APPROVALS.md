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
decisão do usuário:
data:
commit de implementação:
capturas no jogo:
resultado final:
```

## Propostas ativas

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

## Inventário inicial de ambientes

Todas as superfícies abaixo começam como `NÃO REVISADA`.

| Ambiente | Wall | Floor | Ceiling |
|---|---|---|---|
| Industrial Corridor | V2 HD aceita no jogo | Não revisada | V1 HD aceita no jogo |
| Storage | Não revisada | Não revisada | Não revisada |
| Combat Room | Não revisada | Não revisada | Não revisada |
| Cafeteria | Não revisada | Não revisada | Não revisada |
| Laboratory | Não revisada | Não revisada | Não revisada |
| Technical Area | Não revisada | Não revisada | Não revisada |
| Security | Não revisada | Não revisada | Não revisada |
| Central Hub | Não revisada | Não revisada | Não revisada |

## Outros pacotes visuais

| Unidade | Estado inicial |
|---|---|
| Door/Frame | Não revisada |
| Door/Panel | Não revisada |
| Worker/material families | Não revisada |
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
| Lighting/CeilingFixture | V1 HD aceita no jogo |

## Relação com os testes

A aprovação humana decide direção artística, legibilidade e coerência. Os gates
automatizados continuam responsáveis por determinismo, canais válidos, ausência
de NaN, cobertura, hashes, cache, desempenho e estabilidade do framebuffer.
Um teste aprovado não substitui a aprovação visual; uma aprovação visual não
permite ignorar uma regressão técnica.

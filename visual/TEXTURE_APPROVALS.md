# Registro de aprovação visual de texturas

Este arquivo é o gate humano obrigatório para a revisão visual do GDP. Os
materiais e texturas existentes são um **baseline técnico funcional**: podem
continuar sendo usados durante o desenvolvimento, mas não representam arte
final aprovada.

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
decisão do usuário: preferência por A Precision ("eu gostei da hud a");
                    APROVAÇÃO PARA IMPLEMENTAÇÃO PENDENTE
data: 2026-09-27
commit de implementação: nenhum
resultado final: PROPOSTA; v1 A Rail continua na build
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
decisão do usuário: AGUARDANDO APROVAÇÃO
data: 2026-09-27
commit de implementação: nenhum
resultado final: PROPOSTA; bloco violeta continua na build
```

Contrato e limites em
[`proposals/ground_weapon_pickup_proposal_v1.md`](proposals/ground_weapon_pickup_proposal_v1.md).

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
| Player presentation | Não revisada |
| HUD/map visual language | V1 A Rail implementada em validação; V2 A é a preferência, implementação pendente de aprovação; mapa não revisado |
| Weapon/ground pickup | V1 aguardando aprovação; bloco violeta permanece na build |
| Lighting/CeilingFixture | V1 HD aceita no jogo |

## Relação com os testes

A aprovação humana decide direção artística, legibilidade e coerência. Os gates
automatizados continuam responsáveis por determinismo, canais válidos, ausência
de NaN, cobertura, hashes, cache, desempenho e estabilidade do framebuffer.
Um teste aprovado não substitui a aprovação visual; uma aprovação visual não
permite ignorar uma regressão técnica.

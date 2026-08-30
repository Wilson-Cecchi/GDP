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
| Industrial Corridor | V2 HD aceita no jogo | Não revisada | Não revisada |
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
| Weapon/first-person presentation | Não revisada |
| Player presentation | Não revisada |
| HUD/map visual language | Não revisada |
| Lighting/CeilingFixture | Não revisada; asset e material independentes do teto |

## Relação com os testes

A aprovação humana decide direção artística, legibilidade e coerência. Os gates
automatizados continuam responsáveis por determinismo, canais válidos, ausência
de NaN, cobertura, hashes, cache, desempenho e estabilidade do framebuffer.
Um teste aprovado não substitui a aprovação visual; uma aprovação visual não
permite ignorar uma regressão técnica.

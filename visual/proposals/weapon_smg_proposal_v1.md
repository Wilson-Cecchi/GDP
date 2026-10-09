# Weapon/SMG — proposta visual v1

Estado: **AGUARDANDO APROVAÇÃO**. [Prancha ilustrativa](weapon_smg_proposal_v1.png)
gerada pela ferramenta integrada em 2026-10-08, 1536 × 1024 RGB, SHA-256
`5c65ae82b20ec2f728440f96db3cb1f32f83e059c74671f8b75e0af4c409f1a0`.
Não é captura do jogo nem textura pronta.

## Direção proposta

SMG fictícia mais curta que a carabina atual, com coronha rebatível, cano e
guarda-mão curtos, miras baixas, empunhadura separada e carregador curvo de
24 munições. Aço revestido grafite com resposta metálica e desgaste contido nas
arestas; polímero fosco na empunhadura/coronha. **Used** é a variante sugerida.
As três condições representam acabamento da mesma geometria, não armas
diferentes. A silhueta deve se distinguir da carabina tanto na mão quanto no
chão. O enquadramento deve preservar o centro da tela livre e a escala do
viewmodel aprovado. Nenhuma mudança de dano, cadência, munição, FOV ou loot.

Após aprovação específica desta prancha: criar malha própria de baixo/médio
custo, reutilizar a família de materiais metálicos existente, ajustar contatos
das mãos/recarga e o modelo caído. Validar capturas nativas nos perfis MEDIUM e
POTATO, FOV 60°/100°, contato com o chão e comparação direta com a carabina.
A aceitação final ainda dependerá do teste visual dentro do jogo.

## Prompt usado

```text
Use case: stylized-concept. Asset type: visual approval board for a new Iron Seed submachine gun model (concept only, not a game screenshot or usable texture). Landscape 3:2 board with exactly one fictional compact 24-round submachine gun shown consistently in three finish variants along the top: Maintained, Used, Worn. A larger three-quarter close view of the Used variant below, then a small first-person gameplay illustration and a separate floor-pickup view below. Keep the SAME mechanical silhouette in every view. Industrial tactical FPS, serious realistic design: clearly shorter than a carbine, compact slightly curved box magazine, short barrel and simple muzzle, restrained folding stock, separate ergonomic pistol grip and short fore-end, low iron sights. Gunmetal coated steel with genuinely metallic response, brushed bare-steel wear only on contact edges, dark charcoal polymer, moderate Used wear. Physically coherent assembly and believable human scale. Neutral studio light for close views; dark cold industrial room for gameplay inset with neutral ceiling and separate lamp model, clear center crosshair area, weapon lower right and not oversized. Designed as feasible optimized modular low/mid-poly OpenGL game geometry with existing Iron Seed material family. No real-world weapon branding or exact real-model likeness, no glowing sci-fi parts, no giant colored slabs, no hands obscuring geometry, no extra weapons, no blood. Clear simple labels 'MAINTAINED', 'USED', 'WORN', 'FIRST PERSON', 'ON FLOOR'. Footer text exactly 'CONCEITO SMG v1 - AGUARDANDO APROVAÇÃO'.
```

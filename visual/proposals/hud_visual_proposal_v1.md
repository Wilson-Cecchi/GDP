# HUD — proposta visual v1

```text
estado: IMPLEMENTADA EM VALIDAÇÃO (variante A — Rail)
unidade visual: HUD/map visual language (somente HUD de gameplay)
versão: v1
data: 2026-09-26
pranchas: hud_visual_v1_a_rail.png
          hud_visual_v1_b_plate.png
          hud_visual_v1_c_stencil.png
geração: image_gen, conceitos visuais; não são capturas da build nem assets prontos
SHA-256 A: ef199b5b2795b036fe21676635ce95d812319104e7143c966b905a29b749d087
SHA-256 B: 5f796667f3b745d31abcccc8d8b7f91ada18041e7d740213c8894f509065fc3c
SHA-256 C: 4e1312d5e65d8aac80266bb0916dd9dd15bc784b2aa239c38c1c0411cfdba6f6
seeds: não se aplica; o HUD não é uma textura procedural
decisão do usuário: "a" em resposta à escolha A/B/C
```

## Objetivo e escopo

Revisar a aparência do HUD sem perder legibilidade nem espaço de mira. As
pranchas mostram, em cada variante, uma leitura durante gameplay à esquerda e
ampliações dos módulos à direita, sob iluminação neutra. O corredor e a arma
são contexto ilustrativo gerado; **não** representam uma remodelagem do mundo,
da arma ou a aparência real da build. Os textos e números são amostras, não
novos objetivos, temporizadores ou regras.

A unidade inclui a apresentação de HP, Armor, arma/munição, chaves, nível, XP,
objetivo, evento, avisos contextuais e feedback de combate. Não inclui menus,
mapa funcional, sprites de arma, retícula lógica, indicador F3 ou mudanças de
gameplay. Uma aprovação da proposta não aprova esses elementos adjacentes.

## Variantes

| Variante | Direção | Troca principal |
|---|---|---|
| [A — Rail](hud_visual_v1_a_rail.png) | Trilhos finos de instrumento e molduras leves | Menor peso visual; acabamento metálico mais sutil |
| [B — Plate](hud_visual_v1_b_plate.png) | Placas modulares de aço escuro, chanfros e fixadores discretos | Material mais presente; risco maior de ocupar a periferia |
| [C — Stencil](hud_visual_v1_c_stencil.png) | Tipografia técnica e linhas interrompidas sobre fundo mínimo | Máxima economia visual; menos sensação de objeto físico |

Minha preferência inicial era **A**, porque preserva a linguagem industrial com
menos massa na tela. B é útil se o HUD deve parecer um equipamento físico; C
é a opção mais limpa, mas se afasta do acabamento metálico já estabelecido.

O usuário escolheu **A — Rail**. A aprovação cobre apenas a apresentação do HUD
descrita aqui; B, C e as áreas excluídas continuam sem aprovação.

## Implementação e comparação na build

O renderer usa formas e texto próprios, não os pixels da prancha. Quatro módulos
compactos substituíram a caixa única de HP/Armor/arma/chaves; as barras e os
textos agora têm faixas distintas. Nível/XP, objetivo/tempo, evento, prompts e
avisos usam trilhos finos com cor semântica. O compositor de menus e as regras
de combate não foram alterados. A diferença de detalhe entre a ilustração
conceitual e o jogo é esperada: a direção aprovada é a hierarquia leve Rail,
não um corredor, rifle ou material fotográfico novo.

Na [comparação 1280×720](../validation/hud_visual_v1_a_comparison.png), a
captura anterior está à esquerda e a implementação à direita, na mesma cena
neutralmente iluminada. O [teste 960×540](../validation/hud_visual_v1_a_after_960x540_neutral.png)
verifica a menor resolução oferecida nas configurações; a
[captura escura](../validation/hud_visual_v1_a_after_1280x720_dim.png) verifica
contraste. São renders reais do OpenGL com cena fixa, não um playtest manual.
Na comparação 1280×720, o recorte central de 600×400 pixels a partir de
`(340,150)` teve diferença exata de zero pixels antes/depois: a cena e a área
de mira permaneceram intactas nesse cenário.
Aceitação visual final: pendente.

## Contrato para implementação, somente após aprovação

- Preservar exatamente as informações e os estados já existentes. O centro do
  viewport e a retícula continuam livres; alertas são temporários.
- Usar texto e formas nativas do compositor atual, não colar as pranchas como
  overlays. Tipografia precisa continuar legível em 960×540 e 1280×720.
- Ciano indica interação, laranja aviso, vermelho perigo, verde sucesso. HP e
  Armor permanecem independentes; barra de XP mantém leitura imediata.
- Nenhuma alteração em hit tests, navegação por mouse/teclado, escala interna
  do mundo, performance dos perfis POTATO/LOW ou simulação.
- Após implementar: capturas reais em gameplay e em condição de pouca luz,
  inspeção nos dois tamanhos citados, testes de HUD/menus/renderer e pedido de
  aceitação visual final. Divergência relevante volta para revisão.

## Prompts de geração

As três pranchas foram geradas pelo modo integrado de `image_gen` como
`ui-mockup`, separadamente. A base comum pediu uma prancha 16:9 com gameplay de
corredor industrial neutro à esquerda, close-ups do HUD à direita, rifle baixo,
centro 4:3 livre, retícula pequena, HP/Armor/Ammo/Keys abaixo à esquerda,
LV/XP abaixo à direita, Objective/Event acima à direita, e a inscrição
`CONCEPT / NOT IN-GAME`. Pediu visual sério, sem cartoon, neon, menus, mecânicas
novas ou marcas. Para A: trilhos e molduras gunmetal muito finos, metal escovado
e ciano contido. Para B: placas modulares de aço, chanfros, fixadores e pouca
profundidade, sem glow. Para C: stencil militar, base carvão fosco, linhas
interrompidas e números monoespaçados, com mínima ornamentação.

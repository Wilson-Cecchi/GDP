# Sniper — luneta v2 e visão de mira v1

Data: 2026-10-02. Estado: **IMPLEMENTADA EM VALIDAÇÃO — Usado v2 / mira v1**.

Pedido: luneta mais visível no modelo e visão de mira com retículo ao usar
Mouse2. Resposta do usuário à escolha de escopo: **As duas coisas**.
Essa resposta definiu a funcionalidade solicitada. Após apresentação da prancha,
o usuário respondeu **"aprovado pode implementar"**, aprovando a direção Usado
da luneta v2 e a visão de mira v1, incluindo o círculo inteiro no jogo.

## Artefato e unidades

Prancha: [sniper_scope_proposal_v2.png](sniper_scope_proposal_v2.png).
PNG RGB, 1536×1024. SHA-256:
`23d8b4024cae6e07b3dcef96b3f440493b4d33fb77067e9557d08dde9c113ffd`.
Unidades relacionadas, com aprovação independente:

- `Weapon/Sniper optic` — v2, geometria e acabamento da luneta.
- `HUD/Sniper scoped view` — v1, abertura circular e retículo de mira.

A imagem é conceito ilustrativo gerado, não captura da build nem asset usado
no renderer. Os detalhes e a iluminação ilustrados não prometem fidelidade
idêntica na malha procedural do jogo.

Referências: `weapon_sniper_proposal_v1.png` (identidade aprovada da sniper)
e `../validation/weapon_sniper_v1_used_gameplay.png` (enquadramento nativo).
Modo: ferramenta integrada de geração de imagens, sem CLI/API externa.
Seeds: não aplicável à prancha conceitual; não é saída do gerador do jogo.

## Direção visual

Preservar coronha, receptor, carregador, cano, ferrolho e identidade do rifle
v1. Revisar somente a luneta: objetiva e ocular mais legíveis, anéis de fixação
claros, torres de ajuste discretas e vidro escuro não emissivo. Não transformar
o conjunto em uma luneta desproporcional ou que ocupe o centro da tela.

As variantes Mantido / Usado / Gasto compartilham a geometria e diferem no
desgaste. **Usado** é recomendada para acompanhar as armas já aprovadas:
metal grafite revestido, desgaste pontual revelando aço, sem neon, marcas ou
brilho artificial. Mantido e Gasto servem para comparação, não aprovação
automática. A prancha inclui close e duas vistas ilustrativas de gameplay.

Na visão de mira, fundo externo quase preto, abertura redonda, cruz fina
escura com contraste central discreto. Sem valores
inventados de alcance, multiplicadores, assistência de mira ou telemetria.
Não redesenhar os demais elementos do HUD Tático aprovado.

Limitação da prancha entregue: a abertura da mira é cortada acima/abaixo pela
moldura do painel ilustrativo, e o retículo tem contraste claro no centro, sem
as marcas espaçadas pedidas no prompt inicial. Na implementação proposta,
mostrar o círculo inteiro, mantendo a cruz central simples apresentada.
Essa diferença é explicitada para a aprovação, não tratada como captura fiel.

## Contrato de implementação após aprovação

- Respeitar a ação remapeável `Aim`; Mouse2 é o padrão, não uma tecla fixa.
- Preservar o zoom atual da sniper (escala de FOV 0,35 e transição de 0,35 s),
  dano, alcance, cadência, munição e recarga. Nenhuma mudança de IA.
- Usar a câmera de mundo existente, sem segunda renderização da cena ou
  picture-in-picture. A máscara/retículo serão gráficos de UI nativos, não
  uma imagem gerada contendo o cenário ou alvos.
- Centro da mira coincidente com o centro de disparo existente; abertura
  circular calculada em pixels, sem virar elipse em outra proporção de tela.
- Retícula comum substituída somente quando a visão de mira estiver ativa;
  arma e mãos não devem obstruir a abertura quando a mira estiver completa.
- Entrada/saída determinadas pelo estado de apresentação da simulação;
  sair da visão durante recarga/troca/drop, morte ou menu e não deixá-la presa.
  A política exata de transição deverá ser coberta pelos testes da alteração.
- Sem cálculo balístico novo, mira automática, zoom adicional ou promessa de
  ganho de FPS. Medir o custo do overlay antes de concluir sobre desempenho.
- Compartilhar a geometria revisada na mão e no chão. Revalidar os limites
  da malha e contato com o piso: uma objetiva maior não pode atravessar o chão.
- A aprovação do player Usado v1 já foi registrada separadamente e permanece
  válida. Esta proposta não implementa corpo, rig ou mãos do player.

## Validação prevista

CPU: estado de entrada/saída da mira, armas sem luneta preservadas, troca,
drop, recarga e reset; limites finitos da malha e apoio no chão.
OpenGL: centro do retículo, máscara redonda em 16:9/16:10 e tela pequena,
FOV configurado 60/100, transição parcial/completa e retorno sem overlay.
Capturas nativas de luneta na mão/no chão e da visão no Mouse2 em MEDIUM e
POTATO. Regressões de combate e recarga continuam obrigatórias.

Na entrega inicial só houve proposta e documentação. Após aprovação, a
implementação e suas evidências passaram a ser registradas em
[`../../04_gameplay/sniper_scope_2026-10-02.md`](../../04_gameplay/sniper_scope_2026-10-02.md).
A inspeção da prancha não equivale à validação dentro do jogo.

## Prompts utilizados

Geração inicial:

```text
Use case: stylized-concept
Asset type: Iron Seed game visual approval board, sniper optic revision v2 plus scoped aiming view v1. This is concept artwork, NOT a screenshot from the build.
Input images: Image 1 is a style and weapon identity reference: the previously approved sniper board. Image 2 is a native gameplay screenshot, only for first-person framing and industrial scene context; do not claim identical rendered fidelity.
Primary request: preserve the existing dark industrial bolt-action sniper rifle identity, stock, receiver, magazine, barrel and muzzle, but replace its small optic with a clearly readable realistically proportioned medium-size telescopic sight. Show both the physical optic and the view through it when Mouse2 is held.
Composition: clean wide landscape approval sheet on pale gray. Top strip: three equally sized side views of the SAME revised scope and mounting rings, labeled "MANTIDO", "USADO", "GASTO", differing only in coating wear, not design. Middle: a large detailed three-quarter closeup of the USADO scope mounted on the unchanged sniper, showing front objective bell, ocular eyepiece, two secure rail rings and restrained adjustment turrets; metallic coated graphite body, small exposed brushed steel edge wear, subtly cool non-emissive dark glass. Bottom: two equally sized wide first-person illustrative panels. Left "SEM MIRA": sniper compact in lower right, dark gloved hands, scene center unobstructed, recognizable scope silhouette without excessive screen coverage. Right "MOUSE2": centered perfectly circular optical aperture, nearly black outside the circle, thin black centered crosshair with small sparse ticks and a tiny central point, clear industrial interior INSIDE the aperture; no rifle or hands blocking the scoped view. Same industrial target direction in both panels, wall panels, floor grid, restrained yellow-black hazard edges from game context; no characters or enemies.
Style: serious grounded industrial game concept, plausible engineered proportions, polished material readability without glossy toy finish. Scope not oversized relative to rifle. Match USED variant of Image 1. No extra accessories, no flashlight, no suppressor, no brand logos, no laser or neon, no scope cap over the glass.
Text verbatim: title "IRON SEED - SNIPER / MIRA"; panel labels "MANTIDO", "USADO", "GASTO", "SEM MIRA", "MOUSE2"; footer "PROPOSTA VISUAL - NAO IMPLEMENTADA". Clean legible typography, no other invented numeric ballistic readouts, zoom multipliers, range values or watermark.
Constraints: physical scope and circular scoped view must be coherent. Scope circle round not ellipse and aim center exact screen center. Single sight, not a double scope. Preserve rifle design outside optic. Do not invent improved graphics of the released build: all gameplay panels are explicitly illustrative concept. No new HUD redesign.
```

Refinamento tentado, **não aplicado**: a ferramenta integrada rejeitou a saída
(HTTP 400). Nenhuma segunda imagem foi entregue e nenhum fallback CLI foi
usado. A alternativa CLI/API requer escolha explícita e `OPENAI_API_KEY`.
O prompt abaixo documenta a tentativa, não descreve uma imagem gerada:

```text
Use case: precise-object-edit
Input image: edit target, the sniper optic approval board. Preserve every rifle, optic, material, three wear variants, closeup, left SEM MIRA panel, layout and labels unchanged. Change ONLY the right MOUSE2 panel: fit the complete circular scope aperture inside that panel with equal black margins above and below; no cropped top/bottom of circle, no elliptical aperture. Make the reticle a thin black crosshair with a tiny black center point and four sparse short fine ticks per arm, centered exactly at the aperture center. Use a subtle pale outline only where needed for contrast; not a thick white cross. Keep the same industrial scene viewed inside the lens, outside almost black, and no hands or weapon inside the aperture. Keep the footer PROPOSTA VISUAL - NAO IMPLEMENTADA. No other changes.
```

## Aprovação registrada

Luneta **Usado v2** e visão de mira **v1** aprovadas explicitamente em
2026-10-02: "aprovado pode implementar". As variantes Mantido/Gasto não foram
implementadas. A aceitação final da imagem dentro do jogo permanece pendente
de playtest humano; o gate está em `../TEXTURE_APPROVALS.md`.

# Player v2 — torso, cintura e cinto de equipamentos

Estado: **AGUARDANDO APROVAÇÃO — sem implementação**. Data: 2026-10-02.
Unidade: `Player presentation/Torso + Belt`. Variante recomendada: **Usado**.
Prancha: [player_body_proposal_v2.png](player_body_proposal_v2.png).
Revisão da [v1](player_body_proposal_v1.md), não aprovação automática de v2.

## Pedido e diagnóstico

Usuário: "notei que o corpo parado fica meio estranho talvez seja melhor
colocar um torso, talvez até um cinto para segurar a pistola e faca enquanto
não estiverem em uso".

O baseline `ba1c8a9` tem pernas articuladas, mas a parte superior é uma
superfície de anéis elípticos simples, ligada ao quadril. Não tem anatomia
própria de peito/abdômen, divisão clara da cintura, cinto ou coldres.
`PlayerBodyRenderData` publica postura/movimento, não armas guardadas.
Isso explica a pouca definição visual do corpo parado; a percepção de
naturalidade será reavaliada em capturas e playtest, não inferida dos testes.

Referências inspecionadas: `src/game/visual/PlayerBodyAssets.cpp`,
`src/game/visual/GdpRenderData.hpp`, `src/gameplay/Weapon.hpp`,
`visual/validation/player_body_v1_armed_fov100.png` e a prancha v1.

## Direção visual proposta

- Torso humano, com abdômen e quadril proporcionais e conectados às coxas.
  Jaqueta industrial grafite, tecido fosco, zíper/costuras discretos e
  barra encontrando a calça. Sem cone, barriga inflada, saia ou armadura.
- Cinto preto de tecido/webbing com cerca de 4 cm, passadores e fivela
  metálica pequena e fosca. Diferenciar cintura, tecido e equipamento sem neon.
- Coldre compacto no lado direito da cintura, acompanhando a apresentação
  destra atual: pistola existente apontando para baixo, parte do cabo exposta.
- Bainha fechada e estreita no lado esquerdo, faca existente com cabo exposto
  e lâmina coberta. Ambos próximos ao quadril, sem volumes grandes na coxa.
- Preservar uniforme, reforços dos joelhos, botas, luvas e identidade das
  armas já aprovadas. Não adicionar bolsas, munição decorativa, mochila,
  armas novas ou suporte da primária nesta revisão.
- Mantido/Usado/Gasto mostram o mesmo modelo mudando desgaste, não três
  anatomias/equipamentos distintos. Só Usado é recomendado para implementação.

Tecido, cinto, couro/polímero e borracha são **não metálicos**. Apenas a fivela
e o metal existente das armas usam resposta metálica. A prancha ilustra uma
direção, não promete realismo cinematográfico nem entrega mesh/rig/textura.

## Regras dos equipamentos guardados

| Estado real da partida | Pistola no coldre | Faca na bainha |
|---|---|---|
| Primária em uso, pistola possuída | Visível | Visível |
| Pistola em uso | Ausente; coldre vazio | Visível |
| Faca em uso, pistola possuída | Visível | Ausente; bainha vazia |
| Pistola largada, primária em uso | Ausente; coldre vazio | Visível |
| Só faca possuída/em uso | Ausente; coldre vazio | Ausente; bainha vazia |

Essas são regras de **apresentação**, não um novo inventário. `Weapon::ownsWeapon`
e a arma ativa devem ser a fonte de verdade. Não inventar armas de volta após
G, não duplicar equipamento na mão/cinto, não alterar reservas ou propriedade.
A faca permanece não descartável segundo o contrato atual. Coldres/bainha
vazios continuam presos ao cinto. Troca e recarga não criam novos estados de
munição nem fazem o coldre cheio reaparecer por um relógio visual independente.

## Implementação somente após aprovação da prancha exata

1. Remodelar a parte superior em vez de apenas aumentar a superfície atual.
   Ajustar conexão cintura/coxa e postura parada; anatomia dá volume humano,
   enquanto zíper, barra e cinto dão leitura de materiais. Nenhuma mudança
   automática da altura da câmera ou da física para disfarçar o enquadramento.
2. Torso/cinto acompanham pés, yaw e postura do rig. Pitch é da câmera: olhar
   para baixo não gira o corpo inteiro. Deformar naturalmente ao agachar e
   saltar, preservando a trajetória real e a caminhada existente.
3. Cinto e coldres presos ao quadril; reutilizar os modelos Used de pistola
   e faca, com âncoras de repouso próprias. Não redesenhá-los nesta unidade.
   Publicar somente os dados de propriedade/arma ativa necessários no DTO.
4. Manter tudo no GDP, sem classes de torso, cinto ou armas dentro da engine.
   Reutilizar malhas/texturas estáticas; não gerar malhas ou mapas a cada frame.
5. Validar enquadramento e oclusão antes de ampliar animações. Cabeça, interior
   do pescoço/torso e tampas artificiais não podem aparecer em primeira pessoa.
   Torso entra pela borda inferior; cinto/coldres ficam laterais e não cobrem
   o centro de mira. Não crescer volumes de colisão, bloquear portas ou mudar IA.

Braços/mãos foram revisados em [unidade própria v2](player_hands_proposal_v2.md),
aprovada para implementação em 2026-10-03 e agora em validação no jogo.
Essa aprovação não se estende a torso/cinto. Esta proposta não
aprova novas armas, HUD, ambiente, terceira pessoa, saque cinematográfico,
rebalanceamento, inventário adicional ou mudanças nos cérebros das IAs.

## Gates e custo

- CPU: pose finita/determinística, juntas, postura parada, cintura/coldres
  seguindo quadril, propriedade/seleção/drop e retorno de menu/reinício.
- OpenGL: capturas em pé, andando, agachado e no ar; FOV 60/100 e pitch extremo;
  16:9/16:10 e janela compacta; MEDIUM/POTATO e mira da sniper.
- Pistola e faca em uso/guardadas, pistola largada e só faca: nenhuma duplicação.
  Conferir que bainha cobre a lâmina e coldres não atravessam coxas ao agachar.
- Preservar combate, movimento/bhop, recarga, drops, portas e persistência.
- Medir triângulos, draw calls, memória de mapas e tempo de GPU antes/depois.
  O baseline do corpo é 2096 triângulos/quatro materiais; não há benchmark
  da v2 nem promessa de FPS. Reuso de modelos não equivale a custo zero.
- Aprovação da prancha permite implementar; aceitação de naturalidade,
  conforto e qualidade dentro do jogo continua sendo um gate humano separado.

## Artefato e geração

Ferramenta integrada `image_gen`, por meio do skill `imagegen`, sem fallback CLI.
Imagem v1 como alvo de edição não destrutiva; screenshot nativo e modelos
existentes como referências. A v1 e seus screenshots não são sobrescritos.
Saída final inspecionada: 1672 × 941, PNG RGB/sRGB de 8 bits.
SHA-256: `208af9673352a395c0226125b48d1871dd29e4a86dc52808cef0aed05f858190`.
Arquivo original da geração final:
`/home/wilson/.codex/generated_images/01a03e92-1b6d-7e50-b844-35adb1765df0/exec-1a0a14a1-8555-4541-a4eb-53dd5984b86e.png`.
Uma cópia sem retoque foi salva na pasta de propostas do projeto.

Inspeção: Mantido/Usado/Gasto preservam a identidade; close distingue cintura,
cinto, fivela e coldres; as vistas próprias do player mostram pistola à direita
e faca à esquerda. O painel de pistola ativa mostra o coldre direito vazio,
com o modelo simples existente como referência. Uma revisão dirigida corrigiu
lateralidade e retirou a aparência de rifle do painel de pistola em uso.
Nos painéis parado/agachado, a arma primária não aparece no enquadramento;
isso é uma simplificação ilustrativa, não mudança prometida para a build.

Estas são ilustrações geradas, não capturas da v2 no renderer. Não houve
build/testes de gameplay novos porque nenhum código de execução foi alterado.
`git diff --check` passou. Aprovação humana de Usado v2 permanece pendente.
Prompts integrais:

### Geração inicial — edição da prancha v1

```text
Use case: precise-object-edit / stylized-concept.
Asset type: revised visual approval concept sheet for the Iron Seed first-person player, NOT a screenshot or a ready-to-use texture/mesh.
Input roles: Image 1 is the EDIT TARGET, the existing PLAYER V1 approval board. Image 2 is ONLY a native-game style and current-framing reference: the existing idle body's waist is too vague and disconnected. Images 3 and 4 are ONLY shape references for the existing approved pistol and knife. Preserve those weapon designs.
Primary request: revise Image 1 into a coherent PLAYER V2 torso-and-belt proposal. Keep the same human proportions, serious graphite industrial uniform, trouser knee reinforcement, dark work boots and fitted gloves. Add a naturally shaped lower chest/abdomen and anatomically connected pelvis: a restrained work jacket with central zipper, side seams, subtle folds and a hem that clearly meets the waistband. No shapeless cone, hanging skirt, giant belly, metallic armor, robotic limbs or hollow neck interior.
Add ONE practical black woven belt about 4 cm wide with a small matte dark-metal buckle and belt loops. Mount ONE compact pistol holster at the character's RIGHT waist, consistent with the existing right-handed weapon presentation, pistol muzzle down with only its existing grip/rear slide exposed. Add ONE slim closed knife sheath at the LEFT waist, existing knife handle exposed, blade fully covered. Keep both close to the hips, not giant thigh rigs. No new weapon designs, no rifles on the belt, no extra pouches, armor, backpacks, logos or luminous accessories. Cloth, webbing, leather/polymer and rubber are matte NONMETAL; only the buckle and existing weapon metal may have restrained metallic response.
Composition: preserve Image 1's clear wide landscape board format and restrained typography, but replace the reload-only content with waist/holster detail. Top left: three matching neck-down wardrobe studies labeled exactly 'MANTIDO', 'USADO', 'GASTO', same outfit/geometry/holsters, changing wear ONLY; highlight USADO as recommended. Top right: large neutral-light close detail of abdomen, jacket hem, waist belt/buckle, right pistol holster and left knife sheath, their attachment and subtle materials.
Bottom row: three large clearly ILLUSTRATIVE FIRST-PERSON panels, not an external camera facing a second person. 'PARADO': observer looks steeply down from their own eye-height over their OWN abdomen/waist, belt, separated thighs, knees and boots; same existing rifle modestly visible at lower right and both small holstered weapons at lateral hips. Keep the upper-center of view clear: torso only enters from the LOWER edge, holsters stay peripheral, feet must remain visible; a naturally connected body, not isolated legs under a floating oval. 'AGACHADO': same player's lower torso and bent knees, belt stays at waist and holsters don't intersect legs; no giant stomach filling half the frame. 'ARMA EM USO': same first-person downward pose with the referenced pistol in the right gloved hand, RIGHT holster EMPTY, knife still sheathed at LEFT waist. Do NOT duplicate the pistol in hand and holster. Add one concise caption exactly 'ARMA NA MAO = COLDRE VAZIO'. When knife is active its sheath likewise is empty; no drawing or duplication animation is promised by this concept.
Lighting: neutral and legible enough to judge materials, not cinematic darkness. Background of gameplay illustrations follows Image 2's grey industrial tiled floor and restrained indie FPS appearance; do not redesign the environment. This is feasible serious indie game art guidance, not an ultra-photorealism promise. Preserve Image 1's approved outfit identity while improving waist definition.
Text exactly: title 'IRON SEED - PLAYER V2'; footer 'PROPOSTA VISUAL - NAO IMPLEMENTADA'. Sparse labels only, no invented HUD, no performance or implementation claims, no head/face, no neck cap seen from first person. Create a new revised output; the original board remains unchanged.
```

### Revisão dirigida — lateralidade em primeira pessoa e pistola existente

```text
Edit Image 1 ONLY with one targeted consistency correction. This is the current IRON SEED PLAYER V2 approval board. Image 2 is the exact existing game's PISTOL shape reference, NOT a rifle. Preserve all top-row wardrobe studies, material detail panels, outfit, torso, belt, wear variants, lighting, framing, typography, labels, and footer 'PROPOSTA VISUAL - NAO IMPLEMENTADA'. They are already correct.
Correct the three BOTTOM first-person panels ONLY. They are the observer looking down at THEIR OWN body, not a frontal external view: the PLAYER'S RIGHT hip is the VIEWER'S RIGHT edge of those panels, and the PLAYER'S LEFT hip is the VIEWER'S LEFT.
In 'PARADO' and 'AGACHADO', put the holstered PISTOL on the VIEWER'S RIGHT hip, and the closed KNIFE SHEATH with knife handle on the VIEWER'S LEFT hip. Keep them compact and realistic at the belt, blade covered.
In 'ARMA EM USO', show the referenced simple dark-metal PISTOL in the existing right gloved hand, on the VIEWER'S RIGHT. Do NOT depict the long railed carbine or rifle: match Image 2's short rectangular slide, small sights, exposed pistol grip and trigger guard, no rifle rails/stock/foregrip. Its EMPTY PISTOL HOLSTER must be on the VIEWER'S RIGHT hip, under/near the active right hand, with no second pistol; the KNIFE remains sheathed on the VIEWER'S LEFT hip. Preserve the caption 'ARMA NA MAO = COLDRE VAZIO'. Mirror neither the text nor the whole board. Top-row frontal mannequin views already place the anatomical right holster correctly on viewer-left; leave those unchanged.
Do not add new equipment or alter the approved wardrobe/body proportions, floor background, panel layout or text. New revised output; no change to the original files.
```

Nenhum código de gameplay/renderer foi alterado por esta proposta. A build
continua usando a v1 enquanto a nova prancha aguarda aprovação.

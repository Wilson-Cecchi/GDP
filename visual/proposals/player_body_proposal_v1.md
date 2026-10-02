# Player em primeira pessoa — proposta visual v1

Estado: **PARCIALMENTE IMPLEMENTADA EM VALIDAÇÃO — Usado**. Data: 2026-10-02.
Decisão do usuário: "visual aprovado", em resposta à prancha v1 e à
recomendação da variante Usado. Não aprova uma revisão da sniper, que é outra
unidade visual. Corpo inferior v1 implementado; braços/mãos ainda pendentes.
Contrato, capturas nativas e testes: `04_gameplay/player_body_2026-10-02.md`.
Unidade: `Player presentation` — corpo, braços, mãos, uniforme e animações
de primeira pessoa. Prancha: [player_body_proposal_v1.png](player_body_proposal_v1.png).
Gerada com o skill `imagegen`, ferramenta integrada, usando uma captura nativa
da carabina como referência de estilo. A prancha é conceito, não captura do
jogo, textura aplicável, modelo 3D, rig ou animação pronta.
Resolução: 1672 × 941, RGB. SHA-256:
`c3896fd2ae23f48fb80a5db5d631bb9434a5111b71250779b0301e9faf71dae1`.

## Pedido e direção

O usuário quer um personagem visível: pernas ao olhar para baixo, movimento
durante caminhada, salto e agachamento, e mãos executando as recargas.

Proposta: adulto humano de proporções naturais, uniforme industrial grafite,
tecido fosco com costuras e reforço discreto nos joelhos, luvas ajustadas e
botas de trabalho escuras. Três acabamentos da mesma família: Mantido, Usado
e Gasto. A recomendação é **Usado**, coerente com as armas já aprovadas.
Tecido, couro e borracha não são metálicos; metal fica nas armas e fixadores.
Sem armadura volumosa, neon, logotipos ou acessórios que escondam o centro.
O rosto e uma apresentação em terceira pessoa não estão nesta proposta.

A prancha mostra as três condições em iluminação neutra, detalhes próximos
dos materiais e vistas ilustrativas de olhar para baixo, agachar, pular e
recarregar. As vistas não prometem fidelidade cinematográfica no hardware-alvo.
Seu enquadramento e a anatomia deverão ser verificados novamente no renderer.
Não aprova remodelagem das armas nem texturas novas de ambiente.

## Inspeção técnica atual

- `PlayerState` já publica posição dos pés, velocidade, yaw/pitch, grounded,
  crouchAmount e alive. A câmera lógica fica a 1,62 m em pé e 1,02 m agachada.
- `ViewMotionState` usa distância real pós-colisão; há integração de passos
  e proteção contra relocação. `PlayerBodyRenderData` agora leva o snapshot
  do corpo inferior ao renderer, com histórico visual separado de gameplay.
- Armas usam projeção própria de 68 graus para manter proporções em FOV 60–100.
  `WeaponViewmodelRenderData` já recebe recuo, recarga, equip, faca e postura,
  mas não contém braços/mãos ou pontos de contato das mãos.
- A recarga é autoritativa em `Weapon`: pente ejetado aos 32%, pente novo
  consumido ao concluir; escopeta insere cartuchos a cada 0,35 s e finaliza
  em mais 0,30 s. `reloadRoundCount()` fixa a quantidade visual planejada.

Referências: `src/gameplay/Player.hpp`, `src/gameplay/ViewMotion.hpp`,
`src/gameplay/Weapon.hpp`, `src/game/visual/GdpRenderData.hpp`,
`src/game/visual/GdpRenderer.cpp` e `src/game/Application.cpp`.

## Implementação proposta após aprovação

1. Corpo inferior com quadril, coxas, joelhos, pernas e botas, acompanhado
   por um rig humano pequeno. A base acompanha posição/yaw do player, não o
   pitch: olhar para o chão não pode girar as pernas junto da câmera. Não
   renderizar a própria cabeça/interior do pescoço em primeira pessoa.
2. Caminhada pela distância realmente percorrida, incluindo andar para trás
   e strafe. Parar diante de uma parede não anima passos. Postura deve
   convergir sem estalos, com joelhos dobrados ao agachar e pernas recolhidas
   ao saltar; aterrissagem suave, sem saltos cosméticos da câmera/hitbox.
3. Braços e mãos ligados ao passe das armas, com a mesma projeção estável.
   Definir âncoras próprias de empunhadura e suporte por arma. Cotovelos,
   pulsos e dedos precisam de articulação/deformação natural, não blocos que
   atravessam o guarda-mato nem mãos soltas no ar. Mira e recoil acompanham
   os contatos. Armas diferentes não compartilham cegamente as mesmas poses.
4. Mão dominante segura a arma; outra executa extração, descarte, entrada
   do pente novo e mecanismo. Pistola, carabina, SMG e sniper respeitam as
   fases atuais; escopeta recebe um gesto por cartucho efetivamente inserido.
   Faca usa pegada e ataque próprios. Troca, G e cancelamento desfazem somente
   a pose transitória, sem recuperar balas descartadas ou duplicar cartuchos.
5. Capturas nativas, regressões e avaliação de custo em MEDIUM/POTATO antes
   de pedir aceitação visual final. Mãos precisam continuar legíveis também
   nos perfis baixos; medir custo, não prometer FPS sem benchmark.

O corpo inferior pertence ao passe do mundo, com profundidade/oclusão reais;
braços e arma são o passe próximo de primeira pessoa. Os dois usam a mesma
direção artística, sem duplicar braços ou mostrar o interior do torso.
O rig é apresentação; não introduzir um segundo collider/hitbox. Não mudar
movimentação, bhop, câmera lógica, origem/direção do tiro, IA, munição ou saves.
Pausar congela a pose; reiniciar/relocar descarta seu histórico transitório.

## Gates de validação previstos

- CPU: poses finitas, limites de juntas, andamento por distância, postura,
  ar/parado, strafe/recuo, interrupção, reset e determinismo.
- OpenGL: pernas visíveis ao olhar para baixo em pé/agachado/no ar; ausência
  de cabeça/interior, braços duplicados, dedos/arma desconectados e clipping
  excessivo; mãos/peças coerentes em cada fase de recarga e arma.
- FOV 60/100, mira da sniper, 16:9/16:10 e telas compactas; mesmas proporções
  de mãos/armas sem deformar o corpo no mundo.
- Regressões de combate, recarga, drops, movimento, portas e persistência.
  Capturas dos passos/salto/agachamento e das cinco recargas em jogo.
- Playtest humano decide naturalidade, conforto e aceitação artística.

Os itens 1/2 e os gates do corpo inferior foram implementados após aprovação;
itens 3/4 e gates de mãos/recargas seguem pendentes. O gate
`visual/TEXTURE_APPROVALS.md` exige aprovação explícita da prancha/variante
antes da implementação; essa aprovação foi recebida para Usado em 2026-10-02.
Aceitação visual final no jogo permanece com o usuário.

## Prompt final utilizado

```text
Use case: stylized-concept. Asset type: original game character and first-person body approval sheet, NOT an implemented game screenshot. Create one wide polished landscape concept board titled exactly 'IRON SEED - PLAYER V1', with footer exactly 'PROPOSTA VISUAL - NAO IMPLEMENTADA'. The provided image is a STYLE REFERENCE ONLY for the existing restrained grey industrial corridors and simple used metal rifle, not an edit target; do not redesign the rifle or environment. Primary request: a believable human player body for an indie serious industrial FPS, with visible legs and boots looking down, arms and gloved hands naturally gripping and reloading existing guns. Practical graphite-grey industrial work trousers and long-sleeved jacket, subtle reinforced knees, dark worn work boots, fitted dark work gloves with FIVE correctly proportioned fingers each. No giant armor, helmet, insignia, logos, luminous clothes or unrelated gadgets; serious natural proportions, modest indie-feasible mesh complexity, realistic fabric and rubber instead of metallic skin. Layout: upper left three matching neutral neck-down full-body orthographic wardrobe studies labelled 'MANTIDO', 'USADO', 'GASTO', same outfit and palette with increasing wear ONLY. Main recommended direction is USADO, no extreme grime. Upper right larger clear close-up of a gloved hand and jacket cuff, boot, knee material under neutral bright studio light, clearly distinguish fabric weave, stitching, rubber soles, restrained wear. Bottom half four substantial illustrative FIRST PERSON panels with industrial tiled floor and a modest peripheral rifle matching the style reference: 'OLHAR PARA BAIXO' shows the observer's own thighs, two knees, shins and boots from eye-height angled steeply down, not another person's legs; 'AGACHAR' shows the observer's own bent knees and feet from lower crouched eye height; 'PULAR' shows the observer's own flexed legs during an airborne jump, no camera in the stomach; 'RECARREGAR' shows right hand holding existing rifle pistol grip and left hand seating a new magazine in the gun, old magazine falling independently. Include a small inset next to reload with gloved left hand inserting ONE shotgun shell through underside loading port. The right hand stays on the gun. Clearly depict connected forearms, natural wrists and anatomically correct fingers, not floating weapons or extra limbs. Maintain open center for aim and HUD, arms do not occupy half the view. Same wardrobe in every panel, no added characters. Materials: matte grey cloth, nearly black rubber and leather, restrained dark metal ONLY for gun and fasteners; no cartoon, chibi, voxel or block body. Lighting: neutral enough to judge materials, grounded industrial realism without cinematic fog. Attractive coherent original approval sheet, legible sparse typography, no invented HUD or game claims. Need three wear variants, close material view and gameplay-view illustrations all on the ONE board.
```

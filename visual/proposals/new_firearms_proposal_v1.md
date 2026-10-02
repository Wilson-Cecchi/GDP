# Novas armas — propostas visuais v1

Data: 2026-10-02. Geradas com a ferramenta integrada de geração de imagens.
São conceitos ilustrativos; as vistas de gameplay NÃO são capturas do jogo.
Nenhuma prancha é uma textura pronta ou substitui a aprovação de modelo.

## Contrato

- Unidades independentes: `Weapon/Sniper`, `Weapon/Shotgun`, `Weapon/SMG`.
- Direção: equipamento industrial sério, aço metálico e polímero escuro,
  desgaste moderado Used; sem luzes sci-fi, marcas ou silhuetas exageradas.
- Pranchas: Maintained / Used / Worn, vista próxima neutra e composição
  ilustrativa em primeira pessoa, com centro livre e empunhadura correta.
- Sniper: [prancha v1](weapon_sniper_proposal_v1.png), IMPLEMENTADA EM VALIDAÇÃO.
- Escopeta: [prancha v1](weapon_shotgun_proposal_v1.png), IMPLEMENTADA EM VALIDAÇÃO.
- SMG: PROPOSTA EM PREPARAÇÃO. A ferramenta rejeitou a geração; nenhum
  artefato foi obtido ou implementado. Não houve fallback CLI/API.
- Aprovação do usuário em 2026-10-02: "aprovadas a parte visual e funcional".
  Abrange as duas pranchas v1 apresentadas, com direção Used, e as mecânicas
  das três armas; não abrange uma futura prancha de SMG ainda inexistente.
  Dimensões/viewmodel/FOV/drops devem
  ser validados em capturas reais após implementação aprovada.
- No runtime, sniper e escopeta usam geometrias próprias, compartilhadas entre
  viewmodel e pickup caído. Somente a SMG mantém a carabina provisória.
  As pranchas ilustrativas não são carregadas pelo renderer nem pelo pacote Windows.

## Implementação nativa Used v1 — em validação

A sniper tem luneta fixa discreta, alavanca do ferrolho, carregador curto e
coronha ajustável; a escopeta tem pump estriado, tubo sob o cano, coronha
convencional e miras abertas. Ambas usam aço revestido, aço exposto, polímero
escuro e recessos dos mapas Used já existentes, sem glow ou textura adicional.
São modelos procedurais de orçamento baixo/médio, não reproduções fotográficas
das pranchas. Mantêm projeção do viewmodel fixa em 68°, centro livre e posição
inferior direita. Flash acompanha o cano próprio; hitscan, dano e loot não mudam.
Ferrolho/pump ainda não têm animação mecânica independente.

Capturas reais do renderer offscreen, fixture de regressão 1280×720, seed visual
1717 (mão) e 123 (chão), não sessão manual de gameplay:

| Unidade | Primeira pessoa | Caída no chão | POTATO |
| --- | --- | --- | --- |
| Sniper | [captura](../validation/weapon_sniper_v1_used_gameplay.png) | [captura](../validation/weapon_sniper_v1_used_ground.png) | [captura](../validation/weapon_sniper_v1_used_potato.png) |
| Escopeta | [captura](../validation/weapon_shotgun_v1_used_gameplay.png) | [captura](../validation/weapon_shotgun_v1_used_ground.png) | [captura](../validation/weapon_shotgun_v1_used_potato.png) |

Gates CPU verificam geometria finita, empunhadura, silhuetas distintas da
carabina, orçamento inferior a 1.600 triângulos por arma e contato com o chão
nas duas orientações. OpenGL verifica modelos visíveis/distintos em MEDIUM e
POTATO, drops e footprint estável ao alterar FOV 60/100. A aceitação artística
final das implementações continua pendente em `visual/TEXTURE_APPROVALS.md`.

Para recapturar, definir `GDP_NEW_WEAPON_CAPTURE_DIR` ao executar
`gdp_renderer_gl_tests` com `SDL_VIDEODRIVER=offscreen`; BMPs ficam no diretório
indicado. Para playtest interativo: `build/release/gdp --test-mode`, G para liberar
a primária antes de recolher outra, 1 para equipá-la.

## Prompt comum enviado (verbatim)

```text
Use case: stylized-concept. Asset type: concept approval board for one new first-person weapon in Iron Seed, an industrial tactical FPS. This is a proposal, NOT an actual game screenshot or finished game asset. Style: realistic detailed 3D industrial equipment, serious tone, gunmetal coated steel, brushed bare steel wear on edges, charcoal polymer, subdued colors, absolutely no glowing sci-fi elements, no brands or logos. Landscape board with clean off-white/gray studio background, small title and organized three top columns showing the EXACT SAME weapon silhouette in three wear variants labelled "MAINTAINED", "USED", "WORN". Used is the intended implementation direction. Middle: large realistic three-quarter close view of the Used version, enough neutral illumination to inspect metallic material and grip orientation. Bottom: small illustrative first-person gameplay view of Used in lower-right of an industrial room with metal wall panels and yellow floor hazard strip. Correct grip and perspective, muzzle toward vanishing point, weapon leaves central aiming area open, not oversized. Architecture in bottom inset must have neutral ceilings without painted lamps. No people or enemies, no blood, no exploded assembly, no instructional construction details. Footer text exactly "CONCEITO v1 - AGUARDANDO APROVAÇÃO". All three wear variants must share the same exact mechanical proportions, not three different weapons.
```

Cada chamada concatena o prompt comum, uma quebra de linha e o complemento
abaixo. Foram chamadas independentes por unidade, não edição de arte existente.

### Sniper

```text
Text title exactly: "SNIPER".
Subject: Custom compact bolt-action precision rifle, five-round magazine, modest fixed optic on top, visible bolt handle on receiver side, conventional barrel, adjustable polymer stock, well separated trigger guard and grip. Realistic practical long-range weapon, no extreme accessories or excessive large scope. It should be distinct from an assault carbine.
```

### Escopeta

```text
Text title exactly: "ESCOPETA".
Subject: Custom realistic pump-action shotgun, tubular magazine under barrel, clear sliding fore-end pump, six-shell capacity concept, sturdy receiver, simple iron sights, conventional stock with pistol grip, no scope, no rifle box magazine. Clean believable working weapon silhouette, realistic proportions, no extreme accessories.
```

### SMG — tentativa sem artefato

```text
Text title exactly: "SUBMETRALHADORA".
Subject: Custom compact automatic submachine gun, shorter than a carbine, 24-round compact slightly curved box magazine, short barrel with simple muzzle, compact folding stock, separate clearly oriented pistol grip and short fore-end, low iron sights, restrained design. Do not turn it into a pistol or full-length assault rifle.
```

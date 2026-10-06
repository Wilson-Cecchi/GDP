# Mãos e contatos de recarga — proposta v2

Estado: **DIREÇÃO USADO APROVADA; REVISÃO POSTERIOR APROVADA PROVISORIAMENTE**.
Data: 2026-10-03.
Unidade: `Player presentation/Hands + Reload contacts`.
Variante recomendada: **Usado**.
Prancha: [player_hands_proposal_v2.png](player_hands_proposal_v2.png).

## Pedido e limite da aprovação

Usuário: "prepara um visual melhor para as mãos e ajuste as mãos para quando
tiver que recarregar as armas". Pedido anterior: mãos seguem as mesmas regras
das armas em relação ao FOV.

A v1 tem aprovação de direção, mas seu candidato local não recebeu aceitação
final. Após apresentar esta prancha e recomendar Usado, o usuário respondeu
"visual simulado aprovado para implementação" em 2026-10-03.
Isso aprova Usado v2 e o contrato de contatos desta unidade; aceitação visual
final depende das capturas e do teste no jogo.
Torso/cinto v2 é outra unidade, continua pendente e não faz parte desta revisão.

A prancha é **conceito ilustrativo**, não screenshot, textura aplicável, mesh,
rig ou animação pronta. Não é promessa de fidelidade cinematográfica no jogo.

Implementação publicada e rejeitada no playtest, orçamento e capturas:
[mãos v2](../../04_gameplay/player_hands_2026-10-03.md).
Feedback: mãos muito finas/longas, sem proporção com armas; faca com duas
mãos; punhos parecem quebrar nas animações. Correção local para a mesma
referência: [revisão](../../04_gameplay/player_hands_2026-10-04.md).
Revisão posterior das seis armas e braços mais curtos:
[candidato atual](../../04_gameplay/player_weapon_contacts_2026-10-04.md).
A movimentação e os visuais dessa revisão foram aprovados por enquanto no jogo
em 2026-10-05; isso não aprova retroativamente a implementação rejeitada.
Nenhuma captura ou teste anterior comprova fidelidade à prancha.
As seções abaixo preservam o diagnóstico e o contrato da etapa de proposta;
seus números da v1 e gates previstos não são resultados medidos da v2.

## Diagnóstico do candidato atual

Inspeção de `PlayerHandsAssets.cpp`, `GdpRenderData.cpp`, testes de CPU/GL,
capturas nativas revisadas e frames das recargas:

- Palma e mangas são superfícies de anéis; dedos são tubos afunilados. O polegar
  tem oposição simples, mas não uma base anatômica própria. Isso dá pouca
  definição à mão e reforça a aparência de peças cilíndricas.
- A matriz-base da palma direita/esquerda permanece praticamente a mesma
  para suporte, pente, mecanismo e cartucho. A mão muda de posição e flexão
  geral, mas não recebe orientação de contato e postura de dedos específicas.
- O teste atual exige que a direita nunca saia da empunhadura durante TODA
  recarga. Na sniper, isso força a esquerda a alcançar o ferrolho do lado
  direito, em vez de apoiar a arma enquanto a direita opera esse mecanismo.
- Os contatos seguem deslocamentos do pente/ferrolho, mas a validação atual
  de punho não prova alinhamento da superfície da palma ou das pontas dos
  dedos com a peça. Pixels diferentes e ausência de NaN não provam anatomia.
- A SMG usa arte provisória da carabina; seu mecanismo tem janela própria
  (começa antes), enquanto a transição atual da mão usa cortes genéricos.

Estas são limitações de apresentação, não defeitos de munição comprovados.
As capturas inspecionadas são fixtures offscreen, não um novo playtest manual.

## Direção visual

Mesma identidade industrial grafite do player Usado:

- Palma com arco discreto, base do polegar definida e conexão punho/antebraço.
- Cinco dedos de comprimentos diferentes, falanges afuniladas, juntas alinhadas,
  polegar oposto e sem esferas/cilindros desconectados na silhueta.
- Luva ajustada: tecido no dorso, couro fosco na palma/pontas, reforço flexível
  discreto e costuras finas. Dobras perto das articulações, não ruído em tudo.
- Punho e manga proporcionais, sem ombros soltos, manoplas grandes ou braço
  inflado atravessando a tela.
- Mantido/Usado/Gasto mudam somente desgaste. A anatomia e o modelo são iguais.
  Tecido, couro e reforço continuam não metálicos; sem neon ou armadura sci-fi.
- Mão deve caber na empunhadura existente: não aumentar a arma para esconder
  um erro de escala nem redesenhar armas, HUD ou ambientes.

A vista de carabina mostra UM pente novo na esquerda e alojamento vazio.
Uma revisão dirigida removeu o pente já instalado da primeira geração,
evitando duplicação no instante de inserção. A prancha inclui três acabamentos,
close palma/dorso e duas vistas ilustrativas de primeira pessoa.

## Contatos e poses de recarga propostos

Usar contato com **posição + orientação + perfil por dedo**, em vez de trocar
apenas a translação do punho. Interpolar trajetórias e rotações continuamente;
IK mantém alcance e resolve cotovelos sem deslocar artificialmente a câmera.

| Arma/fase | Mãos e contato pretendido |
| --- | --- |
| Pistola | Direita segura a empunhadura; esquerda envolve as laterais/base do pente, solta o velho, busca e alinha o novo; depois pega o slide por cima/trás e volta ao suporte. |
| Carabina | Direita conserva a pegada; esquerda deixa o apoio dianteiro, retira o pente pelo corpo, abre os dedos ao descartar, insere outro pelo eixo do alojamento e alcança o mecanismo. |
| SMG | Preserva modelo provisório, mas usa alcance compacto e janela própria do mecanismo; não copiar cegamente a cronologia da carabina. |
| Sniper | Na abertura/fechamento do ferrolho, esquerda apoia o antebraço da arma e direita deixa a pegada para operar o lado direito. Na troca do pente, direita volta à pegada e esquerda troca o carregador. Retorno suave, sem terceira mão. |
| Escopeta | Direita estabiliza; esquerda tem pose de pinça/empurrar um cartucho, com polegar e indicador próprios e demais dedos recolhidos. Repete por cartucho planejado; depois volta ao pump para finalizar. Sem pente. |
| Faca | Pegada proporcional e movimento existente; não recebe recarga fictícia. |

O pente velho cai independentemente a partir da ejeção; a mão não o acompanha
até o chão. O novo entra sem duplicar um pente instalado e os dedos não
atravessam sua entrada. Durante a busca, a mão pode sair pela borda inferior,
sem criar um novo cinto/bolsa ainda não aprovado.

A prancha extra de storyboard por arma **não foi gerada**: a ferramenta
integrada rejeitou a geração. Nenhuma imagem dessa tentativa existe para
aprovação. As poses por arma acima são o contrato proposto, não uma sequência
visual executada. A prancha entregue mostra apenas empunhadura da pistola e
entrada do pente da carabina; capturas de TODAS as recargas serão necessárias
na implementação para aceitação final.

## Invariantes de implementação após aprovação

- Armas, mãos e mangas permanecem no mesmo passe próximo de **68° fixos**.
  FOV do mundo 60–100 não altera sua proporção; recuo, troca e postura continuam
  compartilhando o root. Corpo inferior continua no passe de mundo.
- `Weapon` é a única autoridade de tempo, munição e cancelamento. Ejeção aos
  32%; pente novo consumido no término. Escopeta: 0,35 s por cartucho + 0,30 s
  de finalização. Não estender tempos/dano/cadência para acomodar animação.
- Dedos e punho devem usar transformações das peças reais amostradas na fase,
  incluindo rotação, não posições aproximadas desconectadas do mecanismo.
- Pausa congela a pose. Troca, G, reinício e cancelamento restauram apresentação,
  sem recuperar munição descartada nem conceder cartuchos fictícios.
- Sniper em mira completa oculta o conjunto próximo; nada de mãos soltas no
  menu/resultados, arma invisível ou braço duplicado no corpo do mundo.
- Rig/receitas/poses são GDP. Não introduzir conceitos de mãos, armas ou recarga
  na WKC Forge. Não alterar movimento/bhop, câmera lógica, origem do tiro,
  collider, saves, geração de mapas ou treinamento/ativos de IA.
- Preferir rig existente de 36 juntas e assets estáticos. Preservar leitura
  da silhueta em POTATO; medir custo antes/depois. Baseline local: três draws,
  2264 triângulos. Não há benchmark nem orçamento medido para a v2.
- Mapas de material novos, se necessários, são estáticos; não gerar por frame,
  adicionar cena/luzes/sombras ou vender a prancha como asset já pronto.

## Gates previstos

1. CPU: finitude/determinismo, limites de juntas, alcance/IK, transições de
   posição E orientação, abertura/fechamento dos dedos e contato por fase.
   Preservar âncora direita nas armas apropriadas; substituir a regra universal
   por janelas explícitas de grip/ferrolho na sniper. Não simplesmente remover
   o teste que revelou a limitação.
2. Pente/shell: contato dos dedos e palma com a peça, liberação independente,
   alinhamento na entrada, mecanismo próprio por arma, endpoints exatos.
   Escopeta com uma/seis inserções e cancelamento entre cartuchos.
3. OpenGL: capturas e GIFs nativos das cinco recargas, empunhadura/faca e
   interrupção, MEDIUM/POTATO, FOV 60/100, 16:9/16:10/telas compactas,
   mira da sniper, pausa e exclusão de menu/resultados.
4. Integração real: atirar/R, concluir/pausar/trocar/G, preservar munição e
   persistência; regressões de combate e fronteiras arquiteturais.
5. Capturas não substituem teste humano: pedir aceitação de proporção, contato,
   naturalidade e espaço ocupado na tela. Divergência da proposta volta à revisão.

## Artefato e validação desta etapa

Ferramenta integrada `image_gen`, skill `imagegen`, sem fallback CLI.
PNG 1672 × 941, RGB/sRGB, 8 bits.
SHA-256: `62199cfbfb58915cc44cdc1aca786a173ebc3242e975712712513bdad2fa8911`.
Original final preservado:
`/home/wilson/.codex/generated_images/01a03e92-1b6d-7e50-b844-35adb1765df0/exec-f53d7ed3-fa45-4edd-a310-7a520b16b1ff.png`.
Cópia no workspace sem retoque. V1 e torso/cinto v2 não foram sobrescritos.

Na etapa de proposta só mudaram documentação e prancha. Código e candidatos
locais preexistentes foram preservados; não se atribui os testes históricos
à proposta v2. Nenhuma nova suíte de gameplay foi necessária/executada.
Aprovação recebida depois dessa etapa; resultados da implementação serão
registrados separadamente, sem atribuir testes antigos à v2.

## Prompts utilizados na prancha entregue

### Geração

```text
Use case: stylized-concept.
Asset type: versioned hand-and-glove visual approval board for Iron Seed, NOT a game screenshot or an implemented asset.
Input roles: Image 1 is the approved PLAYER V1 outfit STYLE REFERENCE only. Images 2 and 3 are native-game pistol and carbine SHAPE AND FRAMING references only, NOT edit targets. Do not preserve their crude hand anatomy; improve the hands, but preserve the weapon designs.
Primary request: one clean landscape 16:9 board titled exactly "IRON SEED - MAOS V2". Believable adult HUMAN hands with fitted full-finger graphite industrial work gloves and matching jacket cuffs. Natural palm arch, distinct thenar/thumb base, five fingers with clearly different lengths, tapered finger segments, aligned knuckles and opposed thumb. Correct human hand proportions relative to the existing weapons: palms fit pistol grips; fingers wrap around rather than penetrate the grip, sleeves are not giant cylinders, wrists do not twist unnaturally. No extra fingers or limbs.
Composition: top left three large neutral-lit views of the SAME gloved hand and cuff, back of hand slightly open, labels exactly "MANTIDO", "USADO", "GASTO"; change surface wear ONLY, not anatomy or glove design. Highlight USADO subtly as recommended. Top right a large two-view close-up of the recommended glove, palm and dorsal side, showing thumb base, subtle knuckle shaping, fine seams, leather palm and fabric back. Bottom half two large ILLUSTRATIVE first-person panels with a quiet grey industrial tiled room as backdrop: label "PISTOLA - EMPUNHADURA" shows the original reference pistol held properly by the right hand and supported by the left; label "CARABINA - PENTE NOVO" shows the original reference carbine, right hand retaining grip and LEFT hand gripping the body of ONE new magazine just beneath its magwell, aligned for insertion, fingers alongside magazine not across its insertion tip. Both arms connected to cuffs and entering from bottom frame; center and most of environment stay clear. Do not duplicate right or left hands. Weapons stay lower-right peripheral with proportionate hands, not enlarged occupying half the frame. Do NOT redesign gun rail, stock, barrel or sights.
Materials: graphite woven jacket fabric, dark fitted fabric glove dorsum, worn matte leather palm/finger pads, restrained flexible NONMETAL reinforcement. Fine stitching, mild creases at actual joints, subtle grey edge wear. No shiny rubber sausages, hard sci-fi knuckle armor, metallic skin, plastic toy look, giant gauntlets or glowing details.
Style: sober detailed 3D game concept art with realistic material guidance, indie-feasible rounded silhouette; neutral legible lighting, NOT cinema post-processing or a fidelity/performance promise. Serious industrial tone matching the approved outfit. No face, body redesign, holsters, new weapons, HUD, logos or watermark.
Text exactly: title "IRON SEED - MAOS V2", footer "PROPOSTA VISUAL - NAO IMPLEMENTADA"; only panel labels listed above and small close-up label "PALMA / DORSO". Three wear variants, close material inspection and two first-person poses on this one board. New output; references remain unchanged.
```

### Revisão única: remover duplicação do pente

```text
Use case: precise-object-edit.
Image 1 is the EDIT TARGET, the complete "IRON SEED - MAOS V2" visual approval board. Make exactly ONE consistency correction: in the bottom-right panel "CARABINA - PENTE NOVO", REMOVE the seated magazine currently protruding downward from the gun's magwell. Leave a visibly EMPTY magazine well with a subtle dark opening, because the LEFT hand is holding the incoming new magazine. Keep that handheld new magazine and the right hand on the pistol grip unchanged. There must not be both a seated magazine and a new magazine at the same insertion phase.
Preserve EVERY other part of the board: three wear variants, anatomy, gloves, cuffs, materials, close-ups, pistol pose, carbine receiver/stock/barrel/rails/sights, camera framing, background, labels, title, footer and lighting. Do not add hands, ammo, parts or other objects. New output; do not alter source.
```

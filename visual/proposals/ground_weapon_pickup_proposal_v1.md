# Arma solta no chão — proposta visual v1

```text
estado: REVISÃO SOLICITADA; substituída pela proposta v2
unidade visual: Weapon/ground pickup — Pistol e Carbine
versão: v1
data: 2026-09-27
pranchas: ground_weapon_pickup_v1_close.png
          ground_weapon_pickup_v1_gameplay.png
geração: image_gen integrado; conceitos, NÃO capturas da build
referência de identidade: weapon_first_person_v1_used.png (Used aprovada)
variantes: três distribuições de desgaste Used, da esquerda para a direita
decisão do usuário: pendente
```

Após esta primeira prancha, o usuário pediu que a arma parecesse **jogada no
chão, como se tivesse caído**, usando a aparência já existente. Esta versão
mostra armas excessivamente arrumadas e um marcador ciano não desejado para a
nova direção. A revisão está em
[`ground_weapon_pickup_proposal_v2.md`](ground_weapon_pickup_proposal_v2.md).

## Problema e direção

O drop funciona, mas hoje o pickup aparece como um bloco violeta. A proposta
mostra a própria Pistol ou Carbine como objeto físico reconhecível no piso,
mantendo a linguagem gunmetal/polímero da variante `Used` já aprovada para a
arma em primeira pessoa. A [prancha próxima](ground_weapon_pickup_v1_close.png)
compara três variações contidas de abrasão e sujeira na mesma silhueta;
a [vista de gameplay](ground_weapon_pickup_v1_gameplay.png) testa leitura no
corredor sob luz neutra. A vista de gameplay mostra **as duas armas juntas só
para comparação**, não uma nova regra de drop simultâneo.

O marcador proposto é apenas um pequeno traço ciano no piso ao lado da arma.
Ele ajuda a achar o item sem tingir a arma inteira ou criar uma caixa luminosa.
Se o usuário preferir a arma sem marcador, isso pode ser aprovado separadamente.
Não há aura grande, flutuação, pedestal nem mudança de colisão.

## Limites de implementação, somente após aprovação

- Reusar a silhueta e materiais `Used` dos viewmodels sempre que fizer sentido;
  aproximar detalhes com malha procedural econômica, sem importar estes PNGs
  como sprites ou texturas de gameplay.
- Distinguir Pistol e Carbine por comprimento, proporção e peças próprias.
  Apoiar cada uma sobre o piso com sombra de contato, sem atravessar o chão.
- Preservar local do drop, raio de coleta automática, munição exata, XP zero
  na recuperação, integridade dos saves e regras de `G`.
- Garantir leitura em luz fraca e em POTATO/LOW; limitar draw calls e permitir
  simplificação de detalhes sem regressar ao bloco violeta.
- A primeira aprovação cobre **somente pickups de arma**; não altera arma em
  primeira pessoa, demais itens, menus, HUD ou texturas de piso.

## Gate de aceitação

Após aprovação explícita da unidade e do marcador (ou pedido de revisão),
implementar e gerar capturas reais no jogo em vista próxima, distância de
coleta, pouca luz e perfil POTATO. Validar regressões de loot, renderização e
desempenho; solicitar aceitação visual final antes de tratar como concluído.

## Prompts usados

As pranchas foram feitas no modo integrado de `image_gen`. Para a vista
próxima, `weapon_first_person_v1_used.png` foi **referência de identidade**, não
alvo de edição: pistola e rifle `Used` apoiados em piso industrial, três
colunas com a mesma silhueta e desgaste procedural sutil, luz branca neutra,
sombra de contato e traço ciano lateral; sem bloco roxo, aura, mãos, marcas ou
outros objetos. Uma revisão do primeiro resultado pediu diferenças visíveis
nos riscos e sujeira **das armas**, sem mudar piso ou geometria. Para a vista
de gameplay, a captura real do corredor foi **alvo de edição** e a prancha
`Used` foi referência: preservar cena, arma segurada, câmera, retícula e HUD;
inserir Pistol e Carbine em escala plausível no piso a aproximadamente
1,5–2 m, cada uma com contato físico e localizador discreto. Não interpretar
a composição editada como captura da build.

## Integridade dos artefatos

```text
close: dcfff79e7c58f4df97e11a920b442873167c7a2986266683cec98874a15d07cb
gameplay: e4bb0993100f56784fe8d175fa75eb70b01665884e323b06eca685d67e387878
```

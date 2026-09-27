# Arma solta no chão — proposta visual v2

```text
estado: IMPLEMENTADA EM VALIDAÇÃO VISUAL
unidade visual: Weapon/ground pickup — Pistol e Carbine
versão: v2
data: 2026-09-27
resposta ao usuário: "jogada no chão, como se tivesse caido mesmo,
                    usando a aparencia que já temos da arma"
pranchas: ground_weapon_pickup_v2_close.png
          ground_weapon_pickup_v2_gameplay.png
geração: image_gen integrado; conceitos, NÃO capturas da build
referência de identidade: capturas in-engine da Pistol e da Carbine Used
variantes: três poses de repouso, sem skins diferentes
decisão de implementação: APROVADA pelo usuário ("pode implementar")
capturas da build: visual/validation/ground_weapon_pickup_v2_{close,gameplay,dim,potato,fall_mid}.png
aceitação visual final em partida: pendente
```

## Direção revisada

A arma deve parecer um objeto que **caiu de lado**: orientação irregular,
peças em contato com o piso e sombra coerente. A Pistol e a Carbine conservam
a silhueta e os materiais gunmetal/polímero dos modelos `Used` já existentes
na build. Não é uma nova arma nem uma textura fotográfica. A
[prancha próxima](ground_weapon_pickup_v2_close.png) apresenta três possíveis
poses de repouso, e a
[vista de gameplay](ground_weapon_pickup_v2_gameplay.png) testa leitura no
corredor à distância normal. A cena com as duas armas juntas é apenas uma
comparação visual, não uma nova regra de drop simultâneo.

Não há bloco roxo, pedestal, levitação, aura ou traço ciano. A leitura vem do
contorno real da arma e da luz da sala. Em sala muito escura, a ausência de
marcador pode exigir ajuste de contraste após playtest; nenhum marcador foi
aprovado antecipadamente.

## Movimento implementado

Ao pressionar `G`, a malha desce e gira por 25 ticks de simulação até repousar
de lado no ponto lógico de drop. A orientação varia deterministicamente por
identidade do pickup, sem consumir o RNG estrutural do mapa. O ponto de coleta,
munição e progressão não mudam. A captura `fall_mid` registra o meio da queda;
as demais mostram o repouso. Não há ragdoll nem física geral.

## Limites da implementação

- Reusar ou derivar as malhas e materiais atuais das armas em primeira pessoa;
  não usar estes PNGs como sprite/texture aplicada ao jogo.
- Pistol e Carbine precisam ter comprimento, corpo e componentes próprios.
  Variar principalmente yaw e lado de repouso, não a identidade do modelo.
- O ponto final deve respeitar piso, paredes e bordas de corredor. Evitar
  clipping, arma em pé, contato suspenso e animação longa que atrase a coleta.
- Nenhuma mudança em dano, munição, raio de coleta, XP, save, portas ou slots.
- Verificar legibilidade e custo nos perfis POTATO/LOW; simplificar detalhes
  distantes sem voltar a um cubo violeta.

## Gate de aceitação

A implementação foi autorizada. Capturas reais em vista próxima, distância de
coleta, pouca luz, POTATO e meio da queda acompanham testes de loot/render e
contato dos vértices com o piso. A aceitação visual em partida permanece pendente;
esta unidade ainda não foi declarada concluída pelo usuário.

## Prompts usados

Modo integrado de `image_gen`. Na vista de gameplay, a v1 foi **alvo de edição**
e as capturas in-engine da Carbine/Pistol foram **referências**: alterar apenas
as armas no piso para poses de queda com yaw irregular, contato e sombra;
preservar corredor, câmera, arma segurada, HUD e retícula; remover marcadores
ciano e acabamento excessivamente realista. Na vista próxima, as capturas
in-engine foram referências de silhueta/material e a v1 foi referência de
diagramação: três colunas com poses diferentes para as mesmas duas armas,
geometria facetada existente, piso industrial e iluminação neutra, sem texto,
brilho ou objetos adicionais.

## Integridade dos artefatos

```text
close: 7b4543da92b38df8fc0f88d4488899d61b12d4464af05bec79ce4de74090439e
gameplay: 89ca9bb42292651817f3cd1f7490e6e3660cd6cb5802813bdda922b49f88c405
```

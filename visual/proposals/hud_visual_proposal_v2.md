# HUD de gameplay — propostas visuais v2

```text
estado: AGUARDANDO APROVAÇÃO
unidade visual: HUD/map visual language — somente HUD de gameplay
versão: v2
data: 2026-09-27
base: captura real da variante v1 A Rail, 1280×720, iluminação neutra
pranchas: hud_visual_v2_a_precision.png
          hud_visual_v2_b_telemetry.png
          hud_visual_v2_c_instrument.png
          hud_visual_v2_close_comparison.png
geração: image_gen integrado; mockups editados, NÃO capturas da build
seeds: não se aplica; o HUD não é uma textura procedural
decisão do usuário: preferência por A ("eu gostei da hud a");
                    aprovação para implementação pendente
```

## Por que revisar

A variante v1 A Rail funciona, mas os módulos atuais ainda parecem caixas
simples. A v2 explora acabamento e hierarquia melhores sem trocar informações,
comandos ou regras de jogo. As três pranchas usam a mesma captura como
referência de composição; o cenário, o viewmodel e alguns pixels podem ter
sido reinterpretados pela geração de imagem. **Somente o HUD é proposto.**
Nenhum mockup foi implementado no renderer.

## Escolhas

| Variante | Direção | Vantagem | Atenção na implementação |
|---|---|---|---|
| [A — Precision](hud_visual_v2_a_precision.png) | Rail refinado com molduras metálicas e ícones discretos | Continuidade com a v1 e leitura robusta | Não deixar as molduras crescerem sobre a cena |
| [B — Telemetry](hud_visual_v2_b_telemetry.png) | Brackets finos e base quase sem moldura | Mais espaço de visão e menor custo visual | Garantir contraste em salas claras e escuras |
| [C — Instrument](hud_visual_v2_c_instrument.png) | Placas técnicas curtas com cantos chanfrados | Presença industrial forte e dados compactos | Evitar excesso de massa nos cantos |

A [prancha aproximada](hud_visual_v2_close_comparison.png) coloca A, B e C
lado a lado, nesta ordem, para avaliar tipografia, espessura e barras. O
usuário indicou que gostou da **A — Precision**. Isso registra a preferência,
mas ainda não é aprovação explícita para implementá-la.

## Contrato preservado

- HP e Armor separados; arma/munição, chaves, nível/XP, objetivo/tempo e evento
  mantêm suas posições periféricas e seus estados atuais.
- Retícula e centro do viewport permanecem livres. Nenhum dado novo, radar,
  objetivo novo ou mecânica de combate é proposto.
- Ciano indica informação/interação; laranja indica aviso. O texto deve
  continuar legível em 960×540, 1280×720 e 16:10, inclusive com pouca luz.
- A implementação aprovada deve usar formas/texto nativos do compositor,
  **não** colar estes PNGs na tela. Sem novo framebuffer, hit test ou custo
  significativo para POTATO/LOW.
- Menus, painel `Tab`, overlay `F3`, modelos das armas e texturas do mundo ficam
  fora desta aprovação. A v1 A Rail segue sendo a build atual até uma decisão.

## Gate de aceitação

Após escolha explícita de A, B ou C (ou pedido de revisão), fazer uma única
implementação delimitada, capturar o HUD real em 1280×720, 960×540 e pouca luz,
rodar regressões de HUD/menus/renderer e pedir aceitação visual em partida.
Variação relevante entre mockup e build volta para revisão.

## Prompts usados

Todas as imagens foram produzidas no modo integrado de `image_gen`, com a
captura v1 A Rail como **alvo de edição**, não como prova de implementação.
A base comum pediu preservar corredor, luz, arma em primeira pessoa, câmera,
retícula, centro livre e os dados `HP 85/115`, `ARMOR 35/60`,
`CARBINE 18/84`, `KEYS 1`, `LV 3`, `XP 70/175`, `OBJECTIVE REACH BOSS`,
`TIME 00:00.00` e `EVENT ACTIVE`; mudar somente a periferia do HUD, sem
marcas ou mecânicas inventadas. A pediu rails gunmetal usinados, textura
metálica sutil e acentos ciano. B pediu telemetria quase sem moldura, brackets
finos e alinhamento rigoroso. C pediu etiquetas de instrumento em metal fosco,
cantos chanfrados e aviso laranja contido. A prancha aproximada usou as três
saídas como referências, preservou A/B/C em colunas e ampliou os módulos de
recursos, objetivo/evento e nível/XP sobre fundo neutro.

## Integridade dos artefatos

```text
A: 817f541cb72bd5f47ba598a49f4e73f255a4f4e9c4b001954d8dfde802408d5a
B: cf6ca029f47c90cde8a6c542b5f2053292be59236d584e6bcb1edd06eeeec360
C: 0e88964079318c7a3137254f754af5c179658d9f49106f2b32c38bbdcc695983
close: 1bd144b3b3e904a497082d55d40e2bf4f8a54ea56a4ca89044af8a6243aef2d8
```

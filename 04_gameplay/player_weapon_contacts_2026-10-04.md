# Mãos por arma e braços mais curtos — revisão local

Data: 2026-10-04. Estado: **APROVADO PROVISORIAMENTE EM 2026-10-05**.
Pedidos: revisar todas as armas para naturalidade das mãos; durante a revisão,
usuário informou que os braços ainda estavam muito compridos.
A referência continua Usado v2. Nenhum novo acabamento foi criado e nenhuma
aceitação artística é inferida de testes ou capturas. O usuário aprovou
movimentação e visuais por enquanto após testar o jogo; torso/cinto continuam
fora de escopo.

## O que foi corrigido

Antes, os contatos eram origens de punho: mudar orientação/espessura da
palma também deslocava a superfície que deveria tocar a peça. Agora são
pontos explícitos na superfície da arma, pente ou mecanismo. O punho é
resolvido ao redor desse contato. Para cartuchos, o ponto pertence à ponta
articulada do indicador, não ao centro da palma.

| Arma | Revisão de empunhadura/apoio e recarga |
| --- | --- |
| Pistola | Contatos nas laterais do grip; polegar menos erguido e dedos menos fechados. Esquerda prende o pente e alcança o topo do slide antes de puxá-lo; solta antes do retorno por mola. |
| Carabina | Apoio sob o guarda-mão, mais perto do receiver; grip direito preservado. Contatos recalibrados no pente e na alavanca lateral de manejo, em vez da pose genérica perto da coronha. |
| SMG | Usa a mesma geometria provisória aprovada da carabina, com contatos adequados a ela. Mantém suas janelas próprias de inserção/manejo/liberação. Não é anunciado um modelo novo. |
| Sniper | Esquerda apoia por baixo e troca o pente; direita opera o knob do ferrolho, acompanhando sua rotação e translação. Apoio retorna antes do fechamento. |
| Shotgun | Apoio sob o pump. Cartucho entre indicador e polegar; desaparece ao entrar no tubo, antes da busca do próximo. Mão acompanha todo o retorno manual do pump. |
| Faca | Apenas a direita é desenhada. Base própria da palma alinha os quatro dedos ao longo do cabo, sem equilibrar a faca em duas pontas de dedos. Mantém contato durante o arco de ataque. |

## Comprimento dos braços

O feedback adicional confirmou que a manga de apoio ainda atravessava a tela
em uma diagonal longa. Além de encurtar os segmentos, a origem corporal e o
polo dos cotovelos foram aproximados do centro inferior da apresentação.
O apoio dos rifles foi trazido para a região proximal do guarda-mão, e o da
shotgun permanece dentro da geometria do pump.

- Braço superior: 0,72 → **0,56** no espaço local da arma (−22%).
- Antebraço: 0,62 → **0,46** (−26%). Total: 1,34 → **1,02** (−24%).
- Manga, bind pose, bounds e escala vertical do UV acompanham o rig menor;
  não é só uma redução visual ou um deslocamento que solta a mão da arma.
- Palma/dedos, modelos das armas e projeção próxima de 68° não foram
  redimensionados para compensar. Dobra do punho continua limitada a 30°,
  com margem numérica de 1°; roll acompanha a palma e os segmentos mantêm
  seus comprimentos durante IK. Raízes das mangas continuam fora do quadro.

[Comparação nativa antes/depois](../visual/validation/player_hands_v2_review_arm_comparison.png).

## Contratos e custo

Sem alterações em munição, tempo efetivo de recarga, dano, troca, pausa,
movimento/bhop, saves ou cérebros ativos. Somente o relógio **visual** do
slide e a visibilidade do cartucho foram coordenados com as mãos. Alcance
do primeiro cartucho e transição para pump usam segundos normalizados pela
duração planejada, inclusive nas guardas de 1/6/32 cartuchos.

Permanece o rig GDP de 36 juntas e o passe próximo compartilhado de arma/mãos,
independente do FOV do mundo. Três draws: 4016 triângulos no par, 2008 na faca.
Mesmo orçamento/mapas da correção anterior, sem nova textura/luz/sombra ou
API da engine. Não foi feito benchmark comparativo de desempenho.
Modificações paralelas de IA foram preservadas, sem staging de seus arquivos.

## Validação técnica

- CPU: contatos comparados com triângulos reais do grip, apoio, pente,
  mecanismo e cartucho, além dos gates anteriores de IK, determinismo,
  continuidade em 1000 fases, endpoints e segurança numérica. Pinça verifica
  indicador articulado e superfície do polegar. Mira parcial também entra
  no gate de punho; nenhuma tolerância anterior foi ampliada.
- OpenGL: seis papéis; MEDIUM/POTATO, FOV 60/100, três proporções/resoluções,
  recarga, recuo, troca, agachar/ar, mira parcial/full scope e quatro fases do
  ataque da faca. Fixtures finais passaram na Quadro K2100M/NVIDIA 470.256.02
  e no Mesa llvmpipe/LLVM 21.1.8/Mesa 26.2.3.
- Release completa final: **65/65**, 50,26 s, incluindo aplicação/recarga/pausa
  e os testes de persistência, sem novos avisos de compilação.
- ASan/UBSan final: CPU passou em **8,48 s**; nove outros gates afetados
  passaram em **57,84 s**. Dez gates no total, sem diagnósticos. Preset mantém
  `detect_leaks=0` conforme restrição de ptrace; não é a suíte ASan inteira.

O teste CPU inicialmente excedeu os 10 s ao procurar contato do polegar em
todos os triângulos para cada vértice. Uma broad-phase por bounds elimina
somente pontos que matematicamente não podem passar o mesmo gate de 0,012;
a aceitação continua sendo pela distância à superfície triangular real,
com parada ao encontrar um contato válido. O timeout não foi aumentado.
Antes da última proporção, a integração ASan da Application passou em 180,59 s;
esse resultado histórico não substitui os gates finais acima.

Uma rodada Release teve timeout isolado de persistência de placar. Sem
alterar código ou timeout desse sistema, a reexecução passou em 0,01 s e o
checkpoint adjacente em 0,16 s. A causa do atraso não foi estabelecida.
Não se trata de uma correção de persistência implementada nesta unidade.
`git diff --check` passou na revisão original. A aprovação posterior autoriza
publicar este candidato; não houve validação ou empacotamento Windows específico
desta correção.

## Evidências finais nativas

[Comparação das seis armas](../visual/validation/player_hands_v2_review_all.png).
Capturas integrais de repouso: [pistola](../visual/validation/player_hands_v2_review_pistol.png),
[carabina](../visual/validation/player_hands_v2_review_carbine.png),
[SMG](../visual/validation/player_hands_v2_review_smg.png),
[sniper](../visual/validation/player_hands_v2_review_sniper.png),
[shotgun](../visual/validation/player_hands_v2_review_shotgun.png),
[faca](../visual/validation/player_hands_v2_review_knife.png).

Operações: [slide](../visual/validation/player_hands_v2_review_slide.png),
[alavanca](../visual/validation/player_hands_v2_review_charging_handle.png),
[ferrolho](../visual/validation/player_hands_v2_review_bolt.png),
[cartucho](../visual/validation/player_hands_v2_review_shell.png),
[ataque da faca](../visual/validation/player_hands_v2_review_knife_attack.png).
Recargas de 31 poses: [pistola](../visual/validation/player_hands_v2_review_pistol_reload.gif),
[carabina](../visual/validation/player_hands_v2_review_carbine_reload.gif),
[SMG](../visual/validation/player_hands_v2_review_smg_reload.gif),
[sniper](../visual/validation/player_hands_v2_review_sniper_reload.gif),
[shotgun](../visual/validation/player_hands_v2_review_shotgun_reload.gif).

Fontes: `build/visual-validation/weapon-hand-review-short-arms-verified-20261004`
e `build/visual-validation/weapon-hand-review-short-arms-verified-reloads-20261004`.
São fixtures offscreen do renderer real, **não um playtest manual** ou um
novo conceito ilustrativo. PNGs/GIFs sem retoque; montagem apenas recorta,
reduz e rotula. GIFs aproximam o tempo e não gravam o relógio da Application.
Capturas anteriores em `build/` e `player_hands_v2_correction_*` são histórico,
não a evidência do rig mais curto. Geometria/material continuam procedurais,
sem promessa de reproduzir o detalhe da prancha aprovada ou de ausência de
qualquer interpenetração em todos os estados possíveis.

## Teste no jogo

Feche a instância antiga e execute:

```bash
build/release/gdp --test-mode
```

Verifique as cinco recargas e a faca, sobretudo comprimento/posição dos braços,
contato no cabo, pinça do cartucho, slide e ferrolho. Teste também recuo,
Ctrl/Space, Q/scroll, pausa e FOV 60/100. A aceitação atual é provisória:
naturalidade e proporções ainda podem receber refinamento posterior.

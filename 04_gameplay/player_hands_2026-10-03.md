# Player Usado — mãos v2 e contatos de recarga

Data: 2026-10-03. Estado: **IMPLEMENTADA EM VALIDAÇÃO**.
Unidade: `Player presentation/Hands + Reload contacts`.
Usuário aprovou a [proposta Usado v2](../visual/proposals/player_hands_proposal_v2.md)
com "visual simulado aprovado para implementação". A aprovação do conceito
autoriza esta implementação; **aceitação visual final no jogo permanece pendente**.
O [candidato v1](player_hands_2026-10-02.md) é histórico, não a implementação atual.
Torso/cinto v2 continua sendo outra unidade, aguardando aprovação e não implementada.

## Anatomia e materiais

- Rig GDP de 36 juntas: braço superior, antebraço, palma e três falanges
  para cada um dos cinco dedos, por lado. IK de dois segmentos preserva
  comprimentos; ombros ficam abaixo do enquadramento próximo.
- Palma arqueada, base do polegar definida, dedos de comprimentos distintos,
  afunilamento e abertura discreta. Pesos de duas juntas suavizam punhos,
  cotovelos e falanges. São superfícies procedurais articuladas, não captura
  de movimento nem um modelo cinematográfico importado.
- Tecido no dorso/mangas, couro fosco nas superfícies de contato e reforço
  flexível discreto. Mapas próprios determinísticos: tecido/couro 512²,
  reforço 256²; albedo, normal e propriedades para cada material. Costuras,
  trama, desgaste leve e vincos são estáticos. Todos são **não metálicos**.
- Posição, orientação e flexão individual dos dedos dependem do contato:
  empunhadura, suporte, pente, busca, mecanismo, cartucho ou prontidão.
  Abrir a mão ao soltar/buscar não usa a mesma pose de segurar um pente.

## Recarga por arma

| Arma | Contatos implementados |
| --- | --- |
| Pistola | Direita conserva empunhadura; esquerda extrai/solta/busca/insere o pente, pega o slide por cima e retorna ao suporte. |
| Carabina | Direita na empunhadura; esquerda sai do apoio dianteiro, troca o pente e alcança o mecanismo com orientação própria. |
| SMG | Arte provisória da carabina preservada, mas entrada visual do pente termina aos 71%, antes da janela própria do mecanismo; tempo lógico não muda. |
| Sniper | Esquerda apoia a arma enquanto direita alcança e opera o ferrolho lateral. Direita retorna à empunhadura antes da troca de pente pela esquerda; esquerda volta ao apoio antes de fechar o ferrolho. |
| Escopeta | Direita estabiliza; esquerda usa pinça de polegar/indicador por cartucho planejado, demais dedos recolhidos, depois alcança o pump. Não busca um cartucho fictício após a última inserção. |
| Faca | Empunhadura/prontidão e ataque existentes; não há recarga. |

Na sniper, as janelas da direita são 1,5–26% e 80–100%, com trechos de
aproximação/retorno. O contato acompanha deslocamento **e rotação em torno
do pivô real** do ferrolho. O movimento cosmético da peça começa depois de
a mão alcançá-la. A regra antiga de nunca tirar a direita da empunhadura
foi substituída por verificações explícitas dessas janelas, não apagada.

O pente descartado cai independentemente após a ejeção. A mão abre e busca
outro; não segue o velho até o chão. A entrada acompanha a mesma instância
visual do novo pente. Os perfis têm transições contínuas de posição, orientação
e flexão; progresso 0/1 e cancelamento retornam exatamente ao repouso.
As curvas cosméticas do ferrolho/sniper e da entrada/SMG foram sincronizadas
com os contatos; não mudaram os relógios ou as regras de munição.

## Autoridade e enquadramento preservados

`Weapon` continua sendo a única autoridade de recarga: pistola 0,82 s,
carabina 1,05 s, SMG 1,40 s, sniper 2,00 s; escopeta 0,35 s por cartucho
mais 0,30 s para finalizar. Ejeção do pente aos 32%; novo pente gasto somente
ao concluir. Cartuchos entram progressivamente na lógica existente.
Não há relógio visual separado nem concessão de munição pela animação.

Arma e mãos compartilham root de recoil, troca, postura e mira, no passe
próximo de **68° fixos**. Metade da escala e da translação produz espaço
próximo equivalente em perspectiva, permitindo alcance sem ampliar a arma.
A empunhadura foi elevada discretamente para não ficar cortada abaixo da tela.
FOV do mundo 60–100 não altera suas proporções. Ombros são ancorados no
espaço da câmera, com IK resolvendo o alcance dos contatos locais.

Mira completa da sniper oculta o conjunto próximo; título/resultados e arma
invisível não deixam mãos soltas. O gate efetivo de mira alimenta arma e mãos,
inclusive nos estados de interface. Corpo inferior permanece no passe do
mundo, com profundidade/oclusão próprias e sem braços duplicados.

Nada muda câmera lógica, origem/direção do tiro, dano/cadência, colisões,
movimento/bhop, inventário, reservas, persistência, geração, IA ou seus ativos.
Não foram promovidos ou substituídos cérebros do jogo nesta entrega; somente
os smokes isolados de treinamento já presentes na suíte foram executados.
Alterações paralelas de IA no checkout foram preservadas e ficam fora do commit.

## Orçamento e limites

**Três draws, 4016 triângulos e 36 matrizes**, contra 2264 triângulos no
candidato v1. Mapas adicionais: **6,75 MiB** RGBA8 base; aproximadamente
**9 MiB** com mipmaps completos, sem contar buffers/overhead do driver.
O corpo inferior conserva seus sete ossos e sua geometria anterior.

Malhas/mapas são criados/enviados na inicialização; GPU deforma os vértices.
Remapeamento compacto reutiliza vértices dentro de cada material. Uniforms
têm nomes fixos; não se constroem strings/malhas/texturas por frame.
Codificação de juntas/UV é do GDP, nos atributos existentes; Vertex/Mesh
e demais APIs da WKC Forge não foram alterados. Skinning volta ao modo normal
antes de flash e outros desenhos. Rasterização próxima é dupla face, como
as armas existentes, por causa da base de câmera refletida.

Não adiciona luzes, sombras, framebuffer, cena ou simulação física.
POTATO conserva a silhueta. O orçamento geométrico e de mapas foi medido;
**não há benchmark comparativo representativo de gameplay nem promessa de FPS**.
O conceito orienta a arte, mas não é o mesh/textura final nem uma screenshot.

## Evidências nativas

Fixture OpenGL offscreen, conversão BMP → PNG/GIF sem retoque. Não é playtest
manual. GIFs têm 31 poses normalizadas por arma, com duração de exibição
aproximada/arredondada; não são uma gravação do relógio real da Application.

- [Pistola em repouso](../visual/validation/player_hands_v2_pistol.png).
- [Carabina em repouso](../visual/validation/player_hands_v2_carbine.png).
- [Entrada do pente](../visual/validation/player_hands_v2_magazine.png).
- [Slide da pistola](../visual/validation/player_hands_v2_slide.png).
- [Ferrolho da sniper](../visual/validation/player_hands_v2_bolt.png).
- [Cartucho da escopeta](../visual/validation/player_hands_v2_shell.png).
- [Faca](../visual/validation/player_hands_v2_knife.png).
- [POTATO/16:10](../visual/validation/player_hands_v2_potato.png).
- Recargas: [pistola](../visual/validation/player_hands_v2_pistol_reload.gif),
  [carabina](../visual/validation/player_hands_v2_carbine_reload.gif),
  [SMG](../visual/validation/player_hands_v2_smg_reload.gif),
  [sniper](../visual/validation/player_hands_v2_sniper_reload.gif),
  [escopeta](../visual/validation/player_hands_v2_shotgun_reload.gif).

## Validação técnica

CPU: índices/pesos/juntas, deformação finita/determinística, IK, seis papéis,
perfis dos dedos, rotações ortonormais, transições de posição/orientação,
endpoints/NaN e orçamento exato. Probes da palma verificam proximidade com
o AABB do pente novo e o puxador transformado do ferrolho; isso não prova
ausência de toda interpenetração dos dedos nem anatomia perfeita.

OpenGL: três draws/4016 triângulos reais, determinismo, silhueta, fases de
recarga, retorno/cancelamento, recoil/agachar/ar/troca/faca, mira da sniper,
exclusão no título/resultados e centro 16×16 livre em repouso. MEDIUM/POTATO,
FOV 60/100, 16:9/16:10/compacta/ultrawide. Integração real: arma dispara,
recarga segue `Weapon`, pausa não avança fase/gasta reserva, conclusão restaura pose.

- Builds Release/instrumentada concluídas, sem novos avisos.
- Release completa: **65/65**, 56,77 s na repetição final após os probes
  adicionais de contato (primeira rodada final: 65/65, 67,80 s).
- ASan/UBSan: dez gates focados aprovados em 61,47 s. CPU repetida depois
  dos probes adicionais: 8,51 s, dentro do timeout original de 10 s.
  Integração completa `application_menu_integration`: passou em 161,66 s.
  Assim, onze gates afetados instrumentados aprovados, sem diagnósticos.
  `detect_leaks=0` é a configuração do preset sob restrição de ptrace.
- Fixture das mãos passou na Quadro K2100M/NVIDIA 470.256.02 e no Mesa
  llvmpipe (LLVM 21.1.8, Mesa 26.2.3), não somente em um backend.
- Não houve validação nativa Windows local desta v2; pacote depende do CI
  da publicação. Resultados anteriores do Windows não validam estas mãos.

Capturas/testes não concedem aceitação artística. Contato, proporção,
naturalidade e conforto visual precisam do teste humano antes de marcar ACEITA.

## Teste manual e recaptura

```bash
cmake --build --preset release --parallel 2
build/release/gdp --test-mode
```

Dispare e pressione R para cada arma. Confira particularmente o ferrolho
da sniper e cada cartucho da escopeta; teste pausa/retomada e cancelamento
por troca/G, sem recuperar munição perdida. Teste 1/2/3, Q, scroll, Ctrl/Space,
FOV 60/100, POTATO/MEDIUM e Mouse2 da sniper. A faca continua não dropável.

```bash
GDP_HANDS_CAPTURE_DIR=build/visual-validation/player-hands-v2-final \
SDL_VIDEODRIVER=offscreen build/release/gdp_renderer_gl_tests --player-hands-only
GDP_RELOAD_CAPTURE_DIR=build/visual-validation/player-hands-v2-reloads \
SDL_VIDEODRIVER=offscreen build/release/gdp_renderer_gl_tests --reload-only
```

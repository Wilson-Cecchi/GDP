# Mãos Usado v2 — correção local após rejeição do playtest

Data: 2026-10-04. Estado: **CANDIDATO LOCAL EM REVISÃO VISUAL**.
Esta página registra a primeira correção local. A revisão posterior de todas
as armas e o encurtamento solicitado dos braços estão em
[contatos por arma — estado atual](player_weapon_contacts_2026-10-04.md).
Os comprimentos/capturas/resultados abaixo são históricos dessa primeira
correção, não os do candidato mais recente.
Referência permanece a [prancha Usado v2 aprovada](../visual/proposals/player_hands_proposal_v2.png).
Não houve aprovação artística do resultado publicado em `88c5f87`.
Esta correção não é uma nova identidade visual, nem autoriza torso/cinto.
Esta correção intermediária não será publicada isoladamente. A revisão posterior
em `player_weapon_contacts_2026-10-04.md` recebeu aprovação provisória em
2026-10-05.

## Feedback e causa

Usuário: mãos finas e longas, proporção diferente da prancha em relação às
armas, faca usando duas mãos e punhos parecendo quebrar durante animações.

A revisão publicada tinha segmentos de braço igualmente longos (0,90/0,90
no espaço local), palma estreita/dedos compridos e manga muito afunilada.
Além disso, o IK do cotovelo e a orientação da palma eram independentes:
matrizes finitas e ortonormais não impediam uma dobra excessiva do punho.
A faca desenhava a mão esquerda em prontidão; isso não atende ao pedido de
usá-la com uma mão. Esses problemas **não são invalidados pelos testes antigos**.

## Correção implementada localmente

- Braço superior/antebraço com comprimentos locais 0,72/0,62. Palmas mais
  largas e menos alongadas; dedos mais curtos/encorpados, polegar proporcional.
  Perfil arredondado mais plano no dorso/palma, não um tubo elíptico estreito.
  Mangas/punhos recebem volume coerente, sem ampliar a arma ou mudar seu FOV.
- Contato é um ponto da superfície interna da palma, não só a translação
  do punho. A orientação pode ajustar-se para o alcance, mas o contato
  permanece na peça ao operar o pente/ferrolho/mecanismo.
- Solver conserva os dois comprimentos, usa polo corporal estável para não
  inverter o plano do cotovelo e mantém a raiz da manga abaixo da borda.
  Limita a dobra do punho a 30°; validação permite somente 1° de margem
  numérica do solver iterativo. Roll do antebraço acompanha o eixo transversal
  da palma, evitando torção independente no punho.
- Acoplamento entre contato/IK usa até 16 iterações determinísticas com
  relaxação, sem relógio extra, RNG ou alocação por frame. Ajusta rotação
  em torno do contato; não empurra a mão para longe da peça para passar um teste.
- Faca seleciona três malhas estáticas **somente da direita**, sem vértices
  da esquerda e sem segunda mão durante repouso ou ataque. Outras armas
  continuam usando o par, inclusive a esquerda no apoio da sniper.

Uma primeira tentativa de alinhar rigidamente o antebraço à palma foi
descartada: capturas mostraram uma manga grande passando perto da câmera.
A tentativa seguinte revelou inversão do plano do cotovelo em seis pontos.
O gate de continuidade não foi enfraquecido; o polo foi estabilizado.
O teste ampliado encontrou mais quatro casos de dobra excessiva ao combinar
recarga com um arco de ataque extremo; relaxação do solver corrigiu esses casos.
Capturas dessas tentativas em `build/` não são evidência final entregue.

## Contratos preservados e orçamento

Arma/mãos permanecem no passe próximo de **68° fixos** e compartilham root
de recuo/troca/postura/mira. Mundo, corpo inferior e câmera lógica não mudam.
`Weapon` continua dono de tempo, munição, ejeção, cancelamento e pausa;
nenhum tempo, cartucho, pente, dano ou save foi alterado para ajustar a pose.
Nada muda movimento/bhop, disparo, mapas, IA ou ativos de cérebros.
Trabalho paralelo de IA foi preservado, sem staging/commit/push.

Três draws: **4016 triângulos** nas armas de fogo, **2008 na faca**.
A variante direita é enviada uma vez na inicialização; soma 2008 triângulos
residentes ao par anterior, mas não duplica desenhos por frame. Reutiliza
os mesmos três materiais/mapas de mãos v2, sem novas texturas/luzes/sombras.
Rig continua GDP/36 juntas; nenhuma API ou conceito novo na WKC Forge.
Não há benchmark comparativo de gameplay ou promessa de desempenho.

## Validação e evidências

Testes novos cobrem malha direita sem índices da esquerda, proporção medida
da palma, comprimentos dos braços, raiz fora da tela, contato preservado,
limite/roll do punho, 1000 amostras de recarga e recuo/troca/agachar/ar/ataque.
Gates antigos de geometria, continuidade, contato nas peças, endpoints,
FOV, mira da sniper e exclusão de menu/resultados permanecem.

- Build Release e instrumentada: concluídas sem novos avisos.
- Release completa: **65/65**, 60,14 s, incluindo aplicação/recarga/pausa.
- ASan/UBSan: **10/10** gates focados, 61,72 s; CPU 9,01 s, dentro do
  timeout original de 10 s, sem diagnósticos. Integração completa da Application
  instrumentada passou em **170,57 s**. Total: onze gates afetados aprovados.
  Preset mantém `detect_leaks=0` conforme restrição de ptrace.
- Fixture das mãos: passou na Quadro K2100M/NVIDIA 470.256.02 e no Mesa
  llvmpipe (LLVM 21.1.8, Mesa 26.2.3).
- Assertions de continuidade e timeouts antigos não foram relaxados.
  O teste de âncora dominante passou a verificar a **superfície de contato da
  palma**, preservada ao ajustar punho/IK, em vez de congelar o punho em uma
  posição que podia produzir uma dobra impossível.
- `git diff --check`: sem erros. Não há nova validação ou pacote Windows
  desta correção local, nem commit/push dela.

Evidências verificadas (sem sobrescrever as imagens da implementação rejeitada):

- [Pistola em repouso](../visual/validation/player_hands_v2_correction_pistol.png).
- [Faca — uma mão](../visual/validation/player_hands_v2_correction_knife.png).
- [Entrada do pente](../visual/validation/player_hands_v2_correction_magazine.png).
- [Slide](../visual/validation/player_hands_v2_correction_slide.png).
- [Ferrolho](../visual/validation/player_hands_v2_correction_bolt.png).
- [Cartucho](../visual/validation/player_hands_v2_correction_shell.png).
- [POTATO/16:10](../visual/validation/player_hands_v2_correction_potato.png).
- Recargas: [pistola](../visual/validation/player_hands_v2_correction_pistol_reload.gif),
  [carabina](../visual/validation/player_hands_v2_correction_carbine_reload.gif),
  [SMG](../visual/validation/player_hands_v2_correction_smg_reload.gif),
  [sniper](../visual/validation/player_hands_v2_correction_sniper_reload.gif),
  [escopeta](../visual/validation/player_hands_v2_correction_shotgun_reload.gif).

São fixtures OpenGL offscreen, **não playtest manual**. BMP → PNG/GIF sem
retoque, 31 poses normalizadas por GIF e duração de exibição aproximada;
não são uma gravação do relógio da Application. Uma captura bonita
isolada não prova que toda a animação está correta, e passar a suíte não
prova fidelidade artística à prancha. O candidato ainda usa a geometria/material
procedural do jogo, não o modelo ilustrativo da imagem; não deve ser anunciado
como reprodução equivalente do nível de detalhe daquela prancha.
O usuário decide naturalidade/proporção a partir das capturas reais e do jogo.

## Teste local

```bash
build/release/gdp --test-mode
```

Teste faca (repouso/ataque), disparo/R nas cinco armas, pausa/retomada,
troca/Q/scroll/G, Ctrl/Space e FOV 60/100, MEDIUM/POTATO. Observe sobretudo
o punho ao pegar o slide, trocar o pente e operar o ferrolho da sniper.
Este registro é histórico da primeira correção; para o estado aprovado
provisoriamente, consulte a revisão posterior citada acima.

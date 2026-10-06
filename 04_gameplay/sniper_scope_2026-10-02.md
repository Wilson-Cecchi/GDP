# Sniper — luneta Used v2 e visão de mira v1

Data: 2026-10-02. Estado: **IMPLEMENTADA EM VALIDAÇÃO**.

## Aprovação e escopo

O usuário confirmou as duas funcionalidades e aprovou a
[prancha v2](../visual/proposals/sniper_scope_proposal_v2.png):
"aprovado pode implementar". A aprovação inclui o círculo inteiro na build,
explicitado na apresentação como diferença do corte do painel ilustrativo.
Registro: `visual/TEXTURE_APPROVALS.md`.

Somente a luneta da sniper foi remodelada: tubo, objetiva cônica, ocular,
anéis, fixadores e ajustes mais legíveis, mantendo o restante da arma v1 e
reutilizando os materiais Used existentes, não emissivos. A mesma malha é
usada na mão e no chão. Mantido/Gasto não foram implementados. O player Usado
segue aprovado, mas corpo, rig e mãos não fazem parte desta implementação.

## Estado e ownership

`Application` lê a ação remapeável `Aim` (Mouse2 por padrão) e avança o blend
no fixed update por `advanceWeaponAimBlend`. A câmera mantém escala de FOV
0,35 e transição de 0,35 s da sniper. Dano, alcance, spread, munição, recarga,
cadência, hitscan, geração de mundo e cérebros não foram rebalanceados.

Na sniper, recarga/equip em andamento bloqueiam a mira. Recarga aceita,
troca, drop, morte, fim da sessão ou pausa limpam seu estado; uma tentativa
de recarga rejeitada com pente cheio não o limpa. Ao continuar segurando Aim
depois da recarga/equip, a entrada reinicia normalmente. Soltar Aim desfaz
o zoom com a duração existente. Reinício/subida/main menu usam os resets já
existentes. Pausa não avança o relógio de gameplay.

`WeaponViewmodelRenderData::aimStrength` é um snapshot de apresentação,
sem input ou dano no renderer. `sniperScopeStrength` restringe o efeito à
sniper visível, sanitiza valores não finitos e suprime equip/recarga. O
renderer bloqueia adicionalmente todos os menus e os estados Victory/Defeat.

## Renderização

- Mesmo passe de mundo/câmera: não há câmera duplicada nem picture-in-picture.
- Luneta 3D reutiliza os materiais e os buffers estáticos das armas.
- Arma desce na entrada e não é desenhada com força de mira >= 0,95,
  evitando obstruir a abertura na mira completa.
- Uma quad de UI cobre a tela depois do mundo e antes do HUD. O shader
  calcula máscara, borda e retículo em pixels, não com uma textura de cenário.
- Raio = 44% da menor dimensão; centro = metade da largura/altura. Assim o
  círculo cabe inteiro em retrato, ultrawide, 16:9 e 16:10 e não vira elipse.
- Fora quase preto, borda suave, cruz fina escura e ponto central claro para
  contraste. O centro corresponde ao da retícula/disparo existentes.
- Na sniper, a retícula comum não é desenhada, com ou sem overlay. O retículo
  óptico aparece durante a mira com Mouse2, sem apagar HP/munição/mapa.
- Overlay permanece na resolução de saída mesmo com mundo a 50% no POTATO.

Tudo permanece no GDP; nenhum conceito Sniper foi adicionado à WKC Forge.
Há uma quad adicional de UI, não uma segunda cena. Não houve benchmark
comparativo de custo do overlay: não se afirma ganho de FPS nem custo zero.

## Evidências

Capturas da fixture OpenGL offscreen, não de playtest manual:

- [Luneta na mão](../visual/validation/sniper_optic_v2_gameplay.png).
- [Modelo no chão](../visual/validation/sniper_optic_v2_ground.png).
- [Mira MEDIUM 1280×720](../visual/validation/sniper_scope_v2_medium_1280x720.png).
- [Mira MEDIUM 960×600](../visual/validation/sniper_scope_v2_medium_960x600.png).
- [Mira MEDIUM 320×200](../visual/validation/sniper_scope_v2_medium_320x200.png).
- [Mira POTATO 1280×720](../visual/validation/sniper_scope_v2_potato_1280x720.png).

Seed de apresentação 1717; contatos de chão cobrem seeds 123/128 e os dois
lados de repouso. As capturas preservam os pixels do renderer, convertidos
de BMP para PNG sem retoque. Câmera da fixture da mira deslocada para x=1,8
para não mirar no obstáculo central; não altera a câmera do jogo.

## Testes adicionados

- CPU: blend, relógios inválidos, bloqueio e parâmetros de zoom; força visual
  da mira, supressão por papel, reload/equip/visibilidade e NaN/Inf.
- Geometria: gates existentes de orçamento (<1600 triângulos), grip,
  validação de malhas, identidade e contato da sniper com o piso preservados.
- OpenGL: pixels da máscara em ambos os eixos, interior idêntico ao mundo,
  retículo central sem cruz comum, ausência da arma na mira completa e
  contagem de geometria do mundo sem passe extra; FOV 60/100, MEDIUM/POTATO,
  retrato, ultrawide, 16:9/16:10; meio blend, reload/equip, menus e resultados,
  outras armas e ida/volta sem estado gráfico residual.
- Aplicação real offscreen: Mouse2, Aim remapeado para T, soltura, reload
  recusado/aceito, pausa, troca e drop na arena.

Atalho da fixture: `SDL_VIDEODRIVER=offscreen build/release/gdp_renderer_gl_tests --scope-only`.
Capturas adicionais: definir `GDP_SNIPER_SCOPE_CAPTURE_DIR` e/ou
`GDP_NEW_WEAPON_CAPTURE_DIR` para uma pasta de build, nunca para sobrescrever
capturas aprovadas de fases anteriores.

Resultados em 2026-10-02:

- Build Release e ASan/UBSan concluídos sem erro.
- `ctest --preset release --parallel 2 --output-on-failure`: **64/64**, 49,45 s.
- ASan/UBSan: dez regressões focadas (CPU/GL, combate, loot, progressão,
  campanha/save, speedrun e fronteiras arquiteturais): **10/10**, 50,98 s.
- Integração da aplicação ASan/UBSan: **1/1**, 227,05 s, sem diagnóstico
  ASan/UBSan. A tentativa anterior terminou com código 143 (interrupção,
  sem diagnóstico); a repetição isolada é a execução concluída válida.
- Fixture `--scope-only`: passou na NVIDIA Quadro K2100M 470.256.02 e no
  Mesa llvmpipe 26.2.3 (LLVM 21.1.8), com os dois perfis e FOVs. Capturas
  realizadas na NVIDIA. A primeira tentativa de forçar software ainda
  selecionou NVIDIA; somente a execução com seleção explícita do fornecedor
  EGL Mesa conta como verificação llvmpipe.
- `git diff --check`: sem erro de whitespace.

Os testes foram executados no workspace compartilhado; as alterações de
treinamento já presentes do outro fluxo não foram revertidas nem incluídas
no commit desta etapa. A CI do commit publicado é uma verificação separada.

## Playtest humano pendente

1. Executar `build/release/gdp --test-mode`, largar a primária com G e pegar
   a sniper da arena. Selecionar a primária com 1.
2. Segurar Mouse2, atirar e soltar; conferir alvo no centro e ausência de arma
   bloqueando a mira completa.
3. Recarregar com R, alternar por 2/3/Q/scroll, pausar com Esc e largar com G.
   Conferir retorno limpo, sem retículo circular preso em outra arma/menu.
4. Comparar FOV 60/100 e POTATO/MEDIUM. Julgar legibilidade e tamanho da luneta.

Aceitação visual no jogo cabe ao usuário, não aos testes automatizados.

## Ajuste solicitado — 2026-10-04

Pedido: remover a mira central da sniper quando não estiver usando Mouse2.
O renderer agora suprime a retícula comum para a sniper selecionada; usa o
papel do viewmodel visível e, na ausência dele, o snapshot do HUD. A luneta 3D
e o retículo óptico ao mirar permanecem. Não altera zoom/blend, input
remapeável Aim, spread, dano ou comportamento das outras armas.

Regressão GL compara os 16×16 pixels centrais do hip fire com o mesmo mundo
sem retícula e confirma que pistola, carabina, SMG, shotgun e faca mantêm a
retícula anterior. Os gates de entrada/saída da mira, menus, reload/equip,
FOV 60/100 e MEDIUM/POTATO continuam passando.

- Release: cinco gates afetados (GL completo, mãos, recarga, integração da
  Application e fronteiras arquiteturais), **5/5**, 18,33 s.
- ASan/UBSan: GL completo **1/1**, 18,43 s, sem diagnósticos; preset com
  `detect_leaks=0` pela restrição de ptrace. Não é uma nova suíte completa.
- Fixture `--scope-only`: passou na NVIDIA Quadro K2100M/470.256.02 e no
  Mesa llvmpipe/LLVM 21.1.8/Mesa 26.2.3.
- `git diff --check` passou. Ajuste local, junto ao candidato de apresentação
  das mãos; sem novo commit/push ou pacote Windows nesta revisão.

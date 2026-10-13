# Mão da granada: correção da orientação

## Defeito reproduzido

Na captura offscreen da fase `grenade_release` em FOV 60/MEDIUM, a mão direita abria com a palma voltada para cima e o punho aparentava torção. A pose da granada ainda usava a base da faca, que orienta a palma para cima; a soltura acrescentava uma rotação local de -70°.

## Ajuste

`PlayerHandsAssets.cpp` aplica à granada a base da mão dominante usada na empunhadura lateral, mantendo a base anterior para a faca. A pose aberta de soltura deixa de acrescentar a rotação local. Os pontos de contato da mão com o corpo da granada e do pino com a mão de apoio permanecem definidos pelo mesmo `sampleGrenadeViewmodelPose`.

`test_renderer.cpp` verifica a direção do dorso da luva na pose pronta e na soltura, a continuidade de orientação durante 100 amostras da soltura e os limites existentes de punho/contato.

## Evidência e limites

Capturas locais, ignoradas pelo Git: `build/validation/grenade_hand_2026-10-13/{before,after}/grenade_{ready,pin_reach,pin_pulled,windup,release}_fov{60,100}_1280x720_{medium,potato}.png` (20 imagens em cada grupo). Comparações em `build/validation/grenade_hand_2026-10-13/grenade_hand_2026-10-12_comparison_fov60_medium.png` e `..._fov100_potato.png`; linha superior antes, linha inferior depois, colunas pronta, alcance do pino, pino puxado, carga e soltura. A captura depois mostra o dorso da mão direita voltado para fora e o braço sem a torção visível na soltura.

Validação técnica: `cmake --build build/debug --target gdp_renderer_tests gdp_renderer_gl_tests -j2` passou; `build/debug/gdp_renderer_tests` passou; `ctest --test-dir build/debug -R '^renderer_cpu_systems$' --output-on-failure` passou (1/1); `SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy GDP_HANDS_CAPTURE_DIR=build/validation/grenade_hand_2026-10-13/after build/debug/gdp_renderer_gl_tests --player-hands-only` passou, com NVIDIA Quadro K2100M/OpenGL 3.3; `git diff --check` passou. A inspeção foi offscreen; a aprovação visual final exige observação em uma partida nativa.

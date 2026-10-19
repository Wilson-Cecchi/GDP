# Ataques dos modelos de referência: capturas técnicas

Capturas de cena fixa em `MEDIUM`, 1280×720, na NVIDIA Quadro K2100M.
O frontal aproximado e a vista oblíqua usam FOV 48°; as laterais usam 68°.
Arquivos `*_attack_front.png` mostram o quadro de disparo ou golpe com pulso
visual 1; `robot_melee_front.png` mostra a garra do Boss. Elas verificam
carregamento, orientação aproximada das armas e o braço que se move. Não são
capturas de uma partida e não constituem aceitação visual em jogo.

O Guard também tem `guard_attack_left.png`, `guard_attack_right.png` e
`guard_attack_oblique_right.png` para verificar que o rifle fica na altura do
ombro direito e aponta para a frente. O antebraço direito coloca a mão na
empunhadura, o esquerdo apoia a frente do receptor, e o ponto de disparo
acompanha o cano nessa pose.

Regenerar BMP e converter os nove quadros de ataque:

```bash
cmake --build --preset release --target gdp_renderer_gl_tests --parallel 4
GDP_ENEMY_REFERENCE_MODELS=1 \
GDP_ENEMY_REFERENCE_CAPTURE_DIR=/tmp/iron_seed_attack_capture \
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
./build/release/gdp_renderer_gl_tests --enemy-reference-capture
```

A geometria dos cinco modelos ainda é candidata. O teste manual precisa
conferir, em movimento, se tiros começam no cano, se as mãos acompanham o
armamento, se o golpe próximo parece conectar e se a cabeça/sensor do Boss
recebe tiros onde aparece, com sniper, escopeta e yaw variados.

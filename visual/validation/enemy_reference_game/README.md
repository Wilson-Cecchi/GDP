# Capturas técnicas dos modelos reconstruídos

As capturas locais desta pasta são de um teste OpenGL com cenário fixo. Foram obtidas
na NVIDIA Quadro K2100M, em `MEDIUM` (frente, esquerda, costas, direita,
mais uma vista frontal de detalhe em FOV 48°) e `POTATO` (frente). Elas comprovam carregamento/renderização, mas não equivalem
a uma partida nem à aceitação visual do jogador.

Regeneração no repositório:

```bash
cmake --build --preset release --target gdp_renderer_gl_tests --parallel 4
GDP_ENEMY_REFERENCE_MODELS=1 \
GDP_ENEMY_REFERENCE_CAPTURE_DIR=visual/validation/enemy_reference_game \
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
./build/release/gdp_renderer_gl_tests --enemy-reference-capture
```

O teste salva BMP. Arquivos PNG locais nesta pasta são conversões para revisão;
essas capturas são ignoradas pelo Git.
Para comparar numa partida da arena local, compilar o preset `build/playtest`
conforme `README.md` e abrir com:

```bash
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
./build/playtest/gdp --test-mode
```

Os cinco modelos e as hitboxes correspondentes são o padrão aprovado. Para
comparar com o conjunto anterior, use `GDP_ENEMY_REFERENCE_MODELS=0`.

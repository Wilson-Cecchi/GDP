# Playtest das salas no modo Campanha — 04/11/2026

As variantes aprovadas foram colocadas na biblioteca de geração da campanha.
O Laboratório grande também entrou para playtest, ainda sem aprovação visual.
A Arena de Combate com escadas presas à parede está disponível para avaliar a
travessia em um mapa real. A prévia isolada continua disponível.

| Semente | Salas garantidas no primeiro andar |
| --- | --- |
| `1793` | Central Hub octogonal, Manutenção em L, Cafeteria longa, Segurança em T e Arena com galerias e escadas. |
| `3710` | Central Hub octogonal, Storage, Cafeteria longa, Segurança em T e Arena com galerias e escadas. |
| `2` | Laboratório grande com colunas físicas e materiais em revisão, como sala do Terminal. |

Para testar, use um slot de campanha novo e selecione **Campanha > Novo jogo**
no menu. O argumento `--seed` controla a geração do primeiro andar; continuar
um checkpoint usa a semente gravada no slot.

```bash
cd /home/wilson/GDP/GDP-3
cmake --build build/combat-preview --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/combat-preview/gdp --seed 1793
```

Repita com `--seed 3710` para conferir Storage. A Sniper é adquirida pelas
regras da campanha; o comando não concede equipamento de prévia. Verifique
subida e descida de ambas as escadas, transição de portas, colisão com móveis,
encontros e circulação dos inimigos. O funcionamento técnico foi validado em
testes automatizados e com abertura do jogo na Quadro K2100M; a aprovação
visual e de jogabilidade depende deste playtest.

Para avaliar o Laboratório em campanha, repita o comando com `--seed 2` e um
slot novo; procure a sala do Terminal no primeiro andar. Confira a parede
clara, colunas azuis em 3D, bancadas, objetos, iluminação, portas e circulação
durante o combate. Esta variante está em playtest e ainda não tem aceite visual.

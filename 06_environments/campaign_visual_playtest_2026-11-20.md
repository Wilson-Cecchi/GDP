# Playtest de campanha — revisão visual das salas (20/11/2026)

Esta build contém a Cafeteria 18 × 18 m mobiliada, estações compactas por tema,
Office com acabamento próprio, corredor com material independente, variação de
carga no Storage, amostras legíveis nos painéis do Laboratório, marcadores de
setor na Arena e a luz focal reposicionada na sala do Boss. O aceite visual e
de combate depende de partida.

## Abrir na NVIDIA

```bash
cd /mnt/projetos/GDP/GDP-3
cmake --build build/technical-ceiling-build --target gdp -j 4
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/technical-ceiling-build/gdp --seed 29
```

No menu, selecione **Campanha > Novo jogo** em um slot novo. A semente `29`
contém a Cafeteria 18 × 18 m no primeiro andar. O checkpoint de um
slot anterior mantém sua semente antiga; por isso, use um slot novo para esta
comparação.

Outras sementes de referência:

- `1793`: Central Hub octogonal, Manutenção em L, Cafeteria longa, Segurança em T
  e Arena com galerias e escadas.
- `15`: Office compacto e médio, além da Segurança quadrada.
- `1116`: Office Hall e Segurança quadrada.
- `4`: Cafeteria 18 × 18 m e Arena com galerias.
- `0`: Laboratório grande.

Troque apenas o valor de `--seed` no comando acima e crie outro jogo novo.
Percorra corredores em ambos os sentidos; compare suas paredes com as duas
salas conectadas. Confira passagem e tiros nas portas, em volta das mesas e
estações compactas, colisão dos móveis, legibilidade do Office e do terminal
do Boss, além da distribuição de luz após quebrar luminárias. Repita em ULTRA
e POTATO se quiser avaliar a leitura das superfícies nos extremos gráficos.

## Validação técnica concluída

- `world_module_systems`, `room_traversal_systems`, `test_arena_systems`,
  `room_material_systems`, `room_lighting_systems`, `renderer_opengl_pixels`,
  `campaign_systems` e `prop_systems`: 8/8 passaram.
- Prévia gráfica do corredor: 60 quadros na NVIDIA GeForce GTX 1060, sem erro.
- Captura técnica da Arena e abertura do jogo por 60 quadros na NVIDIA GeForce
  GTX 1060, sem erro.
- `--headless --frames 10` nas sementes 0, 1, 2, 4, 29, 1793 e 3710:
  geração e inicialização passaram. A semente 29 ainda precisa do teste
  visual em partida.

As capturas offscreen são evidência técnica de material; não substituem o
playtest na campanha com inimigos e portas reais.

# Captura técnica da granada em voo e da explosão

Capturas offscreen: 2026-10-13, antes e depois da revisão visual do renderer.
Estado: **comparação técnica concluída; aprovação visual em jogo pendente**.

## Fixture

`gdp_renderer_gl_tests --grenade-smoke-capture` é um modo manual separado do
teste OpenGL normal. Mantém a câmera em `(1,50; 1,62; 4,00)`, olhando para `-Z`,
e posiciona a granada ou explosão no mesmo eixo a 1,2 m (`near`) ou 4,0 m
(`far`). O deslocamento lateral da câmera deixa ambos visíveis sem a coluna
central obstruir a linha de visão. Cada combinação usa a mesma câmera,
geometria e perfil para o quadro sem efeito (`baseline`), a granada em voo
(`in_flight`) e a explosão no início (`strength=1,0`), meio (`0,5`) e fim
(`0,1`). Os perfis são MEDIUM (escala 100%, materiais detalhados) e POTATO
(escala 50%, sem materiais detalhados), em FOV vertical 60 e 90 graus.

O fixture verifica OpenGL sem erro e compara pixels RGB com o baseline em cada
caso. Ele também imprime pixels alterados, draw calls e triângulos. Os arquivos
são nomeados por perfil, FOV, distância e fase, como
`medium_fov60_far_blast_mid.png`.

## Baseline salvo

Os 40 PNGs estão em
`build/validation/grenade_smoke_2026-10-12/baseline/`: oito combinações de
perfil/FOV/distância, com cinco imagens cada. O modo manual terminou com código
0 no Mesa Intel HD Graphics 4600; os 32 quadros com granada ou explosão diferem
do respectivo baseline. A contagem mínima foi apenas **3 pixels** para
`medium_fov90_far_blast_start`: a explosão inicial distante no renderer anterior
é quase imperceptível, embora tecnicamente detectável.

## Captura após a revisão

Uma primeira iteração posterior foi preservada em
`build/validation/grenade_smoke_2026-10-12/iteration_1/`. Nela a fumaça tinha
bordas de esferas e tom escuro. A segunda iteração, após suavização das bordas,
está em `build/validation/grenade_smoke_2026-10-12/iteration_2/`, junto aos
logs. A terceira, após ajuste da iluminação, ficou em `iteration_3/`. A captura
final, que amplia o clarão inicial distante, está em `post/` (40 PNGs,
`run.log` e `profile.log`).
O fixture terminou com código 0; todos os 32 quadros com granada ou explosão
diferem do respectivo quadro sem efeito. O teste OpenGL normal também terminou
com código 0 (`normal_gl.log`). A compilação do alvo
`gdp_renderer_gl_tests` passou.

| MEDIUM, FOV, distância | Granada em voo: pixels antes → final | Explosão inicial | Meio | Fim |
| --- | ---: | ---: | ---: | ---: |
| 60°, 1,2 m | 13.000 → 11.639 | 1.267 → 91.341 | 11.036 → 251.332 | 17.283 → 127.415 |
| 60°, 4,0 m | 970 → 977 | 65 → 7.548 | 817 → 36.237 | 1.431 → 65.194 |
| 90°, 1,2 m | 4.202 → 3.818 | 389 → 30.389 | 3.616 → 205.486 | 5.669 → 167.416 |
| 90°, 4,0 m | 274 → 268 | 3 → 2.450 | 219 → 12.069 | 398 → 21.729 |

POTATO também passou nas oito fases por FOV e distância, com contagens
semelhantes às de MEDIUM; os números completos constam em `post/run.log`. Em um
exemplo distante (MEDIUM, FOV 60), draw calls foram 15 sem efeito, 18 com a
granada em voo, 16 no início da explosão e 20 no meio/fim; triângulos foram
216, 1.764, 408 e 1.176, respectivamente. FPS de jogo não foi medido por
essas capturas estáticas.

O perfil repetido do fixture existente (`--grenade-blast-profile`) está em
`post/profile.log`. Em MEDIUM, o tempo de GPU mediano foi 3,04 ms sem efeito,
3,01 ms no início próximo, 3,62 ms no meio próximo e 3,01 ms no fim próximo;
em POTATO, 1,44 ms sem efeito, 1,41 ms no início próximo, 1,51 ms no meio
próximo e 1,48 ms no fim próximo. São 32 amostras após aquecimento por caso,
na GPU Intel HD Graphics 4600; a variação entre iterações impede inferir FPS
de jogo ou custo estável com esses números. O perfil usa a câmera padrão,
diferente da câmera deslocada da matriz de capturas; seu `near_start` marcou
0 pixels alterados por ocultação naquela vista. A matriz em `post/` confirma
visibilidade do mesmo estágio com linha de visão livre.

A granada em voo mudou entre as duas versões (por exemplo, 1.055 pixels RGB
entre as imagens MEDIUM/FOV 60/distante), o que confirma uma diferença de
renderização. Essa medida não comprova semelhança visual com a granada na mão.
Durante a iteração houve uma falha de compilação do fragment shader no runtime
(`viewDirection` ausente no caminho low-spec); foi corrigida antes da captura
da iteração 2. O fixture e o teste OpenGL normal passaram depois da correção.

Comandos de reprodução (Pillow é usado apenas para converter as capturas fora
do jogo; o fixture grava BMP via SDL):

```bash
cmake --build build/debug --target gdp_renderer_gl_tests -j 2
GDP_GRENADE_SMOKE_CAPTURE_DIR=build/validation/grenade_smoke_2026-10-12/post \
  ./build/debug/gdp_renderer_gl_tests --grenade-smoke-capture
./build/debug/gdp_renderer_gl_tests --grenade-blast-profile
python3 - <<'PY'
from pathlib import Path
from PIL import Image
folder = Path('build/validation/grenade_smoke_2026-10-12/post')
for bmp in folder.glob('*.bmp'):
    with Image.open(bmp) as image:
        image.save(bmp.with_suffix('.png'))
PY
```

## Limite da validação

As imagens vêm de um contexto OpenGL oculto, com cenário e câmera fixos.
Diferença de pixels confirma que algo foi desenhado, mas não comprova forma,
legibilidade, aparência durante movimento, iluminação de todas as salas ou
aprovação visual em jogo. A comparação com a granada da mão e a aceitação
estética final dependem de revisão das capturas finais e playtest nativo.

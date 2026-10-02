# CI Linux — equivalência de pixels e novas armas

Data: 2026-10-02. Escopo: corrigir o teste autorizado pelo usuário, preservando
instancing, e validar a implementação visual aprovada de sniper/escopeta Used v1.

## Diagnóstico confirmado

[Run 37046728019](https://github.com/Wilson-Cecchi/GDP/actions/runs/37046728019),
commit `19d093c`: Debug, Release e ASan/UBSan compilaram normalmente. Cada job
Linux passou 62/63 testes; somente `renderer_opengl_pixels` falhou. O Windows
x64 package passou, incluindo testes portáveis e verificação do ZIP extraído.
Os três runners Linux usaram llvmpipe LLVM 20.1.2 / Mesa 25.2.8.

Reprodução local: NVIDIA Quadro K2100M / driver 470.256.02 passou o gate original.
Ao selecionar Mesa llvmpipe LLVM 21.1.8 / 26.2.3 no mesmo checkout, a comparação
byte a byte entre draws comuns e instanciados falhou. As verificações de geometria,
redução de draws, repetição de sala, POTATO e clamp de sombra continuaram passando.
Foram medidos **11 canais RGB diferentes entre 192.000**, máximo **7/255**.
Diferenças de precisão/rasterização entre os caminhos são a explicação provável;
não se atribuiu a falha a gameplay ou ao empacotamento Windows.

## Correção limitada ao teste

`compareInstancingPixels` exige vetores RGBA não vazios e do mesmo tamanho,
alpha idêntico e desvio máximo RGB de 8/255. Aceita no máximo
`floor(canaisRGB / 8192) + 1` canais diferentes: 24 de 192.000 em 320×200.
Mesmo um desvio pequeno em uma área maior continua falhando. Casos sintéticos
protegem igualdade, desvio 7 permitido, desvio 9 rejeitado, alpha alterado,
100 canais divergentes e tamanhos incompatíveis.

Só o gate draw comum × instanciado usa essa tolerância. Repetição determinística
da sala, POTATO, clamp de sombras, contagem de triângulos, número de instâncias
e redução de draw calls não foram relaxados. Diagnósticos agora imprimem desvio,
quantidade de canais e invariantes relevantes. Nenhum teste foi removido ou
desativado e o workflow permanece habilitado.

## Reprodução local com Mesa

```sh
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
LIBGL_ALWAYS_SOFTWARE=1 \
ctest --preset release -R '^renderer_opengl_pixels$' --output-on-failure
```

`LIBGL_ALWAYS_SOFTWARE=1` isoladamente não selecionou Mesa neste ambiente GLVND;
o driver reportado no log deve confirmar llvmpipe. CTest fornece SDL offscreen.

## Validação

- Release completo, NVIDIA: 63/63 testes, 33,87 s.
- Debug completo, llvmpipe: 63/63 testes, 203,01 s; pixels OpenGL 18,31 s.
- Release llvmpipe: `renderer_opengl_pixels` passou em 13,84 s.
- ASan/UBSan llvmpipe: o mesmo teste passou em 25,54 s.
- ASan/UBSan: seis gates afetados passaram em 160,67 s (frontend, integração
  da aplicação, renderer CPU, HUD, loot e combate). O contato com o chão foi
  repetido com seeds 123/128 para cobrir as duas laterais: Release 0,12 s e
  ASan/UBSan 8,24 s, ambos passando; Debug reconstruído passou em 0,96 s.
- Modelos nativos: seis capturas 1280×720 inspecionadas (mão, chão e POTATO
  para cada arma), geradas pela fixture OpenGL. Não foi um playtest manual.

As versões locais de Mesa e do runner não são idênticas: a CI do novo commit
ainda precisa confirmar o resultado no runner. Resultados locais incluíram o
fluxo paralelo de IA já presente no worktree; suas alterações foram preservadas
e ficam fora desta publicação. As duas implementações visuais permanecem
`IMPLEMENTADA EM VALIDAÇÃO`, sem declarar aceitação artística pelo usuário.

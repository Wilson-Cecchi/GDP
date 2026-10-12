# Perfil do lag após explosão de granada — 2026-10-12

## Observação e escopo

O relato é de lag após a explosão de uma granada lançada. Este perfil isola a
renderização de um único blast próximo à câmera; não executa a simulação de
detonação, IA, áudio ou reconstrução de colisão. A fixture usa uma cena fixa em
1280×720, MEDIUM (escala 100%) e POTATO (escala 50%). Mede sem blast, blast a
0,25 unidade da câmera com `strength` 1,0/0,5/0,1, blast a 3 unidades e
novamente sem blast. Em cada caso: 8 frames de aquecimento e 32 amostras;
medianas do timer de GPU do renderer, tempo da chamada `render`, `glFinish`,
draw calls e diferença de pixels contra o baseline. As execuções completas
estão em `build/validation/grenade-blast-profile-20261012/run*.log`.

O pavio atual começa **após** a retirada do pino (0,32 s) e dura 4,87 s. Assim,
segurar por 4,00 s desde o clique consome 3,68 s de pavio; restam 1,19 s no
arremesso e cerca de 0,19 s após mais 1,00 s de voo simulado. O teste de combate
confirma a detonação por volta de 1,20 s depois do arremesso a 60 Hz. A regra
corresponde à escolha do usuário; esse caso não reproduz defeito no cálculo do
pavio. O relógio de simulação aceita no máximo 0,25 s por frame, então uma
travada **antes** da detonação pode atrasá-la em tempo real.

```sh
cmake --build build/debug --target gdp_renderer_gl_tests -j2
build/debug/gdp_renderer_gl_tests --grenade-blast-profile
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  build/debug/gdp_renderer_gl_tests --grenade-blast-profile
git diff --check
```

Build e seis execuções do perfil saíram com código 0. O teste percorreu quatro
execuções com OpenGL Intel HD Graphics 4600/Mesa 26.2.3 (padrão) e duas com
NVIDIA Quadro K2100M/driver 470.256.02 (offscreen); uma execução Intel foi
feita antes de acrescentar a medição `none_after`, portanto há quatro logs
Intel e dois NVIDIA. O jogo nativo pode selecionar outro dispositivo: a
variável do driver no teste determina apenas o contexto medido aqui.

## Números

| GPU / perfil | Sem blast (GPU) | Próximo, fim (GPU) | Delta | Sem blast depois (GPU) |
| --- | ---: | ---: | ---: | ---: |
| Intel MEDIUM | 8,39 ms | 8,87 ms | +0,48 ms | 8,43 ms |
| Intel POTATO | 1,36 ms | 1,39 ms | +0,03 ms | 1,37 ms |
| NVIDIA MEDIUM, segunda execução | 1,175 ms | 1,214 ms | +0,038 ms | 1,176 ms |
| NVIDIA POTATO, segunda execução | 0,326 ms | 0,333 ms | +0,007 ms | 0,321 ms |

Os números Intel são medianas das medianas por caso das quatro execuções, salvo
`none_after`, disponível na quarta. A primeira execução NVIDIA começou com GPU
em outro estado de clock/cache (1,693 ms sem blast), mas seu delta do blast
próximo foi semelhante (+0,042 ms); a segunda iniciou e terminou perto de
1,175 ms. O timer GPU reportou 32 amostras em todos os casos. A chamada `render`
ficou perto de 0,2–0,3 ms nos perfis, sem aumento consistente. A espera
`glFinish` variou 40–66 ms na Intel e não acompanhou a variação do timer GPU;
inclui sincronização/driver e não deve ser tomada como tempo de frame do jogo.

O blast adicionou exatamente 2 draws e 24 triângulos (15→17 draws, 216→240
triângulos) na fixture. No fim do efeito, mudou 74.160 pixels em MEDIUM e
76.720 em POTATO, cerca de 8% dos 921.600 pixels da imagem final. A 3 unidades
da câmera mudou só 648/640 pixels. A grande área da câmera explica o aumento
de custo GPU do blast próximo, mas não há salto de dezenas de milissegundos no
timer GPU isolado. A medição pós-blast retornou ao baseline, sem custo gráfico
persistente reproduzido.

## Custo da IA após o alerta

Um perfil separado (`gdp_enemy_sensors_tests --grenade-alert-profile`) executa
900 passos a 60 Hz com oito Guards, a mesma sala, a mesma posição inicial e as
mesmas seeds. A única diferença entre os dois grupos é um chamado de
`alertEnemiesToNoise` no início. Foram feitos um aquecimento e três repetições
medidas. A parede impede visão direta do jogador e provoca busca de caminho.
Os logs estão em `build/validation/grenade-alert-profile/`.

| Build e fase | Antes, IA alertada | Depois, IA alertada | Redução |
| --- | ---: | ---: | ---: |
| Release, Investigate | 689 µs/passo | 230 µs/passo | 67% |
| Release, Recover | 943 µs/passo | 168 µs/passo | 82% |
| Debug, Investigate | 4,59 ms/passo | 1,22 ms/passo | 73% |
| Debug, Recover | 5,92 ms/passo | 0,91 ms/passo | 85% |

O alerta durou 14 passos, a investigação 449, a procura 193 e o período
posterior 244, tanto antes quanto depois. As assinaturas de posição, orientação
e comportamento dos inimigos a cada passo também permaneceram iguais nos dois
builds. A correção reutiliza o waypoint já calculado na procura e evita a
amostragem de segmentos que cabem inteiramente em um volume caminhável sem
interseção possível com sólidos. Quando essa condição não é demonstrável, o
teste de colisão original continua em uso. Os testes de sensores passaram.

Esse cenário reproduz custo de CPU temporário após o alerta e mede a redução.
Ele usa IA roteirizada e geometria sintética, sem áudio, renderização, outras
salas ou o `Application::fixedUpdate`; portanto, não estabelece sozinho a causa
ou a intensidade exata do lag observado no jogo.

## Relação com o código e diagnóstico restante

`Application::fixedUpdate` cria o efeito por 0,36 s e chama
`resolveGrenadeBlast` uma vez por detonação. O renderer desenha duas caixas
cruzadas por blast a cada frame enquanto o efeito está ativo. Logo, o custo
visual observado pode durar no máximo os 0,36 s da vida do efeito, salvo outro
estado de cena alterado pela explosão. `resolveGrenadeBlast` consulta inimigos,
props e linha de visão; em seguida a aplicação pode reconstruir colisão quando
um prop é destruído e alerta IA na sala e nas salas alcançáveis. O perfil de IA
acima mede o trabalho de navegação desencadeado pelo alerta. A fixture GL não
mede o custo da resolução do dano, reconstrução de colisão ou áudio.

Próximo passo estreito: medir por detonação o tempo de
`resolveGrenadeBlast`, `collisionWorldWithProps` e
`alertEnemiesToNoise`, além do tempo dos fixed updates e do número de inimigos
alertados nos segundos seguintes, em uma sessão real. Comparar com um arremesso
sem detonação e com uma detonação sem props destruídos. Só considerar alterar o
desenho do blast se o dispositivo usado pelo jogo mostrar um pico GPU que este
teste isolado não reproduziu.

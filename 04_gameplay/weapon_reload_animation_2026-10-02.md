# Recarga visual por arma — v1

Pedido: "podemos colocar animação de recando as armas para cada arma diferente?"
Data: 2026-10-02. Implementação em validação visual, sem mudar texturas,
silhuetas externas aprovadas ou as regras do combate.

## Perfis

| Arma | Gesto e peças rígidas | Tempo lógico preservado |
| --- | --- | --- |
| Pistola | Inclina lateralmente, retira/reinsere carregador, movimenta slide | 0,82 s |
| Carabina | Inclinação oposta, carregador longo, movimento do mecanismo | 1,05 s |
| SMG | Gesto compacto, troca mais rápida no início, mecanismo curto | 1,4 s |
| Sniper | Abre/gira ferrolho, troca carregador curto, fecha ferrolho | 2,0 s |
| Escopeta | Expõe região inferior, insere cartuchos em sequência, aciona pump | 2,4 s |

A faca não recarrega. A SMG usa as mesmas peças da carabina provisória, mas
mantém sua identidade no snapshot para ter animação diferente. Ainda não há
braços/mãos, IK ou recarga progressiva autoritativa por cartucho. É uma primeira
versão de animação de peças rígidas, não captura de movimento completa.

## Contrato e limites

`Weapon` continua dono de tempo, bloqueio de disparo, reserva e conclusão da
recarga. `Application` publica progressão normalizada e a quantidade real a
carregar: mínimo entre reserva e espaço disponível. `sampleWeaponReloadPose`
amostra curvas determinísticas limitadas; não recebe delta-time, não usa RNG
e não altera munição. Na escopeta, a quantidade determina o número de gestos
(limitado visualmente a 32 para entrada defensiva). A munição segue entrando
toda no término da recarga; cancelar não concede cartuchos parciais.

Progresso 0/1 retorna exatamente ao repouso; inputs inválidos são sanitizados.
Troca/cancelamento restaura as peças quando o estado lógico sai da recarga.
Projeção fixa do viewmodel, retícula e hitscan permanecem inalterados.

Os meshes aprovados são separados por tags semânticas em corpo, carregador
e mecanismo. No chão continuam completos e estáticos. Um pequeno interior de
carregador da pistola permite a extração; a escopeta ganha somente um cartucho
transitório, ausente do modelo em repouso/no chão. Peças usam os materiais
Used já existentes; sem novas texturas. Upload é feito na inicialização, não
por frame. O custo aumenta por algumas submissões do viewmodel, não por novos
mapas de materiais. Não foi prometido ganho de desempenho.

## Validação e recaptura

CPU: perfis distintos das cinco armas, retorno exato, clamps/NaN, limites de
deslocamento, faca sem recarga, cadência da escopeta e partição de geometria
sem perder/duplicar triângulos. OpenGL: início/meio/fim, restauração, diferenças
SMG/carbine, cartucho transitório e quantidades 1/6, MEDIUM/POTATO.
`weapon_reload_opengl` é um gate próprio para não alongar o timeout de pixels
da suíte antiga. Compatibilidade de FOV, chão e combate continua protegida
pelos gates existentes.

Para teste manual:

```sh
build/release/gdp --test-mode
```

Atirar antes de R; G libera a primária para recolher as outras. Conferir também
recarga parcial, interrupção por troca, mira, FOV, agachamento e salto. Aceitação
visual final depende deste playtest do usuário.

Para recapturar uma sequência nativa 640×360 com 31 poses por arma:

```sh
GDP_RELOAD_CAPTURE_DIR=build/visual-validation/weapon-reloads-v1 \
SDL_VIDEODRIVER=offscreen build/release/gdp_renderer_gl_tests --reload-only
```

BMPs vão ao diretório indicado; são capturas da fixture nativa, não sessão
manual nem conceitos gerados. GIFs de referência ficam em `visual/validation/`.

Sequências nativas: [pistola](../visual/validation/weapon_reload_v1_pistol.gif),
[carabina](../visual/validation/weapon_reload_v1_carbine.gif),
[SMG](../visual/validation/weapon_reload_v1_smg.gif),
[sniper](../visual/validation/weapon_reload_v1_sniper.gif),
[escopeta](../visual/validation/weapon_reload_v1_shotgun.gif).

Resultado local final: Release 64/64 testes (58,81 s), incluindo integração
real da Application e gate dedicado OpenGL. Debug/llvmpipe: CPU passou em
1,76 s, recargas em 7,73 s e pixels antigos em 16,84 s na repetição.
Na primeira execução concorrente, pixels em Debug excederam 20 s e CPU com
sanitizers excedeu 10 s; ambos passaram ao repetir com menos concorrência
(CPU ASan/UBSan 8,74 s). Não houve mudança de seus timeouts ou assertions.
A integração ASan/UBSan passou em 240,14 s antes do último refinamento de
altura visual; após esse ajuste, CPU foi reconstruído e repetido.
Repetição final ASan/UBSan: 6/6 gates afetados em 38,59 s (CPU, gameplay,
HUD, combate, pixels antigos e recargas); sem erros de sanitizer reportados.
Validação no worktree com mudanças
do fluxo paralelo de IA, que foram preservadas e não pertencem a esta etapa.

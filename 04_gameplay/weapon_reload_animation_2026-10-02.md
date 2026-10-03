# Recarga por arma — v2: descarte e carregadores completos

Atualização solicitada e autorizada pelo usuário: descartar o pente usado e
suas balas restantes, inserir outro cheio e contar reservas em carregadores.
O registro v1 abaixo é histórico e não descreve mais a reserva das armas.

Atualização de apresentação em 2026-10-03: [mãos Usado v2](player_hands_2026-10-03.md)
adicionam rig/contatos sobre estas peças, após aprovação própria. Curvas
cosméticas do ferrolho/sniper e da entrada/SMG foram sincronizadas com as mãos;
tempos e regras de munição abaixo não mudaram. Capturas/testes desta página
registram a etapa anterior, não validam automaticamente as mãos atuais.

## Regras atuais

- Pistola: 8 balas / 4 carregadores; carabina: 12 / 4; SMG: 24 / 4;
  sniper: 5 / 3. Dano e cadência não mudaram; tempos das armas de pente também não.
- A escopeta continua com 6 cartuchos carregados / 24 de reserva. Completa
  somente os espaços vazios, sem jogar fora os cartuchos já carregados.
  Cada cartucho leva 0,35 s, seguido de 0,30 s para finalizar o pump:
  um cartucho leva 0,65 s; seis levam 2,40 s. A duração considera também a
  reserva disponível. Cada inserção consome um cartucho e aumenta a munição
  carregada durante a recarga. Trocar ou largar preserva os já inseridos.
- R não recarrega arma cheia nem arma sem reserva. Aos 32% da recarga,
  o carregador antigo é ejetado e todas as balas que sobraram são perdidas.
  No término, gasta exatamente um carregador da reserva e enche a arma.
- Trocar ou largar antes da ejeção preserva o carregador antigo. Depois da
  ejeção, a arma continua vazia; o pente novo ainda não concluído permanece
  na reserva. Reiniciar não duplica munição. Não há munição recuperável no
  pente descartado nem entidade de loot gerada por essa animação.
- Drops do jogador preservam carregador e reserva exatos. Duplicatas de arma
  dão até um pente compatível (escopeta: até seis cartuchos). Caixas de munição
  conservam seu orçamento em balas, convertido por arma para pentes inteiros
  com arredondamento para cima, limitado pela capacidade disponível.
- Cada recompensa usual de capacidade (+4 no orçamento antigo) dá +1 pente
  às armas de carregador, ou +4 cartuchos à escopeta, incluindo a arma guardada.
  A troca de tipo de arma mantém o número equivalente de melhorias.
- HUD mostra balas ativas e reserva identificada como MAGS/PENTES ou
  SHELLS/CART. IA inimiga e treinamento não tiveram suas reservas alteradas.

## Persistência e compatibilidade

Campanha usa regras 37 e schema 7. Saves de regras 34/schema 3–5 e
35/schema 3–6 são validados antes da conversão; balas reservadas e capacidade
das armas de carregador tornam-se pentes completos, arredondando para cima
uma única vez. Munição carregada, cartuchos da escopeta, upgrades, andar, seed
e dificuldade permanecem. Ler não sobrescreve o arquivo; o próximo checkpoint
salvo usa schema 7, que não reconverte os valores. Speedrun usa regras 38,
separando recordes do balanceamento antigo.

## Visual v2

O pente antigo desce e gira, saindo do enquadramento, sem retornar à arma.
Um segundo exemplar entra por baixo e se encaixa; slide/ferrolho continuam
com seu gesto próprio. A escopeta publica uma quantidade planejada estável
durante a recarga; seu cartucho visual acompanha cada intervalo de inserção,
sem acelerar a animação conforme a reserva diminui. Reutiliza os meshes e materiais Used aprovados, com
uma passagem adicional temporária para o pente que cai. Nesta etapa não foram
criadas texturas, assets de mãos ou simulação física persistente; mãos foram
implementadas depois na unidade própria ligada acima. A aceitação visual final
continua dependente do playtest do usuário.

Capturas nativas v2: [pistola](../visual/validation/weapon_reload_v2_pistol.gif),
[carabina](../visual/validation/weapon_reload_v2_carbine.gif),
[SMG](../visual/validation/weapon_reload_v2_smg.gif),
[sniper](../visual/validation/weapon_reload_v2_sniper.gif),
[escopeta](../visual/validation/weapon_reload_v2_shotgun.gif).
São fixtures offscreen de 31 poses por arma, não uma sessão manual completa.
Recaptura atual:

```sh
GDP_RELOAD_CAPTURE_DIR=build/visual-validation/weapon-reloads-v2 \
SDL_VIDEODRIVER=offscreen build/release/gdp_renderer_gl_tests --reload-only
```

Validação local v2: Release 64/64 testes em 59,50 s, já incluindo a recarga
proporcional da escopeta e integração da Application. Capturas inspecionadas
na Quadro K2100M/NVIDIA 470.256.02; repetição dos três gates OpenGL no
Mesa llvmpipe 26.2.3 passou em 12,52 s. Sem alteração de timeouts ou tolerâncias.
ASan/UBSan: nove gates de CPU, combate, loot, progressão, campanha, persistência,
Speedrun e OpenGL passaram em 45,28 s, sem erros reportados.
A integração completa da Application com ASan/UBSan passou em 213,26 s;
total de dez gates afetados instrumentados aprovados. Não houve novo teste
nativo Windows local; o pacote desta revisão depende da execução da CI após
o push. Mudanças do fluxo paralelo de IA ficaram preservadas e fora do commit.

## Registro histórico v1

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

# Testing — Estratégia

> **Estado:** especificação técnica inicial do novo GDP.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Camadas

```text
unit
→ subsystem
→ headless procedural
→ integration
→ playtest
```

## Unit

Hash/RNG, math, bounds, connectors, handles geracionais, ciclo de vida de
entidades, Weapon, hitscan, damage, Worker, Guard, Officer, texture operators e config.
Receitas procedurais também possuem validação e hashes dourados próprios.

## Subsystem

MapGraph, RoomModule, Corridor, WorldValidator, população inicial de entidades,
Combat, Worker/Guard/Officer/Engineer AI, EnemyGenerator, RoomMaterialGenerator, TextureGenerator e
AssetCache.

O gate de loot gera 1.000 layouts de sala, exige hashes únicos, orçamento sem
excesso, placement dentro de `lootZones`, separação entre drops e distribuição
de Health/Ammo. Testes adicionais cobrem Key obrigatória em Secret, escala de
dificuldade, coleta automática, capacidades máximas, distância, `roomId`,
jogador morto e destruição adiada. A regressão OpenGL diferencia os três pickups.

O gate de props gera 1.000 layouts, exige hashes únicos, orçamento exato,
footprints contidas, separação e cobertura dos três tipos. Testes funcionais
cobrem dano, reparo, três estados, colisão por sala, prioridade de hitscan,
cobertura de inimigo, proteção de terminal e drop único. A regressão OpenGL
diferencia tipo/estado e confirma a composição com inimigos e pickups.

O gate de eventos gera 1.000 planos, cobre os cinco tipos, hashes únicos,
triggers, waves, zonas e recompensa. Testes de runtime cobrem emboscada do
terminal, entrada, espera do encontro inicial, duas ondas, ownership, lockdown,
suporte, conclusão e proteção contra reativação/drop duplicado.

O gate de iluminação gera 1.000 planos, cobre seis moods, hashes únicos,
papéis/temas, bounds, ambiente legível, pontos e luminárias. As seeds críticas
validam todas as salas e preservam o fingerprint estrutural. A regressão OpenGL
confirma múltiplas luzes e a mudança temporária para alerta de lockdown.

O gate de áudio gera 1.000 receitas com hashes únicos e sintetiza as dezenove
categorias sem samples inválidos ou clipping. Testes do mixer silencioso cobrem
cache, voice budget, descarte, loop, pan, distância e os buses Effects/Ambient/UI.
`updateUi()` deve concluir a voz de interface sem mover os cursores congelados
dos outros buses. Integrações SDL usam driver dummy para saída e driver
inexistente para provar fallback não fatal.

O gate do Player também cobre passos e movimento cosmético por distância real.
Head bob exige fase determinística, amplitudes máximas, fade parado/aéreo,
proteção contra relocação e reset. O teste mantém o offset fora da câmera lógica,
preservando a origem usada por interação e combate.

O gate de progressão cobre valores por fonte, curva/limite, múltiplos níveis no
mesmo award, resto de XP, telemetria e entradas inválidas. Também exige os nove
benefícios reais de Player/Weapon no nível 10 e valida o estado após cada
sequência representativa.

O gate de sessão exige Playing inicial, Boss real e vivo antes de Victory,
estado terminal idempotente, Defeat por morte e rejeição de restart prematuro.
Também separa robôs por `roomId`, distingue vivo/morto e gera mundos válidos,
estruturalmente distintos e reproduzíveis para a seed derivada da segunda run.

O gate de travessia percorre todos os corredores de um mundo representativo nos
dois sentidos, exige edge/destino corretos, spawn dentro dos bounds, distância e
orientação válidas. Também cobre preservação de HP/inventário/upgrades,
rejeição de coordenadas inválidas e colisão nas quatro rotações. O mesmo gate
simula movimento por todos os segmentos, sem teleporte, resolve a sala destino
automaticamente e prova que porta individual fechada e lockdown impedem a saída.
O runtime cobre abertura parcial bloqueada, limiar passável, ocupação, hold,
fechamento, reversão e lock. OpenGL desloca e gira a sala, diferencia porta
aberta, em movimento e lockdown, e detecta a geometria adicional
com a câmera dentro de um corredor. A regressão multissala mantém duas chaves de
material preparadas ao mesmo tempo, troca apenas o perfil da sala conectada e
exige diferença de pixels; o mesmo teste cobre o overlay terminal no viewport.

O gate de assets gera 1.000 recipes visuais, valida pixels e regras materiais,
exercita reuse do cache e mantém o fingerprint estrutural do World separado. O
gate do Worker gera outras 1.000 seeds, valida montagem/mesh/albedo/hash e exige
cobertura de todos os quatro perfis, 18 peças, três famílias materiais, quatro
estados de desgaste, assimetria e zero fallbacks.
O gate do Guard gera mais 1.000 seeds, exige três perfis, 18 peças táticas, três
famílias materiais, quatro estados de desgaste, arma longa, assimetria e zero
fallbacks. A união dos dois papéis precisa manter 2.000 hashes únicos.
O gate do Officer acrescenta 1.000 seeds, três perfis, 18 peças de comando, três
famílias materiais, quatro estados de desgaste, sidearm e módulo de rádio. A
união dos três papéis precisa manter 3.000 hashes únicos. Testes de combate
confirmam comando limitado por range e sala, cooldown próprio e line of sight
para o disparo.
O gate do Engineer adiciona mais 1.000 seeds, três perfis, 18 peças, três
famílias materiais, quatro estados de desgaste, ferramenta técnica e mochila
diagnóstica. O lote dos quatro papéis mantém 4.000 hashes únicos; combate valida
reparo de aliado/prop, alvo mais próximo, cooldown e bloqueio por sala/sólido.
O IndustrialRobot soma 1.000 seeds, três perfis pesados, 18 peças, três
famílias metálicas, quatro estados de desgaste, núcleo/sensor, braços, dois
reatores e arma obrigatórios. O lote dos cinco papéis mantém 5.000 hashes
únicos; combate valida as três fases de HP, envelopes, dano/cadência,
line of sight, colisão, sala, cooldown e morte.
O gate de materiais industriais rasteriza Wall/Floor/Ceiling para 1.000 room
seeds, verifica todas as condições de manutenção, hashes únicos, regras de
composição e integração OpenGL determinística por pixels.
O mesmo executável rasteriza mais 1.000 seeds de Storage, cobre
`Orderly/Dusty/Neglected`, protege um hash dourado independente e testa regras
de oxidação, concreto, logística, arrasto e iluminação irregular.
Outras 1.000 seeds de Combat cobrem `Ready/Active/Scarred`, hash dourado,
blindagem, impactos localizados, delimitação de arena e teto reforçado. A
regressão OpenGL compara os perfis no mesmo framebuffer.
O lote da Cafeteria adiciona 1.000 seeds `Clean/InUse/Faded`, protege faixa
institucional, piso lavável, contatos de mobiliário e iluminação uniforme. A
isolação conjunta abrange os perfis já implementados.
Laboratory adiciona 1.000 seeds `Sterile/Operational/Contained`, hash dourado,
indicadores limitados, contaminação localizada e coverage efetivo de luz. A
isolação conjunta cobre todos os perfis implementados.
Technical Area adiciona 1.000 seeds `Serviced/Working/Overloaded`, hash dourado,
tubos, óleo, grade e teto categorizado.
Security adiciona 1.000 seeds `Secured/Monitored/Alert`, hash dourado, console
com quatro indicadores, alerta vermelho confinado, tampa central e teto com
20–35% de luz.
Central Hub adiciona 1.000 seeds `Open/Active/Rerouted`, cobre os três eixos,
protege uma placa ciano única, rotas contínuas, luzes alinhadas e ausência de
amarelo decorativo. A isolação conjunta exige 8.000 hashes distintos entre os
oito perfis.

## Headless

Gerar 100 → 1.000 → 10.000 seeds conforme maturidade. Toda falha imprime seed, tentativa,
motivo e comando de reprodução.

O teste `first_gameplay_headless` executa 900 fixed updates do loop real e só
passa se o Worker da START produzir a transição `Player died`.

## Golden seeds

Manter seeds para casos: simples, denso, loops, secret, boss, stress e falhas históricas.

## Visual regression

Salvar dumps de seeds conhecidas. Mudanças intencionais precisam ser revisadas, não aceitas automaticamente.

Regressão visual técnica e aprovação artística são gates independentes. Para
uma revisão de textura, primeiro registrar e aprovar a proposta conforme
`visual/TEXTURE_APPROVALS.md`; somente então implementar e atualizar os dumps.
Um framebuffer estável não significa que a direção visual foi aprovada, e uma
prancha aprovada não permite aceitar pixels ausentes, NaN ou perda de contraste.

O gate estrutural da Fase 20 valida os 23 módulos, as três classes de tamanho,
os papéis especiais e todos os connectors nas quatro rotações. O batch de 1.000
mundos exige AABBs sem overlap e corredores limitados; o de 500 mundos percorre
cada ligação nos dois sentidos. A regressão OpenGL inclui uma sala conectada
grande para impedir o retorno das dimensões fixas no renderer.

O gate do menu principal cobre estado inicial seguro, navegação com wrap,
seleção por ponteiro, ativação única e posição absoluta do mouse preservada
entre frames. O teste CPU valida o mesmo layout responsivo usado pelo hit test;
a regressão OpenGL diferencia `START RUN` de `EXIT GAME`. A integração
`main_menu_defers_world_generation` abre duas frames no driver offscreen e
falha caso uma seed de mundo seja gerada antes da confirmação. Os bootstraps
headless continuam criando a run diretamente para exercer o gameplay real.
Receitas distintas protegem navegação, confirmação e retorno; a Application só
emite navegação quando a seleção realmente muda e usa transições de input, sem
repetição por tecla mantida.

O gate da pausa cobre navegação e seleção direta apenas quando o estado está
ativo, rejeita opções inválidas e valida os cinco hit targets responsivos em
resoluções ampla, compacta e mínima. Os centros dos botões são consultados em
coordenadas SDL com origem superior, enquanto o renderer usa os mesmos retângulos
em coordenadas OpenGL; pontos externos não produzem seleção.

O menu de resultado possui gate próprio para estado inativo, opção segura,
wrap, seleção direta, comandos e reset. O teste CPU cobre `ResultMenuLayout` e
seus três hit targets nas mesmas classes de resolução; a regressão OpenGL exige
mudança de pixels ao alternar a seleção entre jogar novamente e sair.

O comando `MAIN MENU` é protegido nos estados puros de pausa e resultado. A
integração mantém `SessionSystem` como dono do encerramento/restart e reinicializa
o renderer no contexto existente; a validação manual deve confirmar que a janela
não fecha e que `START RUN` inicia a seed seguinte.

O gate de configurações cobre origem título/pausa, wrap, seleção direta, retorno,
passos e clamps de todos os valores. Input neutro e adaptador SDL protegem as
novas setas laterais; gameplay confirma a aplicação da sensibilidade. O teste
CPU percorre as nove linhas do layout em resoluções ampla, compacta e mínima, e
a regressão OpenGL exige pixels distintos para tela, seleção e estado
`FULLSCREEN`.

O gate de persistência usa diretório temporário isolado e cobre arquivo ausente,
round-trip dos oito campos, substituição de arquivo existente, remoção do
temporário, campos ausentes/desconhecidos, corrupção parcial, clamps, versão
ausente, migração v1/v2→v3 e versão incompatível. Gameplay compara deltas
verticais normal e invertido, garantindo yaw inalterado. Os bootstraps headless
e OpenGL confirmam que a integração não altera o ciclo de inicialização; headless
não toca nas preferências reais do usuário. Um bootstrap adicional configura um
`XDG_DATA_HOME` temporário, inicia SDL offscreen com `fullscreen = true`, exige a
flag real de fullscreen no log e remove o diretório isolado ao terminar.

## Sanitizers

Quando disponíveis, usar AddressSanitizer e UndefinedBehaviorSanitizer.

O preset CMake `sanitizers` ativa ambos em GCC/Clang e executa a mesma suíte de
Debug/Release. Erros de memória ou comportamento indefinido usam `halt_on_error`.
LeakSanitizer fica desativado nesse preset para não atribuir ao GDP alocações
persistentes dos drivers e bibliotecas gráficas externas.

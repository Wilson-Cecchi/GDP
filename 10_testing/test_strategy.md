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
Combat, Worker/Guard/Officer AI, EnemyGenerator, RoomMaterialGenerator, TextureGenerator e
AssetCache.

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

## Sanitizers

Quando disponíveis, usar AddressSanitizer e UndefinedBehaviorSanitizer.

O preset CMake `sanitizers` ativa ambos em GCC/Clang e executa a mesma suíte de
Debug/Release. Erros de memória ou comportamento indefinido usam `halt_on_error`.
LeakSanitizer fica desativado nesse preset para não atribuir ao GDP alocações
persistentes dos drivers e bibliotecas gráficas externas.

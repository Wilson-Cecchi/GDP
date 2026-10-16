# Gameplay — Eventos procedurais de sala

> **Estado:** geração, validação e runtime funcional implementados no terceiro
> incremento da Fase 12. A START oferece o fluxo jogável pelo terminal;
> transições entre salas e combate nos corredores já são físicos e contínuos.

## Plano determinístico

`generateRoomEventPlan()` usa somente `SeedDomain::Events` e combina
`RoomRole`, `RoomTheme`, depth, dificuldade e `enemyZones` transformadas. O
plano possui trigger, lockdown, ondas, inimigos, recompensa e hash canônico.

```text
None
TerminalAmbush
Reinforcements
Lockdown
SupplyDrop
```

- START: `TerminalAmbush`, acionada com `E` no terminal;
- Normal: chance determinística de um ou dois grupos adicionais, presentes
  juntos desde a criação do andar;
- Hub/Secret: `SupplyDrop` ao entrar;
- Arena: inimigo adicional presente junto ao encontro inicial; mantém lockdown
  até limpar a sala;
- DeadEnd: suporte ou reforço;
- Boss: lockdown até o IndustrialRobot morrer.

## Runtime

`RoomEventRuntime` percorre `Dormant → Active → Completed`. Todos os inimigos
planejados de `Reinforcements` e `Lockdown` nascem ativos com o andar, na sala
dona do plano. Eles preservam archetype, HP, posição e `visualSeed`
precomputados, e os handles são registrados no `RoomInstance`. A marcação de
onda permanece no plano determinístico para preservar hash e contagens, mas
não retarda a aparição desses inimigos. A Arena tem o inimigo adicional junto
com o encontro inicial. As emboscadas de terminal continuam ocultas até a
interação. O evento ainda é ativado ao entrar ou, para reforços adjacentes,
quando a porta se torna passável. Lockdown permanece ativo até a limpeza; a
conclusão cria uma única recompensa. Eventos completos não reativam nem
duplicam entidades. O tempo de reação ao abrir uma sala inédita permanece.

A limpeza considera a sala de origem de cada inimigo, preservada ao nascer,
e também intrusos ainda vivos dentro da sala. Perseguir o Player por um
corredor e assumir outro `roomId` não conta como morte nem libera prematuramente
onda ou recompensa da sala original. Enquanto um inimigo da sala estiver vivo
do lado de fora, as portas deixam de ser seladas pelo lockdown para que o
Player possa persegui-lo; quando ele retorna ou morre, a trava volta a valer.

Na START, usar o terminal ativa uma emboscada de um Worker adicional. A sala
precisa ficar sem inimigos para liberar Ammo. O título da janela mostra tipo,
fase e `[LOCKDOWN]`; logs registram ativação, onda e conclusão.

## Isolamento

Eventos não consomem `World`, `EnemySpawn`, `Loot`, `Props`, `Lighting` ou
`Audio`. Mudar o plano não altera o fingerprint estrutural do mundo nem os
layouts já comprometidos.

## Validação

O gate percorre 1.000 room seeds, exige os cinco tipos, hashes únicos, triggers
compatíveis, ondas completas, spawns/recompensa dentro das zonas e protege o
hash dourado `0xB04A437A8394B962`. Testes funcionais cobrem terminal, entrada,
dois grupos simultâneos, inimigo adicional da Arena, lockdown, suporte,
conclusão e recompensa única.

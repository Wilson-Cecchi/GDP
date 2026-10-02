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
- Normal: chance determinística de uma ou duas ondas;
- Hub/Secret: `SupplyDrop` ao entrar;
- Arena: limpa o encontro inicial, libera uma onda e mantém lockdown;
- DeadEnd: suporte ou reforço;
- Boss: lockdown até o IndustrialRobot morrer.

## Runtime

`RoomEventRuntime` percorre `Dormant → Active → Completed`. Cada onda só nasce
na sala dona do plano, usa archetype/HP/posição/`visualSeed` precomputados e
registra os novos handles no `RoomInstance`. Lockdown permanece ativo até a
condição de limpeza; a conclusão cria uma única recompensa. Eventos completos
não reativam nem duplicam entidades.

Revisão de 2026-10-02: a onda extra de `Lockdown` da Arena não é pré-criada
como corpos visíveis e inativos. Ela só nasce, já ativa, quando o encontro
inicial termina. Isso corrige os inimigos aparentando esperar a morte dos
outros para poder atacar. A primeira onda de `Reinforcements` por entrada
continua usando o staging existente e é ativada ao entrar, sem duplicação;
emboscadas de terminal continuam ocultas até a interação. Não mudam planos,
contagem de ondas, HP, recompensas ou RNG. A regressão cobre espera, inimigo
que saiu da sala, liberação única e ativação dos handles pré-criados.

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
duas ondas, espera da Arena, lockdown, suporte, conclusão e recompensa única.

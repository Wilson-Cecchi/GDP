# Enemies — Sistema geral

> **Estado:** catálogo lógico e shells implementados na Fase 6; Worker completo
> validado nas Fases 7–9; Guard, Officer, Engineer e IndustrialRobot
> funcionais/procedurais validados na Fase 11.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Princípio

```text
ROLE ≠ STATS ≠ BEHAVIOR ≠ VISUAL GENOME
```

A função de gameplay permanece reconhecível mesmo quando aparência, materiais, dano e assimetria variam.

## Elenco

- **Worker:** pressão corpo a corpo e ferramenta industrial.
- **Guard:** combate armado de média distância.
- **Officer:** comando e suporte tático.
- **Engineer:** manipulação de sistemas/ambiente.
- **IndustrialRobot:** Boss pesado em fases.

## Regra narrativa

Cada família precisa parecer ter uma função real no complexo. O combate surge da invasão,
corrupção ou reprogramação dessa função.

## Spawn

Encounter Generator filtra role por `RoomTheme`, `RoomRole`, profundidade, dificuldade e budget.

Na população inicial atual, START recebe um Worker ativo, Normal recebe Worker,
Arena recebe Worker, Guard, Officer e Engineer e Boss recebe IndustrialRobot. Cada Enemy
possui `EnemyArchetype`, HP máximo/atual, comportamento, cooldowns, transform e
`roomId`.
Cada Enemy também recebe uma `visualSeed` derivada do domínio `EnemyVisual` da
seed de sua sala. A aparência pode mudar sem perturbar o grafo, as salas ou o
fingerprint estrutural. Os quatro papéis da Arena e o IndustrialRobot do Boss
recebem update e visual completo quando sua sala se torna atual. O Officer acelera ataques; o Engineer
repara HP ou props `Damaged`, sempre por range, linha de visão e `roomId`, usando
cooldowns independentes. O IndustrialRobot alterna entre longa distância,
pressão e perseguição nos limiares de 66% e 33% de seus 250 HP.

Na Fase 12, `RoomEventPlan` pode acrescentar Workers/Guards em ondas. Cada spawn
usa posição, HP e `visualSeed` precomputados pela substream `Events`, entra no
mesmo `EntityManager`/`RoomInstance` e só atualiza a partir do fixed update
seguinte. Arena espera o elenco inicial morrer antes da onda de lockdown.

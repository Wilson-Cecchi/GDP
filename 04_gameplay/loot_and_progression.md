# Gameplay — Loot, progressão e modos

> **Estado:** especificação técnica inicial do novo GDP.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Loot pipeline

```text
RoomRole + RoomTheme + depth + difficulty + LootSeed
→ budget
→ zonas candidatas
→ seleção
→ placement
→ validação
```

Não usar sorteio independente em cada célula.

## Categorias

Health, Armor, Ammo, Weapon, Key/Objective e Special.
Pickups respeitam capacidade máxima e não bloqueiam connectors.

## Andar

Primeiro fluxo oficial pode ser: alcançar a sala Boss → derrotar Boss → `FloorComplete`.
Segredos são opcionais e nunca bloqueiam o caminho principal.

## Campanha

Campanha armazena seed, andar, dificuldade e estado persistente do jogador. O mundo pode ser
regenerado pela seed quando não houver necessidade de retomar exatamente o estado intermediário.

## Speedrun

Usa a mesma simulação e geração; adiciona cronômetro e ranking. Regras de pausa e término do
timer devem ser únicas e documentadas.

## Dificuldade

Pode alterar dano, densidade, agressividade e recursos. Não deve silenciosamente mudar a
identidade estrutural da seed sem versão própria.

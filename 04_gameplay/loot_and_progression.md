# Gameplay — Loot, progressão e modos

> **Estado:** loot procedural, coleta automática e progressão de sessão até o
> nível 10 implementados e validados na Fase 12. Armor, Weapon pickup, Special
> e persistência entre execuções permanecem planejados.
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

`generateLootLayout()` implementa esse fluxo usando somente `SeedDomain::Loot`.
START garante Ammo; Hub garante Health; Arena e DeadEnd garantem Ammo; Secret
garante uma Key e Health; Boss não recebe recurso antes da luta. Normal usa a
preferência temática entre Health e Ammo. Dificuldade escala o budget sem
permitir overspend e cada layout possui hash canônico.

## Categorias

Health, Ammo e Key/Objective estão implementados. Armor, Weapon e Special
permanecem no contrato. Pickups são posicionados nas `lootZones`, mantêm
separação mínima e não bloqueiam connectors.

## Coleta

`collectNearbyLoot()` opera automaticamente no fixed update, somente no
`roomId` atual e em raio de `0,85 m`. Health respeita HP máximo, Ammo respeita a
reserva máxima e Key incrementa o estado persistente do Player. Recurso já
cheio mantém a entidade no chão; coleta válida usa destruição adiada, remove o
handle da sala e só então faz flush. Jogador morto não coleta.

O renderer usa formas mínimas emissivas verdes, amarelas e ciano para Health,
Ammo e Key. Assets procedurais específicos permanecem para uma etapa visual posterior.

## Progressão de sessão

`ProgressionSystem` recebe apenas resultados já consolidados. Worker, Guard,
Officer, Engineer e IndustrialRobot concedem respectivamente 25/40/55/50/200
XP; prop destruído concede 10, loot coletado 5 e evento concluído 75. Cada fonte
também possui score e contador próprios. Eventos inválidos ou quantidade zero
não alteram estado.

O nível começa em 1, exige 100 XP e cada próximo limiar cresce 75, até o máximo
10. Resto de XP atravessa múltiplos níveis; no cap, XP corrente fica zero e
total/score continuam acumulando.

```text
nível 2, 5, 8  → Vitality: +15 HP máximo e +15 cura
nível 3, 6, 9  → Firepower: +4 dano da pistola
nível 4, 7, 10 → Capacity: +4 reserva máxima e +4 munição
```

Os benefícios são aplicados às instâncias reais de `Player` e `Weapon`. O
título mostra nível, XP atual/limiar e score. O sistema atual dura a sessão; a
Campanha futura decidirá o que persiste entre andares e execuções.

## Validação atual

O gate executa 1.000 room seeds, exige hashes únicos, budget, placement,
separação, categorias e key de Secret válidos e protege o hash dourado
`0x23E65CDDD8252D83`. Testes funcionais cobrem capacidade, clamp, distância,
sala, jogador morto, remoção e ciclo de vida do Entity Manager.

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

# Gameplay — Combate, armas e dano

> **Estado:** pistola, munição, cooldown, recarga, hitscan, dano e morte
> implementados e validados na Fase 7; Guard armado de média distância validado
> na Fase 11, junto do Officer de suporte. Eventos desacoplados, armadura,
> spread e weak points permanecem
> para fases posteriores.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Pipeline

```text
Fire input
→ Weapon valida cooldown/munição
→ raycast ou projétil
→ HitResult
→ DamageSystem
→ eventos visuais/sonoros
```

## Hitscan

O ray lógico parte da câmera/retícula e para no primeiro bloqueador válido. O cano visual não
define a direção real do tiro.

`fireHitscan()` normaliza a direção, limita pelo alcance, consulta somente
Enemies vivos no `roomId` do jogador e compara o primeiro AABB de alvo com o
primeiro solid ou limite da sala. O resultado distingue cooldown, vazio, miss,
impacto no mundo, hit e kill.

## WeaponDefinition

```cpp
struct WeaponDefinition {
    float damage;
    float fireInterval;
    float range;
    int magazineSize;
    int reserveMax;
};
```

A pistola inicial usa 34 de dano, intervalo de `0,22 s`, alcance de `30 m`,
pente de 8 e reserva de 32. Botão esquerdo dispara e `R` transfere a quantidade
necessária da reserva para o pente.

## Dano

`applyDamage()` opera sobre o payload geracional do Enemy, ignora IDs inválidos,
dano não positivo e alvos mortos, faz clamp de HP em zero e produz uma única
transição para `Dead`. Armadura e weak points entram quando forem visualmente
legíveis.

## Eventos

`ShotResult` já separa os resultados lógicos consumidos pela aplicação. A fila
desacoplada `WeaponFired`, `EntityHit`, `EntityDamaged`, `EntityKilled` e
`ImpactWorld` permanece planejada para renderer e áudio.

## Testes

Implementados: sem munição, cooldown, recarga, parede bloqueia, filtro por sala,
alvo mais próximo, overkill, double death, miss por alcance, aproximação do
Guard, recuo sob pressão, envelope de tiro, cooldown e bloqueio de linha de
visão, comando do Officer, filtro de aliados por papel/range/sala e cooldown de
suporte independente. Spread determinístico e weak point entram com suas
respectivas features.

# Audio — Sistema e procedural

> **Estado:** especificação técnica inicial do novo GDP.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Pipeline

```text
Gameplay Event → AudioEvent → AudioSystem → voice → spatialization → output
```

## Categorias

WeaponFire, Impact, Footstep, Door, EnemyAttack, EnemyHit, EnemyDeath, Ambient e UI.

## Buses

Master, Effects, Ambient e UI.

## Variação procedural

`AudioSeed` é independente. Variantes podem combinar pitch, duração, envelope, filtro e
layers, mas precisam preservar reconhecimento funcional do som.

## Entidades

Escolha de variante pode usar `entityAudioSeed + eventCounter`, sem consumir RNG estrutural.

## Ambiência

Cada RoomTheme fornece base loop + máquinas locais + eventos raros. Eventos raros usam budget
temporal, nunca chance por frame.

## Testes

Device ausente; clipping; voice limit; pause; mesma seed/event; transições entre salas.

# Persistence — Saves e configurações

> **Estado:** especificação técnica inicial do novo GDP.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## SaveGame

```cpp
struct SaveGame {
    SaveVersion version;
    Seed campaignSeed;
    int floor;
    Difficulty difficulty;
    PlayerPersistentState player;
};
```

## Slots

Quatro slots independentes são compatíveis com a direção anterior.

## Escrita segura

`write temp → flush → validate → atomic replace`. Arquivo inválido não deve sobrescrever
silenciosamente um save anterior.

## Retomar no meio do andar

Só adicionar serialização de portas, inimigos, pickups e objetivos quando essa funcionalidade
for realmente necessária. Até lá, seed + progresso entre andares é mais simples.

## Settings

Formato legível como TOML/JSON, versionado e tolerante a campos ausentes/desconhecidos.
Valores inválidos usam defaults/clamps.

A Fase 66 implementa somente o estado runtime clampado para volumes,
sensibilidade e FOV. Ele sobrevive a novas runs no mesmo processo, mas ainda não
é lido ou escrito em disco; esta seção continua sendo o contrato da futura
persistência.

# Interface — HUD

> **Estado:** HUD completo permanece especificado; HP, munição, chaves,
> progressão, sala, evento, objetivo e resultado de sessão aparecem
> provisoriamente no título da janela. O objetivo alterna entre `REACH BOSS` e
> `DEFEAT BOSS`.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Direção

Interface militar/industrial. Em widescreen, o centro preserva uma área de gameplay
conceitualmente 4:3 e as laterais recebem informação.

## Layout

```text
┌──────────────┬──────────────────────────────┬──────────────┐
│ HP / ARMOR   │                              │ SETOR        │
│ AMMO / ARMA  │         VIEWPORT 4:3         │ OBJETIVO     │
│ STATUS       │           GAMEPLAY           │ SCANNER      │
│              │                              │ ALERTAS      │
└──────────────┴──────────────────────────────┴──────────────┘
```

## Regra

O viewport domina. Painéis organizam informação e não competem com combate. Alertas
contextuais são temporários.

## Escala

Layout deve funcionar em 16:9, 16:10 e ultrawide sem depender de pixels fixos.

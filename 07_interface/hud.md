# Interface — HUD

> **Estado:** HUD completo permanece especificado. Desde a Fase 14, objetivo e
> resultado de sessão aparecem no viewport por um passe bitmap provisório;
> HP, munição, chaves, progressão, sala e evento permanecem no título. O objetivo
> alterna entre `REACH BOSS` e `DEFEAT BOSS`.
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

O passe atual desenha o objetivo no topo direito e, em estado terminal, um
banner central `VICTORY`/`DEFEAT` com o comando de `Enter`. Ele ocorre após o
blit, não usa a projeção 3D e é deliberadamente menor que o HUD final.

## Escala

Layout deve funcionar em 16:9, 16:10 e ultrawide sem depender de pixels fixos.

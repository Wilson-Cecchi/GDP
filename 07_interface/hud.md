# Interface — HUD

> **Estado:** HUD funcional implementado na Fase 17. HP, munição, chaves, nível,
> XP, objetivo, evento, resultado e prompts contextuais aparecem no viewport;
> o título permanece somente como apoio de diagnóstico.
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

O passe atual ocorre após o blit e não usa a projeção 3D. HP, munição e chaves
ocupam o canto inferior esquerdo; nível e XP, o inferior direito; objetivo e
evento, o superior direito. `VICTORY`/`DEFEAT` preservam o banner central. Os
prompts `E OPEN DOOR`, `DOOR LOCKED`, `E USE TERMINAL` e `R RELOAD` aparecem
somente quando a mesma ação é válida ou necessária na simulação.

## Escala

Layout deve funcionar em 16:9, 16:10 e ultrawide sem depender de pixels fixos.

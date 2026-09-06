# Interface — HUD

> **Estado:** HUD funcional implementado na Fase 17. HP, munição, chaves, nível,
> XP, objetivo, evento, resultado e prompts contextuais aparecem no viewport;
> a Fase 18 acrescenta feedback temporário de combate e eventos e a Fase 58
> integra Armor. A Fase 82 expõe a pressão do andar da Campanha. O título
> permanece somente como apoio de diagnóstico.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Direção

Interface militar/industrial. Em widescreen, o centro preserva uma área de gameplay
conceitualmente 4:3 e as laterais recebem informação.

O painel esquerdo apresenta HP e Armor em linhas e barras independentes. Armor
usa azul técnico, é exibida mesmo vazia e permite perceber exatamente quando o
dano começou a alcançar o HP.

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

Durante a Campanha, a linha do cronômetro inclui `FLOOR n/3` e `THREAT 100%`,
`110%` ou `120%`. O renderer recebe o percentual pronto em `HudRenderData`; ele
não calcula dificuldade nem altera a simulação.

## Feedback temporário

Dano recebido acende as bordas sem cobrir a área central. Acertos usam quatro
marcas claras ao redor da mira e eliminações mudam o marcador para vermelho.
Coleta, início de evento, nova onda, setor limpo e level-up usam mensagens
curtas com duração controlada pelo fixed update; a UI apenas apresenta o
snapshot e não controla a simulação.

## Escala

Layout deve funcionar em 16:9, 16:10 e ultrawide sem depender de pixels fixos.

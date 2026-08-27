# Core — Game loop e tempo

> **Estado:** base de tempo e input implementada e validada na Fase 1; estados
> de jogo permanecem planejados para as fases de gameplay/interface.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `architecture.md`
- `README(6).md — estados de jogo/menu`

## Objetivo

Executar input, simulação e renderização numa ordem previsível e independente da taxa de frames.

## Estados

```cpp
enum class GameState {
    Boot, MainMenu, Loading, Playing, Paused,
    GameOver, FloorComplete, Victory, Shutdown
};
```

## Loop

```text
SDL events → input snapshot → accumulate time → 0..N fixed updates → RenderFrame → render → present
```

## Fixed update

Proposta inicial: `1/60 s`. Física, IA e combate não podem depender do FPS de render.

```cpp
accumulator += frameDelta;
while (accumulator >= fixedStep) {
    game.fixedUpdate(fixedStep);
    accumulator -= fixedStep;
}
```

Limitar acumulador após travamentos para evitar spiral of death.

## Implementação da Fase 1

`FixedStepClock` centraliza o passo de `1/60 s`, limita cada frame acumulado a
`0,25 s`, expõe o alfa de interpolação e conta updates executados. `Input`
mantém estados held/pressed/released de teclado e mouse e acumula movimento do
mouse; as transições e deltas são limpos no começo de cada frame, antes da fila
SDL ser processada. Repetição automática de tecla não cria um novo pressed.

## Ordem lógica

```text
1 input/comandos
2 portas/interações
3 IA
4 movimento
5 colisão
6 combate/projéteis
7 dano/morte
8 pickups/objetivos
9 eventos de sala
10 limpeza de entidades
```

## Pause

Gameplay para; UI continua; áudio pode reduzir. Regras de cronômetro ficam em
`04_gameplay/loot_and_progression.md`.

## Loading

Gerar mundo temporário → validar → criar recursos → commit atômico.

## Métricas

Frame time, fixed updates/frame, geração, upload GPU, entidades, draw calls e memória.

## Testes

- 10 s simulados com 30/60/144 FPS de render produzem estado equivalente;
- menu bloqueia movimento;
- pause não avança combate;
- shutdown libera recursos corretamente.

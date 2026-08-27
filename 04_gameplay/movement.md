# Gameplay — Movimento e câmera

> **Estado:** movimento, mouse look, gravidade e colisão básica implementados e
> validados na Fase 3; portas e head bob permanecem planejados.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Controles modernos

`W/S` frente/trás, `A/D` strafe, mouse yaw/pitch, `E` interação, números/roda para armas e
`Esc` para pausa. Um modo clássico posterior deve alterar apenas o mapeamento.

## Fixed update

```text
input
→ vetor desejado
→ normalização diagonal
→ velocidade
→ collision solve
→ posição final
```

A simulação usa timestep fixo; mouse look não deve ser multiplicado por delta de frame.

Na implementação atual, velocidade é `4,5 m/s`, gravidade é `-20 m/s²` e a
entrada diagonal é normalizada. O mouse usa sensibilidade inicial de `0,12°`
por unidade e é consumido uma única vez por frame, enquanto movimento e
gravidade executam a `60 Hz`.

## Colisão

Capsule/cilindro vertical simplificado é suficiente inicialmente. Portas verificam volume
ocupado antes de fechar; nunca empurram o jogador através de parede.

O collider inicial tem raio `0,30 m` e altura `1,80 m`. O solver restringe
piso/teto e limites internos da sala e resolve X e Z separadamente contra AABBs,
permitindo deslizamento. Portas serão adicionadas junto aos `Connector`s.

## Câmera

Yaw em torno do eixo vertical e pitch limitado aproximadamente a ±89°. FOV altera apenas a
projeção. Head bob é cosmético.

## Testes

Distância equivalente em diferentes FPS; diagonal normalizada; colisão em canto; porta;
pitch clamp; sensibilidade independente do FPS.

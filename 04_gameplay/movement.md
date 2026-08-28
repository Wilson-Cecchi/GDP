# Gameplay — Movimento e câmera

> **Estado:** movimento, mouse look, gravidade e colisão básica implementados na
> Fase 3; portas e corredores contínuos implementados na Fase 13. Head bob
> permanece planejado.
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
piso/teto e uma união de volumes navegáveis e resolve X e Z separadamente contra
AABBs, permitindo deslizamento. Origem e rotação do `RoomModule` transformam
bounds e sólidos juntos. Em estado normal, essa união contém a sala atual, os
segmentos dos corredores conectados e suas salas destino. Cruzar os bounds do
destino troca o `roomId` automaticamente sem zerar velocidade; HP, inventário,
progressão e arma são preservados. Próximo a uma porta, olhar e usar `E` mantém
o reposicionamento imediato como fallback. Lockdown remove corredores e salas
adjacentes da união e consome a interação sem permitir saída.

Na Fase 14, a união visual acompanha a união navegável: conteúdo e decoração da
sala diretamente conectada são enviados ao renderer antes da entrada, cada sala
mantém seu próprio material e a luz ambiente converge durante o corredor. A IA
e as regras de coleta continuam limitadas ao `roomId` atual.

## Câmera

Yaw em torno do eixo vertical e pitch limitado aproximadamente a ±89°. FOV altera apenas a
projeção. Head bob é cosmético.

## Testes

Distância equivalente em diferentes FPS; diagonal normalizada; colisão em canto;
porta nos dois sentidos para cada corredor; caminhada segmentada contínua;
lockdown físico; distância/orientação da interação; preservação de estado;
quatro rotações; pitch clamp; sensibilidade independente do FPS.

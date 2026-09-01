# Gameplay — Movimento e câmera

> **Estado:** movimento, mouse look, gravidade e colisão básica implementados na
> Fase 3; corredores contínuos implementados na Fase 13; portas físicas
> animadas e sem teleporte implementadas na Fase 15; áudio dedicado de portas
> implementado na Fase 59; passos por distância implementados na Fase 60; head
> bob cosmético implementado na Fase 61.
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

Depois do collision solve, `FootstepState` compara as posições anterior/final e
acumula apenas XZ grounded. A passada ocorre a cada `1,60 m`, portanto a cadência
não depende da taxa de frames nem do input quando uma parede bloqueia movimento.
Estado aéreo/morto zera a distância parcial; deslocamento único acima de `0,50 m`
é tratado como relocação e também não emite.

## Colisão

Capsule/cilindro vertical simplificado é suficiente inicialmente. Portas verificam volume
ocupado antes de fechar; nunca empurram o jogador através de parede.

O collider inicial tem raio `0,30 m` e altura `1,80 m`. O solver restringe
piso/teto e uma união de volumes navegáveis e resolve X e Z separadamente contra
AABBs, permitindo deslizamento. Origem e rotação do `RoomModule` transformam
bounds e sólidos juntos. Uma edge fechada mantém seu corredor fora da união.
Olhar para a porta e usar `E` inicia `Opening`; a rota entra na união a 90% de
abertura. A porta permanece aberta enquanto o jogador ocupa o vão/corredor,
aguarda 1,6 s após a saída e então fecha. Cruzar os bounds do destino troca o
`roomId` automaticamente sem zerar velocidade; HP, inventário, progressão e arma
são preservados. Lockdown bloqueia imediatamente a rota e força `Closing/Locked`.
As transições reais para abrir, fechar e concluir a trava emitem cues
espacializados distintos; interagir repetidamente durante `Opening/Open` não
duplica o som.

Na Fase 14, a união visual acompanha a união navegável: conteúdo e decoração da
sala diretamente conectada são enviados ao renderer antes da entrada, cada sala
mantém seu próprio material e a luz ambiente converge durante o corredor. A IA
e as regras de coleta continuam limitadas ao `roomId` atual.

## Câmera

Yaw em torno do eixo vertical e pitch limitado aproximadamente a ±89°. FOV altera apenas a
projeção.

`ViewMotionState` usa distância XZ pós-colisão e um ciclo de `3,20 m`. O offset
máximo é `0,035 m` vertical e `0,018 m` lateral, com blend de entrada/saída a
`7/s`. Parar ou sair do chão reduz o peso sem avançar a fase; deslocamento acima
de `0,50 m` reseta o estado como proteção contra relocação. A Application soma
esse offset somente à `CameraData` entregue ao renderer. `Player::cameraPosition`,
`forwardDirection`, raycasts, hitscan, IA e interação não são alterados. Uma
configuração futura poderá reduzir ou desativar o efeito sem tocar na simulação.

## Testes

Distância equivalente em diferentes FPS; diagonal normalizada; colisão em canto;
porta nos dois sentidos para cada corredor; estados e limiar de abertura;
caminhada segmentada contínua; bloqueio individual; lockdown físico;
distância/orientação da interação; preservação de estado;
quatro rotações; sinais únicos de abrir/fechar/travar; pitch clamp;
cadência de passos por deslocamento real, silêncio parado/aéreo e reset;
head bob determinístico, limites de conforto, fade e proteção contra teleporte;
sensibilidade independente do FPS; câmera lógica sem offset cosmético.

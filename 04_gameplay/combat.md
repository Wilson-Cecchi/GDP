# Gameplay — Combate, armas e dano

> **Estado:** pistola, munição, cooldown, recarga, hitscan, dano e morte
> implementados e validados na Fase 7; Guard armado de média distância validado
> na Fase 11, junto do Officer de suporte, Engineer de reparo e IndustrialRobot
> de três fases. Props destrutíveis e cobertura dinâmica foram validados na
> Fase 12. Eventos procedurais de sala estão funcionais; Armor do Player foi
> integrada na Fase 58. A fila desacoplada de sinais de combate, além de spread
> e weak points, permanece
> para fases posteriores.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Pipeline

```text
Fire input
→ Weapon valida cooldown/munição
→ raycast ou projétil
→ HitResult
→ DamageSystem
→ eventos visuais/sonoros
```

## Hitscan

O ray lógico parte da câmera/retícula e para no primeiro bloqueador válido. O cano visual não
define a direção real do tiro.

`fireHitscan()` normaliza a direção, limita pelo alcance, consulta somente
Enemies vivos e Props sólidos no `roomId` do jogador e compara o primeiro AABB
de alvo com o primeiro solid ou limite da sala. O resultado distingue cooldown,
vazio, miss, impacto no mundo, prop protegido, hit, kill e prop destruído.

## WeaponDefinition

```cpp
struct WeaponDefinition {
    float damage;
    float fireInterval;
    float range;
    int magazineSize;
    int reserveMax;
};
```

A pistola inicial usa 34 de dano, intervalo de `0,22 s`, alcance de `30 m`,
pente de 8 e reserva de 32. Botão esquerdo dispara e `R` transfere a quantidade
necessária da reserva para o pente. Progressão pode aumentar dano em blocos de
4 e reserva máxima em blocos de 4 sem substituir a arma.

## Dano

`applyDamage()` opera sobre Enemy ou Prop destrutível, ignora IDs inválidos,
dano não positivo e alvos terminais, faz clamp de HP em zero e produz uma única
transição terminal. Props passam por `Damaged`, perdem colisão em `Destroyed` e
um crate cria somente seu drop controlado. Weak points entram quando forem
visualmente legíveis.

Na Fase 58, a defesa do Player passou a consumir Armor ponto a ponto antes do
HP. O excedente do mesmo ataque alcança HP, Armor nunca fica negativo e zerar
somente Armor não mata. A regra independe do arquétipo atacante e não altera o
pipeline de dano de Enemy/Prop.

## Eventos

`ShotResult` já separa os resultados lógicos consumidos pela aplicação. A fila
desacoplada `WeaponFired`, `EntityHit`, `EntityDamaged`, `EntityKilled` e
`ImpactWorld` permanece planejada para renderer e áudio. O sistema atual de
eventos de sala é independente dessa futura fila: ele consome estado de
encontro e produz ondas, lockdown e recompensa determinísticos.

Enquanto a fila genérica permanece planejada, `Application` traduz resultados
já consolidados em cues procedurais: tiro/arma vazia, recarga, hit/morte,
impacto/destruição de prop, impacto no mundo, coleta, dano recebido e
ativação/conclusão de evento. Cues espaciais usam o ponto do hitscan; nenhum som
altera resultado, RNG estrutural ou ordem de combate.

## Testes

Implementados: sem munição, cooldown, recarga, parede bloqueia, filtro por sala,
alvo mais próximo, overkill, double death, miss por alcance, aproximação do
Guard, recuo sob pressão, envelope de tiro, cooldown e bloqueio de linha de
visão, comando do Officer, filtro de aliados por papel/range/sala e cooldown de
suporte independente, cura/restauração do Engineer, escolha do alvo reparável
mais próximo e bloqueio de reparo por sólido/sala. Spread determinístico e weak point entram com suas
respectivas features. O Boss também testa limiares exatos de HP, três envelopes,
velocidades/danos/cadências distintos, recuo, perseguição, line of sight,
isolamento por sala, cooldown e estado Dead. Props acrescentam prioridade do
alvo mais próximo, cobertura, três estados, reparo, colisão reconstruída, drop
único e proteção de terminal.

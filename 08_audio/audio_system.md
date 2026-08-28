# Audio — Sistema e procedural

> **Estado:** síntese procedural, mixer estéreo, ambiência por tema, integração
> de gameplay e fallback silencioso implementados na Fase 12.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Pipeline

```text
Gameplay Event → AudioEvent → AudioSystem → voice → spatialization → output
```

## Categorias implementadas

`WeaponFire`, `DryFire`, `Reload`, `EnemyHit`, `EnemyDeath`, `PropImpact`,
`PropDestroyed`, `Pickup`, `EventStart`, `EventComplete`, `PlayerHurt` e
`Ambient`. Footstep, Door e UI dedicada permanecem planejados.

## Mixer e buses

`AudioSystem` mistura até 16 vozes em estéreo float de 48 kHz. Master, Effects
e Ambient possuem volumes independentes; UI permanece planejado. A mixagem
limita a saída a `[-1, 1]`, descarta one-shots concluídos e reutiliza clips pelo
hash integral da receita.

## Variação procedural

`generateAudioRecipe()` usa somente `SeedDomain::Audio` e combina pitch,
duração, envelope, ruído, pulso e ganho. `synthesizeAudioClip()` produz a onda
em memória: nenhum WAV externo é necessário. A mesma seed/kind/counter
reproduz receita e samples bit a bit.

## Entidades

Eventos usam `sourceSeed + kind + eventCounter`, sem consumir RNG estrutural.
`playAt()` calcula atenuação por distância e pan equal-power a partir da posição
do listener. Disparo, impacto e destruição usam localização; recarga, coleta,
dano recebido e estados de evento usam o centro estéreo.

## Ambiência

Os sete `RoomTheme` fornecem fundações distintas de frequência, modulação e
ruído para um loop ambiente. A START seleciona seu perfil na inicialização e
cada travessia de porta troca o loop pela seed/tema da sala destino.
Máquinas locais e eventos ambientes raros permanecem planejados e deverão usar
budget temporal, nunca chance por frame.

## SDL e headless

O jogo abre um `SDL_AudioStream` estéreo e alimenta blocos pelo fixed update.
Se o subsistema, dispositivo ou stream falhar, registra warning e continua em
modo silencioso. Headless executa síntese, vozes e mixer sem abrir dispositivo,
preservando cobertura determinística. O shutdown destrói o stream antes de
encerrar SDL.

## Testes

O gate cobre 1.000 receitas/hash únicos, todas as doze categorias, sete perfis
ambientes, hash dourado `0xEAD1C1A48BF9FF7F`, waveform repetível, clipping,
cache, limite de vozes, ciclo de one-shot/loop, pan, distância, dispositivo
dummy e dispositivo ausente. Pause e som dedicado de porta permanecem planejados.

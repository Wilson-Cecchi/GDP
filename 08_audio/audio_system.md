# Audio — Sistema e procedural

> **Estado:** síntese procedural, mixer estéreo, ambiência por tema, integração
> de gameplay e fallback silencioso implementados na Fase 12; crossfade de
> ambiência por corredor implementado na Fase 15.
> Na Fase 43, `wkc_forge_audio` passou a conter contratos, hash, validação e
> síntese reutilizáveis. Eventos, receitas, perfis de ambiência e a fachada
> concreta do GDP pertencem a `src/game/audio` e ao target `gdp_audio`.
> Na Fase 44, `AudioMixer` levou cache, vozes, buses, espacialização, crossfade e
> backend SDL para `wkc_forge_audio`; a fachada GDP apenas fornece receitas.
> Na Fase 45, esses contratos e o mixer passaram para `wkc::audio`; eventos,
> catálogo de receitas e `AudioSystem` permanecem em `gdp::audio`.
> Na Fase 59, portas receberam cues procedurais e espacializados para abertura,
> fechamento e trava.
> Na Fase 60, o Player recebeu passos metálicos procedurais guiados por distância.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Pipeline

```text
Gameplay Event → AudioEvent → AudioSystem → voice → spatialization → output
```

## Categorias implementadas

`WeaponFire`, `DryFire`, `Reload`, `EnemyHit`, `EnemyDeath`, `PropImpact`,
`PropDestroyed`, `Pickup`, `EventStart`, `EventComplete`, `PlayerHurt` e
`Ambient`, além de `DoorOpen`, `DoorClose`, `DoorLock` e `Footstep`. UI dedicada
permanece planejada.

## Mixer e buses

`AudioMixer` mistura até 16 vozes em estéreo float de 48 kHz. Master, Effects
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

Portas usam a seed da run e uma identidade formada por `edge + sequência local`.
Esse contador por edge evita que a iteração de várias portas altere os sons. A
posição vem do connector da mesma edge mais próximo do listener. Abrir ou
reverter fechamento emite `DoorOpen`; iniciar fechamento emite `DoorClose`; a
passagem efetiva para `Locked` emite `DoorLock`. Repetir `E` em uma porta que já
abre ou está aberta não duplica o cue.

`FootstepState` mede deslocamento horizontal depois da resolução de colisão e
emite uma passada a cada 1,60 m. Grounded é obrigatório; parado, no ar ou morto
não toca. Segmentos acima de 0,50 m zeram o acúmulo para que relocação não soe
como caminhada. O índice de passada pertence a esse estado e reinicia com a
run, mantendo variação procedural independente dos demais efeitos.

## Ambiência

Os sete `RoomTheme` fornecem fundações distintas de frequência, modulação e
ruído para um loop ambiente. A START seleciona seu perfil na inicialização. No
corredor, `setAmbienceBlend()` mantém os loops das duas salas sem reiniciar seus
cursors e usa ganhos `sqrt(1-t)`/`sqrt(t)`; ao entrar, preserva somente o loop da
sala destino. O peso vem da projeção sobre o `CorridorPath` com `smoothstep`.
Máquinas locais e eventos ambientes raros permanecem planejados e deverão usar
budget temporal, nunca chance por frame.

## SDL e headless

O jogo abre um `SDL_AudioStream` estéreo e alimenta blocos pelo fixed update.
Se o subsistema, dispositivo ou stream falhar, registra warning e continua em
modo silencioso. Headless executa síntese, vozes e mixer sem abrir dispositivo,
preservando cobertura determinística. O shutdown destrói o stream antes de
encerrar SDL.

## Testes

O gate cobre 1.000 receitas/hash únicos, todas as dezesseis categorias, sete perfis
ambientes, hash dourado `0xEAD1C1A48BF9FF7F`, waveform repetível, clipping,
cache, limite de vozes, ciclo de one-shot/loop, crossfade de duas ambiências,
pan, distância, assinaturas distintas das três transições de porta, dispositivo
dummy e dispositivo ausente. A cadência de passos testa distância real,
grounded, bloqueio de teleporte e reset. Pause e UI dedicada permanecem
planejados.

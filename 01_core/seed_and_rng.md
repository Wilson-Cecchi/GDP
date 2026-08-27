# Core — Seed, RNG e determinismo

> **Estado:** contrato inicial implementado e validado na Fase 1.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `architecture.md — Seed hierárquica`
- `README(6).md — Seed como geradora do universo`

## Objetivo

Garantir que a mesma seed e a mesma versão das regras reproduzam o mesmo universo sem acoplamento acidental entre sistemas.

## Regra central

É proibido um único RNG global consumido por mapa, áudio, loot, inimigos e texturas.

```text
MASTER
├── WORLD
├── GAMEPLAY
├── VISUAL
└── AUDIO
```

## Tipos

```cpp
using Seed = std::uint64_t;

enum class SeedDomain : std::uint64_t {
    World, Graph, Room, RoomModule, Corridor,
    Props, Lighting, EnemySpawn, EnemyVisual,
    EnemyBody, EnemyParts, EnemyMaterial, EnemyWear,
    Loot, Events, Audio, RoomMaterial
};

Seed deriveSeed(Seed parent, SeedDomain domain, std::uint64_t localId);
```

`deriveSeed` deve ser determinística, independente da ordem de chamadas e versionada. `localId` representa identidade estável (`roomId`, `edgeId`, `spawnIndex`, `partSlot`), nunca apenas quantas vezes uma função foi chamada.

## RandomStream

```cpp
class RandomStream {
public:
    explicit RandomStream(Seed seed);
    std::uint32_t nextU32();
    std::uint64_t nextU64();
    float uniform01();
    int rangeInt(int minInclusive, int maxInclusive);
    float rangeFloat(float minInclusive, float maxExclusive);
    bool chance(float probability);
};
```

## Implementação da Fase 1

- `RandomStream` usa SplitMix64 com sequência dourada coberta por teste;
- `uniform01()` usa 24 bits significativos e retorna no intervalo `[0, 1)`;
- `rangeInt()` é inclusivo e usa rejeição para evitar viés de módulo;
- `deriveSeed()` combina parent, domínio, identidade local e versão de geração
  exclusivamente com aritmética inteira de 64 bits;
- os 17 domínios têm IDs numéricos explícitos e estáveis;
- a versão corrente da geração é `1.0`.

## Árvore de sala

```text
master → world → room[7]
                  ├── props
                  ├── lighting
                  ├── roomMaterial
                  ├── enemySpawn
                  └── loot
```

Alterar iluminação não pode alterar loot.

## Árvore de inimigo

```text
enemyVisualSeed
├── body
├── parts
├── material
├── wear
└── accessories
```

Alterar desgaste não pode trocar BODY/PARTS.

## Versionamento

```cpp
struct GenerationVersion { std::uint32_t major, minor; };
```

Toda mudança deliberada que altera resultados procedurais exige alteração da versão de geração.

## Proibições

- seed baseada em relógio para reproduzir uma partida identificada;
- RNG global escondido;
- endereço de memória como aleatoriedade;
- ordem de threads influenciando resultado.

## Testes

- mesma seed → mesma sequência;
- domínios independentes;
- Debug/Release → mesmos IDs procedurais;
- mudança cosmética não altera gameplay;
- salvar/carregar preserva seed;
- dump da árvore de seeds reproduz falha.

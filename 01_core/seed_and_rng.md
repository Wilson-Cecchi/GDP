# Core — Seed, RNG e determinismo

> **Estado:** contrato inicial implementado na Fase 1; fronteira Engine × GDP
> separada e validada na Fase 29.
> Na Fase 53, `mix64`, `hashCombine` e `fnv1a64` passaram para `wkc::core`,
> preservando a derivação e todos os valores golden existentes.
> Na Fase 54, `Seed`, `GenerationVersion`, `deriveSeed` e `RandomStream` também
> passaram para `wkc::core`, concluindo a fronteira determinística da engine.
> Na Fase 89, a camada concreta de entrada passou a escolher uma seed variável
> no lançamento gráfico normal, sem alterar a reprodução após a escolha.
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
using SeedDomainId = std::uint64_t;

Seed deriveSeed(Seed parent, SeedDomainId domain, std::uint64_t localId);
```

`deriveSeed` deve ser determinística, independente da ordem de chamadas e versionada. `localId` representa identidade estável (`roomId`, `edgeId`, `spawnIndex`, `partSlot`), nunca apenas quantas vezes uma função foi chamada.

O núcleo aceita IDs numéricos e enums tipados sem conhecer o significado de
seus valores. O catálogo `game::SeedDomain` pertence ao GDP e contém os domínios
World, Graph, Room, RoomModule, Corridor, Props, Lighting, EnemySpawn,
EnemyVisual, EnemyBody, EnemyParts, EnemyMaterial, EnemyWear, Loot, Events,
Audio e RoomMaterial.

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
- os 17 domínios do GDP têm IDs numéricos explícitos e estáveis em
  `game/SeedDomains.hpp`;
- a versão corrente da geração é `1.0`.

## Fronteira da Fase 29

- `core/Seed` possui apenas `Seed`, `SeedDomainId`, `GenerationVersion` e o
  algoritmo genérico `deriveSeed`;
- `game/SeedDomains` possui o vocabulário procedural específico do GDP;
- os IDs serializados 1–17 e o golden de derivação de sala continuam iguais;
- a regressão arquitetural impede que o enum específico retorne ao núcleo.

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

O relógio e a entropia do sistema podem participar somente da escolha inicial
de uma execução gráfica sem `--seed`. O número resultante torna-se a master seed
identificada no log; nenhum gerador consulta relógio ou entropia depois disso.
Execuções headless e seeds explícitas ignoram essa fonte variável.

## Testes

- mesma seed → mesma sequência;
- domínios independentes;
- Debug/Release → mesmos IDs procedurais;
- mudança cosmética não altera gameplay;
- salvar/carregar preserva seed;
- dump da árvore de seeds reproduz falha.

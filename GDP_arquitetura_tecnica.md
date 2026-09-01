# GDP — Arquitetura Técnica

> **Estado:** versão-fonte consolidada do documento-mestre da arquitetura técnica.
>
> Dentro deste pacote, `architecture.md` é a autoridade canônica. Este arquivo é
> preservado para rastrear a consolidação recebida e deve permanecer alinhado às
> mesmas decisões globais, sem funcionar como uma segunda autoridade.
>
> Este arquivo define a **arquitetura global**, os limites entre subsistemas,
> os contratos principais, as decisões estruturais e a ordem geral de
> implementação do projeto.
>
> A implementação detalhada de cada subsistema pertence à **GDP Technical Bible**,
> organizada em documentos especializados. Este arquivo não deve tentar substituir
> esses documentos: ele funciona como a visão central que mantém todos os sistemas
> coerentes entre si.
>
> Stack principal:
>
> ```text
> C++20/23
> +
> SDL3
> +
> OpenGL
> +
> engine própria especializada
> ```
>
> As ideias de gameplay procedural, salas modulares e geração de inimigos do
> projeto anterior foram preservadas como direção de design. Contratos, estruturas
> de dados, geração de meshes, materiais/texturas, cache, streams de RNG e
> validação descritos nesta arquitetura representam a base técnica da nova
> implementação.
>
> Os guias visuais anteriores continuam sendo referências de conteúdo:
>
> - biblioteca detalhada de peças de inimigos;
> - especificação de materiais e texturas por ambiente;
> - referências visuais de salas, inimigos e materiais.
>
> A engine deve fornecer os sistemas capazes de representar e expandir essas
> referências sem transformar cada variante em conteúdo codificado manualmente.


---

# Como usar esta arquitetura

Este documento responde principalmente:

```text
quais sistemas existem
+
como eles se relacionam
+
quais responsabilidades pertencem a cada sistema
+
quais invariantes globais precisam ser preservadas
+
em qual ordem a engine deve crescer
```

Os documentos especializados respondem:

```text
como cada sistema funciona internamente
+
quais dados recebe e produz
+
qual algoritmo deve ser implementado
+
como falhas são tratadas
+
como o sistema é testado
```

As referências visuais respondem:

```text
como o resultado deve parecer
+
quais características visuais são obrigatórias
+
quais variações continuam aceitáveis
```

A regra prática é:

```text
architecture.md
      ↓
DEFINE O SISTEMA

documento especializado
      ↓
DEFINE COMO O SISTEMA FUNCIONA

referência visual
      ↓
DEFINE COMO O RESULTADO DEVE PARECER
```

## GDP Technical Bible

A documentação especializada deve acompanhar a seguinte organização:

```text
docs/
│
├── architecture.md
│
├── 01_core/
│   ├── seed_and_rng.md
│   ├── game_loop.md
│   ├── math_and_coordinates.md
│   └── configuration.md
│
├── 02_renderer/
│   ├── renderer_pipeline.md
│   ├── meshes.md
│   ├── materials.md
│   ├── procedural_textures.md
│   ├── lighting.md
│   └── shaders.md
│
├── 03_world/
│   ├── world_generation.md
│   ├── map_graph.md
│   ├── room_modules.md
│   ├── connectors.md
│   ├── corridors.md
│   ├── room_content.md
│   └── world_validation.md
│
├── 04_gameplay/
│   ├── player.md
│   ├── movement.md
│   ├── combat.md
│   └── loot_and_progression.md
│
├── 05_enemies/
│   ├── enemy_system.md
│   ├── enemy_generation.md
│   ├── body_and_compatibility.md
│   ├── enemy_materials.md
│   ├── animation_and_ai.md
│   └── part_library_reference.md
│
├── 06_environments/
│   ├── visual_rules.md
│   ├── industrial_corridor.md
│   ├── storage.md
│   ├── combat_room.md
│   ├── cafeteria.md
│   ├── security.md
│   ├── laboratory.md
│   ├── technical.md
│   ├── central_hub.md
│   └── room_materials_reference.md
│
├── 07_interface/
│   ├── hud.md
│   ├── menus_and_settings.md
│   └── debug_ui.md
│
├── 08_audio/
│   └── audio_system.md
│
├── 09_persistence/
│   └── save_and_settings.md
│
├── 10_testing/
│   ├── test_strategy.md
│   ├── procedural_validation.md
│   └── performance_budgets.md
│
└── visual/
    └── referências visuais
```

A estrutura pode crescer conforme novos sistemas forem realmente necessários.

## Hierarquia da documentação

Em caso de conflito entre documentos, usar a seguinte prioridade:

```text
especificação especializada mais recente
        ↓
architecture.md
        ↓
guias e referências visuais
        ↓
documentação histórica do GDP anterior
```

Exceção:

> Uma decisão arquitetural global não deve ser alterada silenciosamente apenas
> dentro de um documento especializado.

Quando uma mudança especializada altera limites entre sistemas, ownership,
pipeline global, determinismo, dependências ou responsabilidades, este
`architecture.md` também deve ser atualizado.

## Regra de sincronização

Toda mudança relevante deve verificar três níveis:

```text
ARQUITETURA
    ↓
o contrato global mudou?

ESPECIFICAÇÃO
    ↓
o funcionamento interno mudou?

REFERÊNCIA VISUAL
    ↓
o resultado esperado mudou?
```

Atualizar apenas os níveis realmente afetados.

---

# 1. Visão geral

O GDP é um FPS procedural retro-industrial que combina três ideias centrais:

```text
DOOM clássico
    ↓
FPS rápido
engine pequena
controle direto

.kkrieger
    ↓
conteúdo procedural
assets gerados
representação compacta

Lethal Company
    ↓
salas modulares
montagem procedural de interiores
```

A proposta não é copiar os sistemas internos desses jogos.

O GDP utiliza essas referências como princípios:

```text
GAMEPLAY CONTROLADO
        +
MUNDO MODULAR
        +
CONTEÚDO PROCEDURAL
        +
ENGINE ESPECIALIZADA
```

Objetivo final:

> Gerar automaticamente mapas, ambientes, inimigos, materiais e variações de
> gameplay que aparentem ter sido projetados manualmente.

---

# 2. Decisão principal de tecnologia

Stack escolhida:

```text
C++20/23
│
├── SDL3
│   ├── janela
│   ├── eventos
│   ├── teclado
│   ├── mouse
│   ├── gamepad
│   ├── integração com plataforma
│   └── áudio básico / backend
│
├── OpenGL
│   ├── buffers
│   ├── meshes
│   ├── shaders
│   ├── texturas
│   ├── framebuffer
│   └── renderização
│
└── GDP Engine
    ├── Core
    ├── Renderer
    ├── World
    ├── Procedural
    ├── Gameplay
    ├── Audio
    ├── Resources
    └── Debug
```

A engine não será genérica.

Ela existe para executar o GDP.

---

# 3. Objetivos da engine

A engine deve permitir:

- criar uma janela e processar input;
- executar um game loop previsível;
- representar mundo 3D;
- gerar mapas por seed;
- montar salas modulares;
- gerar meshes proceduralmente;
- gerar materiais e texturas proceduralmente;
- montar inimigos por peças;
- renderizar grandes quantidades de variações;
- executar IA e combate;
- reproduzir resultados através de seed;
- testar subsistemas isoladamente;
- reutilizar assets gerados quando possível.

---

# 4. O que a engine NÃO precisa ser

Não é objetivo construir:

```text
editor visual universal
+
física universal
+
scripting universal
+
sistema cinematográfico
+
pipeline AAA genérico
+
editor de terreno
+
sistema universal de partículas
+
marketplace
+
sistema genérico de plugins
```

Regra:

> Um sistema só deve entrar na engine quando existir uma necessidade concreta do GDP.

---

# 5. Princípios arquiteturais

## 5.1. Determinismo

A mesma seed, usando a mesma versão das regras, deve produzir o mesmo resultado.

---

## 5.2. Separação de responsabilidade

```text
GERAÇÃO
    ↓
cria dados

GAMEPLAY
    ↓
modifica estado do jogo

RENDERER
    ↓
visualiza estado

ÁUDIO
    ↓
representa eventos sonoros
```

O renderer não decide gameplay.

O gameplay não decide a aparência procedural detalhada.

---

## 5.3. Procedural com regras

O procedural decide:

```text
o que
onde
quanto
qual variante
```

Bibliotecas e contratos definem:

```text
como deve parecer
como deve encaixar
quais combinações são válidas
quais limites não podem ser quebrados
```

---

## 5.4. Dados primeiro

Sempre que possível:

```text
regra
↓
estrutura de dados
↓
sistema genérico
```

em vez de:

```text
if especial
+
if especial
+
if especial
```

espalhados pelo código.

---

## 5.5. Validação como parte do gerador

Um resultado procedural não é válido apenas porque foi produzido.

Fluxo:

```text
GENERATE
   ↓
VALIDATE
   ↓
VALID?
├── YES → ACCEPT
└── NO  → REGENERATE / REPAIR / REJECT
```

---

# 6. Arquitetura geral

```text
                               GDP
                                │
        ┌───────────────────────┼────────────────────────┐
        │                       │                        │
        ▼                       ▼                        ▼
      CORE                    WORLD                  GAMEPLAY
        │                       │                        │
        │                       │                        │
        ▼                       ▼                        ▼
      RNG                WORLD GENERATOR              ENTITY
      TIME                    ROOMS                   PLAYER
      INPUT                 CONNECTORS                WEAPONS
      LOGGING               CORRIDORS                 ENEMIES
      CONFIG                 CONTENT                    IA
        │                       │                        │
        └──────────────┬────────┴───────────┬────────────┘
                       │                    │
                       ▼                    ▼
                PROCEDURAL ASSETS       GAME STATE
                       │                    │
             ┌─────────┼────────┐           │
             ▼         ▼        ▼           │
           MESH     MATERIAL  TEXTURE       │
             │         │        │           │
             └─────────┼────────┘           │
                       ▼                    ▼
                    RESOURCES ----------> RENDERER
                       │                    │
                       ▼                    ▼
                     CACHE                OPENGL
```

---

# 7. Estrutura de diretórios sugerida

```text
gdp/
│
├── CMakeLists.txt
├── README.md
├── CHANGELOG.md
│
├── src/
│   ├── main.cpp
│   │
│   ├── game/
│   │   ├── Application.cpp
│   │   ├── Application.hpp
│   │   ├── SeedDomains.hpp
│   │   ├── audio/
│   │   │   ├── AudioRecipes.cpp
│   │   │   ├── AudioRecipes.hpp
│   │   │   ├── AudioSystem.cpp
│   │   │   └── AudioSystem.hpp
│   │   └── visual/
│   │       ├── EnemyGenerator.cpp
│   │       ├── EnemyGenerator.hpp
│   │       ├── GdpRenderData.cpp
│   │       ├── GdpRenderData.hpp
│   │       ├── GdpRenderer.cpp
│   │       ├── GdpRenderer.hpp
│   │       ├── RoomMaterialGenerator.cpp
│   │       ├── RoomMaterialGenerator.hpp
│   │       ├── VisualAssets.cpp
│   │       └── VisualAssets.hpp
│   │
│   ├── core/
│   │   ├── Time.cpp
│   │   ├── Time.hpp
│   │   ├── Random.cpp
│   │   ├── Random.hpp
│   │   ├── Hash.cpp
│   │   ├── Hash.hpp
│   │   ├── Log.cpp
│   │   └── Log.hpp
│   │
│   ├── platform/
│   │   ├── Window.cpp
│   │   ├── Window.hpp
│   │   ├── Input.cpp
│   │   ├── Input.hpp
│   │   ├── SdlInput.cpp
│   │   └── SdlInput.hpp
│   │
│   ├── render/
│   │   ├── Camera.cpp
│   │   ├── Camera.hpp
│   │   ├── Mesh.cpp
│   │   ├── Mesh.hpp
│   │   ├── Texture.cpp
│   │   ├── Texture.hpp
│   │   ├── Material.cpp
│   │   ├── Material.hpp
│   │   ├── Shader.cpp
│   │   ├── Shader.hpp
│   │   └── Framebuffer.cpp
│   │
│   ├── world/
│   │   ├── World.cpp
│   │   ├── World.hpp
│   │   ├── Room.cpp
│   │   ├── Room.hpp
│   │   ├── RoomModule.cpp
│   │   ├── RoomModule.hpp
│   │   ├── Connector.cpp
│   │   ├── Connector.hpp
│   │   ├── MapGraph.cpp
│   │   └── MapGraph.hpp
│   │
│   ├── procedural/
│   │   ├── AssetRecipe.cpp
│   │   ├── AssetRecipe.hpp
│   │   ├── MeshGenerator.cpp
│   │   ├── MeshGenerator.hpp
│   │   ├── TextureGenerator.cpp
│   │   └── TextureGenerator.hpp
│   │
│   ├── gameplay/
│   │   ├── Game.cpp
│   │   ├── Game.hpp
│   │   ├── Entity.cpp
│   │   ├── Entity.hpp
│   │   ├── EntityManager.cpp
│   │   ├── EntityManager.hpp
│   │   ├── Player.cpp
│   │   ├── Player.hpp
│   │   ├── Enemy.cpp
│   │   ├── Enemy.hpp
│   │   ├── Weapon.cpp
│   │   ├── Weapon.hpp
│   │   ├── Combat.cpp
│   │   └── Combat.hpp
│   │
│   ├── resources/
│   │   ├── ResourceManager.cpp
│   │   ├── ResourceManager.hpp
│   │   ├── AssetCache.cpp
│   │   └── AssetCache.hpp
│   │
│   ├── audio/
│   │   ├── AudioFoundation.cpp
│   │   ├── AudioFoundation.hpp
│   │   ├── AudioMixer.cpp
│   │   └── AudioMixer.hpp
│   │
│   └── debug/
│       ├── DebugDraw.cpp
│       ├── DebugDraw.hpp
│       ├── DebugUI.cpp
│       └── DebugUI.hpp
│
├── shaders/
│
├── tests/
│   ├── test_random.cpp
│   ├── test_world_generator.cpp
│   ├── test_room_connectors.cpp
│   ├── test_enemy_generator.cpp
│   ├── test_texture_generator.cpp
│   └── test_material_generator.cpp
│
└── docs/
    ├── architecture.md
    │
    ├── 01_core/
    ├── 02_renderer/
    ├── 03_world/
    ├── 04_gameplay/
    ├── 05_enemies/
    ├── 06_environments/
    ├── 07_interface/
    ├── 08_audio/
    ├── 09_persistence/
    ├── 10_testing/
    │
    ├── visual/
    └── adr/
```

A pasta de código pode começar muito menor.

A estrutura de `src/` representa o destino arquitetural da engine, não o primeiro
commit. A estrutura de `docs/`, por outro lado, representa a organização oficial
da Technical Bible e deve acompanhar a evolução dos subsistemas implementados.

`game/Application` pertence à camada concreta do GDP: compõe plataforma,
gameplay, mundo, áudio e renderização. Ela pode depender da infraestrutura, mas
não integra o núcleo reutilizável nem pode ser uma dependência dele.

As dependências entre targets devem ser declaradas no CMake. O diretório `core`
só pode incluir seus próprios contratos; uma regressão automatizada protege essa
fronteira enquanto os demais subsistemas são separados incrementalmente.

`platform/Input` mantém o snapshot de teclado/mouse em tipos neutros, sem expor
SDL. `platform/SdlInput` é um adaptador separado que traduz eventos do backend
para esse contrato. Assim, estado de input e integração de plataforma podem ser
testados e substituídos independentemente.

`core/Seed` fornece a derivação genérica e versionada a partir de um ID numérico,
sem conhecer os sistemas do GDP. O catálogo tipado `game/SeedDomains` pertence
ao jogo; seus IDs explícitos 1–17 são parte do contrato determinístico e não
podem mudar sem uma alteração deliberada da versão de geração.

A fronteira visual intermediária possui cinco targets: `wkc_forge_render`
contém Camera, Framebuffer, OpenGL, Mesh, Shader e Texture;
`wkc_forge_procedural` contém receitas, hashing e geradores genéricos;
`wkc_forge_resources` publica o cache genérico header-only;
`gdp_visual_content` contém Worker, materiais de sala e `game/visual`; e
`gdp_visual_renderer` compõe a apresentação concreta do GDP. Esses nomes
de target GDP permanecem temporários até suas fronteiras estabilizarem;
`wkc_forge_render` é a primeira adoção explícita do nome da engine.

Na Fase 31, todo o conteúdo desse target foi agrupado fisicamente em
`src/game/visual`, e `src/procedural` ficou restrito à fundação reutilizável.
Os namespaces anteriores foram preservados temporariamente para que a mudança
de localização não seja misturada a um rename amplo.

Na Fase 32, o renderer concreto também passou para
`src/game/visual/GdpRenderer.*`. `src/render` agora contém somente Camera,
Framebuffer, GlApi, Mesh, Shader e Texture. A regressão arquitetural cobre toda
a pasta, inclusive arquivos adicionados no futuro.

Na Fase 33, a classe concreta passou de `gdp::render::Renderer` para
`gdp::render::GdpRenderer`, removendo o nome genérico da composição do jogo.

Na Fase 34, os DTOs de câmera, sala, entidades, HUD e iluminação foram extraídos
para o contrato `src/game/visual/GdpRenderData.*`. O namespace foi preservado
temporariamente para separar essa mudança estrutural de um futuro rename e não
confundir tipos GDP com `Camera`, `ImageData`, `Mesh` e `Texture` reutilizáveis.

Na Fase 35, esse contrato passou de `gdp::render` para
`gdp::game::visual`. A aplicação produz dados pertencentes ao jogo e o renderer
concreto apenas os consome; as primitivas da fundação continuam em
`gdp::render`, sem conhecer HUD, objetivos, inimigos ou salas do GDP.

Na Fase 36, `GdpRenderer` também passou para `gdp::game::visual`. A composição
concreta referencia `gdp::render::Camera`, `Framebuffer`, `Mesh`, `Shader` e
`Texture` explicitamente, estabelecendo no código a direção
`GDP visual -> fundação gráfica` sem renomear targets prematuramente.

Na Fase 37, o target concreto passou de `gdp_renderer` para
`gdp_visual_renderer`. O nome separa a composição específica do jogo de
`gdp_render_foundation`; a regressão arquitetural impede que o target ambíguo
retorne antes da futura adoção dos nomes WKC Forge pela fundação.

Na Fase 38, `gdp_render_foundation` passou para `wkc_forge_render`, primeiro
target a assumir explicitamente a identidade da engine. O namespace público
permanece temporariamente `gdp::render`, isolando o rename do target de uma
futura migração das APIs C++.

Na Fase 39, `gdp_core` passou para `wkc_forge_core`. O target contém somente
Hash, Log, Math, Random, Seed e Time, e `wkc_forge_render` agora depende dessa
base sem atravessar um target identificado como GDP. O namespace `gdp::core`
permanece temporariamente compatível.

Na Fase 40, `gdp_platform` e `gdp_platform_sdl` passaram para
`wkc_forge_platform` e `wkc_forge_platform_sdl`. Estado de input e tradução SDL
formam agora a cadeia `wkc_forge_core -> wkc_forge_platform -> SDL`, ainda com
namespace público `gdp::platform` para manter o rename incremental.

Na Fase 41, `gdp_procedural_assets` passou para `wkc_forge_procedural`.
Receitas, hashing e geração de mesh/textura dependem apenas de
`wkc_forge_core` e `wkc_forge_render`; conteúdo Worker, salas e luminária segue
nos targets GDP. O namespace público `gdp::procedural` permanece compatível.

Na Fase 42, `AssetCache.hpp` saiu de `src/procedural` para `src/resources` e
passou a ser publicado pelo target header-only `wkc_forge_resources`. O cache
continua agnóstico ao tipo do recurso e o renderer GDP declara a dependência
explicitamente. API, métricas e namespace permanecem inalterados nesta etapa.

Na Fase 43, contratos, hash, validação e síntese determinística de clips foram
isolados em `wkc_forge_audio`, sem dependências de eventos ou seeds do GDP. O
catálogo `AudioEventKind`, as receitas, os perfis de ambiência e a fachada SDL
orientada ao jogo passaram fisicamente para `src/game/audio` e permanecem em
`gdp_audio`. Hashes, waveforms e namespace público foram preservados.

Na Fase 44, cache de clips, vozes, buses, espacialização, crossfade e backend
SDL foram extraídos para `AudioMixer`, dentro de `wkc_forge_audio`. A fachada
`gdp_audio` preserva a API da aplicação, mas agora somente converte eventos,
seeds e perfis do jogo em `AudioRecipe`; ela não depende diretamente de SDL.

Na Fase 45, os contratos `AudioRecipe`/`AudioClip`, validação, síntese e
`AudioMixer` passaram para `wkc::audio`. `AudioEventKind`, geração de receitas e
`AudioSystem` permanecem em `gdp::audio`; a assinatura do catálogo retorna o
contrato WKC Forge explicitamente, sem aliases reversos ou mudança de dados.

Na Fase 46, `AssetCache` e `AssetCacheStats` passaram de
`gdp::procedural` para `wkc::resources`. O renderer GDP qualifica o cache pela
API WKC Forge, e uma regressão independente liga somente contra
`wkc_forge_resources`. Semântica de ownership, chaves e métricas não mudou.

Na Fase 47, `Input`, `Key`, `MouseButton` e `handleSdlInputEvent` passaram de
`gdp::platform` para `wkc::platform`. A aplicação GDP consome agora a API de
plataforma da WKC Forge de forma explícita, sem aliases reversos e sem mudança
nos snapshots, transições, mapeamentos ou eventos SDL.

Na Fase 48, `Camera`, `Framebuffer`, `GlApi`, `Mesh`, `Shader`, `Texture` e os
tipos auxiliares da fundação gráfica passaram de `gdp::render` para
`wkc::render`. O renderer concreto, o conteúdo visual GDP e os geradores
procedurais consomem a API WKC Forge explicitamente; não há aliases reversos,
mudança de dados ou alteração intencional de pixels.

Na Fase 49, `EnemyGenerator` e `RoomMaterialGenerator` passaram de
`gdp::procedural` para `gdp::game::visual`. Tipos Worker, Guard, Officer,
Engineer, IndustrialRobot e receitas de ambientes agora pertencem
explicitamente ao jogo; os contratos genéricos consumidos permanecem
qualificados em `gdp::procedural`, sem alteração de hashes ou imagens.

Na Fase 50, receitas, validação, hashing e geradores genéricos de mesh e textura
passaram de `gdp::procedural` para `wkc::procedural`. O conteúdo visual GDP
consome a fundação WKC Forge por nomes qualificados; não há aliases reversos,
mudança de receitas, hashes, geometria ou pixels.

Na Fase 51, `FixedStepClock` passou de `gdp::core` para `wkc::core`. A aplicação
GDP referencia agora o contrato de tempo da WKC Forge explicitamente; passo
fixo, limite de delta, acumulador, interpolação e contagem total permanecem
inalterados. Hash, Log, Math, Random e Seed seguem compatíveis nesta etapa.

Na Fase 52, `LogLevel` e `log` passaram de `gdp::core` para `wkc::core`.
Aplicação, áudio e fundação gráfica consomem o diagnóstico WKC Forge por nomes
qualificados; níveis, mensagens e destino de saída permanecem inalterados.

Na Fase 53, `mix64`, `hashCombine` e `fnv1a64` passaram de `gdp::core` para
`wkc::core`. Derivação de seeds, mundo, áudio, receitas procedurais e conteúdo
visual consomem o hashing WKC Forge explicitamente, sem mudar fingerprints ou
valores golden.

Na Fase 54, os contratos matemáticos, `Seed`, `GenerationVersion`,
`deriveSeed` e `RandomStream` passaram para `wkc::core`. Com Time, Log e Hash já
migrados, `wkc_forge_core` não expõe mais `gdp::core`; todos os consumidores
foram atualizados sem aliases e sem alterar coordenadas, RNG ou determinismo.

Na Fase 55, `Entities` e `World` foram confirmados como propriedade do jogo.
Arquétipos de inimigo, itens, props, papéis e temas de sala, encontros e
população codificam regras do GDP, portanto continuam em `gdp_entities`,
`gdp_world`, `gdp::entities` e `gdp::world`. Gates impedem dependências da WKC
Forge em alvos GDP e evitam abstrações prematuras `wkc_forge_entities/world`.

Na Fase 56, a auditoria final concluiu a separação arquitetural dentro do
monorepo. Core, Platform, Render, Resources, Procedural e a fundação de Audio
possuem identidade WKC completa; seus targets não podem ligar `gdp_*` nem seus
arquivos-fonte incluir Game, Gameplay, World ou Entities. Os executáveis de teste
exclusivos de Core e Platform também adotaram nomes WKC, sem renomear os testes
CTest públicos. Application e todas as regras/conteúdos permanecem GDP. Uma
eventual divisão de repositórios fica para uma decisão futura independente.

Na Fase 57, GitHub Actions passou a executar a suíte em Linux nos presets Debug
e Release, além de um job separado com ASan + UBSan. Como headless e OpenGL
offscreen já pertencem ao CTest, ambos entram no gate automaticamente. A
regressão arquitetural exige configure/build/test dos três presets e impede que
a CI aceite ou atualize goldens automaticamente.

Na Fase 58, o Player ganhou Armor funcional com capacidade inicial 50 e reserva
vazia. Dano consome Armor ponto a ponto antes do HP; Vitality aumenta/restaura
15 de HP e 10 de Armor. Coleta, Entity Manager, travessia, HUD e apresentação
técnica do pickup suportam `ItemType::Armor`. A geração procedural de loot e
eventos continua selecionando somente as categorias anteriores para preservar
hashes, budgets e distribuições até uma futura etapa de balanceamento.

Na Fase 59, o catálogo concreto de áudio GDP ganhou `DoorOpen`, `DoorClose` e
`DoorLock`. A Application traduz somente transições reais de `DoorRuntime` em
cues espacializados no connector mais próximo, usando sequência local por edge
para não depender da ordem de iteração quando várias portas mudam juntas. A
fundação WKC Forge continua sem conhecer portas; IDs e golden anteriores foram
preservados.

---

# 8. Sistema de seed

A seed é um dos elementos centrais do GDP.

Não deve existir apenas um RNG global consumido sequencialmente por todos os sistemas.

Isso causaria efeito cascata:

```text
adicionar uma textura
        ↓
consumir um número aleatório extra
        ↓
mudar inimigos
        ↓
mudar loot
        ↓
mudar mundo
```

Esse comportamento deve ser evitado.

---

# 9. Seed hierárquica

Estrutura conceitual:

```text
MASTER SEED
│
├── WORLD
│   ├── GRAPH
│   ├── ROOM_ROLE
│   ├── ROOM_MODULE
│   ├── CONNECTOR
│   └── CORRIDOR
│
├── GAMEPLAY
│   ├── ENEMY_SPAWN
│   ├── LOOT
│   ├── EVENTS
│   └── OBJECTIVES
│
├── VISUAL
│   ├── ROOM_MATERIAL
│   ├── PROP
│   ├── LIGHTING
│   └── ENEMY_VISUAL
│
└── AUDIO
    ├── AMBIENT
    ├── ENEMY
    └── EVENTS
```

---

# 10. Derivação de sub-seeds

Proposta:

```cpp
using Seed = std::uint64_t;
using SeedDomainId = std::uint64_t;

Seed deriveSeed(
    Seed parent,
    SeedDomainId domain,
    std::uint64_t localId
);
```

O GDP define separadamente:

```cpp
enum class SeedDomain : std::uint64_t
{
    World = 1,
    Graph = 2,
    Room = 3,
    RoomModule = 4,
    Corridor = 5,
    Props = 6,
    Lighting = 7,
    EnemySpawn = 8,
    EnemyVisual = 9,
    EnemyBody = 10,
    EnemyParts = 11,
    EnemyMaterial = 12,
    EnemyWear = 13,
    Loot = 14,
    Events = 15,
    Audio = 16,
    RoomMaterial = 17
};
```

Exemplo:

```text
masterSeed
    ↓
derive(WORLD)
    ↓
worldSeed
    ↓
derive(ROOM, roomId)
    ↓
roomSeed
```

Cada sala possui sua própria árvore.

---

# 11. Seed de uma sala

```text
ROOM SEED
│
├── STRUCTURE
├── PROPS
├── MATERIALS
├── LIGHTING
├── ENEMY SPAWN
├── LOOT
└── EVENTS
```

Uma alteração no gerador de iluminação não pode mudar o loot.

---

# 12. Seed de um inimigo

```text
ENEMY SEED
│
├── BODY
├── PARTS
├── MATERIAL
├── WEAR
├── ACCESSORIES
└── VISUAL DETAILS
```

O comportamento do inimigo não deve ser derivado da mesma stream usada para aparência.

---

# 13. Três níveis de proceduralidade

A estrutura original de três níveis continua válida.

---

## 13.1. Nível 1 — Layout do mapa

Define:

```text
quais salas existem
+
como estão conectadas
+
qual caminho é obrigatório
+
quais caminhos são opcionais
```

---

## 13.2. Nível 2 — Conteúdo da sala

Define:

```text
props
+
loot
+
inimigos
+
iluminação
+
eventos
+
decoração
```

---

## 13.3. Nível 3 — Assets procedurais

Define:

```text
meshes
+
texturas
+
materiais
+
aparência dos inimigos
+
desgaste
+
sujeira
+
variações estruturais
```

---

# 14. World Generator

Pipeline:

```text
MASTER SEED
     │
     ▼
MAP GRAPH
     │
     ▼
ROOM ROLES
     │
     ▼
ROOM THEMES
     │
     ▼
ROOM MODULES
     │
     ▼
TRANSFORM / ROTATION
     │
     ▼
CONNECTORS
     │
     ▼
CONNECTION SOLVER
     │
     ▼
CORRIDORS
     │
     ▼
WORLD VALIDATION
     │
     ▼
WORLD DATA
```

---

# 15. Map Graph

O grafo existe antes da geometria.

Cada nó representa uma sala lógica.

Cada aresta representa uma conexão necessária.

Exemplo:

```text
          [Storage]
              │
              │
[Office] ── [Hub] ── [Security]
              │
              │
        [Maintenance]
              │
              │
            [Boss]
```

---

# 16. Room Role

A função de uma sala no grafo deve ser separada de sua identidade visual.

```cpp
enum class RoomRole
{
    Start,
    Normal,
    Hub,
    Arena,
    DeadEnd,
    Secret,
    Boss
};
```

---

# 17. Room Theme

```cpp
enum class RoomTheme
{
    Industrial,
    Storage,
    Office,
    Laboratory,
    Maintenance,
    Security,
    Cafeteria
};
```

Isso permite combinações:

```text
Arena + Maintenance
Secret + Laboratory
DeadEnd + Storage
Hub + Industrial
Normal + Security
```

Sem criar um tipo especial para cada combinação.

---

# 18. Room Module

Um `RoomModule` descreve geometria pré-configurada.

Ele não representa uma instância concreta da sala.

```cpp
struct RoomModule
{
    RoomModuleID id;

    RoomTheme theme;

    Bounds localBounds;

    std::vector<ConnectorDefinition> connectors;
    std::vector<SpawnZone> enemySpawnZones;
    std::vector<SpawnZone> lootZones;
    std::vector<PropZone> propZones;

    MeshRecipe geometry;
};
```

---

# 19. Room Instance

Uma instância utiliza o módulo escolhido.

```cpp
struct Room
{
    RoomID id;

    RoomRole role;
    RoomTheme theme;

    Seed seed;

    RoomModuleID module;

    Transform transform;

    std::vector<ConnectorInstance> connectors;

    LightingProfile lighting;
    MaterialProfile materials;

    std::vector<EntityID> entities;
};
```

---

# 20. Connectors

Connector é um contrato explícito entre módulos.

```cpp
enum class ConnectorType
{
    Door,
    Large,
    Secret,
    Service,
    Elevator
};

enum class Direction
{
    North,
    East,
    South,
    West
};

struct ConnectorDefinition
{
    ConnectorID id;

    ConnectorType type;

    Vec3 position;
    Vec3 normal;

    float width;
    float height;

    bool required;
};
```

---

# 21. Validação de connector

Dois connectors só podem ser conectados quando:

```text
tipo compatível
+
dimensão compatível
+
orientação compatível
+
posição resolvível
+
espaço livre
```

---

# 22. Corredores

Os corredores podem ser gerados para resolver a distância entre connectors.

Formas iniciais:

```text
reto
L
Z
```

A geração deve respeitar:

- direção inicial;
- largura mínima;
- altura;
- colisão;
- espaço ocupado;
- portas;
- caminho navegável.

---

# 23. Invariantes do mundo

Um mundo válido deve garantir:

- START existe;
- BOSS existe;
- BOSS é alcançável;
- todo caminho obrigatório é navegável;
- connectors obrigatórios foram resolvidos;
- nenhuma sala obrigatória está isolada;
- corredor não atravessa geometria inválida;
- spawn do jogador é válido;
- portas não criam softlock;
- encontros obrigatórios podem ser concluídos;
- resultado é determinístico.

---

# 24. Conteúdo de sala

Depois que a geometria é validada:

```text
ROOM
 │
 ├── PROPS
 ├── ENEMIES
 ├── LOOT
 ├── MATERIALS
 ├── LIGHTING
 └── EVENTS
```

Cada sistema usa sua própria sub-seed.

---

# 25. Entidades

Evitar espalhar ponteiros crus de `Enemy*`, `Item*` e `Prop*` por todas as salas.

Proposta:

```cpp
using EntityID = std::uint32_t;
```

`Room` armazena IDs.

O `EntityManager` controla as entidades.

```cpp
class EntityManager
{
public:
    EntityID createEnemy(const EnemyCreateInfo&);
    EntityID createItem(const ItemCreateInfo&);
    EntityID createProp(const PropCreateInfo&);

    void destroy(EntityID id);

    Entity* find(EntityID id);
};
```

Não é necessário implementar um ECS completo inicialmente.

---

# 26. Arquitetura dos inimigos

Uma das mudanças centrais desta revisão é separar:

```text
FUNÇÃO
≠
ATRIBUTOS
≠
COMPORTAMENTO
≠
APARÊNCIA
```

---

# 27. Enemy Role

O papel define identidade de gameplay.

```cpp
enum class EnemyRole
{
    Worker,
    Guard,
    Officer,
    Engineer,
    IndustrialRobot
};
```

---

# 28. Enemy Stats

```cpp
struct EnemyStats
{
    float maxHealth;
    float moveSpeed;

    float attackDamage;
    float attackCooldown;

    float detectionRange;
};
```

Stats não são definidos pela textura ou pelas peças visuais.

---

# 29. Enemy Behavior

```cpp
struct EnemyBehaviorProfile
{
    float preferredDistance;
    float aggression;
    float retreatThreshold;
    float reactionTime;

    bool canUseDoors;
    bool canSupportAllies;
};
```

O comportamento pode variar dentro do papel, mas deve utilizar uma stream de gameplay.

---

# 30. Enemy Visual Genome

A aparência é controlada separadamente.

```cpp
struct EnemyVisualGenome
{
    Seed seed;

    BodyProfileID bodyProfile;

    PartID head;

    PartID torso;

    PartID leftShoulder;
    PartID rightShoulder;

    PartID leftArm;
    PartID rightArm;

    PartID hip;

    PartID leftLeg;
    PartID rightLeg;

    PartID backPrimary;
    PartID backSecondary;

    MaterialSetID materials;

    WearProfileID wear;

    AccessorySetID accessories;
};
```

---

# 31. Enemy final

```cpp
struct Enemy
{
    EnemyRole role;

    EnemyStats stats;
    EnemyBehaviorProfile behavior;

    EnemyVisualGenome visual;

    float health;

    Transform transform;
};
```

Resultado:

```text
DOIS OPERÁRIOS
       │
       ├── mesmo papel
       ├── mesma lógica básica
       ├── stats dentro da mesma família
       │
       └── aparências muito diferentes
```

---

# 32. Pipeline completo do Enemy Generator

```text
ENEMY SPAWN
    │
    ▼
CHOOSE ROLE
    │
    ├─────────────── GAMEPLAY STREAM
    │
    ▼
STATS + BEHAVIOR
    │
    │
    └─────────────── VISUAL STREAM
                         │
                         ▼
                   BODY PROFILE
                         │
                         ▼
                 COMPATIBLE PART SET
                         │
                         ▼
                    PART SELECTION
                         │
                         ▼
                  SOCKET VALIDATION
                         │
                         ▼
                PROPORTION ADJUSTMENT
                         │
                         ▼
                  MESH GENERATION
                         │
                         ▼
                MATERIAL GENERATION
                         │
                         ▼
                  WEAR GENERATION
                         │
                         ▼
                ACCESSORY GENERATION
                         │
                         ▼
                  FINAL VALIDATION
                         │
                         ▼
                     ASSET CACHE
                         │
                         ▼
                       ENEMY
```

---

# 33. Biblioteca de peças

O catálogo anterior continua sendo a referência de conteúdo.

A nova engine não deve codificar cada inimigo como uma classe diferente.

Cada peça precisa possuir metadados.

```cpp
enum class PartSlot
{
    Head,
    Torso,
    Shoulder,
    Arm,
    Hip,
    Leg,
    Back,
    Accessory
};

struct PartDefinition
{
    PartID id;

    PartSlot slot;

    CompatibilityMask compatibility;

    Bounds normalizedBounds;

    std::vector<SocketDefinition> sockets;

    MeshRecipe meshRecipe;

    MaterialFamily defaultMaterial;

    PartSemantic semantic;
};
```

---

# 34. Unidade corporal normalizada

A biblioteca visual utiliza:

```text
H = 1.0
```

Todas as proporções são relativas à altura total.

Exemplo:

```text
cabeça      0.12 H
torso       0.32 H
pernas      0.45 H
```

Isso permite trocar:

- resolução;
- escala real;
- nível de detalhe;

sem reescrever os contratos das peças.

---

# 35. Body Profile

```cpp
struct BodyProfile
{
    float heightScale;

    float torsoWidth;
    float torsoDepth;

    float shoulderWidth;

    float armLength;
    float legLength;

    float headScale;

    float forwardLean;

    float leftMassBias;
    float rightMassBias;
};
```

Perfis podem representar:

- humano padrão;
- alto e fino;
- baixo e largo;
- corcunda;
- braços longos;
- assimétrico;
- mecânico;
- torso pesado.

---

# 36. Sockets

Uma peça não pode simplesmente ser posicionada “aproximadamente” perto da outra.

```cpp
struct SocketDefinition
{
    SocketType type;

    Vec3 position;
    Vec3 direction;

    float radius;

    CompatibilityMask compatibility;
};
```

Exemplos:

```text
Neck
ShoulderLeft
ShoulderRight
Hip
Back
Weapon
Tool
```

---

# 37. Validação de montagem

Uma montagem precisa verificar:

```text
slot correto
+
socket compatível
+
escala válida
+
interseção aceitável
+
silhueta válida
+
papel permitido
```

---

# 38. Assimetria

A assimetria continua sendo permitida.

Exemplo:

```text
braço esquerdo humano
braço direito mecânico
```

Mas deve haver coerência estrutural.

O sistema não deve produzir:

```text
peça flutuando
+
ombro sem encaixe
+
perna atravessando quadril
+
arma sem suporte
```

---

# 39. Geração procedural de mesh

## 39.1. Objetivo

Gerar a geometria visual a partir da descrição da peça.

O código não precisa necessariamente armazenar uma mesh pronta para cada variante.

A engine pode armazenar:

```text
RECIPE
+
PARAMETERS
+
SEED
```

e construir a mesh.

---

# 40. MeshRecipe

Proposta:

```cpp
enum class PrimitiveType
{
    Box,
    RoundedBox,
    Cylinder,
    Capsule,
    Sphere,
    Cone,
    ExtrudedProfile
};

struct PrimitiveRecipe
{
    PrimitiveType type;

    Transform localTransform;

    Vec3 dimensions;

    MaterialSlot material;

    SemanticRegion semantic;
};

struct MeshRecipe
{
    std::vector<PrimitiveRecipe> primitives;
};
```

---

# 41. Construção de uma peça

Exemplo conceitual de braço hidráulico:

```text
ARM_HYDRAULIC
│
├── capsule → estrutura superior
├── cylinder → pistão
├── cylinder → articulação
├── rounded box → proteção
├── cylinder → antebraço
└── socket → ferramenta
```

A seed pode variar:

```text
comprimento
espessura
número de pistões
ângulo
proteções
mangueiras
```

Dentro dos limites definidos pela peça.

---

# 42. Características obrigatórias

Cada ID de peça possui características que não podem desaparecer.

Exemplo:

```text
ARM_HYDRAULIC
```

pode variar, mas precisa continuar exibindo pelo menos:

```text
massa mecânica
+
pistão legível
+
articulação
```

Se a variação deixar de possuir essas propriedades, ela não pertence mais ao mesmo ID.

---

# 43. Semantic Regions

Para permitir materiais e desgaste coerentes, a mesh precisa saber o que cada região representa.

```cpp
enum class SemanticRegion
{
    Generic,
    ArmorPlate,
    Fabric,
    Rubber,
    Joint,
    Piston,
    Cable,
    Lens,
    Tool,
    Weapon,
    Skin,
    Bone,
    MechanicalCore,
    FootContact,
    HandContact
};
```

---

# 44. Por que regiões semânticas são importantes

Sem semântica:

```text
ruído aleatório
↓
ferrugem em qualquer lugar
↓
sujeira em qualquer lugar
↓
resultado visual incoerente
```

Com semântica:

```text
JOINT
   ↓
graxa

ARMOR EDGE
   ↓
tinta gasta

PISTON
   ↓
óleo longitudinal

FOOT
   ↓
poeira

LENS
   ↓
risco / rachadura

FABRIC
   ↓
mancha / dobra / abrasão
```

---

# 45. Material ≠ Texture

Na nova arquitetura:

```text
Material
```

define como a superfície responde à luz.

```text
Texture
```

armazena mapas espaciais usados pelo material.

---

# 46. MaterialDefinition

```cpp
struct MaterialDefinition
{
    MaterialFamily family;

    Vec3 baseColor;

    float baseRoughness;
    float baseMetallic;

    float emissiveStrength;

    TextureHandle albedo;
    TextureHandle roughness;
    TextureHandle metallic;
    TextureHandle normal;
    TextureHandle emissive;
};
```

---

# 47. Famílias de material

Inicialmente:

```cpp
enum class MaterialFamily
{
    PaintedSteel,
    BareSteel,
    Aluminum,
    Fabric,
    Rubber,
    Polymer,
    Polycarbonate,
    Ceramic,
    OrganicSkin,
    Bone,
    CorruptedOrganic
};
```

A biblioteca de inimigos pode mapear suas descrições antigas para essas famílias.

---

# 48. Geração procedural de texturas dos inimigos

Esta é uma parte central da nova arquitetura.

A aparência final não deve ser:

```text
escolher PNG pronto
```

O sistema deve ser capaz de executar:

```text
RECIPE
+
SEED
+
SEMANTIC REGION
+
MATERIAL FAMILY
        ↓
TEXTURE SET
```

---

# 49. TextureSet

Proposta:

```cpp
struct TextureSet
{
    TextureHandle albedo;
    TextureHandle roughness;
    TextureHandle metallic;
    TextureHandle normal;
    TextureHandle emissive;
};
```

Nem todo material precisa utilizar todos os mapas.

---

# 50. TextureRecipe

```cpp
struct TextureRecipe
{
    Seed seed;

    MaterialFamily family;

    Vec3 baseColor;
    Vec3 secondaryColor;

    float macroVariation;
    float microVariation;

    float wearAmount;
    float dirtAmount;
    float scratchAmount;
    float oxidationAmount;

    float emissiveAmount;

    SemanticRegion region;
};
```

---

# 51. Pipeline de uma textura de inimigo

```text
TEXTURE SEED
      │
      ▼
MATERIAL FAMILY
      │
      ▼
BASE COLOR
      │
      ▼
MACRO STRUCTURE
      │
      ▼
MICRO STRUCTURE
      │
      ▼
SEMANTIC MASKS
      │
      ├── EDGE
      ├── JOINT
      ├── CONTACT
      ├── CAVITY
      └── EXPOSED
      │
      ▼
WEAR
      │
      ▼
DIRT
      │
      ▼
SPECIAL DETAILS
      │
      ▼
GENERATE MAPS
      │
      ├── ALBEDO
      ├── ROUGHNESS
      ├── METALLIC
      ├── NORMAL
      └── EMISSIVE
      │
      ▼
GPU UPLOAD
```

---

# 52. Etapa 1 — Base

Exemplo:

```text
MaterialFamily = PaintedSteel
BaseColor      = #4C5155
```

Primeiro toda a região recebe a base.

---

# 53. Etapa 2 — Macroestrutura

Macroestrutura representa formas perceptíveis à distância.

Exemplos:

```text
placas
costuras
faixas
segmentos
painéis
rebites
soldas
```

Não deve ser ruído de alta frequência.

---

# 54. Etapa 3 — Microestrutura

Exemplos:

```text
grão do metal
trama de tecido
microrriscos
porosidade
microvariação
```

A intensidade precisa ser baixa o suficiente para não destruir a leitura retro-industrial.

---

# 55. Máscaras semânticas

A geração de desgaste não deve trabalhar apenas com ruído.

Para cada peça podem ser produzidas máscaras:

```text
EDGE MASK
JOINT MASK
CONTACT MASK
CAVITY MASK
EXPOSED MASK
HEAT MASK
```

---

# 56. Edge Mask

Representa bordas expostas.

Usos:

```text
tinta descascada
metal exposto
arranhões
polimento
```

---

# 57. Joint Mask

Representa pivôs e articulações.

Usos:

```text
graxa
óleo
sujeira
desgaste mecânico
```

---

# 58. Contact Mask

Representa regiões que tocam o ambiente.

Exemplo:

```text
pés
mãos
extremidades de ferramentas
```

Usos:

```text
poeira
abrasão
polimento
```

---

# 59. Cavity Mask

Regiões internas e pouco acessíveis.

Usos:

```text
sujeira acumulada
óleo
escurecimento
oxidação leve
```

---

# 60. Heat Mask

Aplicável a:

```text
maçarico
arma
reator
motor
exaustão
```

Pode controlar:

```text
fuligem
descoloração térmica
emissivo
material queimado
```

---

# 61. Wear Profile

```cpp
struct WearProfile
{
    float edgeWear;
    float jointWear;
    float contactWear;

    float dirt;
    float oxidation;
    float soot;

    float damage;
};
```

---

# 62. Geração de desgaste

Fluxo:

```text
WEAR SEED
   │
   ▼
GLOBAL WEAR LEVEL
   │
   ▼
SEMANTIC MASK
   │
   ▼
LOCAL NOISE
   │
   ▼
CLAMP
   │
   ▼
FINAL WEAR MASK
```

A máscara semântica decide **onde pode acontecer**.

O ruído decide **como varia localmente**.

---

# 63. Painted Steel

Pipeline sugerido:

```text
BASE PAINT
    ↓
METAL STRUCTURE
    ↓
EDGE MASK
    ↓
PAINT LOSS
    ↓
EXPOSED METAL
    ↓
SCRATCHES
    ↓
OPTIONAL OXIDATION
```

Regras:

- `metallic` alto apenas no metal exposto;
- tinta deve reduzir a leitura metálica;
- ferrugem concentrada em regiões justificáveis;
- desgaste mais forte nas bordas.

---

# 64. Bare Steel

Características:

```text
metallic alto
roughness média
variação de luminosidade baixa
riscos lineares
óleo localizado
```

Evitar ferrugem uniforme cobrindo toda a peça.

---

# 65. Fabric

Pipeline:

```text
BASE COLOR
    ↓
WEAVE PATTERN
    ↓
SEAMS
    ↓
FOLDS / SHADE
    ↓
DIRT
    ↓
ABRASION
```

Regras:

- metallic = 0;
- roughness alta;
- sujeira próxima de contato;
- costuras precisam seguir a geometria;
- não aplicar ruído metálico.

---

# 66. Rubber

Características:

```text
baixo metallic
roughness alta
reflexo controlado
abrasão
rachadura rara
```

Mangueiras podem receber:

```text
poeira
graxa nos conectores
desgaste nas curvas
```

---

# 67. Polymer

Características:

```text
superfície relativamente uniforme
roughness média
riscos leves
tinta / pigmento consistente
```

Capacetes e carenagens podem utilizar essa família.

---

# 68. Polycarbonate / Lens

Características:

```text
base escura ou translúcida
reflexo controlado
riscos
rachadura
emissivo opcional
```

A direção do visor deve continuar legível.

---

# 69. Organic Skin

Características:

```text
metallic = 0
roughness variável
variação de tom agrupada
cicatrizes localizadas
manchas
```

Evitar:

```text
noise pixel a pixel
```

Preferir regiões maiores.

---

# 70. Corrupted Organic

Pode combinar:

```text
tecido
carne
placas
cabos
implantes
```

Mas cada sub-região continua utilizando um material coerente.

---

# 71. Albedo Map

Contém:

```text
cor base
variação de cor
desgaste visual
sujeira
marcas
```

Não deve tentar simular toda a iluminação.

---

# 72. Roughness Map

Controla a resposta especular.

Exemplos:

```text
graxa       → menor roughness
tecido      → maior roughness
metal gasto → menor roughness
poeira      → maior roughness
borracha    → alta roughness
```

---

# 73. Metallic Map

Usos principais:

```text
aço exposto
alumínio
metal estrutural
```

Tecido, pele e borracha permanecem não metálicos.

---

# 74. Normal Map

Pode ser produzido proceduralmente a partir de mapas de altura simples.

Detalhes possíveis:

```text
rebites
soldas
costuras
chanfros
placas
grão
```

O normal map não deve substituir geometria importante.

Regra:

> Se um detalhe muda a silhueta, ele provavelmente pertence à mesh.

---

# 75. Emissive Map

Aplicável a:

```text
visor
sensor
reator
display
luz de status
muzzle
```

Emissivos precisam possuir função visual.

Evitar aplicar cor brilhante apenas para aumentar variedade.

---

# 76. Resolução das texturas

A resolução deve ser configurável por família e importância.

Primeira proposta:

```text
peça pequena       64×64
peça comum        128×128
torso importante  256×256
boss              256×256 ou maior
```

Esses valores são parâmetros de implementação, não contratos visuais permanentes.

A arquitetura deve permitir alterá-los.

---

# 77. UV

Duas estratégias podem coexistir.

## 77.1. UV gerado pela receita

Para peças previsíveis:

```text
box
cylinder
capsule
```

o gerador pode produzir UV automaticamente.

## 77.2. Projeção procedural

Para superfícies onde UV tradicional não for necessário:

```text
triplanar
box projection
cylindrical projection
```

A escolha deve ser feita por `MeshRecipe`.

---

# 78. Material por subpeça

Um torso não precisa possuir um único material.

Exemplo:

```text
Torso
│
├── tecido       → Fabric
├── placa        → PaintedSteel
├── mangueira    → Rubber
├── visor        → Polycarbonate
└── sensor       → Emissive Polymer
```

---

# 79. Material Slots

```cpp
enum class MaterialSlot
{
    Primary,
    Secondary,
    Armor,
    Fabric,
    Rubber,
    Lens,
    Organic,
    Emissive
};
```

Primitivas da mesh apontam para slots.

---

# 80. Geração de material do inimigo

```text
ENEMY MATERIAL SEED
        │
        ▼
ROLE PALETTE
        │
        ▼
PART MATERIAL FAMILY
        │
        ▼
COLOR VARIATION
        │
        ▼
WEAR PROFILE
        │
        ▼
TEXTURE RECIPE
        │
        ▼
TEXTURE SET
        │
        ▼
MATERIAL INSTANCE
```

---

# 81. Paleta por papel

A cor não deve ser a única forma de reconhecer um inimigo.

Mas pode reforçar identidade.

Exemplo conceitual:

```text
Worker
    → industrial / utilitário

Guard
    → tático

Officer
    → organizado / comando

Engineer
    → técnico / diagnóstico

IndustrialRobot
    → massa / estrutura / emissivos
```

A silhueta continua tendo prioridade.

---

# 82. Exemplo completo de geração visual

```text
Enemy Seed:
742918351

Role:
Worker

Body:
BODY_09

Parts:
Torso       TOR_WORKER_03
Head        HEAD_GASMASK_02
Left Arm    ARM_HUMAN_04
Right Arm   ARM_DRILL_01
Left Leg    LEG_WORKER_02
Right Leg   LEG_PROSTHETIC_01
Back        BACK_UTILITY_02

Materials:
Fabric          #58605D
PaintedSteel    #535A5C
Rubber          #202224

Wear:
edge       0.31
joint      0.44
contact    0.58
dirt       0.37
oxidation  0.08

Texture generation:
Fabric
 ├── weave
 ├── seam
 ├── lower-body dirt
 └── abrasion

Drill
 ├── painted steel
 ├── exposed edge
 ├── oil mask
 ├── linear scratches
 └── metallic map

Gas mask
 ├── rubber
 ├── polycarbonate lens
 ├── lens scratches
 └── filter oxidation
```

---

# 83. Validação visual do inimigo

Uma variante só é aceita se:

- papel ainda é reconhecível;
- silhueta é coerente;
- ameaça principal é legível;
- cabeça possui orientação visual;
- partes encaixam;
- material combina com a peça;
- desgaste está em regiões plausíveis;
- detalhes não escondem articulações;
- emissivos possuem função;
- variante é determinística.

---

# 84. Procedural Asset System

O antigo conceito de `Asset Generator` passa a ser tratado como um subsistema explícito.

```text
ProceduralAssetSystem
│
├── MeshGenerator
├── TextureGenerator
├── MaterialGenerator
├── AssetHasher
└── AssetCache
```

---

# 85. Asset Hash

Antes de gerar um asset:

```text
VisualGenome
     │
     ▼
Canonical Recipe
     │
     ▼
Hash
     │
     ▼
AssetCache
```

---

# 86. Asset Cache

```text
HASH FOUND?
│
├── YES
│    ↓
│  REUSE
│
└── NO
     ↓
   GENERATE
     ↓
   UPLOAD
     ↓
   CACHE
```

Evita gerar repetidamente a mesma mesh ou textura.

---

# 87. Cache Key

A chave precisa incluir tudo que altera o resultado visual.

Exemplo:

```cpp
struct EnemyAssetKey
{
    EnemyRole role;

    BodyProfileID body;

    std::array<PartID, 10> parts;

    MaterialSetID materials;
    WearProfileID wear;

    Seed visualSeed;
};
```

A estrutura canônica pode ser serializada e hashada.

---

# 88. Lifetime de recursos

Recursos OpenGL não devem ser propriedade direta de cada inimigo.

Exemplo:

```text
Enemy
  ↓
AssetHandle
  ↓
ResourceManager
  ↓
GPU Resource
```

---

# 89. Resource Handle

```cpp
using MeshHandle = std::uint32_t;
using TextureHandle = std::uint32_t;
using MaterialHandle = std::uint32_t;
```

---

# 90. Material das salas

Os princípios do guia anterior continuam válidos.

A geração deve acontecer em duas escalas:

```text
ROOM COMPOSITION
        ↓
TILE / MATERIAL DETAIL
```

Nunca começar sorteando detalhes independentemente em cada superfície.

---

# 91. Room Material Profile

```cpp
struct RoomMaterialProfile
{
    RoomTheme theme;

    MaterialFamily wallFamily;
    MaterialFamily floorFamily;
    MaterialFamily ceilingFamily;

    VisualBudget budget;

    ColorPalette palette;

    WearProfile wear;
};
```

---

# 92. Orçamento visual

```cpp
struct VisualBudget
{
    float neutral;
    float structure;
    float detail;
    float highlight;
};
```

A sala decide quantos elementos especiais pode possuir.

Depois o procedural decide onde colocá-los.

---

# 93. Classes visuais

```cpp
enum class VisualClass
{
    Base,
    Structure,
    Detail,
    Highlight
};
```

---

# 94. Regra de silêncio visual

A maior parte de uma sala comum precisa continuar sendo:

```text
material
+
estrutura
+
sombras
+
desgaste discreto
```

Não:

```text
monitor
+
símbolo
+
placa
+
tubo
+
luz
```

em cada superfície.

---

# 95. Identidade dos ambientes

Os ambientes-base continuam:

```text
Industrial Corridor
Storage
Combat
Cafeteria
Security
Laboratory
Technical
Central Hub
```

Cada ambiente deve ser reconhecido pela composição.

---

# 96. Renderer

O renderer deve consumir dados prontos.

```text
WORLD STATE
+
CAMERA
+
VISIBLE ENTITIES
+
MATERIALS
+
LIGHTS
        ↓
RENDERER
        ↓
OPENGL
```

---

# 97. Pipeline OpenGL inicial

```text
CPU WORLD
   │
   ▼
CULLING
   │
   ▼
RENDER QUEUE
   │
   ├── STATIC WORLD
   ├── PROPS
   ├── ENEMIES
   ├── TRANSPARENT
   └── UI
   │
   ▼
GPU BUFFERS
   │
   ▼
SHADERS
   │
   ▼
FRAMEBUFFER
   │
   ▼
SDL WINDOW
```

---

# 98. Renderer mínimo

Primeiro renderer precisa apenas de:

- câmera;
- perspective projection;
- vertex buffer;
- index buffer;
- shader básico;
- textura;
- depth test;
- framebuffer;
- iluminação simples.

Não começar por efeitos avançados.

---

# 99. Lighting

A iluminação precisa ser controlada pelo mundo.

```cpp
struct Light
{
    Vec3 position;
    Vec3 color;

    float intensity;
    float radius;
};
```

Perfis de sala podem definir:

```text
quantidade
posição permitida
cor
intensidade
estado
```

---

# 100. Gameplay

Gameplay contém:

```text
Player
Weapons
Enemies
Items
Combat
Objectives
Progression
```

O procedural fornece parâmetros.

Gameplay executa as regras.

---

# 101. Player

Base:

```cpp
struct Player
{
    Transform transform;

    float health;
    float maxHealth;

    float moveSpeed;

    WeaponID activeWeapon;
};
```

---

# 102. Primeiro inimigo

O primeiro papel recomendado continua sendo:

```text
Worker
```

Motivos:

- comportamento simples;
- corpo a corpo;
- visual industrial;
- permite validar EnemyGenerator sem exigir sistema tático complexo.

---

# 103. Ordem do elenco

```text
1. Worker
2. Guard
3. Officer
4. Engineer
5. Industrial Robot
```

---

# 104. Áudio

Áudio utiliza seed própria quando procedural.

Separar:

```text
Gameplay Audio
Ambient Audio
UI Audio
```

Mudanças em som nunca podem modificar geração estrutural.

---

# 105. Debug

Uma engine procedural precisa permitir observar seus resultados.

Ferramentas recomendadas:

```text
mostrar seed
mostrar room id
mostrar room role
mostrar room theme
mostrar connectors
mostrar spawn zones
mostrar enemy genome
mostrar asset hash
mostrar material family
mostrar semantic masks
```

---

# 106. Dumps de geração

Sistemas procedurais devem conseguir exportar resultados técnicos.

Exemplos:

```text
world graph → texto / dot
enemy genome → texto
texture maps → imagens
mesh → OBJ temporário de debug
seed tree → texto
```

Essas ferramentas não precisam fazer parte do jogo final.

---

# 107. Teste do RNG

Validar:

- mesma seed → mesma sequência;
- domínio diferente → stream diferente;
- alteração visual não modifica gameplay;
- alteração de áudio não modifica mundo.

---

# 108. Teste do World Generator

Gate inicial:

```text
100 seeds
```

Depois:

```text
1.000 mundos / andares
```

Validar:

- conectividade;
- START;
- BOSS;
- connectors;
- colisão;
- navegação.

---

# 109. Teste do Enemy Generator

Para centenas de seeds:

- role válido;
- body válido;
- peças compatíveis;
- sockets válidos;
- nenhuma peça faltando;
- material válido;
- montagem determinística.

---

# 110. Teste de textura

As texturas implementadas até aqui são baseline técnico, não aprovação
artística final. Antes de modificar uma recipe por direção visual, é obrigatório
seguir `visual/TEXTURE_APPROVALS.md`: produzir uma prancha versionada da unidade,
apresentar seeds representativas e obter aprovação explícita antes da
implementação. Aprovação e regressão automatizada são gates independentes.

Para cada família:

```text
same recipe + same seed
        ↓
same pixels
```

Validar:

- dimensões;
- valores válidos;
- determinismo;
- ausência de NaN;
- canais;
- mapas coerentes;
- emissivo apenas quando permitido.

---

# 111. Teste semântico de desgaste

Exemplo:

```text
JOINT
```

deve possuir maior probabilidade de:

```text
graxa
óleo
```

que:

```text
LENS
```

Enquanto:

```text
LENS
```

pode possuir:

```text
risco
rachadura
```

mas não ferrugem.

---

# 112. Teste de cache

Validar:

```text
mesma receita
    ↓
mesmo hash
    ↓
mesmo asset
```

e:

```text
receita diferente
    ↓
hash / asset diferente
```

---

# 113. Definition of Done

Uma feature procedural só pode ser considerada pronta quando:

- implementação existe;
- compila sem warnings relevantes;
- resultado é reproduzível;
- resultado é validado;
- múltiplas seeds foram testadas;
- não quebra streams independentes;
- não cria combinação inválida conhecida;
- possui debug suficiente para investigar falha;
- documentação foi atualizada.

---

# 114. Ordem recomendada de implementação

A nova engine deve crescer de baixo para cima.

---

## Fase 0 — Bootstrap

**Estado atual:** bootstrap mínimo implementado e validado com CMake, SDL3,
contexto OpenGL 3.3 core, game loop, logging e execução headless.

```text
CMake
SDL3
janela
OpenGL context
game loop
logging
```

---

## Fase 1 — Core

**Estado atual:** implementada e validada. O núcleo possui relógio de passo fixo
de 60 Hz com clamp, snapshots de teclado/mouse, Hash FNV-1a/mistura de 64 bits,
SplitMix64, seeds versionadas e independentes por domínio, vetores, grid,
rotações de módulo em quartos de volta e AABB. A suíte verifica determinismo,
faixas, input, coordenadas e equivalência da simulação a 30/60/144 FPS.

```text
Time
Input
Random
Hash
Seed derivation
Math
```

---

## Fase 2 — Renderer mínimo

**Estado atual:** implementada e validada. `wkc_forge_render` carrega a API
OpenGL 3.3, compila/linka um shader básico iluminado, envia uma box indexada com hard
edges para VAO/VBO/EBO, cria textura RGBA determinística, câmera perspectiva,
depth test e framebuffer redimensionável com color texture e depth buffer. A
cena de integração monta uma sala industrial fixa com nove instâncias e uma luz
pontual. Testes CPU validam malha, imagem e matrizes; o teste GPU offscreen lê os
pixels apresentados e confirma que a cena possui contraste real.

```text
Shader
Mesh
Texture
Camera
Depth
Framebuffer
```

Renderizar uma sala fixa.

---

## Fase 3 — Player

**Estado atual:** implementada e validada. `gdp_gameplay` mantém o estado lógico
do jogador, aplica mouse look uma vez por frame e executa movimento, gravidade e
colisão apenas no passo fixo. O collider é um cilindro vertical simplificado com
resolução separada em X/Z para deslizamento, limites de piso/teto e sólidos da
sala. A câmera do renderer recebe posição/direção do Player. A interação usa ray
curto e escolhe o alvo mais próximo; o terminal visível possui volume lógico
correspondente. Testes cobrem determinismo, diagonal, paredes, canto, gravidade,
pitch, sensibilidade, alcance de interação, HP e morte idempotente.

```text
movimento
mouse look
colisão
interação
```

---

## Fase 4 — World Module System

**Estado atual:** implementada e validada. `gdp_world` separa template
`RoomModule` de `RoomInstance`, define IDs, `RoomRole`, `RoomTheme`, footprint,
bounds, capabilities e zonas. Conectores possuem tipo, direção cardinal,
dimensões, obrigatoriedade, edge consumidora e selagem opcional. Instanciação
transforma bounds, posição e normal dos conectores em `0/90/180/270°`. O gate
offline rejeita módulos, conectores, zonas e clearances inválidos. A aplicação
só inicializa a sala fixa depois desse gate. Testes cobrem compatibilidade,
mating, rotação, IDs, perímetro, uso, selagem e required não resolvido.

```text
RoomModule
RoomRole
RoomTheme
Connector
Transform
```

---

## Fase 5 — World Generator

**Estado atual:** implementada e validada. `MapGraph` cria base conectada com
START/BOSS únicos, main path explícito, branches DeadEnd/Secret, Hub/Arena,
depth BFS e loop budget. `WorldGenerator` deriva streams por tentativa, grafo e
sala, seleciona módulos compatíveis, posiciona sem overlap com espaçamento
padrão de 12 unidades, consome connector pairs e roteia corredores Straight/L/Z
sem reentrar nas salas dos endpoints nem atravessar salas alheias. Loops fecham
somente uma branch e um nó próximo do main path. O candidato
só vira `World` após gates de graph, geometry, connectors e corridors. Um
fingerprint dourado detecta deriva estrutural entre builds; 1.000 seeds passam
em Debug, Release e ASan/UBSan. `--seed N` permite reprodução direta.

```text
MapGraph
module selection
placement
connections
corridors
validation
```

---

## Fase 6 — Entity Manager

**Estado atual:** implementada e validada. `gdp_entities` mantém slots com
`EntityId {index, generation}`, rejeita handles inválidos ou antigos e reutiliza
slots somente depois de `flushDestroyed()`. Player, Enemy, Item e Prop possuem
payloads tipados em `variant`, transform e `roomId`; queries por tipo e sala
ignoram destruições pendentes. `RoomInstance` armazena somente IDs, enquanto o
manager preserva ownership central. Após o commit estrutural,
`populateInitialEntities()` cria conteúdo determinístico por `RoomRole`, conecta
o Player e o terminal reais à interação e sincroniza transform/HP do jogador no
fixed update. Testes cobrem payloads, queries, destruição adiada, limpeza por
sala, reutilização geracional, handles stale, 10.000 entidades e população de
mundo.

```text
EntityID
Enemy
Item
Prop
lifetime
```

---

## Fase 7 — Primeiro gameplay

**Estado atual:** implementada e validada. `Weapon` controla munição de pente e
reserva, recarga e cooldown no fixed update. `fireHitscan()` lança o ray a partir
da câmera, seleciona o inimigo vivo mais próximo da sala e respeita solids e
limites do ambiente antes de aplicar dano. Overkill resulta em uma única
transição para Dead. O primeiro Worker usa Idle/Chase/Attack/Dead, detecção por
distância, line of sight, movimento com colisão e ataque corpo a corpo com
cooldown. A sala START recebe um Worker ativo representado por placeholder de
caixas; retícula e título da janela expõem mira, HP e munição. Testes cobrem
cooldown, vazio, recarga, alvo mais próximo, filtro por sala, bloqueio, miss,
dano, double death, perseguição, ataque, line of sight e pixels do placeholder.
Uma integração headless de 900 fixed updates exige observar a morte do Player.

```text
Worker
arma
hitscan
HP
dano
morte
```

---

## Fase 8 — Procedural Asset Foundation

**Estado atual:** implementada e validada. `PrimitiveRecipe` registra tipo,
transform local, dimensões, material slot e região semântica; `MeshRecipe` e
`TextureRecipe` carregam versão e seed visual. Hashes canônicos incluem todos os
campos e alimentam um `AssetCache` de recursos CPU imutáveis, com métricas de
hit/miss. `generateMesh()` combina Boxes, aplica escala/rotação/translação,
preserva ranges de material e semântica por vértice e rejeita primitivas ainda
não suportadas. `generateAlbedo()` produz RGBA determinístico com variação,
desgaste, sujeira e oxidação condicionada à família. O renderer obtém do cache a
mesh de seis partes e a textura do Worker antes do upload; nenhuma geração
ocorre durante draw. Testes cobrem validação, transforms, bounds, semântica,
regras materiais, hashes dourados, cache e 1.000 seeds visuais. O fingerprint
estrutural do World permanece inalterado.

Na Fase 30, os contratos e geradores reutilizáveis foram isolados no target
depois renomeado, na Fase 41, para `wkc_forge_procedural`; na Fase 42, o cache
foi extraído para `wkc_forge_resources`. Construtores Worker e outros dados
visuais do GDP não fazem mais parte de `AssetRecipe` e são compilados em
`gdp_visual_content`.

```text
MeshRecipe
PrimitiveRecipe
SemanticRegion
MaterialFamily
TextureRecipe
AssetCache
```

---

## Fase 9 — Primeiro inimigo procedural completo

**Estado atual:** implementada e validada. O Worker possui quatro
`BodyProfile`, oito sockets tipados e uma biblioteca de 18 definições de peças
para torso, cabeça, braços, pernas, módulo traseiro e ferramenta. A seleção
ponderada filtra compatibilidade antes de sortear e usa substreams independentes
para corpo, cada slot, material e desgaste. O `WorkerVisualGenome` canônico
registra as peças esquerda/direita separadamente e identifica mesh, textura e
estado visual por hash. Cada instância recebe uma `visualSeed` derivada da seed
da sala sem alterar o fingerprint estrutural do World. Box e Cylinder compõem a
receita de oito primitivas; renderer e cache preparam a variante antes do draw.
Um gate de 1.000 seeds cobriu todos os perfis, 18 peças, três famílias de
material e quatro estados de desgaste, com hashes únicos e zero fallbacks.

```text
BodyProfile
Parts
Mesh
Material
Texture
Wear
```

---

## Fase 10 — Materiais das salas

**Estado atual:** concluída, com `Industrial Corridor`, `Storage`,
`Combat Room`, `Cafeteria`, `Laboratory`, `Technical Area`, `Security` e
`Central Hub` implementados e validados. Todos derivam da seed da
sala três sub-receitas independentes e um hash canônico para
Wall/Floor/Ceiling. Industrial seleciona
`Maintained/Used/Worn` e gera placas com rodapé de segurança, piso
antiderrapante com faixa de tráfego e teto modular neutro. Storage
seleciona `Orderly/Dusty/Neglected` e gera metal amarronzado com oxidação local e
marcação logística curta, concreto com manchas largas/arrasto/rachadura rara e
teto escuro com vigas neutras. Combat seleciona
`Ready/Active/Scarred` e gera blindagem verde-oliva com 1–3 impactos, delimitação
de arena no piso e vigas pesadas sem luz gravada. Hashes de perfil e
substreams não colidem. Cafeteria seleciona `Clean/InUse/Faded` e gera painéis
claros com faixa vermelha contínua, piso lavável com circulação e contatos de
mobiliário e teto regular neutro. Laboratory seleciona
`Sterile/Operational/Contained` e gera painéis frios com módulo científico,
piso técnico azul com contaminação localizada e teto simétrico sem módulos
luminosos pintados. Cache e upload ocorrem antes do draw e o renderer troca o
conjunto completo por perfil. Technical Area seleciona
`Serviced/Working/Overloaded` e combina tubos conectados, óleo/grade no piso e
categorias mecânicas no teto sem abandonar a base simples.
Security seleciona `Secured/Monitored/Alert` e mantém console/indicadores
agrupados, acesso limpo no piso e teto claro neutro. Central Hub seleciona
`Open/Active/Rerouted`, compartilha um eixo `Horizontal/Vertical/Cross` entre as
superfícies e combina sinalização ciano única, uma rota ou cruzamento contínuo e
teto arquitetônico neutro. Luminárias pertencem ao modelo e material
`Lighting/CeilingFixture`, fora desses albedos. Gates de 1.000 room seeds por
perfil cobriram as 24 condições e produziram 8.000 hashes únicos sem alterar o
fingerprint estrutural do World.

```text
Industrial Corridor ✓
Storage ✓
Combat Room ✓
Cafeteria ✓
Laboratory ✓
Technical Area ✓
Security ✓
Central Hub ✓
```

Fases 10, 11 e 12 concluídas; loot, props, eventos, iluminação, áudio e progressão estão funcionais.

---

## Fase 11 — Elenco

**Estado atual:** concluída. Guard opera em 4–8 m com arma longa; Officer em
6–10 m com sidearm/comando; Engineer em 3–6 m com ferramenta/reparo. Cada papel
possui três perfis, 18 peças, materiais, desgaste, substreams, hash dourado,
cache GPU e gate de 1.000 seeds sem fallback. A Arena recebe os quatro papéis.
Officer reduz cooldowns de aliados; Engineer repara o alvo visível mais próximo,
recuperando HP ou restaurando Prop `Damaged`. Ambas as ações respeitam range,
`roomId` e cooldown próprio; ataques e reparos respeitam line of sight.
IndustrialRobot completa o Boss com três perfis, duas unidades traseiras,
núcleo emissivo, arma pesada e fases >66%, 33–66% e <=33% de HP.

```text
Guard ✓
Officer ✓
Engineer ✓
IndustrialRobot ✓
```

---

## Fase 12 — Procedural avançado

```text
loot ✓
props ✓
events ✓
lighting ✓
audio ✓
progression ✓
```

## Fase 13 — Vertical slice navegável

```text
porta por connector/edge ✓
travessia bidirecional ✓
estado do jogador preservado ✓
sala ativa: colisão/material/luz/áudio ✓
lockdown bloqueia saída ✓
corredor contínuo: geometria/colisão ✓
troca automática de sala ✓
Boss: engagement e Victory ✓
Player morto: Defeat ✓
restart com nova seed determinística ✓
```

## Fase 14 — Apresentação multissala

```text
materiais de todas as salas preparados antes do primeiro frame ✓
shell e decoração com material por sala ✓
inimigos, pickups e props adjacentes visíveis antes da entrada ✓
IA e coleta restritas à sala lógica ✓
mistura de ambiente/emissão ao longo do corredor ✓
objetivo e resultado dentro do viewport ✓
regressão OpenGL para material multissala e HUD ✓
```

## Fase 15 — Portas e áudio contínuos

```text
DoorRuntime por edge: Closed/Opening/Open/Closing/Locked ✓
E solicita abertura sem reposicionar o jogador ✓
colisão liberada somente após 90% de abertura ✓
porta segura enquanto vão/corredor estão ocupados ✓
fechamento temporizado e lockdown físico ✓
painéis deslizantes guiados por openFraction ✓
crossfade equal-power entre ambiências das duas salas ✓
regressões de runtime, colisão, mixer e pixels ✓
```

## Fase 16 — Continuidade de encontros e luz

```text
primeira onda de eventos de entrada pré-materializada e dormente ✓
entidades dormentes visíveis, sem IA, dano ou contagem de limpeza ✓
ativação reutiliza entidades sem duplicação ou aparecimento tardio ✓
emboscada de terminal permanece oculta até interação ✓
até oito luzes combinam sala atual e adjacentes ✓
regressões de evento, combate, headless e pixels ✓
```

## Fase 17 — HUD funcional

```text
HP, munição, chaves, nível e XP dentro do viewport ✓
objetivo e estado de evento nas bordas ✓
prompts de porta, terminal, bloqueio e recarga contextuais ✓
layout responsivo preserva o centro de combate ✓
renderer recebe snapshots sem possuir regras de gameplay ✓
regressão OpenGL em resolução estreita ✓
```

## Fase 18 — Feedback de combate

```text
timers determinísticos de feedback no fixed update ✓
bordas temporárias para dano recebido ✓
marcadores distintos para acerto e eliminação ✓
avisos de coleta, evento, onda, setor limpo e level-up ✓
restart limpa feedback e passos inválidos não avançam timers ✓
regressões unitárias, headless e OpenGL ✓
```

## Fase 19 — Pausa funcional

```text
Esc alterna pausa sem encerrar a run ✓
fixed update, look e comandos ficam congelados ✓
mouse relativo é liberado e restaurado ✓
Resume, Restart Run e Exit Game funcionais ✓
navegação W/S, setas, Enter e wrap determinístico ✓
overlay responsivo oculta mira e feedback transitório ✓
regressões unitárias, headless e OpenGL ✓
```

## Estabilização de travessia pós-Fase 19

```text
segmentos intermediários não reentram nas salas dos endpoints ✓
seeds anteriormente quebradas atravessam cada corredor nos dois sentidos ✓
batch permanente de 500 mundos fisicamente atravessáveis ✓
roomId só muda após raio e altura do collider liberarem o vão ✓
lockdown mantém porta passável aberta enquanto ocupada ✓
colisão de lockdown acompanha o estado físico por edge ✓
loops locais e limite auditado de 16,3 unidades por corredor ✓
juntas em L sem paredes internas cruzando a passagem ✓
shells, portas, conteúdo e luz antecipados por duas conexões ✓
auditoria ampliada: 10.000 gerações e 2.000 travessias ✓
```

## Fase 20 — Variedade estrutural de salas

```text
23 RoomModule compactos, médios e grandes ✓
Hub, Arena e Boss grandes; Secret compacta ✓
worldBounds como fonte do shell no renderer e da colisão ✓
connectors, zonas, spacing e corredores derivados dos bounds reais ✓
quatro rotações, 1.000 mundos e 500 travessias validadas ✓
```

A biblioteca varia footprints de `7 × 4 × 9` a `14 × 6 × 13`. O placement usa
as bordas reais do módulo anterior e dos vizinhos de cada branch, preservando
corredores compactos sem sobrepor salas de proporções diferentes. O renderer
recebe os bounds da instância; nenhuma receita visual foi alterada.

## Fase 21 — Menu principal

```text
START RUN antes da geração do mundo ✓
EXIT GAME e Esc encerram sem criar uma dungeon ✓
teclado e mouse com layout responsivo compartilhado ✓
headless preserva início automático para automação ✓
regressões unitárias, OpenGL e bootstrap sem geração antecipada ✓
```

`Application::initialize` prepara apenas plataforma, áudio, janela e renderer.
`initializeRun` concentra mundo, entidades, eventos, iluminação e recursos da
run e só é chamado pela confirmação do menu ou pelo bootstrap headless. O
overlay usa o mesmo layout puro para desenho e hit test em pixels, inclusive em
janelas high-DPI.

## Fase 22 — Revisão visual com aprovação prévia

```text
texturas atuais classificadas como baseline técnico
uma proposta versionada por superfície ou família visual
prancha com seeds representativas antes de alterar a implementação
aprovação explícita do usuário limitada à unidade apresentada
capturas in-engine e regressões técnicas depois da implementação
registro em visual/TEXTURE_APPROVALS.md
```

`Industrial Corridor/Wall v1` retornou para `REVISÃO SOLICITADA` depois das
capturas in-engine: a geração de `64×64`, o filtro nearest e o UV único por vão
produziram uma parede simples, ampliada e sem profundidade material suficiente.
Floor, Ceiling, Door e demais unidades não foram alteradas.

`Industrial Corridor/Wall v2 HD` foi proposta com material `1024×1024`, 256
texels por metro, UV por dimensão física, mipmaps trilineares e canais Albedo,
Normal e Roughness/Metalness/AO. A base HD será compartilhada e a variação por
sala usará masks/offsets leves para conter a VRAM.

A implementação v2 usa três arquétipos HD compartilhados por condição, gera os
três mapas em uma única passagem estrutural, aplica normal mapping por TBN no
fragment shader e mantém a faixa de segurança ancorada somente à base mesmo em
spans sobre portas. Somente a parede Industrial ativa esse pipeline; os demais
materiais preservam filtro e UV anteriores.

A v2 foi aceita no jogo em 2026-08-30 após a revisão das capturas `1280×720`.
Tetos e luminárias passam a obedecer um contrato global de separação: a textura
do teto contém apenas a superfície arquitetônica neutra, sem lâmpadas, lentes,
faixas emissivas ou brilho pintado; `Lighting/CeilingFixture` é uma unidade
visual própria, ligada ao modelo físico e ao material emissivo independente.
Todo prompt futuro de teto deve declarar explicitamente essa exclusão.

## Fase 23 — Separação entre teto e luminária

As oito famílias de albedo de teto não desenham mais lâmpadas, lentes ou faixas
emissivas. O renderer também envia emissão zero para o material do teto. Os
pontos de luz continuam posicionando o modelo físico, agora ligado a uma
textura técnica própria e filtrada de forma independente; `ON`, `OFF` e
`ALERT` permanecem estados de runtime do material da luminária.

Testes contam zero ocorrências da cor reservada de luminária em cada teto. A
proposta `Lighting/CeilingFixture v1 HD` foi aprovada e implementada como um
modelo composto por carcaça, aro, junta, lente e quatro fixadores. Um atlas
`1024×1024` fornece Albedo, Normal, RMA e Emission com filtragem trilinear; a
máscara explícita mantém carcaça/metal/junta sem emissão e limita a luz à lente.
O estado `OFF` reduz emissão a zero sem remover a geometria, enquanto `ALERT`
usa vermelho controlado no mesmo material.

As capturas `ON`, `OFF` e `ALERT` foram aceitas no jogo em 2026-08-30.

## Fase 24 — Teto HD do corredor industrial

A proposta `Industrial Corridor/Ceiling v1 HD` foi aprovada com a condição de
que a textura permaneça claramente metálica. A implementação usa três
arquétipos HD compartilhados por condição, material `1024×1024`, UV por
dimensão física, filtragem trilinear e mapas Albedo/Normal/RMA. Metalness é
predominante nas chapas e aumenta em vigas, bordas expostas e fixadores; testes
quantitativos impedem regressão para aparência plástica.

Não existe canal de emissão nem representação de lâmpada no albedo; a
luminária permanece como asset físico independente já aprovado. As seis
capturas de gameplay/aproximação foram aceitas no jogo em 2026-08-30.

---

# 115. Primeiro vertical slice técnico

O primeiro grande objetivo da arquitetura deve ser:

```text
abrir jogo
    ↓
gerar pequeno mapa
    ↓
entrar em sala
    ↓
encontrar Worker
    ↓
Worker foi montado por peças
    ↓
mesh foi gerada
    ↓
materiais foram gerados
    ↓
texturas foram geradas
    ↓
asset foi cacheado
    ↓
combate funciona
```

Esse vertical slice prova praticamente todas as decisões centrais da engine.

---

# 116. O que não deve bloquear esse vertical slice

Não implementar antes dele:

- catálogo completo de 146 peças;
- oito ambientes completos;
- Boss final;
- campanha;
- saves;
- ranking;
- Speedrun;
- HUD final;
- física complexa;
- editor;
- sistema universal de animação.

---

# 117. Arquitetura resumida final

```text
                           MASTER SEED
                                │
             ┌──────────────────┼──────────────────┐
             ▼                  ▼                  ▼
           WORLD             GAMEPLAY             VISUAL
             │                  │                  │
             ▼                  ▼                  ▼
       WORLD GENERATOR     SPAWN / LOOT       ASSET SYSTEM
             │                  │                  │
             ▼                  ▼          ┌───────┼────────┐
         MAP GRAPH           ENTITIES       ▼       ▼        ▼
             │                  │          MESH  MATERIAL  TEXTURE
             ▼                  │           │       │        │
        ROOM ROLES              │           └───────┼────────┘
             │                  │                   ▼
             ▼                  │                CACHE
        ROOM THEMES             │                   │
             │                  │                   ▼
             ▼                  └───────────────> ENEMY
       ROOM MODULES                                 │
             │                                      ▼
             ▼                                  GAME STATE
        CONNECTORS                                   │
             │                                      ▼
             ▼                                   RENDERER
        CORRIDORS                                    │
             │                                      ▼
             ▼                                    OPENGL
        VALIDATION
             │
             ▼
           WORLD
```

---

# 118. Filosofia final

O GDP deve manter:

```text
DETERMINISMO
      +
CONTROLE ARTÍSTICO
      +
VARIEDADE
      +
LEGIBILIDADE
      +
MANUTENIBILIDADE
```

O procedural não existe para produzir o maior número possível de combinações.

Ele existe para produzir:

> **muitas combinações que continuam parecendo pertencer ao mesmo jogo.**

---

# Apêndice A — Alternativas consideradas

## C# + Unity

Continua sendo uma alternativa válida caso a prioridade mude para velocidade de produção.

Vantagens:

- editor;
- física;
- animação;
- tooling;
- desenvolvimento rápido.

Desvantagem principal para a proposta atual:

- grande parte da arquitetura central já é determinada pela engine.

---

## C++ + Unreal

Tecnicamente capaz.

Vantagens:

- renderer;
- ferramentas;
- física;
- animação;
- navegação.

Para o GDP atual, porém, adicionaria uma estrutura muito maior do que a necessária.

---

# Apêndice B — Decisão

Direção principal:

```text
C++20/23
+
SDL3
+
OpenGL
+
GDP Engine especializada
```

O objetivo não é competir com engines comerciais.

O objetivo é construir exatamente a infraestrutura necessária para:

```text
FPS
+
salas modulares
+
mapa procedural
+
inimigos procedurais
+
meshes procedurais
+
materiais procedurais
+
texturas procedurais
+
conteúdo procedural
```

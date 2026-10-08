# Sistema de IA Neuroevolutiva para Inimigos — GDP / WKC Forge

## 1. Objetivo

Implementar no projeto **GDP**, preferencialmente como um módulo reutilizável da **WKC Forge**, um sistema de IA para inimigos baseado em **redes neurais pequenas + neuroevolução**.

A proposta **não é simular literalmente o cérebro completo de uma mosca**. O connectoma da *Drosophila melanogaster* serve apenas como inspiração estrutural para organizar o fluxo:

**sensores → processamento neural → decisões → ações**

O objetivo principal é substituir comportamentos totalmente previsíveis por inimigos capazes de:

- reagir ao estado atual do mundo;
- apresentar variações individuais;
- melhorar seu desempenho ao longo das gerações;
- adaptar-se parcialmente ao estilo do jogador;
- desenvolver comportamentos emergentes sem regras explícitas para cada situação;
- continuar sendo divertidos e imperfeitos, evitando uma IA impossível de vencer.

---

## 2. Conceito principal

Cada inimigo deve possuir um pequeno "cérebro" representado por uma rede neural.

A rede recebe informações do ambiente:

```text
Sensores
   |
   v
+----------------+
| Neural Brain   |
|                |
| 32-64 nós      |
+-------+--------+
        |
        v
      Ações
```

Exemplos de entradas:

```text
- distância até o jogador
- ângulo relativo do jogador
- jogador está visível?
- vida atual
- munição
- distância até paredes
- distância até cobertura
- levou dano recentemente?
- direção do último dano recebido
- posição relativa de aliados
- quantidade de aliados próximos
- velocidade do jogador
- jogador está atirando?
- jogador está recarregando?
```

Exemplos de saídas:

```text
- mover para frente
- mover para trás
- mover lateralmente
- mirar
- atacar
- recuar
- desviar
- perseguir
- buscar cobertura
- flanquear
- aguardar
```

A IA não deve necessariamente ter uma saída exclusiva para comportamentos complexos como "flanquear".

Idealmente, comportamentos como flanqueamento devem surgir pela combinação de ações mais simples:

```text
movimento lateral
+ controle de distância
+ visão do jogador
+ tentativa de manter cobertura
```

---

## 3. Inspiração biológica

O sistema pode seguir uma estrutura simplificada inspirada no cérebro de insetos:

```text
VISÃO -----------+
SOM -------------+
DANO ------------+
DISTÂNCIA -------+
ALIADOS ---------+
                 |
                 v
        +----------------+
        | Interneurônios |
        | / Neural Brain |
        +-------+--------+
                |
        +-------+--------+
        |       |        |
        v       v        v
      MOVE     AIM     ATTACK
```

Não é necessário reproduzir neurônios biológicos individuais.

O objetivo é aproveitar o conceito de:

```text
neurônios sensoriais
        ↓
processamento intermediário
        ↓
neurônios motores
```

---

## 4. Neuroevolução

Os pesos da rede neural de cada inimigo formam seu **genoma**.

Exemplo simplificado:

```text
Enemy Brain
|
+-- peso 0 = 0.35
+-- peso 1 = -0.72
+-- peso 2 = 1.12
+-- peso 3 = 0.04
+-- ...
```

Após uma luta ou morte do inimigo, o jogo calcula um valor de **fitness**.

Exemplo inicial:

```c
fitness =
    damage_dealt * 2.0f
    + survival_time * 0.1f
    + hits * 5.0f
    + successful_flanks * 10.0f
    + time_in_effective_range * 0.2f
    - damage_received
    - deaths * 20.0f;
```

Essa fórmula deve ser configurável e provavelmente diferente para cada arquétipo de inimigo.

---

## 5. Ciclo evolutivo

Fluxo geral:

```text
Geração N
    |
    v
Criar inimigos
    |
    v
Executar gameplay
    |
    v
Coletar desempenho
    |
    v
Calcular fitness
    |
    v
Selecionar melhores indivíduos
    |
    v
Crossover opcional
    |
    v
Mutação
    |
    v
Geração N + 1
```

Exemplo:

```text
Enemy A -> fitness 120
Enemy B -> fitness 340
Enemy C -> fitness 80
Enemy D -> fitness 290

Melhores:
B + D

        |
        v

Reprodução / cópia

        |
        v

Mutação dos pesos

        |
        v

Nova população
```

Mutação simples:

```c
new_weight = old_weight + random_float(-0.05f, 0.05f);
```

Também deve existir uma probabilidade de mutação:

```c
if (random_float(0.0f, 1.0f) < mutation_rate)
{
    weight += random_float(-mutation_strength, mutation_strength);
}
```

---

## 6. Evitar aprendizado ilimitado

A IA não deve evoluir indefinidamente.

Problema possível:

```text
Jogador joga durante muitas horas
        ↓
IA aprende padrões recorrentes
        ↓
inimigos exploram constantemente as mesmas fraquezas
        ↓
dificuldade cresce demais
        ↓
jogo deixa de ser divertido
```

Portanto, implementar limites.

Possíveis mecanismos:

### 6.1 Clamp dos pesos

```c
weight = clamp(weight, -MAX_WEIGHT, MAX_WEIGHT);
```

### 6.2 Fitness máximo

Impedir que pequenas diferenças acima de determinado nível continuem dominando a seleção.

### 6.3 Taxa de mutação decrescente

Quanto mais madura estiver uma população, menor pode ser a intensidade das mutações.

### 6.4 Injeção de indivíduos aleatórios

A cada geração:

```text
90% descendentes dos melhores indivíduos
10% indivíduos novos aleatórios
```

Isso evita convergência excessiva.

### 6.5 Limite de gerações persistentes

Exemplo:

```text
Geração máxima persistida: 100
```

Após isso, manter evolução apenas local ou reiniciar parcialmente a população.

---

## 7. Personalidade individual

Inimigos não devem se tornar cópias perfeitas uns dos outros.

Cada indivíduo pode possuir modificadores de personalidade:

```c
typedef struct
{
    float aggression;
    float caution;
    float curiosity;
    float teamwork;
    float preferred_distance;
    float reaction_speed;
} AI_Personality;
```

Esses valores influenciam sensores, decisões ou fitness.

Exemplo:

```text
Enemy A
aggression = 0.90
caution    = 0.20

Enemy B
aggression = 0.35
caution    = 0.85
```

Mesmo usando cérebros semelhantes, os comportamentos podem ser diferentes.

---

## 8. Arquétipos de inimigos

Cada classe de inimigo deve possuir uma configuração própria.

### Grunt

```text
neurônios: 24-32
perfil: agressivo
fitness principal:
- dano causado
- pressão sobre o jogador
```

### Hunter

```text
neurônios: 32-48
perfil: perseguidor
fitness principal:
- manter contato com o jogador
- perseguição eficiente
- interceptação
```

### Sniper

```text
neurônios: 32-48
perfil: distância
fitness principal:
- precisão
- distância segura
- sobrevivência
```

### Stalker

```text
neurônios: 48-64
perfil: emboscada
fitness principal:
- evitar detecção
- sobrevivência
- ataques surpresa
```

Esses valores são apenas referências iniciais e devem permanecer configuráveis.

---

## 9. Arquitetura sugerida

Estrutura proposta:

```text
src/
└── ai/
    ├── neural_network.c
    ├── neural_network.h
    │
    ├── genome.c
    ├── genome.h
    │
    ├── evolution.c
    ├── evolution.h
    │
    ├── sensors.c
    ├── sensors.h
    │
    ├── fitness.c
    ├── fitness.h
    │
    ├── ai_brain.c
    ├── ai_brain.h
    │
    ├── ai_personality.c
    ├── ai_personality.h
    │
    ├── ai_population.c
    └── ai_population.h
```

Persistência:

```text
data/
└── ai/
    ├── grunt_brain.dat
    ├── hunter_brain.dat
    ├── sniper_brain.dat
    └── stalker_brain.dat
```

Se a WKC Forge possuir uma divisão entre engine e game:

```text
engine/
└── ai/
    ├── neural_network.*
    ├── genome.*
    ├── evolution.*
    └── ai_population.*

game/
└── ai/
    ├── enemy_sensors.*
    ├── enemy_fitness.*
    ├── enemy_brain.*
    └── enemy_personality.*
```

A engine deve conter os componentes genéricos.

O GDP deve conter regras específicas do gameplay.

---

## 10. Estruturas de dados

Exemplo inicial:

```c
typedef struct
{
    int input_count;
    int hidden_count;
    int output_count;

    float *input_hidden_weights;
    float *hidden_output_weights;

    float *hidden_biases;
    float *output_biases;
} NeuralNetwork;
```

Genoma:

```c
typedef struct
{
    float *weights;
    size_t weight_count;

    float fitness;

    unsigned int generation;
    unsigned int id;
} Genome;
```

Cérebro do inimigo:

```c
typedef struct
{
    NeuralNetwork network;
    Genome genome;
    AI_Personality personality;

    float last_fitness;
} AI_Brain;
```

---

## 11. Sensores

Os sensores devem produzir valores normalizados.

Preferência:

```text
-1.0 até +1.0
```

ou:

```text
0.0 até 1.0
```

Escolher um padrão único para todo o sistema.

Exemplo:

```c
typedef struct
{
    float player_distance;
    float player_angle;
    float player_visible;

    float health;
    float ammo;

    float wall_front;
    float wall_left;
    float wall_right;

    float incoming_damage;
    float last_damage_direction;

    float ally_distance;
    float ally_count;
} AI_Sensors;
```

Normalização:

```c
player_distance =
    clamp(distance_to_player / MAX_SENSOR_DISTANCE, 0.0f, 1.0f);
```

---

## 12. Saídas da rede

Exemplo:

```c
typedef struct
{
    float move_forward;
    float move_backward;

    float move_left;
    float move_right;

    float attack;
    float aim;

    float seek_cover;
} AI_Output;
```

Valores acima de um threshold podem ativar ações.

Exemplo:

```c
if (output.attack > 0.65f)
{
    enemy_attack(enemy);
}
```

Para movimento, pode ser melhor combinar valores:

```c
movement_x =
    output.move_right
    - output.move_left;

movement_y =
    output.move_forward
    - output.move_backward;
```

---

## 13. Rede neural inicial

Começar com uma rede feed-forward simples:

```text
Input Layer
    ↓
Hidden Layer
    ↓
Output Layer
```

Não começar implementando:

- redes recorrentes;
- transformers;
- reinforcement learning profundo;
- backpropagation;
- redes convolucionais;
- modelos grandes.

Objetivo inicial:

```text
rede pequena
+
inferência barata
+
evolução por pesos
```

Depois que o sistema estiver funcional, avaliar redes recorrentes para memória.

---

## 14. Função de ativação

Usar inicialmente `tanh`.

```c
float activation(float value)
{
    return tanhf(value);
}
```

Vantagens:

```text
saída entre -1 e +1
boa para controle de movimento
implementação simples
```

Para outputs booleanos, threshold pode ser aplicado depois.

---

## 15. Fitness

O fitness não deve recompensar apenas matar o jogador.

Caso contrário, a evolução tende a produzir inimigos excessivamente agressivos.

Exemplo mais equilibrado:

```c
fitness =
      damage_dealt        * 2.0f
    + accuracy            * 25.0f
    + survival_time       * 0.1f
    + useful_movement     * 0.2f
    + tactical_advantage  * 5.0f
    + teamwork_score      * 4.0f
    - damage_received     * 0.8f
    - idle_time           * 0.3f;
```

Cada tipo de inimigo deve poder fornecer sua própria função de fitness.

---

## 16. Evitar exploits da IA

O sistema pode descobrir estratégias que dão fitness alto sem serem comportamentos desejáveis.

Exemplo:

```text
fitness recompensa sobrevivência
        ↓
IA aprende a fugir para sempre
```

Ou:

```text
fitness recompensa movimento
        ↓
IA fica girando em círculos
```

Portanto:

- usar múltiplos critérios;
- penalizar comportamento inútil;
- acompanhar métricas individualmente;
- registrar decisões durante testes;
- nunca usar somente uma métrica como objetivo.

---

## 17. Seleção

Começar com **elitismo + seleção por torneio**.

### Elitismo

Os melhores indivíduos são copiados diretamente.

Exemplo:

```text
Population = 100

Top 5
→ copiados sem alteração
```

### Tournament Selection

Selecionar aleatoriamente alguns indivíduos:

```text
A
B
C
D
```

Escolher o de maior fitness.

Isso mantém pressão evolutiva sem depender diretamente de probabilidades complexas.

---

## 18. Crossover

Primeira versão pode não usar crossover.

Começar com:

```text
melhor indivíduo
        ↓
cópia
        ↓
mutação
```

Depois implementar crossover:

```text
Parent A:
A A A A A A

Parent B:
B B B B B B

Child:
A A B A B B
```

Exemplo:

```c
for (size_t i = 0; i < weight_count; i++)
{
    child[i] =
        random_bool()
        ? parent_a[i]
        : parent_b[i];
}
```

---

## 19. Persistência

Salvar apenas os melhores genomas.

Evitar salvar o estado completo de todos os inimigos.

Formato inicial possível:

```text
MAGIC
VERSION
ARCHETYPE
GENERATION
WEIGHT_COUNT
WEIGHTS
PERSONALITY
FITNESS
CHECKSUM
```

Exemplo de header:

```c
typedef struct
{
    char magic[4];       /* WKAI */
    uint32_t version;
    uint32_t archetype;
    uint32_t generation;
    uint32_t weight_count;
} AI_SaveHeader;
```

---

## 20. Versionamento do arquivo

O formato salvo deve possuir versão.

Exemplo:

```c
#define AI_SAVE_VERSION 1
```

Isso permitirá alterar a rede futuramente sem quebrar saves antigos silenciosamente.

---

## 21. Determinismo

Permitir configurar uma seed:

```c
ai_set_seed(12345);
```

Isso é importante para:

- testes;
- debugging;
- reproduzir comportamentos;
- comparar gerações;
- investigar bugs.

---

## 22. Debug visual

Adicionar ferramentas de debug.

Exemplo:

```text
Enemy #42
Generation: 18
Fitness: 382.5

Health: 72%
Distance: 14.2m

Brain:
attack       0.81
move_left    0.12
move_right   0.73
seek_cover   0.66
```

Opcionalmente desenhar:

```text
Enemy
  |
  +-- target line
  +-- field of view
  +-- sensor rays
  +-- chosen movement direction
```

---

## 23. Logging

Durante desenvolvimento:

```text
logs/
└── ai/
    ├── generation_0001.csv
    ├── generation_0002.csv
    └── generation_0003.csv
```

Exemplo CSV:

```csv
generation,id,fitness,damage_dealt,damage_received,hits,survival_time
1,1,124.5,50,70,4,18.4
1,2,230.2,110,40,9,24.8
```

Isso permitirá avaliar se a evolução está realmente melhorando os inimigos.

---

## 24. Treinamento separado do gameplay

Idealmente, implementar dois modos.

### Runtime Mode

Usado durante o jogo normal:

```text
pouca evolução
baixo custo
persistência limitada
```

### Training Mode

Usado pelo desenvolvedor:

```text
simulações aceleradas
centenas de agentes
sem gráficos
milhares de gerações
```

Exemplo:

```bash
./gdp --ai-training grunt --generations 10000
```

Possível saída:

```text
Generation 1
Best fitness: 125.3

Generation 100
Best fitness: 344.8

Generation 1000
Best fitness: 618.2
```

---

## 25. Headless training

O modo de treinamento deve poder executar sem renderização.

Objetivo:

```text
SDL rendering OFF
audio OFF
particle effects OFF
UI OFF

AI + physics + simplified combat ON
```

Isso permite executar simulações muito mais rapidamente.

---

## 26. Pool de cérebros

Em vez de um único cérebro global:

```text
Grunt Population
├── Brain 1
├── Brain 2
├── Brain 3
├── ...
└── Brain N
```

Ao spawnar um inimigo:

```text
escolher genoma
+
adicionar pequena variação
+
criar personalidade
```

Isso evita que todos os inimigos ajam exatamente igual.

---

## 27. Integração com dificuldade

Não usar apenas buffs artificiais como:

```text
mais vida
mais dano
mais velocidade
```

A dificuldade pode também selecionar populações diferentes.

Exemplo:

```text
EASY
→ gerações iniciais

NORMAL
→ gerações intermediárias

HARD
→ gerações avançadas
```

Isso permite aumentar a dificuldade por comportamento.

Ainda assim, manter limites para que a IA não obtenha informações impossíveis para o jogador.

---

## 28. Fairness

A rede deve receber somente informações que o inimigo poderia razoavelmente perceber.

Não fornecer diretamente:

```text
posição exata do jogador através de paredes
```

Em vez disso:

```text
player_visible = false
last_known_player_position
time_since_last_seen
last_heard_sound_position
```

Isso evita comportamento semelhante a wallhack.

---

## 29. Memória futura

A primeira versão será stateless:

```text
sensores atuais
        ↓
decisão atual
```

Posteriormente avaliar memória:

```text
sensores
+
estado interno anterior
        ↓
decisão
```

Possibilitando:

```text
lembrar onde o jogador foi visto
lembrar de dano recente
manter perseguição
recuar por alguns segundos
```

Pode ser implementado inicialmente sem redes recorrentes, usando variáveis explícitas:

```c
typedef struct
{
    Vec2 last_known_player_position;
    float time_since_player_seen;
    float time_since_damage_received;
} AI_Memory;
```

---

## 30. Performance

A IA deve ser projetada para permitir dezenas de inimigos simultâneos.

Evitar inferência a cada frame.

Exemplo:

```text
render = 60 FPS
AI decision = 10 Hz
```

Ou:

```c
if (ai_timer >= 0.1f)
{
    ai_update(enemy);
    ai_timer = 0.0f;
}
```

Entre atualizações, continuar executando a ação escolhida.

---

## 31. LOD da IA

Possível otimização futura:

```text
Enemy próximo:
AI 10 Hz

Enemy médio:
AI 5 Hz

Enemy distante:
AI 1 Hz

Enemy fora da área ativa:
AI pausada
```

---

## 32. Threading

Não implementar multithreading na primeira versão.

Primeiro garantir:

```text
correção
determinismo
testabilidade
```

Depois avaliar processamento paralelo de múltiplos cérebros.

---

# Plano de implementação

## Fase 1 — Rede neural básica

Criar:

```text
neural_network.c
neural_network.h
```

Implementar:

```c
neural_network_create()
neural_network_destroy()
neural_network_randomize()
neural_network_forward()
neural_network_clone()
```

Testes:

- rede pode ser criada;
- pesos são inicializados;
- mesma entrada + mesmos pesos = mesma saída;
- saída permanece no intervalo esperado;
- clone produz resultado idêntico.

---

## Fase 2 — Genome

Criar:

```text
genome.c
genome.h
```

Implementar:

```c
genome_create()
genome_destroy()
genome_clone()
genome_mutate()
genome_crossover()
```

---

## Fase 3 — Sensores

Criar:

```text
sensors.c
sensors.h
```

Implementar inicialmente:

```text
distância do jogador
ângulo do jogador
visibilidade
vida
distância de parede frontal
distância de parede esquerda
distância de parede direita
```

---

## Fase 4 — Ações

Integrar outputs com o inimigo:

```text
movimento
rotação
ataque
```

Primeiro objetivo:

```text
um inimigo controlado inteiramente pela rede
```

Mesmo que inicialmente seja ruim.

---

## Fase 5 — Fitness

Criar:

```text
fitness.c
fitness.h
```

Registrar:

```text
damage_dealt
damage_received
hits
shots
survival_time
distance_traveled
idle_time
```

---

## Fase 6 — População

Criar:

```text
ai_population.c
ai_population.h
```

Implementar:

```c
population_create()
population_evaluate()
population_select()
population_evolve()
```

---

## Fase 7 — Training Mode

Adicionar modo:

```bash
./gdp --ai-training
```

Permitir:

```text
escolher arquétipo
definir população
definir gerações
definir seed
```

Exemplo:

```bash
./gdp \
    --ai-training grunt \
    --population 100 \
    --generations 5000 \
    --seed 42
```

---

## Fase 8 — Persistência

Salvar e carregar melhores genomas.

```c
ai_brain_save()
ai_brain_load()
```

---

## Fase 9 — Integração com gameplay

Ao iniciar o jogo:

```text
carregar população
        ↓
spawnar inimigos
        ↓
selecionar cérebros
        ↓
executar gameplay
        ↓
coletar estatísticas
        ↓
evoluir quando apropriado
```

---

# Critérios de sucesso

A primeira versão será considerada funcional quando:

1. Um inimigo puder ser controlado exclusivamente pela rede neural.
2. A rede reagir a entradas reais do mundo.
3. Diferentes pesos produzirem comportamentos diferentes.
4. O jogo calcular fitness após uma simulação.
5. Uma população puder ser ordenada por fitness.
6. Os melhores genomas puderem gerar descendentes mutados.
7. Gerações posteriores apresentarem aumento médio de fitness em um ambiente controlado.
8. Os melhores cérebros puderem ser salvos e carregados.
9. O sistema for reproduzível usando uma seed fixa.
10. O gameplay continuar executando sem queda significativa de desempenho.

---

# Restrições importantes

Não fazer inicialmente:

```text
- simulação literal do cérebro da mosca
- centenas de milhares de neurônios
- deep learning
- TensorFlow
- PyTorch
- dependências externas pesadas
- GPU compute
- redes gigantes
- aprendizado por backpropagation
```

Preferir:

```text
C puro
estrutura simples
rede pequena
baixo custo
determinismo
testes unitários
arquitetura modular
```

---

# Diretriz para o Codex

Ao implementar este sistema:

1. Analise primeiro a arquitetura existente do GDP/WKC Forge.
2. Não introduza dependências externas sem necessidade.
3. Reutilize tipos matemáticos, vetores, entidades e sistemas existentes.
4. Não duplique funcionalidades que já existam na engine.
5. Preserve o estilo atual do projeto.
6. Separe código genérico de IA do código específico dos inimigos.
7. Não tente implementar todas as fases de uma única vez.
8. Comece pela rede neural e pelos testes.
9. Compile e execute os testes após cada etapa.
10. Documente estruturas públicas e decisões arquiteturais relevantes.

---

# Primeira tarefa recomendada para implementação

Antes de qualquer integração com inimigos, implementar apenas o núcleo genérico:

```text
src/ai/neural_network.c
src/ai/neural_network.h

src/ai/genome.c
src/ai/genome.h
```

Com suporte a:

```text
- criação
- destruição
- pesos aleatórios
- forward pass
- clone
- mutação
- seed determinística
```

Criar testes unitários cobrindo essas funções.

Após essa etapa estar estável e compilando corretamente, integrar sensores e um único inimigo experimental.

---

# Resultado esperado

O sistema final deve permitir que o GDP tenha inimigos que não dependam apenas de árvores de decisão ou sequências rígidas como:

```c
if (player_visible)
{
    attack();
}
else
{
    patrol();
}
```

O comportamento deverá emergir principalmente de:

```text
sensores
+
rede neural
+
fitness
+
seleção
+
mutação
```

produzindo inimigos com comportamentos variados, adaptativos e progressivamente mais eficientes sem abandonar limites de balanceamento e desempenho definidos pelo jogo.

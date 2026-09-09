# Enemies — Sistema geral

> **Estado:** catálogo lógico e shells implementados na Fase 6; Worker completo
> validado nas Fases 7–9; Guard, Officer, Engineer e IndustrialRobot
> funcionais/procedurais validados na Fase 11.
> A Fase 100 acrescentou patrulha local determinística aos cinco papéis sem
> alterar seus contratos de combate. A Fase 101 acrescentou percepção visual e
> investigação temporária da última posição conhecida. A Fase 102 tornou esse
> ciclo validável atrás de uma cobertura larga na sala START. A Fase 103
> acrescentou investigação provocada por ruído de disparo e a Fase 104 separou
> sua reação inicial no estado `Alert`. A Fase 105 acrescentou `Recover` antes
> do retorno à patrulha. A Fase 106 acrescentou rotas globais dentro da sala.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Princípio

```text
ROLE ≠ STATS ≠ BEHAVIOR ≠ VISUAL GENOME
```

## Propriedade arquitetural

O catálogo de entidades pertence ao GDP. `EnemyArchetype`, os tipos de item e
prop, estados de comportamento e dados de gameplay são linguagem específica do
jogo, enquanto Math/Seed/Transform vêm da WKC Forge. Assim, o alvo permanece
`gdp_entities` e a API, `gdp::entities`; não existe uma camada genérica de
entidades na engine sem um caso real de reutilização.

A função de gameplay permanece reconhecível mesmo quando aparência, materiais, dano e assimetria variam.

## Elenco

- **Worker:** pressão corpo a corpo e ferramenta industrial.
- **Guard:** combate armado de média distância.
- **Officer:** comando e suporte tático.
- **Engineer:** manipulação de sistemas/ambiente.
- **IndustrialRobot:** Boss pesado em fases.

## Regra narrativa

Cada família precisa parecer ter uma função real no complexo. O combate surge da invasão,
corrupção ou reprogramação dessa função.

## Spawn

Encounter Generator filtra role por `RoomTheme`, `RoomRole`, profundidade, dificuldade e budget.

Na população inicial atual, START recebe um Worker ativo, Normal recebe Worker,
Arena recebe Worker, Guard, Officer e Engineer e Boss recebe IndustrialRobot. Cada Enemy
possui `EnemyArchetype`, HP máximo/atual, comportamento, cooldowns, transform e
`roomId`.
Cada Enemy também recebe uma `visualSeed` derivada do domínio `EnemyVisual` da
seed de sua sala. A aparência pode mudar sem perturbar o grafo, as salas ou o
fingerprint estrutural. Os quatro papéis da Arena e o IndustrialRobot do Boss
recebem update e visual completo quando sua sala se torna atual. O Officer acelera ataques; o Engineer
repara HP ou props `Damaged`, sempre por range, linha de visão e `roomId`, usando
cooldowns independentes. O IndustrialRobot alterna entre longa distância,
pressão e perseguição nos limiares de 66% e 33% de seus 250 HP.

Enquanto o Player vivo permanece além do alcance de detecção, cada inimigo
ativo da sala atual alterna `Patrol` e `Idle` ao percorrer e observar waypoints
locais em torno do spawn. A percepção existente interrompe essa rotina antes de
Chase/Reposition/Attack; inimigos dormentes e de outras salas não são atualizados.

Alcance sem linha de visão não concede conhecimento da posição. Depois de uma
aquisição visual, `EnemyData` conserva apenas no runtime a última posição vista
e um timer por arquétipo. Perder visão ou alcance inicia `Investigate`, que usa
o mesmo movimento físico até o ponto lembrado; readquirir atualiza a memória e
retoma o papel de combate, enquanto a expiração devolve o inimigo à patrulha.
Player morto, Enemy morto, entidades dormentes e salas inativas não sustentam
uma perseguição oculta.

Um disparo consumido também pode preencher essa memória sem conceder visão. A
origem é a posição do Player no instante do tiro e o alcance efetivo combina o
raio do estímulo com a audição específica do arquétipo. A notificação fica
restrita a inimigos vivos e ativos da sala atual; áudio, renderer e RNG não
participam desse contrato. Se houver linha de visão no fixed update seguinte,
a observação real substitui imediatamente a posição apenas ouvida.

Quando a memória estava inativa, esse estímulo começa com `Alert`: movimento e
ações de papel param enquanto o inimigo gira para a origem por uma janela curta
específica do arquétipo. Ruídos seguintes atualizam a origem e o tempo de
investigação, mas não renovam a pausa. A transição evita resposta instantânea
sem permitir que tiros sucessivos funcionem como controle de grupo.

Quando a busca expira, `Recover` conserva o inimigo em uma varredura curta sem
ação ofensiva antes de apagar a memória e retornar à patrulha. A direção inicial
é derivada da identidade estável e inverte na metade da janela. Visão direta
cancela o estado em favor do papel de combate; novo ruído volta à investigação
sem repetir a pausa inicial.

Movimentos com destino — patrulha, investigação e perseguição — usam desde a
Fase 106 um grafo de visibilidade local à sala. Ele encontra desvios completos
ao redor de conjuntos de AABBs, mas não move inimigos entre salas nem substitui
o `CollisionWorld`, a separação física ou as regras específicas dos papéis.

Desde a Fase 107, aquisição visual também respeita a orientação corporal.
Cada papel possui meia abertura explícita entre `65°` e `85°`; um alvo fora
desse cone não atualiza a memória mesmo com alcance e geometria livres. Audição
continua omnidirecional e pode iniciar Alert sem conceder visão automática.

Na Fase 108, inimigos com o mesmo destino recebem lados preferenciais estáveis
no desempate do pathfinding. Um bloqueio real pode trocar essa preferência por
meio do desvio existente. O resultado reduz concentração em uma única rota sem
reservas globais, RNG ou alteração da ordem de atualização das entidades.

Na Fase 12, `RoomEventPlan` pode acrescentar Workers/Guards em ondas. Cada spawn
usa posição, HP e `visualSeed` precomputados pela substream `Events`, entra no
mesmo `EntityManager`/`RoomInstance` e só atualiza a partir do fixed update
seguinte. Arena espera o elenco inicial morrer antes da onda de lockdown.

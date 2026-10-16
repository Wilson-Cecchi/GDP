# World — Conteúdo procedural das salas

> **Estado:** zonas e proteção de clearance implementadas na Fase 4; shells
> determinísticos de Player, Enemy, Item e Prop implementados na Fase 6;
> loot, props, eventos e iluminação por role/theme/depth, com substreams independentes,
> validados na Fase 12.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `README(6).md — Decoração procedural`
- `Guia_de_texturas(1).md`

## Ordem

```text
ROOM GEOMETRY
→ reserved zones
→ gameplay anchors
→ encounter
→ loot
→ props
→ events
→ lighting
→ decoration
```

## Streams

EnemySpawn, Loot, Props, Lighting, Events e Audio independentes.

## Reservas

Marcar connectors, portas, navegação, player entry e objective volume antes de qualquer spawn.

## Budgets

```text
propBudget
enemyBudget
lootBudget
visualDetailBudget
lightBudget
```

Não sortear cada detalhe independentemente sem limite global.

## Coerência temática

Security favorece painéis/monitores/barricadas; Laboratory equipamentos científicos; Technical tubulação/máquinas; Storage caixas/prateleiras.

## Conteúdo inicial implementado

`populateInitialEntities()` roda depois do gate estrutural e registra em cada
`RoomInstance` apenas os handles controlados pelo `EntityManager`. START recebe
Player, Ammo e um crate procedural, sem inimigos nem terminal. A sala Terminal,
colocada separadamente no mapa, recebe o console da missão, um Worker, um Guard
e um Officer ativos desde a criação, além de Ammo e props. Seu evento acrescenta
mais dois inimigos também presentes desde a criação, para cinco adversários no
encontro. Normal recebe
um ou dois inimigos (Worker, com Guard possível em maior profundidade), props e
loot variáveis; Hub recebe Health e prop temático; Arena recebe dois a quatro
inimigos distintos escolhidos entre Worker, Guard, Officer e Engineer, mais
Ammo e props; DeadEnd recebe Ammo e props; Secret recebe Key, Health, Weapon e
crate; Boss recebe um IndustrialRobot com 360, 390 ou 420 HP por seed, door e
barricadas/props. Os quatro papéis possíveis da Arena e o IndustrialRobot já
possuem IA, visual e combate; itens
possuem coleta e efeitos funcionais.

Na candidata de balanceamento, os 360/390/420 HP continuam a base determinística
antes dos multiplicadores. Em Standard/Speedrun, o robô recebe `1,90×` dessa
base; na Campanha, `1,65×` no piso 1, `1,72×` nos pisos 2–4, `1,82×` nos
pisos 5–7 e `1,90×` nos pisos 8–10, além do multiplicador efetivo de
dificuldade/andar. No Standard Normal isso resulta em 684–798 HP; na Campanha
Hard do piso 10, em aproximadamente 1347–1572 HP. A população das salas
`Normal` não muda.

Os ajudantes iniciais do Boss são:

| Modo/andar | Easy | Normal | Hard |
| --- | --- | --- | --- |
| Standard e Campanha piso 1 | Guard | Officer + Guard | Officer + 2 Guards |
| Campanha pisos 2–7 | Officer | Officer + Guard | Officer + 2 Guards |
| Campanha pisos 8–10 | Officer + Guard | Officer + 2 Guards | 2 Officers + 2 Guards |

Speedrun usa a composição Standard Hard. Os ajudantes são ativos e ocupam
pontos distintos à esquerda do robô, longe de portas, terminal e props;
Engineer e Worker não fazem parte desse encontro. Duas
barricadas fixas no flanco esquerdo, uma de `2,4 × 1,35 × 0,6 m` e outra de
`0,6 × 2,25 × 2,4 m`, oferecem cobertura para a aproximação sem bloquear o
eixo central. A mais baixa protege o jogador agachado; a alta protege em pé.
A conclusão de Standard/Speedrun e a liberação do terminal na Campanha exigem
derrotar o robô e todos os ajudantes do encontro. Um ajudante que perseguir o
jogador para fora da sala ainda conta. A Campanha também exige concluir a missão
do andar; matar o robô primeiro não dispensa nenhum desses requisitos.

Referência de equilíbrio anterior à ampliação, obtida por probe temporária e
proxy ideal de tiro, sem combate em partida: Standard seeds 1/2/3 tinham rota
pelos centros de 66,5/60,5/64,5 m, 9/8/7 inimigos iniciais no caminho com
541/516/416 HP somados, e Boss de 420/420/390 HP. Campanha seed 1, pisos
1/5/10, tinha rotas de 54,3/74,7/76,7 m, 5/11/11 inimigos com
316/865,9/1240,1 HP somados, e Boss de 420/554,4/670,8 HP. A probe não está
preservada no repositório; estes números servem como referência histórica, não
como medição pós-alteração. Playtest ainda deve conferir ritmo, uso da Sniper
em 20–30 m e aproximação eficaz da escopeta.

O evento da Arena acrescenta um inimigo já ativo com a população inicial,
totalizando três a cinco adversários no encontro.
A variação usa `SeedDomain::EnemySpawn` e não altera os streams de loot, props,
eventos ou o visual dos inimigos. A sala Secret continua garantindo a Carbine,
mas sua ligação no grafo varia; o boss tem mais tempo para apresentar as três
faixas de comportamento. A dificuldade efetiva ainda exige playtest humano.

A operação exige um world com START, Terminal e um manager vazio; se uma dessas
pré-condições falhar, retorna handles inválidos sem criar conteúdo parcial.

## Props funcionais

`generatePropLayout()` usa somente `SeedDomain::Props`, aplica budget de 1–3,
posiciona footprints completas dentro das `propZones` e impede sobreposição.
Storage/Cafeteria favorecem crates, Laboratory/Maintenance favorecem canisters e
Security favorece barricadas. Cada instância registra HP máximo, drop opcional,
`visualSeed`, escala e estado `Intact`, `Damaged` ou `Destroyed`.

Crate possui 60 HP e libera Health ou Ammo; canister possui 35 HP; barricade
possui 90 HP. Dano abaixo de metade marca `Damaged`; dano letal marca
`Destroyed`, remove a colisão e impede drops repetidos. Engineer restaura HP e
estado. Terminal e door não recebem HP e absorvem tiros sem serem destruídos.

## Eventos funcionais

Cada sala recebe um `RoomEventPlan` derivado de `SeedDomain::Events` depois do
commit estrutural. A sala Terminal usa o terminal; Normal/DeadEnd podem receber reforços;
Hub/Secret recebem suporte; Arena/Boss mantêm lockdown até suas condições de
limpeza. Os inimigos planejados já estão ativos desde a criação do andar; as
recompensas surgem quando o evento termina. Ambos preservam ownership por sala
e não duplicam após `Completed`.

## Iluminação funcional

Cada sala recebe um `RoomLightingPlan` derivado somente de
`SeedDomain::Lighting`. Papel, tema e bounds definem mood, ambiente e de uma a
quatro luminárias próximas ao teto. Secret permanece escura mas legível;
Hub/Terminal/Arena/Boss recebem quatro pontos. Terminal/Arena/Boss e Security podem reagir
ao lockdown com alerta vermelho temporário sem regenerar o plano.

## Variação

Mesmo RoomModule pode variar iluminação, props, inimigos, loot, dano e sujeira sem mudar geometria principal.

## Testes

O teste de população confirma ownership por sala, IDs válidos, ordem determinística
e ausência de inimigos ou console na START. Cada Item é
comparado com tipo, quantidade e posição de seu `LootLayout`; budget, zonas,
separação e distribuição são gates dedicados. Cada prop procedural também é
comparado integralmente com seu `PropLayout`.
As mesmas duas seeds críticas validam todos os `RoomEventPlan`; o runtime cobre
triggers, ondas, limpeza, lockdown e recompensa única.
Os planos de luz percorrem 1.000 seeds e seis moods; bounds, intensidade mínima,
hash dourado, todos os rooms das seeds críticas e isolamento estrutural são
gates dedicados.

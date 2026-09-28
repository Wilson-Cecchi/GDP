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
Player, terminal, um Worker ativo, Ammo e um crate procedural; Normal recebe
um ou dois inimigos (Worker, com Guard possível em maior profundidade), props e
loot variáveis; Hub recebe Health e prop temático; Arena recebe dois a quatro
inimigos distintos escolhidos entre Worker, Guard, Officer e Engineer, mais
Ammo e props; DeadEnd recebe Ammo e props; Secret recebe Key, Health, Weapon e
crate; Boss recebe um IndustrialRobot com 360, 390 ou 420 HP por seed, door e
barricadas/props. Os quatro papéis possíveis da Arena e o IndustrialRobot já
possuem IA, visual e combate; itens
possuem coleta e efeitos funcionais.

O evento da Arena ainda acrescenta uma onda de um inimigo depois de limpar a
população inicial, totalizando três a cinco adversários ao longo do encontro.
A variação usa `SeedDomain::EnemySpawn` e não altera os streams de loot, props,
eventos ou o visual dos inimigos. A sala Secret continua garantindo a Carbine,
mas sua ligação no grafo varia; o boss tem mais tempo para apresentar as três
faixas de comportamento. A dificuldade efetiva ainda exige playtest humano.

Desde a Fase 102, START também recebe uma divisória industrial fixa de
`0,46 × 2,25 × 2,80 m`, posicionada fora do eixo das portas e no lado oposto ao
Worker. A escala horizontal acompanha a rotação da sala, mantendo o mesmo espaço
de ocultação em qualquer orientação. O vão atrás da divisória permite caminhar
por aproximadamente `2 m` ainda sem linha de visão e sair por uma borda para
forçar a readquisição. Ela reutiliza o papel visual funcional de Barricade, tem
180 HP e não consome budget nem RNG do layout procedural. O acabamento HD final
permanece pendente de aprovação visual.

A operação exige um world com START e um manager vazio; se uma dessas
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
commit estrutural. START usa o terminal; Normal/DeadEnd podem receber reforços;
Hub/Secret recebem suporte; Arena/Boss mantêm lockdown até suas condições de
limpeza. Inimigos e recompensas surgem somente no runtime, preservam ownership
por sala e nunca duplicam após `Completed`.

## Iluminação funcional

Cada sala recebe um `RoomLightingPlan` derivado somente de
`SeedDomain::Lighting`. Papel, tema e bounds definem mood, ambiente e de uma a
quatro luminárias próximas ao teto. Secret permanece escura mas legível;
Hub/Arena/Boss recebem quatro pontos. START/Arena/Boss e Security podem reagir
ao lockdown com alerta vermelho temporário sem regenerar o plano.

## Variação

Mesmo RoomModule pode variar iluminação, props, inimigos, loot, dano e sujeira sem mudar geometria principal.

## Testes

O teste atual confirma ownership por sala, IDs válidos, ordem determinística e
40 objetos para a seed 12345 do mundo padrão de oito salas. Cada Item é
comparado com tipo, quantidade e posição de seu `LootLayout`; budget, zonas,
separação e distribuição são gates dedicados. Cada prop procedural também é
comparado integralmente com seu `PropLayout`.
As mesmas duas seeds críticas validam todos os `RoomEventPlan`; o runtime cobre
triggers, ondas, limpeza, lockdown e recompensa única.
Os planos de luz percorrem 1.000 seeds e seis moods; bounds, intensidade mínima,
hash dourado, todos os rooms das seeds críticas e isolamento estrutural são
gates dedicados.

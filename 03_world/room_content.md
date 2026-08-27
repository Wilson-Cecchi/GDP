# World — Conteúdo procedural das salas

> **Estado:** zonas e proteção de clearance implementadas na Fase 4; shells
> determinísticos de Player, Enemy, Item e Prop implementados na Fase 6.
> Budgets e streams detalhados entram nas fases de gameplay procedural.
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
→ lighting
→ decoration
```

## Streams

EnemySpawn, Loot, Props, Lighting e Events independentes.

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
Player, terminal e um Worker ativo; Normal recebe Worker e crate; Hub recebe health e decoração;
Arena recebe Worker, Guard e Officer; DeadEnd recebe ammo e crate; Secret recebe
key e health; Boss recebe IndustrialRobot e door. Worker, Guard e Officer já
possuem IA e combate; itens e os demais papéis continuam como shells lógicos.

A operação exige um world com START e um manager vazio; se uma dessas
pré-condições falhar, retorna handles inválidos sem criar conteúdo parcial.

## Estados de prop

`Intact`, `Damaged`, `Destroyed` quando o gameplay exigir.

## Variação

Mesmo RoomModule pode variar iluminação, props, inimigos, loot, dano e sujeira sem mudar geometria principal.

## Testes

O teste atual confirma ownership por sala, IDs válidos, ordem determinística e
os 18 objetos do mundo padrão de oito salas. Sem overlap, saída livre,
distribuição por theme e budgets permanecem gates das próximas fases.

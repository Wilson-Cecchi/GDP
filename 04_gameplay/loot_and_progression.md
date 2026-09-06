# Gameplay — Loot, progressão e modos

> **Estado:** loot procedural, coleta automática e progressão de sessão até o
> nível 10 implementados na Fase 12; Victory, Defeat e nova run determinística
> implementados na Fase 13. O runtime de Armor e sua recompensa por Vitality
> foram integrados na Fase 58; Weapon pickup, Special e persistência entre
> execuções permanecem planejados. A base determinística do cronômetro de run
> foi implementada na Fase 74, o modo Speedrun tornou-se selecionável na Fase 75
> e o recorde pessoal local foi integrado na Fase 76. A Fase 77 adicionou
> repetição integral da mesma seed; a Fase 78 permite retomar a seed do recorde.
> A Fase 79 iniciou uma Campanha jogável de três andares; a Fase 80 adicionou
> carry-over validado de recursos e progressão entre eles. A Fase 81 tornou a
> entrada do andar um checkpoint persistente retomável pelo menu principal. A
> Fase 82 adicionou pressão progressiva determinística entre os andares.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Loot pipeline

```text
RoomRole + RoomTheme + depth + difficulty + LootSeed
→ budget
→ zonas candidatas
→ seleção
→ placement
→ validação
```

`generateLootLayout()` implementa esse fluxo usando somente `SeedDomain::Loot`.
START garante Ammo; Hub garante Health; Arena e DeadEnd garantem Ammo; Secret
garante uma Key e Health; Boss não recebe recurso antes da luta. Normal usa a
preferência temática entre Health e Ammo. Dificuldade escala o budget sem
permitir overspend e cada layout possui hash canônico.

## Categorias

Health, Ammo, Key/Objective e Armor estão implementados no contrato de coleta.
Weapon e Special permanecem planejados. Pickups são posicionados nas `lootZones`, mantêm
separação mínima e não bloqueiam connectors.

## Coleta

`collectNearbyLoot()` opera automaticamente no fixed update, somente no
`roomId` atual e em raio de `0,85 m`. Health respeita HP máximo, Ammo respeita a
reserva máxima, Armor respeita sua capacidade e Key incrementa o estado do Player. Recurso já
cheio mantém a entidade no chão; coleta válida usa destruição adiada, remove o
handle da sala e só então faz flush. Jogador morto não coleta.

O renderer usa formas mínimas emissivas verdes, amarelas, ciano e azul para
Health, Ammo, Key e Armor. Assets procedurais específicos permanecem para uma
etapa visual posterior. Nesta fase as tabelas procedurais continuam sorteando
somente Health/Ammo/Key, preservando hashes e distribuição; Armor chega ao fluxo
jogável pela recompensa Vitality.

## Progressão de sessão

`ProgressionSystem` recebe apenas resultados já consolidados. Worker, Guard,
Officer, Engineer e IndustrialRobot concedem respectivamente 25/40/55/50/200
XP; prop destruído concede 10, loot coletado 5 e evento concluído 75. Cada fonte
também possui score e contador próprios. Eventos inválidos ou quantidade zero
não alteram estado.

O nível começa em 1, exige 100 XP e cada próximo limiar cresce 75, até o máximo
10. Resto de XP atravessa múltiplos níveis; no cap, XP corrente fica zero e
total/score continuam acumulando.

```text
nível 2, 5, 8  → Vitality: +15 HP máximo e +15 cura
nível 3, 6, 9  → Firepower: +4 dano da pistola
nível 4, 7, 10 → Capacity: +4 reserva máxima e +4 munição
```

Os benefícios são aplicados às instâncias reais de `Player` e `Weapon`. O
título mostra nível, XP atual/limiar e score. Em Standard e Speedrun, o sistema
dura uma run; na Campanha, a Fase 80 preserva esse estado ao avançar de andar.

## Validação atual

O gate executa 1.000 room seeds, exige hashes únicos, budget, placement,
separação, categorias e key de Secret válidos e protege o hash dourado
`0x23E65CDDD8252D83`. Testes funcionais cobrem capacidade, clamp, distância,
sala, jogador morto, remoção e ciclo de vida do Entity Manager.

## Andar

O primeiro fluxo oficial é: alcançar a sala Boss → registrar o
IndustrialRobot vivo → derrotá-lo → `Victory`. Uma Boss sem entidade válida não
conclui a run. Morte do jogador produz `Defeat`; ambos os resultados congelam
Player, inimigos, armas, interações e eventos.
Segredos são opcionais e nunca bloqueiam o caminho principal.

`SessionSystem` é independente do loop visual. Ele recebe vida do jogador,
papel da sala e status do Boss, mantém um estado terminal idempotente e expõe a
seed da run. `NEW RUN/NEXT SEED` e `REPLAY SEED`, confirmados por teclado ou
mouse no menu de resultado, solicitam reinício com políticas distintas.
`runIndex` conta tentativas e `seedIndex` conta avanços na sequência; ambos
começam em zero. A primeira seed preserva o valor fornecido e as seguintes usam
`deriveSeed(initialSeed, SeedDomain::World, seedIndex)`. Todo restart limpa mundo,
entidades, Player, Weapon, progressão, eventos, iluminação, áudio e renderer
antes de inicializar a nova seed.
Enquanto Playing, o título indica `REACH BOSS` ou `DEFEAT BOSS` conforme a sala
ativa; o índice mostrado ao jogador começa em um.

## Campanha

Na Fase 79, `CAMPAIGN` passou a ser um modo selecionável no título e encadeia
três andares. O primeiro usa a seed atual; cada `NEXT FLOOR` deriva o próximo
mundo por `seedForRun(initialSeed, seedIndex)`. Somente Victory nos andares um e
dois permite esse avanço. Defeat, estado jogável, modo diferente e vitória no
terceiro rejeitam `NextCampaignFloor` sem alterar a sessão.

HUD e título mostram `FLOOR n/3`. Vitórias intermediárias exibem `FLOOR CLEAR`;
a final exibe `CAMPAIGN CLEAR`. `REPLAY FLOOR` mantém andar e seed, enquanto uma
nova campanha volta ao primeiro andar e avança a sequência determinística.
Desde a Fase 80, `NEXT FLOOR` captura um `CampaignCarryover` com HP, Armor,
capacidades máximas, chaves, definição da arma, munição e todo o
`ProgressionState`. O snapshot só é aceito com jogador vivo, números finitos,
recursos dentro das capacidades e ranks consistentes. A restauração ocorre
depois do spawn, preservando a posição do novo andar, e zera cooldowns e estados
transitórios. Uma segunda cópia registra a entrada do andar; `REPLAY FLOOR` e
restart restauram essa base, sem perder os ganhos de andares anteriores nem
manter dano/munição gastos na tentativa. Título e outros modos descartam os
snapshots.

Na Fase 81, essa base segura recebe também seed inicial/atual, índices de run e
seed, andar e versão das regras em `CampaignCheckpoint`. `CONTINUE` restaura
somente uma sessão ainda não iniciada e regenera deterministicamente o andar;
estado intermediário de portas, inimigos, pickups e eventos não é transportado.
Defeat ou encerramento preserva a entrada para nova tentativa. Uma nova Campaign
substitui o checkpoint quando o andar inicializa, e `CAMPAIGN CLEAR` o remove.

Na Fase 82, cada andar recebe respectivamente `1.00x`, `1.10x` e `1.20x` de
pressão. Além do combate e dos eventos, o mesmo valor alimenta os parâmetros de
loot e props: andares posteriores reduzem gradualmente o budget de recursos e
podem elevar a pressão espacial sem alterar mapa, seed estrutural ou categorias
obrigatórias. O HUD mostra o percentual ativo.

## Speedrun

Usa a mesma simulação e geração. A Fase 74 adicionou ao `SessionState` um
cronômetro contado em ticks fixos de 60 Hz: ele começa em zero, avança somente
durante gameplay ativo, inclui o passo que determina Victory/Defeat e congela no
estado terminal. Título, HUD e log usam a mesma representação `MM:SS.cc` (ou
`HH:MM:SS.cc` após uma hora). Pausa, configurações, menus e tempo de renderização
não alteram o resultado; `restart()` zera o cronômetro junto da próxima run.

Na Fase 75, `SPEEDRUN` tornou-se uma opção própria no menu principal. Ele usa a
mesma seed, geração, combate e condição de vitória da run Standard, mas o timer
continua avançando enquanto pausa, configurações ou a confirmação de saída
congelam a simulação. Assim, menus não oferecem vantagem de tempo. O resultado
terminal congela o valor; qualquer reinício preserva o modo e voltar ao título
permite selecionar Standard novamente.

Na Fase 76, toda vitória válida em Speedrun é comparada a um recorde pessoal
local. O primeiro tempo cria o recorde; somente um valor estritamente menor o
substitui. Empates, tempos piores, Defeat, Standard e resultados com zero ticks
não alteram o dado. O registro conserva também a seed vencedora e
`currentSpeedrunRulesVersion`, impedindo que regras diferentes sejam comparadas
silenciosamente.

O arquivo `speedrun_record.toml` usa schema próprio e troca atômica após releitura
do temporário. Um arquivo ausente inicia sem recorde; conteúdo inválido,
incompatível ou de versão futura é ignorado com segurança. HUD e título mostram
`BEST` durante Speedrun, e Victory destaca `NEW RECORD` quando o tempo acabou de
ser criado ou melhorado. Ranking online continua planejado sobre esse contrato.

Na Fase 77, `REPLAY SEED` passou a reconstruir integralmente a run com a seed
atual. A tentativa avança, mas o índice da sequência de seeds não; ao escolher
`NEW RUN/NEXT SEED` depois, o próximo mundo determinístico ainda é o sucessor
imediato. Isso permite treinar e comparar uma rota sob o mesmo mapa. O comando
`RESTART RUN` da pausa segue a mesma política de repetição.

Na Fase 78, `BEST SEED` usa a seed guardada no recorde para iniciar um novo
Speedrun diretamente pelo título. `SessionSystem::selectRunSeed()` só aceita a
troca antes do primeiro tick e não altera a seed inicial nem o índice da
sequência; assim, `REPLAY SEED` repete o desafio e `NEXT SEED` continua capaz de
retomar a progressão determinística normal.

## Dificuldade

A progressão da Campanha altera dano, mobilidade, vida, eventos e budgets de
recursos/props, mas não muda a identidade estrutural da seed. O multiplicador é
derivado do andar, limitado ao intervalo da Campanha e coberto por uma versão
própria de regras. Modos Standard e Speedrun preservam o balanceamento original.

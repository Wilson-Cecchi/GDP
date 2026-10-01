# Persistence — Saves e configurações

## Contrato atual — revisão de 2026-10-01

- Três slots: `campaign_slot_1.toml` a `campaign_slot_3.toml`, no diretório
  de preferências SDL de WKC/GDP. Schema 5 adiciona a dificuldade base;
  schemas 3/4 compatíveis continuam legíveis e assumem Normal.
  Checkpoint válido aceita andares 1–10; recursos e seed continuam validados.
- O arquivo único legado `campaign_checkpoint.toml` é importado somente para
  slot 1 vazio e válido. Após salvar/verificar o novo arquivo, o legado é
  preservado em `campaign_checkpoint.legacy.toml`, que impede nova importação
  após completar a campanha. Slots já existentes nunca são sobrescritos por
  migração. Arquivos inválidos/incompatíveis ficam preservados e indisponíveis.
- Settings schema 7 adiciona `binding_0` a `binding_21` com nomes estáveis.
  Schemas 1–6 mantêm as configurações e recebem os controles originais.
  Bindings inválidos ou duplicados recuperam o conjunto padrão, sem ações
  inacessíveis. A gameplay usa o conjunto salvo, enquanto a UI permanece fixa.
- `speedrun_leaderboard.txt` tem cabeçalho `GDP_SPEEDRUN_BOARD 1 N` e no máximo
  10 registros de regras, ticks e seed, ordenados por ticks/seed. Somente
  vitórias Hard entram. A regra de Speedrun 36 não reaproveita tempos das regras
  antigas; seus arquivos não são apagados. O recorde único atual continua
  alimentando o HUD e Best Seed, junto do primeiro colocado do placar.
- Gravações conservam temporário, flush, validação e substituição final;
  falha não remove o save anterior. Campanhas concluídas removem somente o
  slot ativo. Execuções headless não acessam preferências do jogador.
- Save no meio do andar e placar online continuam fora desta revisão.

O restante deste documento registra a evolução histórica; referências a um
slot, três andares ou settings v6 não descrevem mais o fluxo atual.

> **Estado:** configurações e recorde de Speedrun persistentes; checkpoint único
> de entrada de andar da Campaign implementado na Fase 81. Slots completos e
> save no meio do andar permanecem planejados.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## SaveGame

```cpp
struct SaveGame {
    SaveVersion version;
    Seed campaignSeed;
    int floor;
    Difficulty difficulty;
    PlayerPersistentState player;
};
```

## Slots

Quatro slots independentes são compatíveis com a direção anterior.

## Escrita segura

`write temp → flush → validate → atomic replace`. Arquivo inválido não deve sobrescrever
silenciosamente um save anterior.

## Retomar no meio do andar

Só adicionar serialização de portas, inimigos, pickups e objetivos quando essa funcionalidade
for realmente necessária. Até lá, seed + progresso entre andares é mais simples.

## Checkpoint da Campaign

A Fase 81 implementou `CampaignCheckpointStore` como um save único e automático
em `campaign_checkpoint.toml`, no diretório de preferências da plataforma. Ele
registra versão do arquivo e das regras, seed inicial e atual, índices de
tentativa e sequência, andar e o snapshot permanente de Player, Weapon e
Progression capturado na entrada do andar.

O arquivo usa `write temp → flush → reload/validate → rename`. Seeds precisam
corresponder a `seedForRun`, o andar deve estar entre 1 e 3 e todo recurso,
capacidade, munição e rank precisa formar um estado válido. Arquivo ausente,
malformado, futuro ou de regras incompatíveis não habilita `CONTINUE`; campos
desconhecidos são ignorados para extensão futura.

O checkpoint representa o início do andar, nunca um frame intermediário. Assim,
Defeat, saída e retorno ao título preservam uma tentativa reproduzível sem
serializar entidades e portas. `NEXT FLOOR`, replay e nova Campaign substituem
o arquivo somente após inicialização válida; concluir o terceiro andar o remove.

A Fase 82 avançou `currentCampaignRulesVersion` para 2 porque a pressão por
andar altera combate e geração de conteúdo. Checkpoints da regra 1 não são
migrados silenciosamente: ficam indisponíveis, e uma nova Campanha cria um
checkpoint coerente com o balanceamento atual.

A Fase 83 avançou as regras da Campanha para 3 e as do Speedrun para 2, pois a
entrada de Armor nas tabelas procedurais muda decisões de rota, sobrevivência e
tempo. Checkpoints da regra 2 e recordes da regra 1 ficam incompatíveis sem
serem apagados ou reinterpretados.

A Fase 84 introduziu `WeaponArchetype` no carry-over. O schema do checkpoint
avançou para 2 e grava `weapon_archetype` junto dos atributos e munição; schema
1 não é reinterpretado. As regras da Campanha/Speedrun avançaram para 4/3, pois
encontrar a Carbine altera sobrevivência, rota e tempo de conclusão.

A Fase 85 avançou as regras da Campanha/Speedrun para 5/4 porque o Special de
Arena altera combate e tempo. Overdrive não é serializado: cooldown e timer são
estado transitório e voltam a zero na entrada, replay ou troca de andar. O
schema do checkpoint permanece 2, pois sua estrutura persistente não mudou.

A Fase 86 avançou as regras da Campanha/Speedrun para 6/5 porque inimigos que
contornam cobertura alteram sobrevivência e tempo. O compromisso lateral é
estado transitório de cada Enemy e não pertence ao checkpoint de entrada do
andar; o schema continua em 2.

A Fase 87 avançou as regras da Campanha/Speedrun para 7/6 porque a separação de
grupos altera posições de tiro e tempos. Nenhum dado novo é persistente: o
steering é recalculado do estado vivo da sala, mantendo o checkpoint schema 2.

A Fase 88 avançou as regras da Campanha/Speedrun para 8/7 porque volumes físicos
mudam rotas e tempos em encontros densos. O contato e a correção de sobreposição
são recalculados no fixed update; nenhum campo foi adicionado e o checkpoint
permanece no schema 2.

A Fase 90 não altera regras nem schema: poses de inimigo são apresentação
derivada do comportamento, do tick visual e da `visualSeed`. O contador reinicia
com a run, não é serializado e não muda IA, colisão, dano ou geração;
Campaign/Speedrun permanecem em 8/7 e o checkpoint em schema 2.

A Fase 91 também mantém regras e schema: o yaw é estado visual transitório do
`Transform`, recalculado deterministicamente durante a IA e regenerado com o
andar. Checkpoints continuam representando apenas a entrada do andar;
Campaign/Speedrun permanecem em 8/7 e o schema em 2.

A Fase 92 mantém o mesmo contrato: a convergência angular é transitória e
recalculada pelo fixed update, sem campos persistentes. Campaign/Speedrun
continuam em 8/7 e o checkpoint permanece no schema 2.

A Fase 93 não altera schema nem checkpoint. `hitReactionTime` é feedback visual
transitório, reiniciado por dano não letal e descartado ao reconstruir uma run;
não deve ser serializado. Campaign/Speedrun permanecem nas regras 8/7.

A Fase 94 também não altera dados persistentes. A hierarquia e as poses locais
são reconstruídas da receita visual, seed, estado e tick; a força de Hit deriva
do timer transitório já existente. Campaign/Speedrun continuam em 8/7 e o
checkpoint permanece no schema 2.

A Fase 95 mantém o mesmo contrato: perfis de movimento são constantes visuais
selecionadas pelo papel e não são serializados. Regras Campaign/Speedrun seguem
em 8/7 e o checkpoint permanece no schema 2.

A Fase 96 mantém apenas durante a run um histórico visual por `EntityId`, usado
para interpolar estado anterior e atual em fixed ticks. O mapa é limpo ao
inicializar ou repetir a run e não entra em checkpoint, settings ou recorde. A
queda terminal usa somente a `visualSeed` existente; regras 8/7 e checkpoint
schema 2 permanecem inalterados.

A Fase 97 calcula o ragdoll somente do histórico transitório, papel,
`visualSeed`, índice da parte e progresso de Death. Nenhuma pose, velocidade ou
junta é serializada; reiniciar a run reconstrói tudo deterministicamente.
Campaign/Speedrun seguem em 8/7 e o checkpoint permanece no schema 2.

A Fase 98 apenas amplia a duração transitória de Death conforme o papel e muda
suas curvas visuais. O histórico continua descartável e não é serializado;
Campaign/Speedrun permanecem em 8/7 e o checkpoint no schema 2.

A Fase 99 deriva o azimute e os tempos de contato exclusivamente da
`visualSeed`, do papel e do índice da parte. Direção, progresso e repouso não
entram no save; a nova janela de 48–66 ticks continua sendo apenas apresentação.
Campaign/Speedrun permanecem em 8/7 e o checkpoint no schema 2.

A Fase 100 mantém origem, waypoint, timeout e pausa da patrulha somente no
`EnemyData` runtime; nenhum desses campos entra no checkpoint, que continua no
schema 2 e recria o andar pela seed. Como a patrulha muda rotas e tempos,
Campaign avança das regras 8 para 9 e Speedrun das regras 7 para 8. Checkpoints
e recordes de regras anteriores são rejeitados pelos contratos existentes.

A Fase 101 mantém última posição conhecida e tempo restante de investigação no
mesmo estado transitório de `EnemyData`; o checkpoint continua no schema 2 e
reconstrói os inimigos sem memória ao recriar o andar. Como aquisição visual e
busca temporária mudam rotas e tempos, Campaign avança das regras 9 para 10 e
Speedrun das regras 8 para 9. Fixtures das regras imediatamente anteriores
confirmam o isolamento de checkpoints e recordes.

A Fase 102 adiciona uma cobertura fixa à população da START sem alterar o
schema 2: o checkpoint continua armazenando seed e recria o conteúdo da sala.
Como o novo volume muda visão, hitscan, navegação e tempo, Campaign avança das
regras 10 para 11 e Speedrun das regras 9 para 10. Fixtures das regras 10/9
confirmam o isolamento da versão anterior.

A Fase 137 mantém a recarga como estado transitório de `Weapon`: checkpoint de
entrada, replay, troca de andar e equip restauram a arma em repouso, sem novo
campo no schema 2. Como o tempo real sem disparar altera sobrevivência e duração
das runs, Campaign/Speedrun avançam para regras 24/23; checkpoints 23 e recordes
22 permanecem no disco, mas são rejeitados como incompatíveis.

A Fase 138 avança o checkpoint para schema 3. Além da arma ativa, o arquivo
grava presença, definição, pente e reserva do slot guardado; identidades iguais
ou munição acima da capacidade invalidam o estado. Cooldown, recarga, spread e
Overdrive continuam transitórios. O schema 2 de arma única não é reinterpretado,
e Campaign/Speedrun avançam para regras 25/24 por causa das novas decisões de
troca, munição e tempo.

A Fase 139 mantém o schema 3: o tempo restante de entrada da arma é transitório,
assim como cooldown, recarga, spread e Overdrive, e restore sempre entrega os
dois slots em repouso. Como os `0,34 s`/`0,48 s` sem disparar alteram combate e
tempo de conclusão, Campaign/Speedrun avançam para regras 26/25. Checkpoints 25
e recordes 24 continuam no disco, mas são rejeitados como incompatíveis.

A Fase 140 também mantém o schema 3, pois atalhos, duração e interrupção da
entrada continuam transitórios. Como a seleção mais rápida altera decisões e
tempo, Campaign/Speedrun avançam para regras 27/26. Checkpoints 26 e recordes 25
permanecem preservados no disco e incompatíveis com a regra atual.

## Settings

Formato legível como TOML/JSON, versionado e tolerante a campos ausentes/desconhecidos.
Valores inválidos usam defaults/clamps.

A Fase 66 implementou o estado runtime clampado para volumes, sensibilidade e
FOV. A Fase 67 adicionou `gdp_persistence/SettingsStore`: um `settings.toml` de
versão 1 no diretório de preferências resolvido por SDL. Na Fase 68, o schema
avançou para versão 2 e adicionou `invert_mouse_y`; na Fase 69, a versão 3
adicionou `fullscreen`; na Fase 70, a versão 4 adicionou o par de resolução; e
na Fase 72, a versão 5 adicionou `vertical_sync`; e na Fase 118, a versão 6
adicionou `graphics_quality`. Os campos atuais são
`master_volume`, `effects_volume`, `ambient_volume`, `ui_volume`,
`mouse_sensitivity`, `invert_mouse_y`, `field_of_view`, `graphics_quality`,
`window_width`, `window_height`, `fullscreen` e `vertical_sync`.

Campos ausentes mantêm defaults e campos desconhecidos são ignorados para
compatibilidade futura. Campos conhecidos malformados voltam ao default e
valores numéricos fora da faixa são clampados, com status explícito de
recuperação. Versão ausente, inválida ou incompatível não é interpretada como o
schema atual.

O leitor v6 também aceita arquivos v1 a v5. Em v1, `invert_mouse_y` e
`fullscreen` assumem `false`; em v2, somente `fullscreen` assume `false`. Nas
três versões anteriores, a resolução assume 1280×720. O resultado informa
migração, mas o arquivo original não é reescrito até uma mudança real feita no
menu. Valores booleanos aceitam apenas `true` ou `false`; outra representação
recupera `OFF`. O par de tamanho precisa corresponder a um preset completo;
valor parcial, malformado ou não suportado recupera 1280×720. Em todos os
schemas anteriores ao v5, VSync assume `ON`, preservando o comportamento
histórico do jogo. Schemas anteriores ao v6 assumem `ULTRA`, que corresponde
ao renderer existente antes dos presets; instalações novas começam em `MEDIUM`.

A escrita usa um temporário no mesmo diretório, faz flush, relê o arquivo pelo
mesmo parser e compara o estado canônico antes do rename final. Se qualquer etapa
falhar, o temporário é removido e o arquivo anterior não é apagado. A Application
salva ao fechar `SETTINGS` e repete no shutdown apenas se ainda houver alteração
pendente. Execuções headless não acessam preferências do usuário.

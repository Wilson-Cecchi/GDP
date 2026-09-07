# Persistence — Saves e configurações

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

## Settings

Formato legível como TOML/JSON, versionado e tolerante a campos ausentes/desconhecidos.
Valores inválidos usam defaults/clamps.

A Fase 66 implementou o estado runtime clampado para volumes, sensibilidade e
FOV. A Fase 67 adicionou `gdp_persistence/SettingsStore`: um `settings.toml` de
versão 1 no diretório de preferências resolvido por SDL. Na Fase 68, o schema
avançou para versão 2 e adicionou `invert_mouse_y`; na Fase 69, a versão 3
adicionou `fullscreen`; na Fase 70, a versão 4 adicionou o par de resolução; e
na Fase 72, a versão 5 adicionou `vertical_sync`. Os campos atuais são
`master_volume`, `effects_volume`, `ambient_volume`, `ui_volume`,
`mouse_sensitivity`, `invert_mouse_y`, `field_of_view`, `window_width`,
`window_height`, `fullscreen` e `vertical_sync`.

Campos ausentes mantêm defaults e campos desconhecidos são ignorados para
compatibilidade futura. Campos conhecidos malformados voltam ao default e
valores numéricos fora da faixa são clampados, com status explícito de
recuperação. Versão ausente, inválida ou incompatível não é interpretada como o
schema atual.

O leitor v5 também aceita arquivos v1, v2, v3 e v4. Em v1, `invert_mouse_y` e
`fullscreen` assumem `false`; em v2, somente `fullscreen` assume `false`. Nas
três versões anteriores, a resolução assume 1280×720. O resultado informa
migração, mas o arquivo original não é reescrito até uma mudança real feita no
menu. Valores booleanos aceitam apenas `true` ou `false`; outra representação
recupera `OFF`. O par de tamanho precisa corresponder a um preset completo;
valor parcial, malformado ou não suportado recupera 1280×720. Em todos os
schemas anteriores ao v5, VSync assume `ON`, preservando o comportamento
histórico do jogo.

A escrita usa um temporário no mesmo diretório, faz flush, relê o arquivo pelo
mesmo parser e compara o estado canônico antes do rename final. Se qualquer etapa
falhar, o temporário é removido e o arquivo anterior não é apagado. A Application
salva ao fechar `SETTINGS` e repete no shutdown apenas se ainda houver alteração
pendente. Execuções headless não acessam preferências do usuário.

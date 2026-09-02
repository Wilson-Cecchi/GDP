# Persistence — Saves e configurações

> **Estado:** especificação técnica inicial do novo GDP.
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

## Settings

Formato legível como TOML/JSON, versionado e tolerante a campos ausentes/desconhecidos.
Valores inválidos usam defaults/clamps.

A Fase 66 implementou o estado runtime clampado para volumes, sensibilidade e
FOV. A Fase 67 adicionou `gdp_persistence/SettingsStore`: um `settings.toml` de
versão 1 no diretório de preferências resolvido por SDL. Na Fase 68, o schema
avançou para versão 2 e adicionou `invert_mouse_y`; na Fase 69, a versão 3
adicionou `fullscreen`; na Fase 70, a versão 4 adicionou o par de resolução. Os
campos atuais são
`master_volume`, `effects_volume`, `ambient_volume`, `ui_volume`,
`mouse_sensitivity`, `invert_mouse_y`, `field_of_view`, `window_width`,
`window_height` e `fullscreen`.

Campos ausentes mantêm defaults e campos desconhecidos são ignorados para
compatibilidade futura. Campos conhecidos malformados voltam ao default e
valores numéricos fora da faixa são clampados, com status explícito de
recuperação. Versão ausente, inválida ou incompatível não é interpretada como o
schema atual.

O leitor v4 também aceita arquivos v1, v2 e v3. Em v1, `invert_mouse_y` e
`fullscreen` assumem `false`; em v2, somente `fullscreen` assume `false`. Nas
três versões anteriores, a resolução assume 1280×720. O resultado informa
migração, mas o arquivo original não é reescrito até uma mudança real feita no
menu. Valores booleanos aceitam apenas `true` ou `false`; outra representação
recupera `OFF`. O par de tamanho precisa corresponder a um preset completo;
valor parcial, malformado ou não suportado recupera 1280×720.

A escrita usa um temporário no mesmo diretório, faz flush, relê o arquivo pelo
mesmo parser e compara o estado canônico antes do rename final. Se qualquer etapa
falhar, o temporário é removido e o arquivo anterior não é apagado. A Application
salva ao fechar `SETTINGS` e repete no shutdown apenas se ainda houver alteração
pendente. Execuções headless não acessam preferências do usuário.

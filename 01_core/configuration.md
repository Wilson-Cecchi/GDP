# Core — Configuração e tuning

> **Estado:** especificação técnica inicial do novo GDP.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `README(6).md — configurações persistentes`

## Objetivo

Separar:

```text
constantes arquiteturais
configurações do usuário
parâmetros de balanceamento
parâmetros de debug
```

## Estruturas

```cpp
struct EngineConfig { double fixedStep; int maxEntities; int assetCacheBudgetMB; };
struct GameplayTuning { float playerMoveSpeed; float playerMaxHealth; float workerSpeed; };
```

## Arquivos sugeridos

```text
config/defaults.toml
config/debug.toml
```

Settings pessoais persistentes são tratados em `09_persistence/save_and_settings.md`.

## Regra

Não espalhar números mágicos. Todo valor ajustável precisa ter dono, unidade, default e range.

## Compatibilidade

- campo ausente → default;
- desconhecido → ignorar quando seguro;
- inválido → clamp/fallback + log;
- schema antigo → migrar quando necessário.

## Testes

Arquivo ausente, parcial, inválido, versão antiga e atualização preservando valores válidos.

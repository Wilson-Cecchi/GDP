# Auditoria de candidatos cooperativos — 02/11/2026

## Decisão

Nenhum cérebro novo passou o protocolo de promoção completo. Os cinco arquivos
`assets/ai/*.brain` permanecem iguais. Worker 1820 e Robot 2030 continuam
ativos; Guard, Officer e Engineer mantêm os cérebros anteriores.

| Candidato | Origem no ZimaOS | Avaliação local | Resultado |
| --- | --- | --- | --- |
| Officer 30870 | Ampla: 1,0301; 0 regressões | Ampla: 1,0301; 0 regressões. Final: 0,9226; 16 regressões e 3 pares com mais acertos em aliados | Reprovado na final |
| Officer 54950 | Rápida: 1,2745; 0 regressões | Ampla: 1,2112; 4 regressões | Reprovado na ampla |
| Engineer 72100 | Rápida: 1,1262; 0 regressões | Ampla: 1,1929; 1 regressão | Reprovado na ampla |
| Guard | Nenhum índice amplo elegível | Sem candidato para a final | Manter ativo |

O Officer 30870 perdeu dano ao Player em `center_cover` e `bhop` e teve
regressões em equipe, inclusive dano a aliados. O Engineer 72100 caiu de 15
para 10 de dano em `bhop` na seed 152227, acima da tolerância desse papel.
Score agregado alto não cancela uma regressão por cenário e seed.

## Protocolo e rastreabilidade

- Avaliador: `build/cafeteria-furniture/gdp_ai_gameplay_eval`, SHA-256
  `f3cb78a5d013e34053ad0b40c4506bf553ecb63139eea387a182b9dac151188b`.
  Fonte local `f0c1c0e54d85664846a55dc1f105cecea3ca77c1` com alterações de
  trabalho; o hash do binário identifica exatamente o avaliador executado.
- Comparação pareada: mesmos cinco cérebros ativos, binário, seeds, quadros e
  cenários para ativo e candidato, alterando apenas o papel avaliado. Modos:
  mapas, duelo, equipe com autoria de fogo amigo e investigação após perder visão.
- Ampla: seeds 152201, 152213, 152227 e 152239, 1200 quadros. Final inédita:
  seeds 10007, 11003, 13001, 17011, 19001, 23003, 29009 e 31013, 1800
  quadros. A final foi executada apenas para o Officer que passou na ampla.
- Gate v4 de `tools/zimaos/training_evaluation.py`: score combinado acima de
  1,0001 e nenhuma regressão em mapas, duelo, equipe ou investigação. Esta
  auditoria também rejeita aumentos de acertos em aliados, mesmo quando o
  dano desses acertos é zero.
- Candidatos copiados dos índices `best.json` do serviço
  `gdp-perception-v4-r3`, sem interromper o treino. SHA-256: Officer 30870
  `4db335909ab54518394a1c680df9c081f0f24d79ab69d12e30d124d237052f90`;
  Officer 54950
  `149f3f4b81e415e8a16a55bbbf81af3f539f92b41134c7f43ab5b35a40be2648`;
  Engineer 72100
  `203b27f2b9710ddd84550e5b50ba2eb0380564380d61a4686e6a1d0ab7c3d132`.
- Manifesto, CSVs pareados e relatórios JSON ficam em
  `build/ai-audit-2026-11-01/` (artefatos locais ignorados pelo Git). O diretório
  mantém a data de início da auditoria; esta decisão foi fechada em 02/11.

Os testes `active_enemy_brain_catalog`, `active_enemy_brains_headless`,
`ai_gameplay_eval_deterministic`, `enemy_sensors_systems` e `combat_systems`
passaram. Não houve substituição de asset, commit, push ou publicação.

## Serviço de treino

Na checagem de 02/11, o contêiner `gdp-perception-v4-r3` estava parado com
código 255, sem indicação de OOM e com os dados e índices preservados. Foi
reiniciado pelo Compose existente para retomar os checkpoints. Após quatro
minutos, o processo ainda estava ativo, porém o healthcheck marcava
`unhealthy`, `status.json` seguia em `stopped` e nenhum novo lote havia
começado. O processo estava bloqueado em leitura de diretório EXT4; a rotina
`latest_population()` percorre recursivamente o histórico de gerações.
O reinício do treino ainda não foi confirmado. Nenhum diretório de treino foi
apagado ou modificado manualmente.

# Seleção de IA para o próximo Download — Worker 280

O playtest de gameplay da prévia com Worker 280 foi aprovado pelo responsável pelo jogo em 19/10/2026. Esta seleção troca somente `assets/ai/worker.brain`. Os cérebros ativos de Guard, Officer, Engineer e Robot permanecem byte a byte iguais aos anteriores. A avaliação automática tornou o Worker elegível; a aprovação do playtest é uma decisão separada.

## Comparação do Worker

- Candidato: Worker, geração 280, SHA-256 `343ca0029718805bd8a8c3aa81d7a8de1a599c5e4c6daeaffea9b1fb68be3650`.
- Ativo anterior: SHA-256 `23220e82047115191c6552d8102754a256e2e31b319a5fb904ddb98c555dfb6c`.
- Protocolo 4: `paired-solo-team-perception-attributed-v4`, com autoria de fogo amigo atribuída ao atirador. Ativo e candidato foram executados na mesma imagem/binário, com as mesmas sementes, duração e cenários de mapas, duelos, equipes e investigação após perda de visão.
- Sementes `152201`, `152213`, `152227` e `152239`; 1200 quadros por avaliação. Score combinado `1,1147783251231527`, elegível, zero regressões.
- Revisão de origem registrada no relatório: `433f51b5eb64+worktree-747ed7d9d9988fa3+attribution-v4-r2-7d5a5bcdc16b`.
- Evidência local da avaliação: `build/ai-playtest-2026-10-19/worker280-elegivel/avaliacao.json` (artefato de build, fora do Git).

Essas quatro sementes são de desenvolvimento. O relatório não documenta uma avaliação final com sementes inéditas; a aprovação em partida não deve ser confundida com essa evidência automática adicional.

## Demais papéis

Guard, Officer, Engineer e Robot não passaram os gates disponíveis e não são promovidos nesta versão. Os candidatos isolados de Officer 70, Engineer 280 e Robot 70 tiveram, respectivamente, 4, 9 e 11 regressões na avaliação ampla; Guard 1960 teve 17 regressões na avaliação rápida. Score isolado ou geração mais alta não substituem a comparação por regressões e segurança de equipe.

| Papel mantido | SHA-256 do cérebro ativo |
| --- | --- |
| Guard | `a4fb3621f4fdecbaa9a83a84cd6c915eb2fc2ae75f7cec5b43ad249323017261` |
| Officer | `df8556cda163e90278b833412781cc69b5d7c0f094315ad34ed6de05b1d67b34` |
| Engineer | `93bf1bfc554f4a3885fd7e875e29adf974bf8763f4fa2e42f5bf33d3482d641d` |
| Robot | `78d57f01aacca39fd03c3fe06e268c0a52094e5d5beb9c08035757efa15c974a` |

Este registro cobre apenas a seleção dos cérebros. Build, testes de integração, empacotamento e publicação são verificados no fechamento da versão.

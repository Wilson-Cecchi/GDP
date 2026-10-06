# Campanha — variedade de andares e missões

## Escopo

Na Campanha de 10 andares, o layout e uma tarefa secundária são derivados da
seed do andar. Standard, Speedrun e Test Mode mantêm a geração e o fluxo
anteriores. A mesma seed, andar e versão de regras reproduzem as mesmas escolhas.
Nenhum objetivo depende de texto narrativo ou de um objeto novo: a chave da
sala Secret, o evento Lockdown da Arena e o terminal com emboscada da START já
fazem parte do conteúdo obrigatório.

## Layout

`campaignFloorLayout(floorSeed, floor)` usa o domínio de seed `CampaignLayout`.
O orçamento é 7–9 salas nos andares 1–4, 8–10 nos andares 5–8 e 9–10 nos
andares 9–10; zero ou uma ligação extra forma uma rota alternativa. O gerador
continua validando conectores, corredores, START, Arena, Secret e Boss. Em
Campanha, há até 64 tentativas determinísticas de geração. O grafo e a
população permanecem reproduzíveis; o layout não é alterado por dificuldade.

## Missões

- `SecureArena`: concluir o evento Lockdown da Arena. É a missão fixa do
  primeiro andar para apresentar o sistema.
- `RecoverKey`: recolher a chave que já aparece na sala Secret. Não basta ter
  chaves de andares anteriores: conta o pickup neste andar.
- `SecureTerminal`: ativar o terminal da START e eliminar a emboscada que ele
  libera. A ativação isolada não conclui a missão; o HUD muda de `ACTIVATE
  TERMINAL` para `CLEAR AMBUSH` (com traduções PT-BR).

Nos andares 2–10, `CampaignMission` seleciona uma das três tarefas pela seed.
A tarefa é concluída uma vez e não pode ser desfeita no mesmo andar. A saída
na sala Boss só é liberada quando **boss morto e missão concluída**. A ordem
dessas ações é livre; se o boss morrer primeiro, o jogador pode voltar e
terminar a missão. Interagir com o terminal antes disso não avança. O HUD
mostra a tarefa pendente e o aviso de bloqueio em inglês ou português (Brasil).
No décimo andar, a interação liberada abre a vitória final.

## Orientação pelo minimapa

Enquanto a missão estiver pendente, o mapa destaca sua sala em âmbar quando
ela for descoberta. Depois, destaca a sala Boss em vermelho; com Boss morto e
saída liberada, troca para um contorno verde na mesma sala. Se o Boss morrer
primeiro, a missão continua marcada até ser concluída. Esses marcadores não
revelam salas desconhecidas, não alteram o grafo nem afetam Standard, Speedrun
ou Test Mode. O mapa mantém os traçados reais dos corredores.

## Persistência e falhas

O checkpoint continua representando a entrada do andar. Replay/Continue
regeneram a mesma missão e o mesmo mapa; progresso intermediário da missão não
é persistido. A versão de regras passa a 39. Saves de regras 37 ou 38/schema
7 são aceitos como entrada do andar e normalizados em memória; o arquivo
original não é regravado no carregamento. Uma missão sem sala-alvo ou sem
chave na Secret ou terminal na START falha na inicialização da run, em vez de criar um andar sem
saída. Os formatos e regras de Standard/Speedrun não mudam.

## Validação

Os testes geram 240 combinações de seed/andar, verificam os limites de salas,
variação de rotas, determinismo e existência dos alvos. Há testes de conclusão
de missão, bloqueio/liberação do terminal, migração de checkpoint, tradução e
renderização do HUD. Playtest manual recomendado: iniciar com uma seed fixa,
repetir o andar, matar o boss antes da missão, voltar à Arena/Secret/START e então
ativar o terminal; verificar os três tipos de missão em andares diferentes.

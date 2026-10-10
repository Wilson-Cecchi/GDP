# Campanha — variedade de andares e missões

## Escopo

Na Campanha de 10 andares, o layout é derivado da seed do andar e a sequência
de tarefas da seed inicial da campanha. Standard, Speedrun e Test Mode mantêm a
geração e o fluxo anteriores. A mesma seed inicial, andar e versão de regras
reproduzem as mesmas escolhas.
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

Nos andares 2–10, `CampaignMission` embaralha três blocos de três andares
(2–4, 5–7 e 8–10) pela seed inicial da campanha. Cada bloco contém as três
tarefas uma vez, e o primeiro item do bloco é rotacionado quando repetiria a
missão do andar anterior. O andar 1 continua com `SecureArena`. Assim, não há
duas missões iguais em andares consecutivos; campanhas diferentes ainda podem
ter ordens diferentes.
A tarefa é concluída uma vez e não pode ser desfeita no mesmo andar. A saída
na sala Boss só é liberada quando **boss morto e missão concluída**. A ordem
dessas ações é livre; se o boss morrer primeiro, o jogador pode voltar e
terminar a missão. Interagir com o terminal antes disso não avança. O HUD
mostra a tarefa pendente e o aviso de bloqueio em inglês ou português (Brasil).
No décimo andar, a interação liberada abre a vitória final.

Ao avançar para um novo andar, a Campanha recupera uma granada de fragmentação,
até o limite de duas. O bônus é aplicado apenas na transição de andar, antes
do checkpoint de entrada; reiniciar ou continuar o mesmo andar restaura esse
checkpoint sem conceder outra granada. Standard, Speedrun e Test Mode não
recebem essa reposição de campanha.

## Orientação pelo minimapa

Enquanto a missão estiver pendente, o mapa destaca sua sala em âmbar quando
ela for descoberta. Depois, destaca a sala Boss em vermelho; com Boss morto e
saída liberada, troca para um contorno verde na mesma sala. Se o Boss morrer
primeiro, a missão continua marcada até ser concluída. Esses marcadores não
revelam salas desconhecidas, não alteram o grafo nem afetam Standard, Speedrun
ou Test Mode. O mapa mantém os traçados reais dos corredores.

Segurar `Tab` abre a lista de situação do andar: missão atual (inclusive a
transição de ativar terminal para eliminar emboscada), boss vivo/derrotado e
saída bloqueada/liberada. Essa leitura continua disponível no confronto com
o boss; não altera objetivos, não descobre salas e não aparece nos demais modos.

## Persistência e falhas

O checkpoint continua representando a entrada do andar. Replay/Continue
regeneram a mesma missão e o mesmo mapa; progresso intermediário da missão não
é persistido. A versão de regras passa a 40. Saves de regras 37–39/schema 7
são aceitos como entrada do andar e normalizados em memória; o arquivo
original não é regravado no carregamento. Ao continuar um save de regras 39,
recursos, seed e layout do andar são preservados, mas a missão da entrada segue
a sequência nova. Uma missão sem sala-alvo ou sem
chave na Secret ou terminal na START falha na inicialização da run, em vez de criar um andar sem
saída. Os formatos e regras de Standard/Speedrun não mudam.

## Validação

Os testes geram 240 combinações de seed/andar, verificam os limites de salas,
variação de rotas, determinismo e existência dos alvos. Há testes de conclusão
de missão, bloqueio/liberação do terminal, migração de checkpoint, tradução e
renderização do HUD. Outros 1.000 seeds verificam todos os blocos de missões,
ausência de repetição consecutiva e variação de ordem. Playtest manual
recomendado: iniciar com uma seed fixa,
repetir o andar, matar o boss antes da missão, voltar à Arena/Secret/START e então
ativar o terminal; verificar os três tipos de missão em andares diferentes.

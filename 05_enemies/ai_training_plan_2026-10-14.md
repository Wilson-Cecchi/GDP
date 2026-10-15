# Novo treino de IA para as mecânicas da 0.1.6 — 14/10/2026

## Decisão e estado

O usuário aprovou as salas do Boss e, até o momento, Tab e Campanha. A granada
teve aprovação visual parcial; a revisão geral de texturas fica para outra
etapa. O próximo ciclo de IA deve aprender e ser avaliado nas salas maiores,
com alcance longo do Robot, ajudantes e janela de reação na entrada. Esses
elementos não devem ser retirados do jogo para favorecer um candidato.

Os cinco cérebros de `assets/ai/` continuam sendo a referência da versão
0.1.6. A [avaliação de 13/10](ai_evaluation_2026-10-13.md) não aprovou
nenhum candidato para Download. O serviço cooperativo anterior usa código
anterior à 0.1.6 e não inclui o Robot na avaliação de equipe. Em 14/10, após
a VPN voltar, o ZimaOS respondeu ao SSH: o contêiner antigo estava parado
desde 13/10, com `restart=no`; seu estado registrava 303 lotes concluídos
por cada um dos quatro papéis.

## Protocolo novo, separado do anterior

- O treino cooperativo usa currículo novo e populações incompatíveis com as
  antigas. `--resume-population` só aceita uma população completa do mesmo
  protocolo. Os números são 16 (básico), 17 (todos os cenários), 18 (mapas
  variados) e 19 (ambos); o serviço usa 19 e mantém `reward_rules=6`. O ciclo
  começa em diretório novo, sem importar um campeão antigo.
- A rotação inclui Worker, Guard, Officer, Engineer e Robot. O encontro do
  Robot usa a sala Boss ampla, Officer e Guard como ajudantes e combate de
  longo alcance. Mapas, duelos e equipe são avaliados pelo **mesmo binário**
  para o ativo e o candidato.
- O avaliador `--team` acrescenta `boss_helpers`, registra o papel de cada
  inimigo e separa dano causado pelo Robot e pelos ajudantes. O relatório
  mantém tiros/acertos, retenções, dano e mortes aliadas, duração e resultado
  por cenário e seed. O protocolo novo tem identificação própria; CSVs do
  serviço antigo não entram em suas médias.
- As seeds de desenvolvimento do serviço novo são 150001 e 150011 na bateria
  rápida e também 150031 e 150053 na ampla. Uma bateria final deve usar seeds
  inéditas congeladas **antes** de consultar qualquer novo candidato:
  151001, 151013, 151027, 151039, 151051, 151061, 151073 e 151099.
  `evaluate_final.py` só registra esse resultado; não atualiza o ranking.
- Um ganho médio não compensa piora grave em um par, aumento de dano aliado,
  morte aliada ou Robot passivo enquanto os ajudantes fazem todo o dano. Os
  resultados de mapas e duelos precisam ser lidos junto com a equipe.

## Ordem de validação

1. Compilar o jogo, o treino e o avaliador na mesma revisão; executar testes
   do currículo, retomada de população, fixture e parser do serviço.
2. Fazer um piloto local curto em diretório ignorado pelo Git. Confirmar que
   ele produz `.brain`, população completa e CSV reproduzível, sem considerar
   sua pontuação como aprovação de qualidade.
3. Registrar imagem/revisão, estado, temperatura, espaço e últimos checkpoints
   do ZimaOS; manter o serviço antigo parado, preservar seus dados e iniciar a
   imagem nova em outro diretório. Nunca deixar os dois treinos disputarem CPU.
4. Confirmar logs, healthcheck, limite térmico, continuidade após parada e
   avaliação rápida/ampla. Só depois executar o ciclo contínuo.
5. Congelar candidatos por hash; comparar com os ativos em seeds finais
   inéditas, cenários equivalentes e no mesmo binário; fazer playtest em
   Easy/Normal/Hard, especialmente na sala Boss. A promoção de
   `assets/ai/*.brain` e a inclusão no Download exigem decisão separada.

O manifesto `tools/zimaos/compose.coop-new-mechanics.yaml` começou com
`restart: "no"`. Após o piloto r2 e a retomada da população serem validados,
o serviço r2 passou a `restart: always`; os serviços anteriores permanecem
parados com `restart=no`. Nenhum release ou download mudou.

## Implantação e correção de 14/10

O primeiro serviço novo, da revisão `9724f42`, foi iniciado em
`/DATA/AppData/gdp-coop-new-mechanics` com imagem
`gdp-training:coop-new-mechanics-v2`. O teste C++ passou dentro da imagem e o
avaliador produziu o encontro Boss com ajudantes e dano do Robot separado.
O primeiro Worker completou 70 gerações. Sua bateria rápida deu score 1,009,
mas foi rejeitada por regressão no cenário `group`; a ampla não o aprovou.
O lote seguinte, Guard, encontrou uma seed para a qual as 32 tentativas
normais de geração do mapa não bastaram. O serviço foi parado com
`restart=no`. Seus dados e imagem permanecem para auditoria.

A correção limita a quatro sementes de mapa derivadas, com até 64 tentativas
cada, somente no currículo cooperativo. A mesma seed do Guard reproduziu a
falha antes da correção e concluiu dez gerações depois dela. Por mudar o
currículo efetivo, a revisão `ba4c0e1` começou no diretório novo
`/DATA/AppData/gdp-coop-new-mechanics-r2`, sem reaproveitar a população do
primeiro serviço. A imagem r2 é
`sha256:b02a93852ad57a66a2c6527e034318a9a556570f4d8aa726b8f7f8b4f25d8ea1`.
O teste C++ passou dentro da imagem; o avaliador r2 produziu dano separado do
Robot no encontro Boss. A suíte Release local teve 66/66 testes, os testes
Python 15/15 e a CI da revisão `ba4c0e1` ficou verde.

No serviço r2, Worker e Guard concluíram 70 gerações cada, com população
completa e avaliações rápida/ampla. O Worker teve score 1,009 na rápida, mas
regrediu em `group`; a ampla não o aprovou. O Guard teve score 0,588 na rápida
com 19 regressões e score 0,594 na ampla com 30 regressões. Nenhum desses
candidatos foi arquivado como elegível. Uma parada controlada durante o lote
do Officer e a retomada a partir da população da geração 31 foram confirmadas:
o novo processo começou na geração 32, com healthcheck saudável. Depois disso,
o reinício automático foi habilitado para o contêiner r2. O treino contínuo
segue nos cinco papéis; avaliação final e playtest ainda não ocorreram.

O primeiro ciclo completo terminou e voltou ao Worker na geração 71 por
`resume_population=1`. Todos os cinco papéis produziram população e relatório
do protocolo 2. Os scores abaixo são relativos ao cérebro ativo da 0.1.6;
qualquer regressão por cenário bloqueia elegibilidade.

| Papel | Geração | Rápida: score / regressões | Ampla: score / regressões |
| --- | ---: | ---: | ---: |
| Worker | 70 | 1,009 / 1 | 1,000 / 0 |
| Guard | 70 | 0,588 / 19 | 0,594 / 30 |
| Officer | 101 | 1,049 / 2 | 1,014 / 5 |
| Engineer | 70 | 0,699 / 9 | 0,577 / 21 |
| Robot | 70 | 1,314 / 2 | 1,048 / 2 |

Nenhum dos cinco foi elegível nessa rodada. No Robot, `center_cover` e `bhop`
regrediram na rápida, apesar do score médio maior. O treino registrou mapas
longos, dois ajudantes, pressão e perseguição do Boss. O contêiner r2 estava
saudável a 49 °C, com 397 GiB livres em `/DATA`; sua imagem e revisão seguem
as registradas acima. Os hashes dos cinco `assets/ai/*.brain` continuam iguais
aos da avaliação de 13/10.

Para acompanhar o serviço r2:

```sh
cd /DATA/AppData/gdp-coop-new-mechanics-r2
docker compose -f compose.coop-new-mechanics.yaml ps
docker compose -f compose.coop-new-mechanics.yaml logs --tail 20
cat data/status.json
cat data/state.json
```

## Evidência local desta preparação

- Build Release e suíte completa: **66/66 CTest**, 149,88 s. Testes Python de
  avaliação e serviço: **15/15**. Um par controle com o cérebro ativo dos dois
  lados deu score 1,0 em mapas, duelos e equipe; o parser aceitou os CSVs do
  protocolo 2. O manifesto Compose foi lido por PyYAML; esta máquina não tem
  o subcomando `docker compose` para validá-lo com o Docker.
- Piloto local do Robot, seed 20261014, população 8, sete gerações, currículo
  19: produziu cérebro e população completa. A retomada dessa população
  começou corretamente na geração 8. Os arquivos estão somente em
  `build/ai-new-mechanics-pilot/`, ignorado pelo Git.
- O cérebro da geração 7 **foi reprovado** na bateria rápida pareada de seeds
  150001/150011, 480 frames: score solo 0,873, equipe 1,231 e três regressões
  individuais (`boss_cover` nas duas seeds e `ground_strafe` em uma). O piloto
  mostra execução e discriminação do gate, não qualidade suficiente para
  implantação. O treino longo agora roda no r2; as seeds finais e o playtest
  continuam pendentes.
- Os SHA-256 dos cinco cérebros ativos são os mesmos registrados na
  [avaliação de 13/10](ai_evaluation_2026-10-13.md). O ZimaOS respondeu após
  a VPN voltar: imagem antiga `gdp-training:cooperative-v1`, revisão
  `dcfbb0b66fd4240e156a8ad3d850700a959064d6` com scripts do serviço,
  dados antigos em `/DATA/AppData/gdp-coop-training/data`, 398 GiB livres em
  `/DATA`. Nenhum contêiner foi alterado durante essa inspeção.
- Em build com sanitizadores, o teste determinístico do avaliador passou;
  `worker_training_systems` excedeu o timeout de 600 s nessa configuração.
  A suíte Release completa passou. Este timeout não demonstra falha de
  memória, mas limita a evidência do teste instrumentado.

# Proposta — variedade e distribuição de armas

Estado: mecânicas implementadas após autorização do usuário; arte definitiva
pendente de aprovação. O relato de excesso de armas motivou revisar a
distribuição junto com a variedade, sem aumentar o número de pickups.

## Base atual

Há carabina, pistola, faca, sniper, escopeta e submetralhadora. Na Campanha,
salas elegíveis têm 25% de chance de pickup extra, antes 55%. A escolha de
armas usa pistola 15%, carabina 40%, SMG 20%, escopeta 17% e sniper 8%.
Nas salas Secret a arma continua garantida, excluindo pistola e normalizando
os pesos restantes. Standard e Speedrun conservam sua tabela anterior.

## Opções sugeridas

- **Sniper de ferrolho, rara:** precisão e alto dano por tiro, pouca munição,
  cadência baixa e mira mais lenta. Prioridade sugerida para a próxima arma.
  Não prometer morte instantânea de boss; validar tiros no ponto fraco, custo
  de munição e tempo de combate sem modificar seus atributos aprovados.
- **Escopeta:** alternativa de curto alcance, dispersão e recarga lenta;
  complementa a sniper e muda a decisão de aproximação/cobertura.
- **Submetralhadora:** automática, menor dano individual e perda de precisão
  sustentada; implementada como alternativa à carabina, não um slot adicional.

## Implementação inicial para playtest

As novas armas usam o orçamento existente. Duplicatas procedurais da arma
possuída oferecem até um carregador-base de munição para sua reserva, inclusive
quando guardada; não trocam a arma selecionada nem enchem seu carregador.
Armas largadas pelo jogador conservam exatamente sua munição e não recebem
essa conversão. É preciso usar G para liberar a primária antes de pegar outra.

| Arma | Dano-base | Intervalo | Alcance | Carregador/reserva | Recarga |
| --- | --- | --- | --- | --- | --- |
| Sniper | 65 | 1,20 s | 90 m | 5/15 | 2,0 s |
| Escopeta | 14 por pellet, seis rays | 0,85 s | 18 m | 6/24 | 2,4 s |
| SMG | 16 | 0,085 s | 28 m | 24/96 | 1,4 s |

A escopeta consome uma cartucha por disparo e tem queda de dano com distância.
A sniper tem mira de 0,35 s e FOV de mira a 35% do FOV configurado; a SMG aceita
gatilho segurado. Trocas não zeram cooldowns. Bônus de dano são distribuídos
pelos seis pellets da escopeta, evitando multiplicar cada upgrade por seis.
Vida, fases e atributos do boss permanecem inalterados.

O Test Mode oferece as três armas para ensaio imediato. Slot 1 é a primária
possuída, 2 a pistola, 3 a faca; 4/5 continuam reservados. A Campanha usa regras
35/schema 6. Checkpoints antigos de regras 34/schema 3–5 recebem migração
explícita ao ler, preservando recursos, andar e seed; o loot usa o novo balanço.
O arquivo original não é regravado pela leitura. Speedrun mantém regras 36.

Distribuição e invariantes foram cobertos por testes determinísticos, incluindo
1.000 seeds. Esses valores ainda precisam de playtest humano nas três
dificuldades; não são declarados balanceamento final.

## Gate de implementação

As três armas reutilizam temporariamente o modelo aprovado da carabina,
inclusive no chão. Essa reutilização não representa sua aparência definitiva.
Sniper e escopeta têm pranchas v1 em `visual/proposals/`; a prancha da SMG
permanece em preparação após falha do gerador. Aprovar cada unidade exata antes
de implementar modelos/texturas novos, conforme `visual/TEXTURE_APPROVALS.md`.
Contrato e prompts: `visual/proposals/new_firearms_proposal_v1.md`.

# Proposta — variedade e distribuição de armas

Estado: proposta para aprovação; nenhuma nova arma, chance de loot ou atributo
do boss foi alterado nesta etapa. O relato de excesso de armas motiva revisar
a distribuição junto com a variedade, não simplesmente aumentar os pickups.

## Base atual

Há carabina, pistola e faca. Na Campanha, salas elegíveis têm 55% de chance de
pickup extra, escolhendo carabina (70%) ou pistola (30%), além do loot habitual.
Esses percentuais continuam vigentes até aprovarmos e testarmos o balanceamento.

## Opções sugeridas

- **Sniper de ferrolho, rara:** precisão e alto dano por tiro, pouca munição,
  cadência baixa e mira mais lenta. Prioridade sugerida para a próxima arma.
  Não prometer morte instantânea de boss; validar tiros no ponto fraco, custo
  de munição e tempo de combate sem modificar seus atributos aprovados.
- **Escopeta:** alternativa de curto alcance, dispersão e recarga lenta;
  complementa a sniper e muda a decisão de aproximação/cobertura.
- **Submetralhadora:** opção futura de mobilidade, menor dano individual e
  perda de precisão sustentada. Menos prioritária por sobrepor a carabina.

## Distribuição proposta, ainda não implementada

Experimentar reduzir a chance de pickups extras de 55% para 25%, usando novas
armas no mesmo orçamento em vez de adicioná-las por cima. Avaliar duplicatas
substituídas por munição compatível e raridade por tipo. Preservar uma rota
viável de aquisição de arma primária e o uso livre da faca sem exigir sorte.
Comparar contagens por andar em muitas seeds e jogar nas três dificuldades
antes de fixar os valores. Não alterar silenciosamente os saves atuais.

## Gate de implementação

Escolher a próxima arma e aprovar sua proposta visual antes de modelagem/
textura no jogo, conforme `visual/TEXTURE_APPROVALS.md`. Validar empunhadura,
escala independente do FOV, recarga, mira, drop e munição. Os slots 4/5
continuam reservados; a nova arma de fogo deve integrar o inventário existente.

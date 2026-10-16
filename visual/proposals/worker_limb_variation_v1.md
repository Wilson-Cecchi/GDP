# Worker / variação de braços e pernas — proposta visual v1

**Estado: IMPLEMENTADA EM VALIDAÇÃO; proposta aprovada em 15/10/2026 (resposta: "aprovado").** A [prancha SVG](worker_limb_variation_v1.svg)
é uma ilustração vetorial; não é captura no jogo nem saída do gerador atual.
**A prancha é conceitual, não uma captura do jogo.** Os números 101, 202 e 303
identificam exemplos ilustrativos, não resultados reproduzidos do executável.

## Unidade e objetivo

Unidade em revisão: **Worker / silhueta dos membros v1**. Um Worker continuará
quadrado e reconhecível pelos painéis de trabalho, visor e ferramenta. A
variação cria indivíduos do *mesmo papel* com braços e pernas visualmente
distintos. A prancha mostra A (compacto), B (esguio) e C (robusto), cada um de
perto e em escala pequena para julgar a silhueta à distância. Luz neutra
simulada e paleta funcional permitem avaliar formas; esta versão não pede
aprovação de textura, material ou paleta final.

Referência atual: `BodyProfile` contém os perfis Standard, Compact, Lanky e
Heavy. Seus comprimentos de braço são 0,36–0,45 da altura do corpo, e os das
pernas são 0,39–0,46. O gerador escolhe peças esquerda/direita de forma
independente, mas as dimensões básicas ainda vêm quase inteiramente do perfil.
Esta proposta acrescentaria parâmetros de forma **por indivíduo e por membro**
derivados de `visualSeed`; não cria outro arquétipo, classe ou perfil de combate.

## Limites propostos para implementação posterior

Os limites abaixo são uma direção visual a validar com malha e animação. Cada
fator multiplica a dimensão do `BodyProfile` selecionado, mantendo altura geral
e características de Worker dentro do envelope de jogo.

| Parâmetro visual | Intervalo proposto | Restrição |
| --- | ---: | --- |
| Comprimento do braço esquerdo/direito | 0,86–1,14 × base | Ombro encaixado; a mão direita e a ferramenta usam o mesmo alcance final. |
| Espessura de cada braço | 0,78–1,25 × base | Diferença entre lados limitada a 20%; não cobrir visor ou ferramenta. |
| Comprimento aparente de cada perna | 0,92–1,08 × base | Quadril coberto pela união; base de cada pé exatamente no piso. |
| Largura de cada perna | 0,80–1,22 × base | Diferença entre lados limitada a 18%; sem interseção grave entre pernas. |

As duas pernas podem variar separadamente, mas não devem deixar uma perna
visivelmente suspensa. A malha deverá posicionar cada perna entre a união do
quadril e o chão; se um sorteio incompatível criar vão ou penetração, aplicar
limite/fallback **local** daquele membro. As peças continuam caixas ou volumes
de leitura quadrada. A assimetria é moderada, não uma deformação quebrada.

O primeiro protótipo manteria **colisão, hitboxes, alcance de ataque, velocidade,
HP, dano, animações e seleção da IA** existentes. A nova aparência não pode
prometer ao jogador uma área de acerto ou um alcance de arma diferente. A
diferença de altura/largura da malha deve caber no envelope de renderização e
na aproximação de parede/porta já usados pelo Worker. Se isso não for possível
com esses intervalos, reduzir os limites antes da implementação.

## Contrato de determinismo e montagem

- Derivar sub-seeds próprias para os quatro membros a partir de `visualSeed` e
  uma versão de receita explícita. Mesma seed + versão = mesmos fatores, receita
  e hash, inclusive após checkpoint/replay. Não sortear a cada frame.
- Preservar a ordem das partes da receita: torso, cabeça, braço esquerdo,
  braço direito, perna esquerda, perna direita, costas, ferramenta. O atual
  `splitEnemyRigidParts()` depende dos índices 2–5 para pivôs dos membros.
- Recalcular pivôs visuais a partir dos encaixes. O ombro fica coberto pela
  manga/placa; cada pé toca o plano do piso. A mão que segura a ferramenta e o
  socket da ferramenta seguem o comprimento final daquele braço. No futuro,
  armas maiores devem seguir a mesma regra de contato da mão.
- Registrar fatores sorteados na receita/hash canônico versionado e validá-los
  antes de gerar malha. Manter a seed de mundo separada das substreams de IA e
  de gameplay; a variação não muda comportamento de inimigo.

## Aceitação após aprovação da proposta

1. Gerar amostra representativa de seeds incluindo as três silhuetas da prancha
   e perfis Standard, Compact, Lanky e Heavy; conferir zero juntas soltas,
   braços atravessando torso/visor, ferramentas flutuantes e pés sem piso.
2. Repetir a mesma seed e a mesma versão da receita após reiniciar e retomar
   checkpoint; comparar parâmetros, malha e hash. Cobrir limites, fallback local
   e assimetria em teste automatizado.
3. Capturar Workers **dentro do jogo** de perto e a distância de combate, sob
   iluminação representativa e perfis gráficos diferentes. Conferir animação,
   portas/cobertura, legibilidade da ferramenta e leitura do papel.
4. Pedir aceitação visual no jogo separadamente da aprovação desta prancha.

A aprovação cobre apenas a direção de **proporção dos membros do Worker v1** e
seus limites visuais. Guard, Officer, Engineer, Robot e materiais/texturas
continuam fora desta unidade de aprovação. O resultado no jogo exige capturas
nativas e aceitação visual própria após a implementação.

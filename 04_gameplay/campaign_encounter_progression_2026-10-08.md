# Campanha — progressão dos encontros

## Regras

Esta fase muda apenas os inimigos iniciais das salas Normal e Arena na
Campanha. O primeiro andar preserva a população anterior. Standard, Speedrun,
Test Mode, a sala START, as ondas dos eventos e o boss não mudam. Os mesmos
arquétipos continuam usando seus cérebros ativos; o treinamento e a promoção
automática não são afetados. A dificuldade Easy/Normal/Hard continua escalando
vida pelas regras existentes, sem alterar a composição.

| Andares | Sala Normal | Arena |
| --- | --- | --- |
| 1 | 1–2 Workers, Guard por profundidade | 2–4 tipos embaralhados |
| 2–4 | 1–2; par mais frequente, Guard por profundidade | 2–4, posições alternadas |
| 5–7 | 1–2; par mais frequente, Officer possível | 3–4, posições alternadas |
| 8–10 | 2; Worker/Guard com Guard ou Officer de apoio | 4; Guard, Officer, Engineer e outro Guard |

A seed da sala e o número do andar definem deterministicamente a formação. As
novas posições ocupam a metade central/esquerda da sala para evitar a faixa
onde o gerador coloca obstáculos. Nenhuma sala Normal recebe mais de dois
inimigos iniciais, e nenhuma Arena recebe mais de quatro; o evento Lockdown
continua controlando sua própria onda separadamente. As salas Hub, DeadEnd e
Secret continuam sem população inicial.

## Checkpoints e validação

A versão de regras da Campanha é 41. Checkpoints schema 7 das regras 37–40 são
aceitos como entrada do andar e migram em memória; o arquivo não é regravado
até o próximo salvamento. A seed e os recursos do checkpoint são preservados,
mas o encontro inicial do andar segue as regras novas. A versão de arquivo não
mudou.

O teste de mundo verifica 128 seeds em andares iniciais, intermediários e
finais: contagem, repetibilidade, boss intacto e distância das novas posições
às paredes, aos obstáculos sólidos e entre inimigos. O teste de persistência
verifica a migração inclusive da regra 40. Ainda é necessário playtest visual
nos andares 5 e 9 para julgar o ritmo real de combate, XP e munição antes de
publicar uma nova versão para download.

# Engineer — Unidade Técnica de Reparo e Suporte

> **Projeto:** GDP
> **Categoria:** Inimigo de suporte · reparo e combate armado
> **Status:** conceito visual e comportamental de referência; sujeito a ajustes durante a implementação.

## 1. Identidade e função

O **Engineer** é um robô técnico que atua durante o combate para **reparar aliados e equipamentos danificados**, enquanto mantém capacidade limitada de atacar o jogador. Ele aumenta a duração dos confrontos quando não é interrompido e introduz decisões de prioridade de alvos.

Deve parecer um trabalhador técnico especializado, com equipamentos próprios de manutenção, em vez de uma cópia militarizada do Guard.

## 2. Aparência e silhueta

- **Estrutura:** robô humanoide industrial, torso relativamente largo para acomodar conexões e alimentação dos equipamentos.
- **Blindagem:** placas verdes em tom **verde-petróleo** ou azul-esverdeado, com detalhes laranja, cinza e grafite.
- **Cabeça:** módulo compacto com visor âmbar e proteção reforçada.
- **Mochila de diagnóstico:** unidade traseira grande, com grelhas, indicadores luminosos, mangueiras, conexões e reservatórios.
- **Ferramenta característica:** braço ou dispositivo de **soldagem/reparo** com ponteira luminosa, fixadores e cabos.
- **Arma secundária:** equipamento de disparo de porte moderado, visualmente distinto da ferramenta de reparo.

## 3. Vistas de referência

**Frente:** peito em verde-petróleo e laranja, visor âmbar; um lado com arma e outro com manipulador/ferramenta de reparo, criando assimetria intencional.

**Lateral esquerda:** mochila saliente, cabos saindo para o braço técnico, articulações expostas e perfil da ferramenta.

**Costas:** **mochila de diagnóstico em destaque**, com grelhas de ventilação, enrolador de cabos ou mangueiras, visor de status e suportes de serviço.

**Lateral direita:** perfil do armamento, ombreiras, fiação protegida e pontos de conexão; preservar a mesma distribuição de braços e equipamentos nas quatro vistas.

## 4. Equipamentos e habilidades

**Ferramenta de reparo:** módulo industrial que restaura a integridade de robôs aliados ou equipamentos que o sistema do jogo marque como reparáveis.

**Sistema de diagnóstico:** procura entidades danificadas elegíveis, respeitando alcance, visibilidade e possibilidade de alcançar o alvo.

**Arma secundária:** permite reação ofensiva quando reparar não for viável ou quando o jogador se aproximar.

**Fluxo sugerido de reparo:**

1. Selecionar um alvo danificado válido.
2. Deslocar-se até uma distância operacional segura.
3. Iniciar uma animação de conexão ou soldagem facilmente reconhecível.
4. Restaurar integridade ao longo do tempo ou em pulsos.
5. Encerrar o procedimento ao atingir uma condição de parada.

## 5. Comportamento no combate

- Procura alvos danificados próximos e avalia se é seguro repará-los.
- Alterna estados de movimentação, combate e reparo, evitando executar ações incompatíveis ao mesmo tempo.
- Pode interromper o reparo para fugir ou responder a uma ameaça imediata, se a IA permitir.
- Mantém um comportamento de suporte mais evidente que o de atirador.
- Não repara alvos destruídos a menos que uma mecânica de reconstrução seja implementada separadamente.

**Ponto forte:** prolonga a sobrevivência de outros inimigos ou equipamentos.
**Ponto fraco:** durante o reparo, fica mais previsível e oferece uma oportunidade de interrupção.

## 6. Animação e feedback

- **Idle:** checagem do dispositivo portátil, leitura de sensores e pulsos nas luzes da mochila.
- **Locomoção:** oscilação controlada de cabos e peças externas.
- **Início de reparo:** aproximação da ponteira e acendimento de indicadores.
- **Reparo ativo:** feixe curto, faíscas ou brilho técnico direcionado ao alvo, sem dificultar a leitura da cena.
- **Disparo:** animação claramente diferente do uso da ferramenta.
- **Destruição:** perda de alimentação da mochila e queda dos braços, com luzes se apagando.

## 7. Diretrizes de implementação visual

Os marcadores principais são **verde-petróleo**, **mochila técnica** e **ferramenta de reparo**. A pose de reparo deve ser diferenciável da pose de ataque, especialmente em tamanhos reduzidos. Definir efeitos visuais distintos para dano ao alvo e recuperação de integridade.

> **Observação:** a taxa de reparo, a quantidade recuperada, a prioridade de alvos e a distância operacional serão definidos durante os testes de gameplay.

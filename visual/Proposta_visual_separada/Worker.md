# Worker — Unidade Industrial de Ataque Corpo a Corpo

> **Projeto:** GDP
> **Categoria:** Inimigo comum · aproximação e combate corpo a corpo
> **Status:** conceito visual e comportamental de referência; sujeito a ajustes durante a implementação.

## 1. Identidade e função

O **Worker** é um robô industrial originalmente construído para executar tarefas pesadas em ambientes de produção. Adaptado ao combate, ele transforma uma ferramenta de trabalho em uma arma de curta distância e tenta reduzir rapidamente a distância até o jogador. Sua presença deve criar pressão de movimentação: permanecer parado ou permitir que ele se aproxime torna o confronto perigoso.

É a unidade de visual mais diretamente associado ao ambiente fabril do jogo, com uma silhueta robusta, simples de reconhecer e distinta das unidades armadas.

## 2. Aparência e silhueta

- **Estrutura:** corpo humanoide largo e quadrado, peito em formato de bloco, ombros reforçados, membros espessos e pés pesados.
- **Blindagem:** placas metálicas laranja bastante desgastadas, com tinta lascada, riscos, manchas de óleo, fuligem e pequenas marcas de impacto.
- **Mecânica exposta:** articulações grafite, discos de rotação, pistões, parafusos, conduítes e mangueiras protegidos parcialmente pela blindagem.
- **Cabeça e sensores:** módulo pequeno e encaixado no torso, com **visor âmbar** luminoso, protegido por uma moldura metálica.
- **Assinatura visual:** arma-ferramenta industrial volumosa em um dos braços; no conceito atual, uma **serra circular de grande diâmetro** é a principal referência.
- **Linguagem de formas:** superfícies retangulares e cantos rígidos, com poucos elementos delicados.

## 3. Vistas de referência

**Frente:** tórax largo em placas laranja; visor âmbar central; braços assimétricos, sendo um equipado com a serra e o outro com mão ou garra mecânica. Faixas industriais de advertência podem aparecer no torso e nos ombros.

**Lateral esquerda:** espessura das ombreiras, conexões mecânicas do braço-ferramenta, dentes da serra, estrutura do quadril e perfil dos pés reforçados.

**Costas:** painéis de manutenção, grelhas de ventilação, tubulações, suportes de fixação e ligações do mecanismo da ferramenta.

**Lateral direita:** perfil do braço de manipulação, placas laterais, dobradiças e pistões das pernas; manter proporções coerentes com a vista esquerda.

## 4. Equipamento e ataque

**Arma principal — Serra industrial:** ferramenta de corte mecânico usada em golpes de curta distância. O movimento da serra deve ser perceptível antes do contato, permitindo ao jogador identificar o momento de perigo. Alternativas futuras podem reutilizar a mesma base de robô com martelo hidráulico ou braço de impacto, sem modificar sua função principal.

**Ataques sugeridos:**

1. **Golpe simples:** avanço curto seguido de corte frontal.
2. **Golpe pesado:** preparação mais demorada, varrendo uma área maior à frente.
3. **Aproximação agressiva:** aceleração curta para encurtar distância; não é necessariamente um ataque com dano.

## 5. Comportamento no combate

- Detecta o jogador e procura uma rota viável de aproximação.
- Prioriza encurtar a distância e manter-se ao alcance da ferramenta.
- Em alcance próximo, alterna preparação, ataque e recuperação, criando janelas de esquiva e contra-ataque.
- Se perder o caminho, procura outra rota em vez de atacar através de obstáculos.
- Não precisa ser um inimigo particularmente inteligente; seu papel é impor pressão direta.

**Ponto forte:** resistência visual e ameaça quando está próximo.
**Ponto fraco:** ataques curtos e anunciados, que podem ser evitados com posicionamento.

## 6. Animação e feedback

- **Idle:** pequenas vibrações, ajustes de servo e brilho constante no visor.
- **Locomoção:** passadas pesadas, braços com inércia e ruído de atuadores.
- **Preparação de ataque:** elevação da ferramenta e aumento da rotação da serra.
- **Ataque:** movimento amplo, com efeito de giro e faíscas discretas no contato com superfícies apropriadas.
- **Dano:** breve recuo, falha luminosa localizada ou deslocamento de uma placa.
- **Destruição:** perda de força nas juntas, queda pesada e desligamento do visor.

## 7. Diretrizes de implementação visual

Manter a **silhueta quadrada**, o **laranja desgastado** e a **ferramenta industrial** legíveis mesmo em resolução reduzida. Separar visualmente corpo e serra para facilitar animações. Os detalhes pequenos — como arranhões finos ou parafusos individuais — não devem substituir elementos grandes que comuniquem a identidade do inimigo.

> **Observação:** valores de vida, dano, velocidade, alcance e tempo de ataque não foram definidos aqui; devem ser balanceados nos testes do GDP.

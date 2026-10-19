# Officer — Unidade de Comando Tático

> **Projeto:** GDP
> **Categoria:** Inimigo de suporte ofensivo · comando e coordenação
> **Status:** conceito visual e comportamental de referência; sujeito a ajustes durante a implementação.

## 1. Identidade e função

O **Officer** é uma unidade de comando responsável por **aumentar a frequência de ataques dos aliados próximos**, sem deixar de participar do tiroteio. Ele modifica as prioridades do jogador: eliminá-lo pode reduzir a pressão de todo o grupo inimigo.

Seu visual precisa comunicar uma unidade de coordenação, e não apenas um Guard com uma arma diferente.

## 2. Aparência e silhueta

- **Estrutura:** formato humanoide mais contido e compacto que os robôs industriais pesados; postura ereta e controlada.
- **Blindagem:** torso rígido com módulos de comunicação, ombreiras estruturadas e componentes eletrônicos protegidos.
- **Paleta:** azul-marinho escuro, grafite e bordô, com eventuais detalhes oliva e metal exposto.
- **Cabeça:** visor âmbar menor, encaixado em uma proteção angular.
- **Arma:** carabina curta, pistola industrial ou outro armamento compacto de disparo.
- **Elementos distintivos:** **antenas, módulo de comando e indicadores luminosos**, dispostos principalmente nas costas, ombros ou peito.

## 3. Vistas de referência

**Frente:** módulo torácico de comando, luzes de status em coluna, arma curta e placas escuras com inserções bordô.

**Lateral esquerda:** perfil compacto da arma, volume do módulo de comunicação e antenas verticais ou articuladas.

**Costas:** unidade de comunicação retangular, portas de acesso, grelhas e conexões de dados/energia; deve ser reconhecível sem depender do visor.

**Lateral direita:** estrutura lateral da blindagem, braço de disparo e equipamento de transmissão; observar que antenas e caixa traseira mantenham posição coerente.

## 4. Armamento e capacidade especial

**Arma principal — Armamento curto:** permite atacar de modo independente, mas sua ameaça individual não precisa superar a do Guard.

**Capacidade de comando — Aceleração de aliados:** emite um sinal para inimigos elegíveis em uma área próxima, elevando a cadência ou reduzindo o intervalo entre ações ofensivas, conforme a lógica de balanceamento adotada.

**Regras sugeridas para legibilidade:**

1. A ativação do comando tem preparação visual e/ou sonora própria.
2. Os aliados afetados recebem um indicador temporário identificável.
3. O efeito termina ao expirar sua duração ou por outra condição explicitamente definida no código.
4. O acúmulo de bônus de múltiplos Officers deve ser controlado para evitar escalada imprevisível.

## 5. Comportamento no combate

- Prefere ficar próximo de aliados aos quais pode conceder bônus.
- Tenta preservar condições de ativação do comando, sem abandonar totalmente a capacidade de atirar.
- Pode se reposicionar para incluir mais unidades na área de efeito.
- Quando isolado, continua sendo um inimigo armado, porém perde parte de seu valor tático.
- Pode priorizar a capacidade especial ao identificar um grupo em combate, respeitando intervalos de uso.

**Ponto forte:** multiplica a pressão ofensiva de encontros com vários inimigos.
**Ponto fraco:** menor influência quando está sozinho ou longe dos aliados.

## 6. Animação e feedback

- **Idle:** movimentos discretos de sensores, antenas e indicadores.
- **Comando:** pausa identificável, ativação de luzes do módulo e pulso visual ou transmissão de rádio.
- **Disparo:** recuo curto da arma compacta.
- **Deslocamento:** movimentação controlada, com estabilidade no módulo traseiro.
- **Dano:** interferência breve na luz ou sinal, sem necessariamente cancelar uma habilidade em curso.
- **Destruição:** interrupção definitiva da emissão e apagamento progressivo do módulo.

## 7. Diretrizes de implementação visual

A identidade do Officer deve continuar evidente **pela antena, pelo módulo de comando e pelas luzes de status**, mesmo com a arma fora de vista. Diferenciar claramente *sinal ativo* e *sinal inativo*. Evitar que o efeito visual da habilidade obscureça o reconhecimento dos outros inimigos.

> **Observação:** raio do bônus, percentual de aceleração, duração e recarga dependem do balanceamento futuro do GDP.

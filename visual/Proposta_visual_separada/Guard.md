# Guard — Unidade de Segurança de Média Distância

> **Projeto:** GDP
> **Categoria:** Inimigo comum · combate armado de média distância
> **Status:** conceito visual e comportamental de referência; sujeito a ajustes durante a implementação.

## 1. Identidade e função

O **Guard** é uma unidade de segurança adaptada para combate armado. Sua função é exercer pressão de fogo a **média distância**, buscar linhas de tiro e evitar que o jogador chegue perto demais. Enquanto o Worker tenta invadir o espaço do jogador, o Guard procura **preservar distância útil** para continuar atirando.

## 2. Aparência e silhueta

- **Estrutura:** robô humanoide com proporções militares, menos largo que o boss e mais ágil visualmente que o Worker.
- **Blindagem:** placas modulares sobre torso, ombros, antebraços, coxas e canelas.
- **Paleta:** verde-oliva dessaturado, areia, cinza e grafite, com acabamento fosco e sinais de desgaste.
- **Cabeça:** capacete mecânico angular com visor ou sensor âmbar protegido.
- **Equipamento de identificação:** **arma longa** de perfil bem visível, com corpo robusto, cano estendido e componentes táticos.
- **Acessórios:** módulos traseiros de alimentação ou suprimentos, compartimentos laterais e pequena antena.

A figura deve comunicar imediatamente que é um atirador, e não um operário ou unidade de suporte.

## 3. Vistas de referência

**Frente:** rifle segurado junto ao corpo ou à frente do torso; proteção frontal em tons militares; visor âmbar e placas com volumes assimétricos moderados.

**Lateral esquerda:** perfil completo da arma longa, ombreiras, compartimentos na coxa, articulações do joelho e antena traseira.

**Costas:** conjunto modular de suprimentos, conexão de alimentação, painéis de manutenção e cabos protegidos.

**Lateral direita:** outra face das placas táticas, empunhadura e volume do rifle; preservar coerência do conjunto visto pelos outros ângulos.

## 4. Armamento

**Arma principal — Rifle industrial de combate:** dispositivo de disparo de médio alcance, com silhueta comprida, miras ou sensores embutidos e carregador ou módulo de energia visível.

**Ações sugeridas:**

1. **Disparo controlado:** sequência curta e previsível de tiros.
2. **Reposicionamento:** deslocamento lateral ou recuo para recuperar distância adequada.
3. **Nova aquisição de alvo:** pequena pausa de mira após perder contato visual com o jogador.

## 5. Comportamento no combate

- Identifica o jogador e procura linha de tiro disponível.
- Mantém uma **faixa preferencial de distância**, em vez de avançar continuamente.
- Recuará ou fará movimentações laterais quando pressionado de perto, se existir espaço.
- Usa obstáculos como quebra de linha de visão quando essa ação for suportada pela IA.
- Evita disparar através de paredes ou aliados, conforme as regras de colisão e combate do jogo.
- Em encontros combinados, cobre áreas que dificultam a fuga dos ataques corpo a corpo do Worker.

**Ponto forte:** ameaça consistente em corredores ou áreas com boa visibilidade.
**Ponto fraco:** depende de linha de tiro e de espaço para manter a distância.

## 6. Animação e feedback

- **Idle:** postura alerta, varredura discreta do visor e ajustes de arma.
- **Movimentação:** passos firmes, rifle estabilizado em relação ao torso.
- **Mira:** alinhamento claro do cano na direção do alvo antes do disparo.
- **Disparo:** recuo de braços, flash ou emissão compatível com a tecnologia da arma.
- **Reposicionamento:** pequenos deslocamentos laterais e mudanças de apoio.
- **Destruição:** falha do sensor, queda da arma e colapso do corpo.

## 7. Diretrizes de implementação visual

A **arma longa** e a **paleta militar apagada** são os identificadores prioritários. Diferenciar as poses de mira, disparo e recuperação. A leitura do cano deve indicar a direção do perigo, especialmente se os sprites forem utilizados em um jogo com perspectiva em primeira pessoa.

> **Observação:** cadência, precisão, distância ideal, dano e alcance devem ser determinados por testes; não são valores fechados do GDD.

# IndustrialRobot — Chefe Industrial Pesado

> **Projeto:** GDP
> **Categoria:** Boss · combate à distância e perseguição por fases
> **Status:** conceito visual e comportamental de referência; sujeito a ajustes durante a implementação.

## 1. Identidade e função

O **IndustrialRobot** é um robô industrial de grande porte convertido em uma plataforma de combate pesado. Ele funciona como **chefe de confronto**, impondo pressão inicialmente por meio de ataques à distância e passando a um comportamento mais agressivo de perseguição conforme sua integridade diminui.

A transição de comportamento deve ser percebida tanto na IA quanto no aspecto visual e nos efeitos de combate, para que o jogador entenda que o encontro evoluiu.

## 2. Aparência e silhueta

- **Escala:** muito maior, mais largo e mais pesado do que os inimigos comuns.
- **Estrutura:** torso de grande volume, ombros enormes, braços robustos, pernas reforçadas e pés largos.
- **Blindagem:** metal industrial laranja escurecido, grafite e aço exposto; danos de fuligem, amassados, riscos profundos e tinta bastante gasta.
- **Cabeça/sensor:** sensor frontal compacto com forte brilho âmbar ou alaranjado, profundamente protegido por armadura.
- **Arma principal:** **canhão pesado** integrado a um braço; no conceito visual, conjunto de múltiplos canos com partes emissivas.
- **Outro braço:** garra ou mão mecânica pesada, adequada à aproximação e a ataques de impacto.
- **Duas unidades traseiras:** grandes módulos verticais de alimentação, refrigeração ou exaustão, com grelhas e iluminação quente.

Os dois módulos das costas e o grande armamento tornam a silhueta imediatamente identificável.

## 3. Vistas de referência

**Frente:** tórax enorme em placas industriais, canhão integrado ocupando um lado do corpo, mão/garra de impacto no lado oposto, sensor âmbar central e duas unidades traseiras visíveis acima dos ombros.

**Lateral esquerda:** profundidade do canhão, componentes cilíndricos, juntas do ombro, projeção das unidades traseiras e placas espessas das pernas.

**Costas:** **dois módulos verticais bem definidos**, com grelhas emissivas, ventiladores ou dissipadores, tubulações largas e blindagem de serviço.

**Lateral direita:** perfil da garra mecânica, espessura do torso, articulações maciças e a segunda unidade traseira. O boss deve parecer igualmente volumoso de qualquer direção.

## 4. Armamento e padrão de combate

**Canhão pesado:** fonte principal de pressão à distância. Seus ataques devem ter preparação visual distinguível e intervalos de recuperação coerentes com o tamanho do robô.

**Braço de impacto:** arma secundária para ameaçar o jogador durante a aproximação ou em alcance curto.

**Módulos traseiros:** fontes de iluminação e feedback de estado; podem servir como pontos vulneráveis *se* essa mecânica for incorporada ao combate.

## 5. Fases de comportamento sugeridas

### Fase 1 — Controle de área

Com maior integridade, o chefe mantém pressão por ataques à distância, procura ângulos de tiro e obriga o jogador a aproveitar cobertura e janelas entre disparos.

### Fase 2 — Escalada de pressão

Quando perde parte de sua integridade, reduz as pausas entre sequências ofensivas ou combina padrões, sinalizando maior intensidade por meio da iluminação, dos sons e da movimentação.

### Fase 3 — Perseguição

Com a integridade baixa, passa a avançar mais agressivamente em direção ao jogador, ameaçando com o braço pesado e reduzindo oportunidades de permanecer parado em locais seguros. Seus ataques ainda precisam ser legíveis e evitáveis.

> Os limiares que acionam cada fase são **propostos**, não valores fixos; podem ser consolidados em duas ou três fases conforme os testes.

## 6. Animação e feedback

- **Idle:** vibração do chassis, ruído de ventilação e pulsações nos módulos traseiros.
- **Mira:** alinhamento lento e imponente do canhão com o alvo.
- **Disparo pesado:** preparação inequívoca, efeitos de emissão e recuo pronunciado.
- **Dano:** descargas, fuligem crescente e falhas em luzes ou carenagens.
- **Transição de fase:** mudança clara do padrão de luzes, som de sobrecarga e nova postura de combate.
- **Perseguição:** passadas mais frequentes, inclinação do torso e uso expressivo do braço de impacto.
- **Destruição:** perda sequencial de potência nos módulos traseiros, colapso do chassis e desligamento do sensor.

## 7. Diretrizes de implementação visual

Mesmo visto de longe, devem permanecer claros **o canhão**, **os dois módulos traseiros** e a **diferença de escala**. As mudanças de fase não devem depender exclusivamente de cor: combinar alterações de pose, som e efeitos para comunicar a transição. O desgaste deve reforçar massa e perigo sem encobrir a forma central.

> **Observação:** vida total, limiares de fase, dano, mobilidade, padrões de tiro, alcance e eventuais pontos fracos serão definidos no balanceamento do GDP.

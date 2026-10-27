# GDP — Especificação Detalhada de Texturas por Cômodo

> **Nota histórica:** as descrições visuais continuam válidas como referência,
> mas a seção final de status registra a implementação C/SDL2 do GDP anterior.
> Ela não representa o estado atual da nova engine C++/SDL3/OpenGL.
> O estado corrente de Industrial Corridor, Storage, Combat Room, Cafeteria,
> Laboratory, Technical Area, Security e Central Hub está nos documentos
> específicos de ambiente, no resumo
> [walls_current_reference.md](walls_current_reference.md) e em
> `RoomMaterialGenerator`. As resoluções citadas abaixo são convenções da
> proposta histórica, não uma exigência para novas referências.
> Desde a Fase 49, esse gerador pertence a `gdp::game::visual` e mantém os
> contratos procedurais genéricos explicitamente separados.
> Na Fase 50, esses contratos passaram para `wkc::procedural`, sem alterar os
> perfis ou pixels aprovados.

Este documento descreve, com o máximo de precisão prática, a aparência das **paredes, chão e teto** dos principais ambientes do GDP. O objetivo não é apenas definir uma direção artística, mas criar uma referência que possa ser convertida posteriormente em regras de **geração procedural em C**.

A proposta visual geral é um FPS retro-industrial inspirado na leitura visual de jogos dos anos 1990, mas sem copiar texturas específicas. As superfícies devem parecer construídas a partir de materiais simples, com formas grandes e legíveis, detalhes pequenos controlados e variações procedurais discretas.

A prioridade é a **leitura da sala inteira durante o gameplay**, não fazer cada tile parecer interessante isoladamente. Uma textura pode ser bonita em um atlas e ainda produzir uma cena excessivamente carregada quando repetida dezenas de vezes. Por isso, todas as regras deste documento devem ser avaliadas em dois níveis: **textura individual** e **composição global do cômodo**.

---

# Convenções gerais

## Escala visual

Para manter leitura retro e facilitar a geração procedural:

- textura-base recomendada para parede: **64×64 px**;
- variações mais detalhadas ou paredes especiais: **128×128 px**;
- chão: **64×64 px**;
- teto: **64×64 px**;
- elementos como painéis, grades, monitores e placas podem ocupar apenas uma região da textura ou ser gerados como módulos separados;
- detalhes menores que **2 px** devem ser usados com cuidado, pois podem desaparecer ou virar ruído em baixa resolução.

Em uma textura de parede 64×64, é melhor trabalhar com:

- elementos estruturais grandes: 8–32 px;
- juntas: 1–2 px;
- rebites/parafusos: 1–3 px;
- desgaste: manchas de 2–8 px;
- detalhes raros: 8–24 px.

## Camadas recomendadas

Cada textura pode ser pensada como uma composição de camadas:

1. **cor/material base**;
2. **estrutura principal** — placas, blocos, painéis ou concreto;
3. **juntas e bordas**;
4. **sombreamento estrutural**;
5. **fixadores** — rebites, parafusos, grampos;
6. **microvariação do material**;
7. **sujeira e desgaste**;
8. **detalhes específicos do ambiente**;
9. **detalhes raros**.

A textura nunca deve depender de todas essas camadas ao mesmo tempo. O importante é que exista uma base visual reconhecível mesmo quando os detalhes extras não forem gerados.

## Regra para variação de cor

Evitar escolher cores completamente aleatórias. Para cada material, usar uma cor-base e variar a luminosidade em torno dela.

Exemplo:

- metal cinza base: `#55585A`;
- variação clara: aproximadamente +5% a +12% de luminosidade;
- variação escura: aproximadamente -5% a -15%;
- sujeira: normalmente -15% a -35%;
- desgaste metálico exposto: +10% a +25%.

A matiz deve variar pouco. A identidade do material deve vir principalmente de luminosidade, estrutura e desgaste.

---

# 1. Corredor Industrial

O corredor deve ser um dos ambientes visualmente mais neutros do jogo. Ele existe principalmente para conectar salas, portanto precisa ser reconhecível, mas não competir visualmente com ambientes importantes.

Paleta sugerida:

- metal médio: `#55585A`;
- metal escuro: `#343638`;
- sombra profunda: `#202224`;
- metal gasto: `#707477`;
- amarelo de segurança: `#D49A00`;
- amarelo desgastado: `#A97900`;
- preto da sinalização: `#171819`.

## Parede

### Estrutura principal

A parede é construída com **grandes placas metálicas retangulares**, visualmente pesadas, fixadas sobre uma estrutura industrial.

Em uma textura 64×64:

- usar preferencialmente **2 a 4 placas visíveis**;
- placas podem ter aproximadamente 28–32 px de largura por 28–32 px de altura;
- juntas entre placas: 1–2 px;
- borda interna escura de 1 px;
- ocasional linha clara de 1 px em um dos lados para sugerir relevo;
- evitar mosaicos muito pequenos.

As placas não devem ser perfeitamente uniformes. Cada placa pode receber uma diferença de luminosidade de aproximadamente ±5%.

### Relevo

Cada junção pode usar três níveis simples:

1. linha escura;
2. linha média;
3. pequena borda clara em um dos lados.

Isso cria a sensação de placa encaixada sem exigir normal map.

### Parafusos e rebites

Os fixadores devem ficar próximos aos cantos das placas.

Em 64×64:

- tamanho: 1–2 px;
- distância da borda: 2–4 px;
- aproximadamente 2 a 8 fixadores por textura;
- não colocar fixadores em todas as interseções obrigatoriamente.

Variação visual:

- 70%: fixador escuro simples;
- 20%: fixador mais claro/gasto;
- 10%: ausência ou marca vazia.

### Rodapé de segurança

A faixa amarela/preta ocupa **aproximadamente 5% da altura da parede**.

Em uma parede representada por 64 px de altura:

- altura visual aproximada: **3 a 4 px**.

Caso a engine use paredes visualmente muito altas, a proporção deve continuar sendo aproximadamente 5%, em vez de fixar a faixa a uma quantidade absoluta de pixels.

A faixa deve ficar **exclusivamente na base da parede**.

Padrão recomendado:

- listras diagonais de 45°;
- largura de cada segmento amarelo/preto: aproximadamente 4–8 px;
- contraste forte;
- borda superior escura de 1 px separando a sinalização da placa metálica.

A faixa pode receber desgaste, mas nunca desaparecer completamente em um corredor normal.

**Regra rígida de composição:** se o corredor já usa esse rodapé, a mesma parede não deve receber outras faixas amarelo/preto contínuas no meio, no topo, entre painéis ou contornando módulos comuns. O amarelo/preto é sinalização funcional de alto contraste, não moldura decorativa.

### Desgaste

O corredor é bastante utilizado, então o desgaste deve ser perceptível, porém moderado.

Possíveis marcas:

- riscos finos verticais ou diagonais;
- pequenos pontos de metal exposto;
- sujeira acumulada junto às juntas;
- escurecimento próximo ao rodapé;
- pequenos respingos escuros.

Distribuição sugerida por tile:

- 60%: desgaste leve;
- 25%: praticamente limpo;
- 12%: desgaste médio;
- 3%: desgaste forte/localizado.

### Elementos ocasionais

Não fazem parte da textura-base de todas as paredes.

Podem aparecer proceduralmente:

- painel técnico fechado;
- grade de ventilação;
- pequena caixa elétrica;
- placa numérica;
- conduíte vertical;
- conduíte horizontal próximo ao teto;
- pequeno sinal de advertência.

Probabilidade inicial sugerida: **5% a 10% dos segmentos de parede**, dependendo do tipo de elemento e sempre respeitando o **orçamento visual global do cômodo** definido nas regras compartilhadas. Essa porcentagem não autoriza sorteios independentes ilimitados em cada tile.

### O que evitar

- ferrugem pesada em todos os corredores;
- painéis eletrônicos em excesso;
- faixa amarela/preta muito alta;
- detalhes pequenos cobrindo toda a superfície;
- sujeira completamente aleatória sem relação com juntas e bordas.

## Chão

### Material

Metal industrial escuro, com acabamento fosco e antiderrapante.

Paleta:

- base: `#353739`;
- placa clara: `#414447`;
- placa escura: `#292B2D`;
- juntas: `#181A1B`;
- desgaste: `#55595C`.

### Estrutura

Usar placas quadradas ou retangulares grandes.

Em 64×64:

- grade 2×2 ou 2×4;
- placas de 16–32 px;
- juntas de 1–2 px;
- borda escura nas extremidades.

Cada placa pode variar levemente de tom.

### Superfície antiderrapante

Usar detalhes discretos:

- pequenos sulcos paralelos;
- pontos rebaixados;
- linhas curtas;
- padrão de chapa apenas em alguns tiles.

O padrão não deve ser muito contrastado.

### Desgaste direcional

Como o corredor possui fluxo constante, o centro do caminho pode ser ligeiramente mais claro ou polido.

Sugestão:

- bordas: mais escuras e sujas;
- região central: 3% a 8% mais clara;
- juntas: sujeira mais concentrada.

### Detalhes raros

- pequena grade de drenagem;
- tampa de manutenção;
- placa substituída;
- mancha de óleo pequena;
- marca de arrasto.

Probabilidade sugerida: 2% a 8%.

## Teto

### Material

Metal escuro semelhante ao chão, mas ligeiramente mais uniforme.

Paleta:

- base: `#2B2D2F`;
- painel: `#35383A`;
- junta: `#181A1B`;
- iluminação: branco levemente quente `#E2E0D6`.

### Estrutura

Painéis retangulares alinhados com o corredor.

Em 64×64:

- 2–4 grandes placas;
- juntas finas;
- pouco desgaste.

### Iluminação

Luminárias retangulares devem formar uma sequência visual ao longo do corredor.

Exemplo de módulo:

- moldura escura de 1–2 px;
- área luminosa central clara;
- pequeno halo pintado de 1–2 px opcional;
- luminária ocupando aproximadamente 15% a 30% da largura do tile quando presente.

A iluminação deve ser regular. Não colocar uma luminária diferente em cada tile.

### Elementos secundários

- grade de ventilação;
- placa de manutenção;
- conduíte;
- pequeno tubo;
- cabo preso por suportes.

A densidade deve ser baixa.

---

# 2. Sala de Segurança

A sala de segurança deve parecer um ambiente de **controle, observação e operação**, mais organizado e tecnológico que o corredor.

Paleta sugerida:

- metal: `#3A3D40`;
- painel: `#25282B`;
- moldura: `#555B60`;
- tela apagada: `#101719`;
- verde de status: `#3B9B47`;
- âmbar: `#C98A19`;
- vermelho: `#A93232`;
- azul discreto: `#3B6577`.

## Parede

### Estrutura-base

A base continua sendo metálica, mas usa painéis menores e mais organizados.

Em 64×64:

- 2–6 módulos retangulares;
- alinhamento mais regular que no corredor;
- molduras de 1–2 px;
- pouco desgaste.

A parede deve ter grandes zonas neutras para evitar excesso de informação.

### Painéis eletrônicos

Um painel pode conter:

- moldura metálica;
- tela apagada;
- 2–6 pequenos indicadores;
- botão ou chave;
- pequena etiqueta;
- grade de ventilação inferior.

Monitores devem aparecer **desligados ou com conteúdo abstrato simples**, evitando imagens complexas que exigiriam geração adicional.

### Telas

Formato sugerido:

- retângulo 8×6 px até 20×14 px em textura 64×64;
- moldura de 1–2 px;
- vidro quase preto;
- brilho muito discreto na borda superior.

Estados possíveis:

- 55% desligado;
- 25% verde/azul muito escuro;
- 15% interface abstrata;
- 5% aviso âmbar/vermelho.

### Luzes de status

Tamanho: 1–2 px.

Nunca espalhar aleatoriamente pela parede. Devem estar agrupadas dentro de painéis.

### Cabos

A maioria deve estar escondida. Cabos aparentes podem surgir apenas:

- saindo da parte inferior de um painel;
- seguindo verticalmente uma junta;
- entrando em um conduíte.

### Desgaste

Baixo.

- poeira em cantos;
- pequenas marcas próximas aos painéis;
- bordas levemente gastas;
- quase nenhuma ferrugem.

## Chão

### Material

Metal modular ou piso técnico elevado.

Paleta:

- base: `#3D4144`;
- placa: `#484C4F`;
- junta: `#222527`;
- desgaste: `#5A5E61`.

### Estrutura

Grade 4×4 ou 2×2 de placas regulares.

O aspecto deve lembrar módulos removíveis de piso técnico.

Detalhes:

- pequenas travas nos cantos;
- juntas limpas;
- poucas manchas;
- possíveis placas de acesso.

### Marcas de uso

Próximo a consoles e cadeiras:

- arranhões semicirculares;
- riscos curtos;
- pequenas áreas polidas.

Esses detalhes devem preferencialmente ser associados ao prefab da sala, não sorteados em qualquer posição.

## Teto

### Estrutura

Teto modular organizado, com painéis de tamanho regular.

Cores:

- cinza médio: `#55595B`;
- junta: `#343638`;
- moldura: `#686C6F`.

### Iluminação

Mais clara que o corredor.

As luminárias podem ocupar 20% a 35% do tile e usar luz branca neutra.

Distribuição sugerida no ambiente:

- linhas regulares;
- espaçamento constante;
- evitar zonas completamente escuras.

### Equipamentos

Podem existir:

- câmera de segurança;
- detector/sensor;
- grade de ventilação;
- pequeno alto-falante;
- painel de manutenção.

Esses elementos devem ser raros e posicionados de forma funcional.

---

# 3. Sala de PvP / Combate

O espaço de combate deve comunicar robustez, desgaste e função militar/industrial. A textura não pode prejudicar a leitura dos inimigos, coberturas e portas.

Paleta sugerida:

- verde oliva escuro: `#50583D`;
- verde profundo: `#373E2C`;
- metal: `#4A4B46`;
- sombra: `#262824`;
- metal exposto: `#686A63`;
- sinalização amarela discreta: `#B68113`.

## Parede

### Estrutura

Placas reforçadas maiores e visualmente mais grossas que as do corredor.

Em 64×64:

- 1–4 placas principais;
- bordas de 2–3 px;
- parafusos maiores;
- reforços diagonais ou retangulares ocasionais.

### Reforço estrutural

Alguns tiles podem possuir:

- barra horizontal;
- perfil vertical;
- placa sobreposta;
- canto reforçado.

O relevo deve ser mais pronunciado que no corredor.

### Marcas de impacto

Impactos pequenos podem ser incorporados à textura-base, mas os impactos causados pelo jogador devem idealmente existir como decals separados.

Para variações procedurais estáticas:

- 1–3 pontos de impacto;
- área de 2–5 px;
- centro escuro;
- borda levemente clara;
- probabilidade baixa.

### Desgaste

Mais intenso que em outros ambientes:

- riscos;
- lascas de tinta;
- metal exposto nas bordas;
- sujeira baixa;
- pequenas áreas de oxidação.

Evitar ferrugem laranja saturada em excesso. O objetivo é desgaste, não abandono.

### Sinalização

Pode incluir:

- número de setor;
- seta;
- pequena faixa amarela;
- marca de perigo.

Não usar textos longos dentro da textura.

## Chão

### Material

Pode alternar entre metal grosso e concreto industrial.

Base recomendada:

- concreto/metal escuro `#3D3C36`;
- manchas `#292A27`;
- desgaste `#56564E`.

### Estrutura

Placas grandes ou lajes.

- poucas divisões;
- linhas grossas o suficiente para leitura;
- pequenas irregularidades.

### Desgaste

Maior perto de coberturas e zonas de passagem.

Pode conter:

- riscos de caixas;
- marcas de impacto;
- pequenas rachaduras;
- diferença de tonalidade entre placas substituídas.

### Regra de legibilidade

Nenhum detalhe de piso deve se parecer com item coletável, munição ou inimigo distante. Evitar pequenos pontos muito saturados.

## Teto

### Estrutura

Visual pesado:

- vigas aparentes;
- placas escuras;
- iluminação pontual;
- grades de ventilação.

### Vigas

Podem atravessar a textura de uma borda à outra.

Em 64×64:

- largura aproximada de 4–8 px;
- sombra forte de 1–2 px;
- destaque discreto na borda inferior.

### Iluminação

Mais espaçada que na sala de segurança.

Pode criar áreas de sombra entre módulos, mas o espaço de combate deve continuar jogável.

---

# 4. Refeitório

O refeitório deve ser imediatamente reconhecido como uma área mais limpa, clara e civil da instalação, sem perder a estética industrial.

Paleta sugerida:

- branco envelhecido: `#C7C7C2`;
- cinza claro: `#9D9E9B`;
- cinza médio: `#6C6E6D`;
- vermelho institucional: `#9F2F2D`;
- vermelho gasto: `#77302F`;
- sujeira: `#6E685E`.

## Parede

### Estrutura

Painéis claros, grandes e simples.

Em 64×64:

- 2–4 painéis;
- juntas de 1–2 px;
- superfície mais lisa;
- poucos fixadores aparentes.

### Faixa vermelha

A faixa funciona como código visual da área de alimentação.

Recomendação:

- altura: 8% a 12% da parede;
- posição: aproximadamente 35% a 45% da altura medida a partir do chão;
- contínua entre tiles;
- bordas retas;
- variação de desgaste discreta.

A faixa deve ser muito mais fina que um grande bloco colorido e deve manter alinhamento entre paredes adjacentes.

### Desgaste

Tipo de desgaste diferente das áreas técnicas:

- manchas de mão;
- marcas de móveis;
- pequenos riscos;
- sujeira suave próxima à base;
- descoloração.

Pouca ferrugem.

### Elementos ocasionais

- placa de identificação;
- quadro de avisos;
- painel simples;
- nicho técnico;
- pequena ventilação.

## Chão

### Material

Piso industrial lavável, visualmente mais claro.

Paleta:

- base: `#696C6C`;
- claro: `#787B7B`;
- escuro: `#555858`;
- junta: `#454747`.

### Estrutura

Lajes ou placas grandes.

- 2×2 ou 4×4 por textura;
- juntas finas;
- pouco relevo.

### Marcas

- manchas suaves;
- áreas polidas pelo tráfego;
- riscos próximos às mesas;
- pequenas marcas circulares de pés de móveis.

Evitar sujeira pesada para manter o ambiente claramente distinto do depósito.

## Teto

### Estrutura

Painéis claros e regulares.

- linhas retas;
- módulos repetíveis;
- baixa quantidade de desgaste.

### Iluminação

Luz uniforme.

- luminárias grandes e claras;
- espaçamento regular;
- branco neutro ou levemente quente.

### Detalhes

- ventilação;
- sprinkler;
- detector;
- pequena tampa de manutenção.

---

# 5. Laboratório

O laboratório deve parecer mais limpo, frio e preciso que qualquer outro ambiente. A sensação principal é de controle técnico e pesquisa.

Paleta sugerida:

- branco frio: `#C8CCCB`;
- cinza claro: `#A8ADAE`;
- cinza técnico: `#697174`;
- azul escuro: `#31445E`;
- azul médio: `#46688A`;
- verde de status: `#3F8C51`;
- sombra: `#252A2C`.

## Parede

### Estrutura

Painéis brancos ou cinza-claro com molduras técnicas discretas.

Em 64×64:

- 2–6 módulos;
- alinhamento preciso;
- juntas finas;
- poucos rebites visíveis.

### Superfície

Mais lisa que nos corredores.

Microvariação deve ser pequena:

- ±2% a ±5% de luminosidade;
- praticamente nenhuma ferrugem;
- sujeira limitada a bordas e áreas técnicas.

### Módulos científicos

Podem incluir:

- painel de leitura;
- pequeno visor;
- encaixe de tubo;
- grade de ventilação;
- compartimento fechado;
- unidade numerada;
- conexão de cabo.

### Tubos

Tubos visíveis devem ser organizados e intencionais.

Podem usar:

- metal claro;
- azul;
- verde;
- vidro escuro estilizado.

Evitar redes caóticas de tubulação, que pertencem à área técnica.

### Luzes e indicadores

Poucos, organizados e geralmente verdes ou azuis.

Vermelho deve ser reservado para falha/alerta.

## Chão

### Material

Piso modular técnico em tom frio.

Paleta:

- azul acinzentado base: `#34465D`;
- placa clara: `#40536B`;
- escuro: `#263548`;
- junta: `#1F2832`.

### Estrutura

Placas regulares, perfeitamente alinhadas.

- 2×2 ou 4×4;
- juntas finas;
- pequenas travas nos cantos;
- baixo desgaste.

### Variações

- uma placa ligeiramente diferente;
- tampa técnica;
- grade estreita;
- pequena sinalização de área.

Evitar manchas de óleo.

## Teto

### Estrutura

Teto modular claro, organizado e simétrico.

### Iluminação

Luminárias brancas intensas e regulares.

Sugestão:

- 20% a 40% de tiles de teto com componente luminoso dependendo da escala do mapa;
- restantes: painéis neutros, filtros ou ventilação.

### Filtros e ventilação

Grades claras, preferencialmente quadradas ou retangulares.

Podem ter:

- moldura clara;
- centro cinza médio;
- padrão interno simples de linhas.

---

# 6. Área Técnica / Casa de Máquinas

A área técnica é o ambiente com maior densidade mecânica. Ela deve parecer funcional, apertada e construída em torno de máquinas, tubos e manutenção.

Paleta sugerida:

- metal base: `#3A3B39`;
- metal médio: `#50514E`;
- metal escuro: `#222422`;
- amarelo industrial: `#B57B00`;
- laranja queimado: `#9A5A17`;
- ferrugem: `#77452C`;
- óleo/sujeira: `#1E211E`.

## Parede

### Base

A superfície atrás das máquinas continua sendo feita de placas metálicas simples.

Essa base precisa existir para que nem todo tile fique visualmente carregado.

### Tubulações

Tubos são o principal elemento visual.

Em uma textura 64×64:

- tubo fino: 3–4 px;
- médio: 5–7 px;
- grosso: 8–12 px.

Um tubo deve possuir pelo menos:

1. sombra escura em um lado;
2. cor média no centro;
3. pequena faixa clara no lado oposto.

Isso cria volume usando apenas pixels.

### Orientações

Tubos podem ser:

- verticais;
- horizontais;
- em L;
- paralelos;
- conectados a caixas.

Evitar curvas arbitrárias complexas em toda parede.

### Suportes

A cada trecho grande de tubo, pode existir um suporte:

- cinta metálica;
- parafuso;
- pequena sombra.

### Válvulas

Elementos raros.

Podem ocupar 6–12 px e usar:

- círculo/polígono vermelho escuro;
- amarelo;
- cinza.

Válvula deve estar conectada a um tubo real, nunca flutuando aleatoriamente.

### Caixas técnicas

Retângulos metálicos com:

- moldura;
- tampa;
- dois parafusos;
- pequena alça;
- opcional indicador.

### Sujeira

Mais forte que em outros cômodos.

Deve se concentrar:

- abaixo de conexões;
- perto do chão;
- ao redor de caixas;
- próximo a válvulas.

### Ferrugem

Localizada.

Aparece principalmente:

- juntas;
- parafusos;
- base de tubos;
- bordas danificadas.

Nunca transformar toda a parede em laranja.

## Chão

### Material

Metal muito resistente, podendo alternar entre placa sólida e grade.

Paleta:

- base `#343633`;
- escuro `#20221F`;
- metal gasto `#50534E`;
- óleo `#171A17`.

### Placas

- grandes;
- grossas;
- parafusos visíveis;
- juntas escuras.

### Grades

Quando usadas:

- moldura de 2–3 px;
- barras internas de 1–2 px;
- fundo quase preto.

Grades não devem aparecer em todos os tiles.

### Manchas de óleo

Formato irregular, porém controlado:

- 4–16 px de extensão;
- borda suave/pixelada;
- diferença de luminosidade, não preto absoluto;
- normalmente próximas a máquinas.

## Teto

### Estrutura

Teto mais complexo do jogo.

Pode reunir:

- grandes tubos;
- dutos;
- vigas;
- conduítes;
- cabos;
- luminárias protegidas.

### Regra de densidade

Não usar todos os elementos em cada tile.

Sugestão de categorias:

- 35%: painel metálico simples;
- 25%: 1–2 tubos;
- 15%: duto;
- 10%: conduíte/cabo;
- 10%: luminária;
- 5%: composição especial.

Os percentuais são ponto de partida, não obrigação rígida.

### Dutos

Retangulares e maiores que tubos.

- 10–24 px de largura;
- bordas marcadas;
- sombra forte;
- pequenos segmentos de conexão.

---

# 7. Depósito / Armazém

O depósito deve parecer mais antigo, seco, empoeirado e menos mantido que o restante da instalação. A maior parte da complexidade visual virá de caixas, estantes e objetos do prefab, portanto as texturas devem ser relativamente simples.

Paleta sugerida:

- metal amarronzado: `#5C5244`;
- metal escuro: `#393633`;
- ferrugem discreta: `#72482F`;
- poeira: `#71695C`;
- concreto: `#555552`;
- sombra: `#292A28`.

## Parede

### Estrutura

Placas metálicas antigas ou concreto industrial revestido.

Se metal:

- painéis grandes;
- juntas mais irregulares;
- placas substituídas;
- fixadores envelhecidos.

### Variação de placas

Uma placa pode ser 5% a 15% mais clara ou escura que a vizinha, simulando substituição em manutenção.

### Ferrugem

Leve e localizada.

Padrões úteis:

- linha fina descendo de um parafuso;
- mancha pequena em canto;
- borda oxidada;
- ponto irregular próximo ao chão.

### Marcas de caixas

Próximo à parte inferior da parede podem aparecer:

- riscos horizontais;
- área ligeiramente mais limpa onde uma caixa esteve encostada;
- manchas quadradas suaves;
- pequenas batidas.

### Identificação

Elementos raros:

- número de setor;
- etiqueta;
- faixa curta;
- seta simples.

## Chão

### Material

Concreto industrial é a opção principal.

Paleta:

- base `#555552`;
- claro `#656560`;
- escuro `#40413E`;
- sujeira `#363632`.

### Textura do concreto

Não usar ruído puro em cada pixel.

Em vez disso:

- 3–8 manchas grandes e suaves;
- pequenas diferenças de tom;
- 0–3 agregados/pontos discretos;
- algumas linhas de desgaste.

### Rachaduras

Raras.

- largura: 1 px;
- comprimento: 6–20 px;
- poucas bifurcações;
- baixo contraste.

Rachaduras demais fazem o chão parecer destruído em vez de apenas antigo.

### Marcas de arrasto

Linhas paralelas ou curvas suaves, mais escuras ou mais claras que o piso.

Devem aparecer preferencialmente próximo a caixas e estantes.

## Teto

### Material

Metal escuro simples.

### Estrutura

- painéis grandes;
- algumas vigas;
- poucas tubulações;
- iluminação espaçada.

### Iluminação

Mais fraca que nos demais ambientes.

Luminárias podem ter:

- moldura escura;
- luz branca amarelada;
- área luminosa menor;
- algumas unidades apagadas apenas como variação visual.

Não escurecer tanto a ponto de prejudicar navegação.

### Poeira

Pode haver leve escurecimento próximo:

- às vigas;
- às juntas;
- à ventilação.

---


# 8. Hub Central

O Hub é um espaço de **circulação, orientação e distribuição**. Ele deve funcionar como um ponto de referência visual entre setores e, por isso, precisa ser um dos ambientes mais fáceis de ler à distância.

A identidade do Hub deve vir principalmente de:

- escala arquitetônica;
- caminhos claramente organizados;
- portas e acessos destacados;
- sinalização de setor;
- iluminação direcional;
- pequenas referências cromáticas.

A identidade **não** deve vir de preencher todas as superfícies com painéis, círculos, luzes, faixas ou detalhes técnicos.

Paleta sugerida:

- metal médio: `#464B4D`;
- metal escuro: `#2C3032`;
- sombra profunda: `#171B1D`;
- metal gasto: `#5A6062`;
- ciano de navegação: `#2B8994`;
- ciano escuro: `#1E5860`;
- amarelo de segurança: `#C38B00`;
- preto da sinalização: `#151718`.

O ciano é o principal acento de orientação do Hub. O amarelo/preto deve permanecer reservado a risco físico real.

## Parede

### Estrutura-base

Usar painéis metálicos grandes, simples e visualmente calmos.

Em uma textura 64×64:

- preferir **1 a 3 divisões principais**;
- juntas de 1–2 px;
- moldura estrutural de 1–3 px quando necessária;
- poucas bordas de alto contraste;
- nenhuma obrigação de colocar um elemento no centro de cada painel.

A maior parte da parede deve continuar reconhecível mesmo se todos os detalhes opcionais forem removidos.

### Zonas neutras

O Hub deve possuir grandes áreas de silêncio visual.

Em uma parede ampla vista durante o gameplay:

- aproximadamente 65% a 80% da área deve ser formada por base neutra;
- 15% a 25% pode ser estrutura arquitetônica;
- 5% a 10% pode conter informação funcional;
- no máximo 2% a 5% deve conter destaque de alto contraste.

Uma zona neutra pode possuir:

- material base;
- junta;
- sombra estrutural;
- pequena variação de luminosidade;
- desgaste muito discreto.

Ela não deve possuir simultaneamente:

- monitor;
- símbolo;
- luz;
- faixa;
- tubo;
- círculo decorativo;
- grade chamativa;
- placa;
- cor saturada.

### Colunas e molduras

Colunas podem separar grandes trechos da parede e ajudar a estruturar a sala.

Devem ser:

- mais escuras ou ligeiramente mais claras que os painéis;
- largas o suficiente para serem percebidas como arquitetura;
- simples;
- repetidas em ritmo controlado.

Evitar transformar cada coluna em um painel técnico completo.

### Sinalização de navegação

Pode indicar:

- setor;
- direção;
- número de acesso;
- porta;
- elevador;
- conexão entre áreas.

Preferir ciano, branco frio ou cinza claro.

A sinalização deve aparecer próxima do elemento que ela explica. Não espalhar símbolos abstratos pela parede apenas para preencher espaço.

### Painéis técnicos

São elementos secundários.

Quando presentes:

- usar moldura simples;
- no máximo 1 tela ou área funcional principal;
- poucos indicadores;
- contraste moderado;
- evitar repetir o mesmo desenho em módulos adjacentes.

Painéis técnicos não devem ocupar toda a parede do Hub.

### Motivos circulares

Elementos circulares podem existir como:

- ventilação;
- sensor;
- encaixe mecânico;
- indicador de porta;
- componente específico de equipamento.

Não devem ser usados como preenchimento genérico de todos os painéis.

Regras:

- no máximo um motivo circular claramente reconhecível em uma pequena região de parede;
- evitar motivos iguais em tiles adjacentes;
- aplicar raio de exclusão de pelo menos 1–2 módulos para motivos de alto contraste;
- grandes paredes devem conter trechos extensos sem nenhum círculo.

### Amarelo/preto

Uso permitido:

- borda de elevador ou plataforma;
- desnível;
- máquina perigosa;
- porta industrial específica;
- região de risco físico;
- limite de circulação que precise ser percebido rapidamente.

Uso proibido:

- contornar todos os painéis;
- separar todas as fileiras de placas;
- criar faixas em múltiplas alturas da mesma parede;
- decorar o teto;
- repetir a mesma sinalização no chão e na parede sem motivo funcional;
- preencher regiões vazias.

Em uma parede do Hub, deve existir preferencialmente **zero ou uma zona dominante** de amarelo/preto.

### Desgaste

Baixo a moderado.

Pode aparecer como:

- riscos discretos;
- bordas levemente polidas;
- pequenas marcas próximas a portas;
- sujeira leve junto ao chão;
- diferenças sutis entre placas substituídas.

Evitar:

- ferrugem pesada;
- manchas saturadas;
- grandes áreas destruídas;
- desgaste uniforme em toda a parede.

## Chão

### Material

Metal modular escuro, resistente e relativamente simples.

Paleta:

- base: `#323638`;
- placa clara: `#3D4244`;
- placa escura: `#292D2F`;
- junta: `#181B1C`;
- linha de navegação: `#276E76`.

### Estrutura

Usar placas grandes.

Em 64×64:

- grade 2×2 ou 2×4;
- juntas finas;
- variação de luminosidade pequena;
- poucos fixadores;
- evitar padrões antiderrapantes de alto contraste em toda a superfície.

### Navegação

Linhas discretas podem conduzir o jogador até acessos importantes.

Devem:

- usar ciano escuro ou tom dessaturado;
- ter largura pequena;
- formar caminhos contínuos;
- não aparecer em todas as placas;
- não competir com itens coletáveis ou inimigos.

### Sinalização de risco no chão

Amarelo/preto somente em:

- bordas de plataforma;
- desníveis;
- partes móveis;
- elevadores;
- áreas de máquina.

Não usar como moldura de todas as placas do piso.

### Desgaste

Mais visível nas rotas de circulação.

- centro de caminhos ligeiramente polido;
- pequenas marcas próximas a portas;
- sujeira nas juntas;
- raras marcas de manutenção.

## Teto

### Estrutura

O teto deve reforçar a orientação da sala sem competir com o chão e as paredes.

Usar:

- grandes painéis escuros;
- vigas principais;
- luminárias lineares;
- poucas grades ou conduítes.

Evitar:

- mosaicos muito pequenos;
- faixas amarelo/preto decorativas;
- painéis técnicos em todos os módulos;
- alternância rápida de cores.

### Iluminação

A iluminação deve ajudar a indicar:

- eixo principal de circulação;
- portas;
- plataformas;
- centro da sala.

Preferir sequências regulares e previsíveis.

O teto pode ser visualmente mais simples que as paredes.

## Regra de composição do Hub

Antes de gerar detalhes individuais:

1. escolher os principais eixos de circulação;
2. marcar portas, elevadores e acessos;
3. reservar grandes superfícies neutras;
4. escolher poucas zonas focais;
5. distribuir detalhes somente depois;
6. validar se os destaques não formam padrões repetitivos.

Se a sala inteira parecer um mosaico de pequenos módulos igualmente importantes, a geração falhou.

---

# Regras Procedurais Compartilhadas

Estas regras têm prioridade sobre probabilidades locais descritas em cada cômodo. O objetivo é impedir que texturas individualmente válidas produzam uma sala visualmente poluída quando repetidas em grande quantidade.

## 1. Composição antes do detalhe

A geração deve acontecer em duas escalas diferentes:

1. **composição do cômodo**;
2. **acabamento dos tiles**.

Nunca começar sorteando detalhes independentemente em cada tile.

Primeiro o cômodo deve definir:

- grandes superfícies;
- zonas neutras;
- estruturas principais;
- pontos focais;
- acessos;
- sinalização;
- regiões de risco;
- quantidade máxima de detalhes.

Somente depois os tiles recebem variações locais.

A pergunta principal não é:

> “Este tile pode receber um detalhe?”

A pergunta correta é:

> “A sala ainda precisa de outro detalhe nesta região?”

## 2. Separar estrutura de decoração

A geração de cada textura deve ser dividida em pelo menos dois níveis.

### Estrutura obrigatória

Define o material e garante que a textura seja reconhecida mesmo sem detalhes extras.

Exemplo para corredor:

- fundo metálico;
- divisão em placas;
- juntas;
- rodapé de segurança.

### Decoração probabilística

Adiciona variedade:

- riscos;
- sujeira;
- parafusos ausentes;
- painel;
- ventilação;
- conduíte.

Assim, um seed diferente muda os detalhes sem transformar o corredor em outro tipo de ambiente.

A decoração probabilística deve poder ser completamente desativada sem destruir a identidade visual básica da superfície.

## 3. Orçamento visual global

Cada cômodo deve possuir um **orçamento visual**.

Esse orçamento controla a quantidade de informação visível na sala inteira e não apenas em um tile.

Classificar elementos em quatro classes.

### Classe 0 — Base neutra

Elementos:

- placa simples;
- concreto;
- metal liso;
- junta discreta;
- sombra estrutural;
- pequena variação de luminosidade.

Cobertura recomendada:

- ambientes comuns: **60% a 80%**;
- áreas técnicas deliberadamente densas: **45% a 65%**.

A base neutra é necessária para que inimigos, itens, portas e elementos especiais tenham contraste suficiente.

### Classe 1 — Estrutura

Elementos:

- vigas;
- molduras;
- colunas;
- reforços;
- divisões;
- grandes tubos funcionais;
- estruturas de porta.

Cobertura recomendada:

- aproximadamente **15% a 30%** da área visual.

Esses elementos podem ser frequentes, mas devem usar contraste baixo ou médio.

### Classe 2 — Detalhe

Elementos:

- painel técnico;
- monitor;
- ventilação;
- caixa elétrica;
- tubulação secundária;
- placa;
- módulo científico;
- componente mecânico.

Cobertura recomendada:

- aproximadamente **5% a 15%**.

Nem toda superfície precisa possuir um detalhe.

### Classe 3 — Destaque

Elementos:

- amarelo/preto;
- vermelho de alerta;
- tela brilhante;
- luz saturada;
- símbolo grande;
- placa de advertência;
- indicador muito contrastado;
- elemento decorativo de forma extremamente reconhecível.

Cobertura recomendada:

- normalmente **1% a 5%**;
- preferir o limite inferior em corredores, Hub e ambientes de combate;
- ultrapassar 5% somente quando o próprio gameplay exigir sinalização intensa.

Elementos Classe 3 são recursos escassos.

## 4. Tiles neutros são obrigatórios

Uma sala interessante precisa conter tiles visualmente desinteressantes.

Distribuição inicial recomendada para grupos de 10 segmentos de parede:

- 6 a 8: neutros;
- 1 a 2: estrutura secundária;
- 0 a 1: detalhe técnico;
- 0 a 1: composição especial.

Isso é uma orientação de densidade, não uma sequência fixa.

Um tile neutro pode ter juntas e pequenas variações de material, mas não deve receber automaticamente um objeto central.

Evitar a lógica:

```text
cada painel = alguma coisa no centro
```

Preferir:

```text
muitos painéis = apenas material e estrutura
alguns painéis = informação funcional
pouquíssimos painéis = destaque
```

## 5. Silêncio visual

Toda parede grande deve possuir regiões cuja função é descansar a visão.

Uma zona de silêncio visual não possui:

- monitor;
- texto;
- símbolo;
- luz forte;
- faixa de segurança;
- tubo chamativo;
- círculo decorativo;
- grade de alto contraste;
- cor saturada.

Pode possuir:

- material base;
- juntas;
- pequenas sombras;
- desgaste discreto;
- variação sutil de luminosidade.

Regra prática:

- em ambientes neutros, pelo menos 50% de uma grande parede vista à distância deve continuar sendo silêncio visual;
- no Hub, corredor e arena, preferir 60% ou mais;
- na Área Técnica, pode cair para aproximadamente 40% a 50%, mas nunca chegar a zero.

## 6. Probabilidade local não substitui limite global

Probabilidades por tile são apenas uma etapa secundária.

Exemplo:

```text
50 segmentos × 15% de chance = 7 ou 8 detalhes esperados
```

Isso pode ser aceitável isoladamente, mas torna-se excessivo quando várias paredes, chão e teto fazem o mesmo sorteio.

Portanto, usar primeiro um limite por cômodo.

Exemplo conceitual:

```text
orcamento_detalhes_parede = 6
orcamento_destaques_parede = 2

para cada zona:
    selecionar candidatos

embaralhar candidatos

aplicar somente até atingir o orçamento
```

O sorteio decide **onde**, não **quantos sem limite**.

## 7. Exclusão entre detalhes vizinhos

Detalhes visuais não devem formar aglomerados acidentais.

Quando um segmento recebe um elemento Classe 2:

- reduzir fortemente a chance dos segmentos imediatamente vizinhos receberem outro elemento Classe 2;
- permitir exceção apenas quando os elementos formarem uma composição funcional única.

Quando um segmento recebe um elemento Classe 3:

- aplicar raio de exclusão de aproximadamente 2 a 4 módulos para outros destaques equivalentes;
- não permitir dois sinais de alto contraste lado a lado apenas por sorteio.

Exemplo ruim:

```text
[PAINEL][PAINEL][PAINEL]
[PAINEL][PAINEL][PAINEL]
```

Exemplo melhor:

```text
[      ][PAINEL][      ]
[      ][      ][      ]
[PAINEL][      ][      ]
```

## 8. Evitar repetição de motivos muito reconhecíveis

Formas como:

- círculos;
- alvos;
- ventiladores;
- telas;
- grandes parafusos;
- sinais triangulares;
- luzes vermelhas;
- símbolos;
- grades muito contrastadas;

devem possuir frequência menor que formas estruturais simples.

Um motivo reconhecível repetido em todos os painéis deixa de ser detalhe e vira ruído.

Regras sugeridas:

- não repetir o mesmo motivo especial em tiles adjacentes;
- evitar o mesmo motivo especial mais de duas vezes dentro de uma região 3×3;
- variar posição, escala e função quando a repetição for necessária;
- permitir grandes regiões sem nenhum motivo especial.

## 9. Uso de amarelo/preto

A sinalização amarelo/preto é **informação funcional de alto contraste**, não textura decorativa geral.

Uso apropriado:

- borda perigosa;
- desnível;
- elevador;
- máquina;
- porta industrial específica;
- região de risco;
- limite operacional.

Não usar:

- em todas as bordas de painéis;
- entre todas as placas;
- em múltiplas alturas da mesma parede;
- como moldura do teto;
- em toda viga;
- em toda porta;
- simultaneamente em parede, piso e teto sem justificativa;
- apenas para preencher espaço vazio.

Regra de prioridade:

1. se o ambiente possui um rodapé de segurança contínuo, ele já é o principal elemento amarelo/preto;
2. outras faixas contínuas na mesma parede devem ser proibidas;
3. sinalização adicional só pode aparecer em um risco local real;
4. um elemento adicional deve ser curto e localizado;
5. não criar várias linhas horizontais amarelo/preto empilhadas.

Em uma única vista da câmera deve existir preferencialmente apenas uma zona dominante desse padrão.

## 10. Saturação e contraste também possuem orçamento

Não basta controlar quantidade de objetos. Também é necessário controlar contraste.

Cores saturadas devem indicar função.

Exemplos:

- ciano: navegação, tecnologia ou interface;
- verde: estado normal;
- âmbar: atenção;
- vermelho: alerta/falha;
- amarelo/preto: risco físico.

Evitar distribuir essas cores uniformemente por toda a sala.

Regra prática:

- a maior parte da superfície deve permanecer em tons próximos da paleta-base;
- elementos saturados devem ocupar pequenas áreas;
- duas cores de destaque diferentes não devem competir em todos os planos da cena ao mesmo tempo.

## 11. Leitura à distância

Toda superfície deve ser testada em três distâncias.

### Longe

Ao olhar uma parede do outro lado da sala, devem ser percebidos primeiro:

1. volume do cômodo;
2. portas e caminhos;
3. grandes divisões estruturais;
4. poucos pontos focais.

Microdetalhes não devem dominar.

### Média distância

Devem começar a aparecer:

- painéis;
- molduras;
- reforços;
- tubos;
- sinalizações secundárias.

### Perto

Somente próximo da superfície devem ficar evidentes:

- parafusos;
- riscos;
- ferrugem;
- pequenas etiquetas;
- microvariação.

Se dezenas de detalhes individuais forem claramente reconhecíveis do outro lado de uma sala grande, o contraste ou a densidade está alto demais.

## 12. Três escalas de detalhe

Cada superfície deve possuir três escalas.

### Macro

Forma principal.

Exemplos:

- placas;
- faixa principal;
- grande tubo;
- painel;
- coluna;
- porta.

### Médio

Estrutura secundária.

Exemplos:

- juntas;
- molduras;
- reforços;
- manchas;
- dutos menores.

### Micro

Pequenos detalhes.

Exemplos:

- parafusos;
- riscos;
- pontos de ferrugem;
- etiquetas pequenas.

A escala macro deve dominar a leitura.

Se as três escalas tiverem contraste semelhante, a textura ficará ruidosa.

## 13. Evitar ruído por pixel

Não gerar cada pixel independentemente.

Isso tende a produzir textura semelhante a televisão fora do ar.

Preferir elementos com escala:

- manchas;
- linhas;
- placas;
- riscos;
- bordas;
- regiões de luminosidade.

O material deve ser reconhecido por formas, não apenas por ruído.

Microvariação deve possuir baixo contraste.

## 14. Variações devem respeitar contexto

Não sortear qualquer detalhe em qualquer lugar.

Exemplos:

- ferrugem aparece mais em juntas e parafusos;
- sujeira aparece mais perto do chão;
- manchas de óleo ficam próximas a máquinas;
- marcas de caixa aparecem no depósito;
- luzes de status ficam dentro de painéis;
- válvulas ficam ligadas a tubos;
- desgaste de circulação aparece no centro dos corredores;
- sinalização aparece próxima daquilo que ela explica.

A posição deve comunicar causa.

## 15. Frequência sugerida de detalhes

Estas porcentagens são candidaturas locais e continuam subordinadas ao orçamento global.

Como ponto inicial:

- microvariação de cor: 80–100%, com contraste muito baixo;
- juntas estruturais: 100%;
- fixadores: 30–70%, conforme material;
- desgaste leve: 30–60%;
- desgaste médio: 5–20%;
- mancha grande: 2–8%;
- detalhe técnico: 3–10%;
- detalhe muito chamativo: 1–3%.

Em ambientes deliberadamente densos, como Área Técnica, alguns limites podem ser elevados, desde que a área neutra mínima continue preservada.

## 16. Continuidade entre tiles

Alguns elementos não podem ser gerados isoladamente por tile.

Precisam considerar vizinhos:

- faixa horizontal;
- tubulação;
- conduíte;
- viga;
- faixa de sinalização;
- grandes manchas;
- rachaduras longas;
- linhas de navegação.

Uma solução é gerar primeiro uma **máscara lógica da superfície**, definindo por onde esses elementos passam, e só depois rasterizar os pixels.

A continuidade não significa repetição infinita. Uma faixa ou tubo também deve possuir início e fim coerentes.

## 17. Bordas da parede

Em paredes altas, reservar zonas lógicas pode ajudar:

- base: 0–10%;
- corpo: 10–85%;
- topo: 85–100%.

Isso não significa que todas as paredes terão três faixas visíveis. Serve para orientar a distribuição de detalhes.

Exemplo corredor:

- base 0–5%: rodapé amarelo/preto;
- corpo 5–90%: placas majoritariamente neutras;
- topo 90–100%: placas + conduítes ocasionais.

O corpo da parede não deve ser dividido automaticamente em várias faixas horizontais contrastadas.

## 18. Escolher pontos focais antes de decorar

Cada sala deve possuir poucos pontos de maior interesse visual.

Exemplos:

- porta principal;
- terminal;
- máquina;
- elevador;
- objetivo;
- janela;
- elemento arquitetônico;
- centro da arena.

Procedimento:

1. selecionar de 1 a 3 pontos focais principais, conforme tamanho da sala;
2. reservar contraste para esses pontos;
3. reduzir detalhes nas áreas ao redor;
4. impedir que paredes secundárias possuam a mesma intensidade visual.

Uma sala em que todas as paredes são pontos focais não possui ponto focal.

## 19. Relação com gameplay

A hierarquia de contraste deve priorizar:

1. inimigos;
2. projéteis e ameaças;
3. itens importantes;
4. portas, objetivos e elementos interativos;
5. arquitetura;
6. decoração.

Nenhum detalhe do piso deve se parecer com:

- munição;
- pickup;
- inimigo distante;
- mira;
- projétil;
- indicador de dano.

Nenhum detalhe de parede deve competir diretamente com silhuetas de inimigos.

## 20. Teste de silhueta da sala

Durante validação visual, observar uma captura reduzida ou desfocada.

Mesmo sem perceber detalhes pequenos, ainda deve ser possível identificar:

- chão;
- paredes;
- teto;
- portas;
- caminhos;
- ponto focal.

Se a imagem reduzida se transformar em uma massa uniforme de padrões pequenos e contrastados, a densidade deve ser reduzida.

## 21. Teste em escala real de gameplay

Não validar texturas apenas pelo atlas.

Para cada ambiente:

1. gerar uma sala pequena;
2. gerar uma sala média;
3. gerar uma sala grande;
4. observar de uma extremidade à outra;
5. testar com arma, inimigo e pickup presentes;
6. verificar se os elementos de gameplay continuam sendo os objetos mais fáceis de localizar.

Uma textura aprovada isoladamente pode ser reprovada no contexto da sala.

## 22. Densidade por tipo de cômodo

Valores de referência para a composição global:

| Cômodo | Base neutra | Estrutura | Detalhes | Destaques |
|---|---:|---:|---:|---:|
| Corredor Industrial | 65–80% | 15–25% | 4–8% | 1–3% |
| Sala de Segurança | 55–70% | 15–25% | 8–15% | 2–4% |
| Sala de PvP / Combate | 65–80% | 15–25% | 3–8% | 1–3% |
| Refeitório | 65–80% | 10–20% | 5–10% | 2–4% |
| Laboratório | 60–75% | 10–20% | 8–12% | 2–4% |
| Área Técnica | 45–60% | 20–30% | 10–20% | 2–5% |
| Depósito | 65–80% | 10–20% | 5–10% | 1–3% |
| Hub Central | 65–80% | 15–25% | 4–10% | 1–3% |

As faixas não precisam somar exatamente 100% porque alguns elementos podem se sobrepor visualmente. Elas servem como limite perceptivo.

## 23. Seed hierárquico

Para manter consistência, é recomendável derivar seeds separados.

Exemplo conceitual:

```text
seed_mapa
 └── seed_comodo
      ├── seed_composicao
      │    ├── seed_zonas_neutras
      │    ├── seed_pontos_focais
      │    └── seed_orcamento_visual
      ├── seed_paredes
      │    ├── seed_estrutura
      │    ├── seed_desgaste
      │    └── seed_detalhes
      ├── seed_chao
      └── seed_teto
```

Isso permite modificar regras de acabamento sem alterar completamente a composição global da sala.

## 24. Biblioteca de módulos

Uma abordagem compatível com o estilo do GDP é criar pequenas primitivas procedurais reutilizáveis:

```text
painel_retangular()
junta_horizontal()
junta_vertical()
rebite()
parafuso()
risco()
mancha()
ferrugem()
grade()
tubo_vertical()
tubo_horizontal()
curva_tubo_90()
valvula()
luminaria()
monitor()
caixa_eletrica()
placa_identificacao()
faixa_seguranca()
linha_navegacao()
coluna_estrutural()
```

Cada cômodo escolhe quais primitivas pode utilizar e com quais probabilidades, limites e raios de exclusão.

## 25. Metadados recomendados para módulos

Cada módulo procedural pode possuir metadados de composição.

Exemplo conceitual:

```c
typedef enum {
    VIS_BASE,
    VIS_ESTRUTURA,
    VIS_DETALHE,
    VIS_DESTAQUE
} VisualClass;

typedef struct {
    VisualClass classe;
    int raio_exclusao;
    int repeticao_max_3x3;
    bool alto_contraste;
    bool exige_contexto;
} VisualRule;
```

Isso permite controlar a densidade sem espalhar exceções por todo o gerador.

## 26. Identidade visual por cômodo

| Cômodo | Elemento que deve identificá-lo imediatamente |
|---|---|
| Corredor Industrial | placas cinza grandes + único rodapé amarelo/preto de aproximadamente 5% |
| Sala de Segurança | módulos eletrônicos organizados cercados por grandes zonas neutras |
| Sala de PvP | placas verde-oliva reforçadas, poucos destaques e desgaste de combate |
| Refeitório | painéis claros + faixa vermelha contínua e controlada |
| Laboratório | painéis claros frios + detalhes azuis/verdes organizados |
| Área Técnica | tubos, dutos e caixas mecânicas sobre base ainda visível |
| Depósito | metal/concreto envelhecido e poeira/desgaste de armazenamento |
| Hub Central | arquitetura ampla + orientação em ciano + grandes superfícies neutras |

A identidade de um cômodo deve ser percebida pela composição geral. Ela não precisa ser impressa em cada tile.

## 27. Checklist anti-poluição visual

Antes de aprovar uma composição, verificar:

- há uma grande quantidade de superfície neutra?
- o olho encontra portas e caminhos antes de detalhes decorativos?
- existem várias faixas amarelo/preto empilhadas?
- o mesmo motivo especial aparece repetidamente?
- cada painel parece obrigado a conter alguma coisa?
- parede, chão e teto estão competindo ao mesmo tempo?
- há mais de um ponto focal principal na mesma pequena região?
- inimigos ainda se destacam contra a parede?
- pickups ainda se destacam contra o chão?
- detalhes micro só aparecem de perto?
- o teto pode ser simplificado?
- alguma decoração pode ser removida sem perder identidade?

Se a resposta às perguntas de poluição for “sim”, reduzir detalhes antes de adicionar novos elementos.

## 28. Falhas visuais explicitamente proibidas

Evitar os seguintes resultados:

### Parede em camadas de sinalização

```text
AMARELO/PRETO
painéis
AMARELO/PRETO
painéis
AMARELO/PRETO
painéis
AMARELO/PRETO
```

Esse padrão produz linhas horizontais de alto contraste repetidas e fragmenta a leitura da arquitetura.

### Todo painel com símbolo

```text
[ O ][ O ][ O ][ O ]
[ O ][ O ][ O ][ O ]
[ O ][ O ][ O ][ O ]
```

Motivos especiais devem aparecer como exceção.

### Todos os planos com padrão forte

Evitar simultaneamente:

- parede muito detalhada;
- chão muito detalhado;
- teto muito detalhado.

Se um plano for visualmente complexo, os outros dois devem ser simplificados.

### Contraste sem função

Não usar ciano, verde, vermelho ou amarelo apenas para aumentar variedade.

Toda cor saturada deve possuir papel visual claro.

---

# Ordem sugerida para implementar

Para testar o sistema procedural sem criar complexidade demais de uma vez:

1. **Corredor Industrial** — melhor ambiente para validar placas, juntas, rodapé e desgaste;
2. **Depósito** — adiciona concreto, poeira e envelhecimento;
3. **Sala de PvP** — adiciona reforços e desgaste pesado;
4. **Refeitório** — testa continuidade de faixa horizontal e materiais claros;
5. **Sala de Segurança** — adiciona módulos eletrônicos;
6. **Laboratório** — refina geração limpa e organizada;
7. **Área Técnica** — por último entre os ambientes mecânicos, pois exige tubos, conexões, válvulas e continuidade entre tiles;
8. **Hub Central** — reutiliza placas, colunas, sinalização e iluminação dos ambientes anteriores, mas exige a validação mais rígida de densidade global e silêncio visual.

Essa ordem foi usada como referência para a reconstrução consolidada. Os oito
ambientes agora compartilham a mesma biblioteca de primitivas, camadas e
orçamentos, mantendo identidades e seeds independentes.

## Status de implementação

- [x] **1. Corredor Industrial** — `WALL_CORRIDOR`, `FLOOR_CORRIDOR` e
  `CEILING_CORRIDOR` implementados proceduralmente em 128×128, mantendo as
  proporções descritas para 64×64. Possui quatro composições por plano, detalhes
  técnicos subordinados ao orçamento global, rodapé de 4,7%, piso modular e teto
  com luminárias espaçadas. O atlas de inspeção é `test_corridor_guide.ppm`.
- [x] **2. Depósito** — concreto/metal envelhecido, poeira, placas
  substituídas, marcas de caixas, rachaduras raras e teto de iluminação quente.
- [x] **3. Sala de PvP / Combate** — blindagem oliva reforçada, impactos de
  baixo contraste, lajes pesadas e vigas; integrada às salas `ARENA`.
- [x] **4. Refeitório** — superfícies claras laváveis, faixa vermelha contínua,
  marcas de mobiliário e iluminação uniforme; integrada ao bloco civil
  `ROOM_CAT_OFFICE` enquanto não existe prefab exclusivo.
- [x] **5. Sala de Segurança** — módulos eletrônicos organizados, telas,
  indicadores agrupados, piso elevado e teto claro regular.
- [x] **6. Laboratório** — painéis claros frios, módulos azuis/verdes, tubos
  organizados, piso técnico e filtros de teto.
- [x] **7. Área Técnica / Casa de Máquinas** — tubos, dutos, suportes, válvula
  conectada, caixa técnica, grade localizada, óleo e luminária protegida.
- [x] **8. Hub Central** — `WALL_HUB`, `FLOOR_HUB` e `CEILING_HUB` usam
  superfícies neutras, arquitetura ampla, orientação ciano, poucos pontos
  focais e nenhum amarelo/preto decorativo. `ROOM_CAT_START` e `ROOM_CAT_HUB`
  são associados explicitamente ao conjunto.

As **24 superfícies implementadas** estão ligadas ao jogo. Cada parede
contextual ocupa o pé-direito inteiro, sem repetir rodapés, faixas, monitores ou
linhas de navegação em várias alturas. Piso e teto usam quatro composições por
ambiente, escolhidas por máscaras espaciais determinísticas; luzes, grades,
manchas e tampas deixam de aparecer em todos os tiles. No Hub, a máscara do
piso forma poucas rotas ciano contínuas. As camadas usam subseeds separados para
estrutura, detalhe, desgaste e variação; o acabamento não injeta ferrugem
aleatória em Corredor, Refeitório, Segurança, Laboratório ou Hub. Elementos
funcionais permanecem associados aos módulos corretos, e o conduíte técnico e
a faixa vermelha atravessam bordas alinhadas.

Validação registrada após a reconstrução: **12.720 combinações determinísticas
em 20 seeds**, onze atlas, oito salas completas renderizadas e contratos de
identidade/orçamento. O gate de 1.000 andares cobriu 13.659 setores de Corredor,
1.075 de Depósito/beco, 1.842 de Combate/Boss, 426 Refeitórios/blocos civis,
1.151 de Segurança/servidor, 1.500 de Laboratório/secreta, 430 Áreas Técnicas e
2.827 setores de Hub/START.

`test_material_visual` valida determinismo, contratos, paletas, continuidade e
as quatro composições de cada plano. `test_prop_visual` gera uma sala completa
para cada ambiente e também verifica inimigos, props, drops, degraus, portas e
fim de corredor no contexto real da renderização. A regressão de geometria
também garante uma tampa neutra no corredor e impede que um teto distante mais
alto reabra pixels já ocultos pelo teto próximo. Vistas de pitch extremo e de
uma porta WARNING a curta distância limitam o rodapé a oito pixels, rejeitam
lajes quase pretas e usam uma cor-sentinela opcional para detectar qualquer
pixel realmente sem cobertura. A suíte, os builds SDL/headless
e os testes focados com ASan/UBSan foram aprovados sem novos avisos ou erros.

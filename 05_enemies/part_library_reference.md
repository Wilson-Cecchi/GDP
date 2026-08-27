# GDP — Biblioteca Técnica Detalhada de Peças para Inimigos Procedurais

> Documento de referência visual e procedural. O objetivo é permitir que cada componente seja desenhado ou gerado por código sem depender de uma interpretação vaga do nome da peça.

> **Escopo estrutural:** 146 peças principais de montagem, 18 perfis corporais e catálogos de acessórios, detalhes, materiais e estados.

## Estado de integração no jogo anterior

> **Nota histórica:** esta seção registra a implementação C/SDL2 do GDP anterior.
> Ela comprova decisões já exercitadas, mas não descreve o estado atual da nova
> engine C++/SDL3/OpenGL. Para contratos atuais, consultar `architecture.md` e os
> demais documentos de `05_enemies/`.

- Os 18 perfis e os 146 IDs estruturais possuem enumeração estável em `enemy_parts.h`.
- `EnemyBlueprint` escolhe perfil, peças, materiais, conservação e acessórios com sub-seed própria.
- Operário, Guarda, Oficial, Engenheiro e Robô Industrial usam subconjuntos compatíveis deste catálogo; IDs fora desses kits permanecem reservados para expansão, sem entrar aleatoriamente em famílias erradas.
- O Robô Industrial combina dois módulos traseiros; os demais papéis usam no máximo um.
- `enemy_blueprint_validate()` rejeita montagens fora do kit do papel.
- `test-enemy-visual` valida 320 blueprints, variação real de perfil/torso/cabeça/braço/equipamento, determinismo, máscaras alfa e cinco poses; também gera `test_enemy_roles.ppm` e `test_enemy_variants.ppm`.
- Desgaste é agrupado em regiões de 3×3 e concentrado em pés, mãos e extremidades, evitando ruído aleatório de um pixel.
- O bake mistura cápsulas para anatomia, juntas e cabos com caixas arredondadas para placas, tecido estruturado, viseiras, armas e módulos. Uma ordem única de camadas impede que detalhes menores desapareçam dentro do volume principal.
- O sombreado é calculado por peça e por material: tecido recebe trama/costura, metal recebe chanfro/rebite/oxidação localizada, policarbonato recebe reflexo diagonal, borracha conserva baixo brilho e componentes orgânicos recebem variação agrupada.
- O acabamento profissional adiciona joelhos e cotovelos articulados, punhos, cintos, bolsos e elementos próprios: suspensório do Operário, carregadores do Guarda, patente/rádio do Oficial, diagnóstico/cabos do Engenheiro e grelha/reator/pistões do Robô Industrial.
- `test-enemy-visual` também exige placas rígidas válidas, ordem determinística e contraste interno suficiente para que a riqueza estrutural não regrida para uma silhueta chapada.

Essa integração implementa primeiro as peças efetivamente utilizadas pelo elenco atual. O catálogo completo permanece como contrato técnico para ampliar inimigos sem renumerar IDs nem quebrar determinismo.

---

## 0. Convenções de construção

### Unidade de medida

Todas as dimensões são normalizadas pela altura final do inimigo (`H = 1.0`). Isso evita amarrar o design a 64×64, 128×128 ou outra resolução. A rasterização final pode converter essas proporções para pixels.

### Ordem de desenho de uma peça

1. máscara externa / silhueta;
2. volumes principais;
3. placas, tecido ou massas orgânicas;
4. articulações e conectores;
5. elementos funcionais (pistões, cabos, lentes, armas);
6. sombra estrutural;
7. material;
8. desgaste semanticamente localizado;
9. marcas, números e acessórios;
10. emissivos e efeitos animados, quando existirem.

### Regra de variação

A variação procedural deve modificar **subcomponentes**, e não apenas cor. Uma peça só continua pertencendo ao mesmo ID se preservar as “Características obrigatórias” descritas em sua ficha. Quando uma alteração remove essas características, o resultado deve ser outro ID ou uma peça especial.

### Regra de encaixe

Os conectores devem possuir posição, direção e largura normalizadas. Uma peça não pode “flutuar” sobre outra. Quando dois componentes possuem larguras muito diferentes, o quadril, ombro ou adaptador deve justificar visualmente a transição.

### Regra de pixel art / retro FPS

Os detalhes menores devem ser agrupados em formas legíveis. Em resoluções baixas, um conjunto de parafusos ou cabos pode ser simplificado, mas a silhueta, o mecanismo principal e as zonas de material devem permanecer reconhecíveis.


## 1. Perfis corporais — regras para montar as peças

### `BODY_01` — Humano padrão

Altura 100%; ombros 24–30% da altura; pernas 43–47%; torso 30–34%; cabeça 12–14%. Centro de massa próximo à pelve. Serve como referência neutra para peças orgânicas e uniformes.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_02` — Alto e fino

Altura 110–125%; torso 75–88% da largura padrão; braços 110–130%; pernas 108–125%. Cabeça pode ficar em 85–100%. Deve formar grande espaço negativo entre braços e tronco.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_03` — Baixo e largo

Altura 85–95%; torso 120–140% da largura padrão; pernas 85–95%; cabeça 90–110%. Centro de massa baixo e pés mais afastados.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_04` — Torso pesado

Torso 125–155% da largura/profundidade padrão; pernas 90–105%; cabeça 75–95%. Ombros e quadril precisam justificar a massa superior.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_05` — Pernas longas

Pernas 112–130%; torso 90–100%; braços 95–115%. Quadril elevado e passada visualmente longa.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_06` — Corcunda

Torso inclinado 8–20° para frente; BACK/TOR traseiro sobe 15–35%; cabeça avança 5–12% da altura do torso. Joelhos podem ficar ligeiramente flexionados.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_07` — Ombros largos

Largura total dos ombros 125–155%; torso 110–130%; quadril normal. Ombreiras ou sockets precisam construir uma linha superior forte.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_08` — Cabeça afundada

Cabeça 70–90% da escala; gola/ombros sobem até 25–50% da altura da cabeça. O topo do torso deve cercar o sensor sem ocultar a direção visual.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_09` — Assimétrico leve

Um lado recebe 5–15% de diferença de largura/comprimento. No máximo duas regiões principais alteradas para não parecer defeito de alinhamento.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_10` — Assimétrico extremo

Diferença lateral de 20–60% em braço, ombro, perna ou módulo traseiro. O gerador deve compensar visualmente o peso com postura, quadril ou perna de apoio.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_11` — Arqueado

Eixo torso-quadril forma curva ou inclinação de 10–25°; cabeça corrige parcialmente a direção para manter olhar legível. Pode ser orgânico ou mecânico.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_12` — Mecânico compacto

Altura 85–100%; torso curto/largo; cabeça pequena; membros próximos ao núcleo. Espaços entre peças reduzidos, mas articulações ainda precisam ser visíveis.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_13` — Pernas invertidas

LEG_07 ou arquitetura equivalente obrigatória; quadril elevado; joelho traseiro visível em silhueta. Torso pode ser humanoide ou mecânico.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_14` — Braços longos

Braços 120–150%; mãos alcançam abaixo do joelho. Torso não deve crescer junto, para preservar a leitura desproporcional.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_15` — Braço dominante

Um braço possui 130–180% da massa do outro; ombro correspondente +10–35%. Torso pode inclinar 2–8° para compensar.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_16` — Sem pescoço

HEAD conecta diretamente à gola/placa superior; pescoço visual = 0–2% da altura. Adequado para capacetes pesados e sensores mecânicos.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_17` — Torso suspenso

Quadril e torso conectados por haste/junta estreita visível; espaço negativo entre massa torácica e base. BACK e braços não podem ocultar totalmente essa suspensão.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---

### `BODY_18` — Corpo remendado

Estrutura humanoide inicial, porém 2–4 regiões usam peças de origens diferentes. Pelo menos uma transição deve mostrar solda, cinta, prótese ou adaptador.

**Regra:** o perfil corporal altera distribuições de escala e postura; ele não substitui os IDs das peças. As peças continuam escolhidas individualmente e precisam respeitar o envelope criado por esse perfil.

---


## 2. Pernas e bases locomotoras

### `LEG_01` — Calça industrial + bota

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Perna humanoide reta; coxa ligeiramente mais larga que a panturrilha; bota ocupa 22–28% da altura da peça e projeta a biqueira 8–12% para frente.

**Construção exata.** Tecido dividido por costura lateral; joelho sem placa rígida; bota com cano alto, sola grossa em duas camadas e biqueira reforçada.

**Materiais e leitura de superfície.** Tecido industrial fosco + borracha/couro escuro; costuras 1–2 tons mais escuras.

**Parâmetros procedurais permitidos.** comprimento ±10%; número de bolsos 0–2; joelheira têxtil opcional; sola lisa ou segmentada; faixa refletiva opcional abaixo do joelho.

**Características obrigatórias.** Silhueta humana, bota pesada e ausência de mecanismo exposto.

**Desgaste, sujeira e dano.** Poeira no terço inferior, vincos escurecidos atrás do joelho, desgaste na biqueira e borda da sola.

**Compatibilidade recomendada.** `worker, technical, salvaged; compatível com quase todos os quadris humanos.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_02` — Calça tática

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Perna humanoide mais seca; joelho marcado por volume angular; bota 18–24% da peça.

**Construção exata.** Calça com painéis reforçados na coxa, joelheira rígida central e caneleira têxtil; bota com cadarço simplificado e sola dentada.

**Materiais e leitura de superfície.** Tecido tático escuro + polímero nas joelheiras.

**Parâmetros procedurais permitidos.** joelheira redonda/quadrada; 1–3 bolsos; tira de fixação; pequenas placas laterais; variação de azul-petróleo, grafite ou preto.

**Características obrigatórias.** Joelheira funcional e leitura de uniforme de segurança/militar.

**Desgaste, sujeira e dano.** Desgaste em joelho, canela e biqueira; pouca ferrugem.

**Compatibilidade recomendada.** `security, military, ranged, elite.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_03` — Caneleira metálica

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Base humanoide com placa frontal cobrindo 45–70% da canela.

**Construção exata.** Perna de tecido ou estrutura simples por baixo; caneleira trapezoidal com 4–8 rebites, borda dobrada e duas tiras posteriores.

**Materiais e leitura de superfície.** Aço pintado ou exposto + tecido/borracha.

**Parâmetros procedurais permitidos.** placa lisa/segmentada; 4/6/8 rebites; borda chanfrada; símbolo ou número; espessura ±20%.

**Características obrigatórias.** Placa frontal claramente separada do suporte inferior.

**Desgaste, sujeira e dano.** Arranhões verticais, tinta perdida nas bordas e ferrugem em rebites.

**Compatibilidade recomendada.** `worker, security, salvaged, military.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_04` — Prótese simples

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Estrutura mecânica fina com haste principal; largura 55–75% de uma perna humana; pé mecânico 18–25% da altura.

**Construção exata.** Soquete superior acolchoado, haste tubular central, junta de joelho circular, pequena biela posterior e pé de duas placas.

**Materiais e leitura de superfície.** Aço escuro/alumínio + borracha no soquete.

**Parâmetros procedurais permitidos.** 1 ou 2 hastes; joelho circular/quadrado; pé humanoide/industrial; cabo externo 0–1; placa protetora pequena opcional.

**Características obrigatórias.** Deve parecer prótese leve: estrutura aberta e pouca blindagem.

**Desgaste, sujeira e dano.** Graxa na junta, desgaste na borda do pé, oxidação leve nos parafusos.

**Compatibilidade recomendada.** `worker, technical, salvaged, biofusion.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_05` — Prótese hidráulica

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Perna mecânica grossa; coxa e canela formadas por dois conjuntos; pé largo 120–150% da largura de uma bota.

**Construção exata.** Dois pistões principais paralelos, joelho encapsulado por discos laterais, mangueiras externas e pé com amortecimento.

**Materiais e leitura de superfície.** Aço reforçado, tubos escuros, detalhes amarelo-industrial.

**Parâmetros procedurais permitidos.** pistões 2–4; placas protetoras 0–3; pé quadrado ou bifurcado; mangueiras 1–3; cilindro auxiliar.

**Características obrigatórias.** Massa alta, pistões visíveis e base de apoio ampla.

**Desgaste, sujeira e dano.** Óleo nas hastes, riscos nos cilindros, ferrugem em cantos e sujeira no pé.

**Compatibilidade recomendada.** `heavy, industrial_machine, salvaged; exige HIP_03/04/10 ou equivalente reforçado.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_06` — Perna mecânica fina

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Silhueta esquelética; duas hastes estreitas formam coxa e canela; articulações maiores que as hastes.

**Construção exata.** Haste dupla ou treliça, joelho com rolamento circular, cabos acompanhando a parte posterior e pé pequeno articulado.

**Materiais e leitura de superfície.** Aço exposto/alumínio + cabos emborrachados.

**Parâmetros procedurais permitidos.** haste simples/dupla; 1–3 cabos; pé plano/garra; anéis nas juntas; pequenas placas de identificação.

**Características obrigatórias.** Estrutura aberta, pouco volume e articulações expostas.

**Desgaste, sujeira e dano.** Desgaste concentrado nos pivôs; graxa; arranhões lineares.

**Compatibilidade recomendada.** `industrial_machine, fast, experimental.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_07` — Perna invertida

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Joelho recuado; segmento inferior inclina para frente até o pé, criando silhueta digitígrada.

**Construção exata.** Articulação superior compacta, “joelho” traseiro grande, amortecedor diagonal e pé alongado.

**Materiais e leitura de superfície.** Metal escuro, aço pintado ou material experimental.

**Parâmetros procedurais permitidos.** ângulo do joelho 25–45°; pé em garra/almofada; pistão 1–2; cobertura parcial opcional.

**Características obrigatórias.** Geometria invertida deve permanecer imediatamente visível.

**Desgaste, sujeira e dano.** Riscos no amortecedor, desgaste na articulação traseira, sujeira na ponta do pé.

**Compatibilidade recomendada.** `machine, fast, experimental, biofusion.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_08` — Bota blindada

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Perna humana com bota muito volumosa; biqueira e cano recebem chapas externas.

**Construção exata.** Bota base, placa frontal de canela, biqueira de metal, protetor lateral do tornozelo e sola profunda.

**Materiais e leitura de superfície.** Couro/borracha + aço pintado/blindagem composta.

**Parâmetros procedurais permitidos.** 2–5 placas; rebites; faixas de segurança; joelheira opcional; sola 2–4 segmentos.

**Características obrigatórias.** Pé grande e proteção rígida contínua do tornozelo à canela inferior.

**Desgaste, sujeira e dano.** Lascas na biqueira, bordas polidas por impacto, barro/poeira na sola.

**Compatibilidade recomendada.** `security, military, heavy.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_09` — Perna orgânica deformada

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Contorno irregular; coxa assimétrica, joelho deslocado 5–12% para um lado e panturrilha com volumes não humanos.

**Construção exata.** Massa orgânica em 3 volumes; articulação parcialmente aparente; tendões/cabos orgânicos opcionais; pé com dedos grossos ou fundidos.

**Materiais e leitura de superfície.** Carne pálida, tecido orgânico mutado, áreas escurecidas.

**Parâmetros procedurais permitidos.** tumores 0–3; joelho lateralizado; dedos 2–5; placas implantadas opcionais; veias/tubos.

**Características obrigatórias.** Assimetria orgânica e articulação anormal.

**Desgaste, sujeira e dano.** Manchas, cicatrizes, áreas secas/necrosadas; evitar ruído uniforme.

**Compatibilidade recomendada.** `biofusion, experimental, corrupted.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_10` — Perna híbrida

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Forma humanoide quebrada por uma armação externa que acompanha coxa e canela.

**Construção exata.** Tecido/carne interna, trilho metálico lateral, joelho reforçado por anel e 1–2 pistões pequenos.

**Materiais e leitura de superfície.** Tecido/carne + aço + cabos.

**Parâmetros procedurais permitidos.** estrutura no lado interno/externo; 1–3 grampos; prótese parcial; placas 0–2; mangueira opcional.

**Características obrigatórias.** Mistura explícita de parte orgânica/humana e suporte mecânico.

**Desgaste, sujeira e dano.** Ferrugem onde metal toca tecido, manchas no soquete, óleo nas juntas.

**Compatibilidade recomendada.** `biofusion, salvaged, worker, experimental.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_11` — Perna com mangueira

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Perna base humanoide ou mecânica média com uma linha hidráulica externa dominante.

**Construção exata.** Canal de fixação na coxa, mangueira flexível contornando o joelho, grampos e conector próximo ao tornozelo.

**Materiais e leitura de superfície.** Base variável + borracha técnica e terminais metálicos.

**Parâmetros procedurais permitidos.** mangueira 1–2; rota interna/externa; cor preta/amarela/vermelha; conector reto/cotovelo.

**Características obrigatórias.** Mangueira deve cruzar ao menos dois segmentos e ser visualmente rastreável.

**Desgaste, sujeira e dano.** Óleo perto dos conectores; abrasão em grampos; poeira inferior.

**Compatibilidade recomendada.** `technical, industrial_machine, experimental.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_12` — Perna com amortecedor

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Silhueta atlética/mecânica; amortecedor externo ocupa 35–60% da canela.

**Construção exata.** Mola helicoidal ou cilindro telescópico, suporte superior, articulação de tornozelo reforçada e pé com retorno elástico.

**Materiais e leitura de superfície.** Aço/alumínio + mola escura; detalhes coloridos opcionais.

**Parâmetros procedurais permitidos.** mola/cilindro; 1 ou 2 amortecedores; inclinação 5–20°; proteção parcial.

**Características obrigatórias.** Elemento elástico deve ser a característica visual dominante.

**Desgaste, sujeira e dano.** Graxa, marcas de compressão e riscos longitudinais.

**Compatibilidade recomendada.** `fast, industrial_machine, experimental.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_13` — Pé de garra

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Perna pode ser fina ou média; a identidade está no pé com 2–4 dedos metálicos.

**Construção exata.** Tornozelo central, dedos articulados em segmentos e pequenas pontas de contato.

**Materiais e leitura de superfície.** Aço escuro, ferro, material experimental.

**Parâmetros procedurais permitidos.** 2–4 garras; comprimentos diferentes; unha/ponta substituível; micro-pistões opcionais.

**Características obrigatórias.** Pelo menos duas garras separadas e espaço negativo entre elas.

**Desgaste, sujeira e dano.** Pontas polidas, riscos no lado inferior e sujeira nas articulações.

**Compatibilidade recomendada.** `machine, fast, ambush, experimental.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_14` — Pé industrial quadrado

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Base de apoio retangular maior que a canela; baixa altura e alto volume.

**Construção exata.** Placa superior, bloco amortecedor central, quatro cantos reforçados e sola segmentada.

**Materiais e leitura de superfície.** Aço pintado, borracha industrial.

**Parâmetros procedurais permitidos.** largura 120–180% da canela; 2–4 blocos de sola; cantos chanfrados; faixa de perigo opcional.

**Características obrigatórias.** Base claramente quadrada/retangular e pesada.

**Desgaste, sujeira e dano.** Desgaste nos quatro cantos, óleo sob o amortecedor, sujeira na sola.

**Compatibilidade recomendada.** `heavy, industrial_machine.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_15` — Blindagem assimétrica

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Perna base com proteção concentrada em um dos lados, alterando a silhueta lateral.

**Construção exata.** Placa grande lateral, 1–2 placas menores frontais e tiras/grampos de fixação.

**Materiais e leitura de superfície.** Aço pintado/blindagem composta sobre base de tecido ou máquina.

**Parâmetros procedurais permitidos.** lado protegido; tamanho 30–70% da perna; placas 1–3; rebites; símbolo.

**Características obrigatórias.** Um lado deve parecer claramente mais pesado que o outro.

**Desgaste, sujeira e dano.** Desgaste forte na placa externa, borda descascada, impactos.

**Compatibilidade recomendada.** `security, military, salvaged, asymmetric.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_16` — Perna queimada

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Arquitetura-base ainda reconhecível, mas com material retraído e superfícies carbonizadas.

**Construção exata.** Usa uma perna humana/híbrida como núcleo; tecido queimado revela proteção, pele ou metal em áreas locais.

**Materiais e leitura de superfície.** Tecido carbonizado, metal escurecido, carne queimada quando aplicável.

**Parâmetros procedurais permitidos.** percentual queimado 25–80%; rasgos; metal exposto; cinzas; faixa de cor marrom/preto.

**Características obrigatórias.** O dano térmico deve dominar sem destruir o perfil estrutural.

**Desgaste, sujeira e dano.** Bordas quebradiças, fuligem ascendente, brilho metálico em áreas onde pintura foi perdida.

**Compatibilidade recomendada.** `worker, security, biofusion, salvaged; estado especial sobre bases compatíveis.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_17` — Perna oxidada

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Perna mecânica média/pesada com superfícies corroídas e juntas visualmente rígidas.

**Construção exata.** Placas aparafusadas, eixo de joelho, suporte de tornozelo e cavidades onde ferrugem se acumula.

**Materiais e leitura de superfície.** Ferro/aço oxidado.

**Parâmetros procedurais permitidos.** nível de ferrugem 35–85%; furos de corrosão 0–3; placa substituída opcional.

**Características obrigatórias.** Oxidação estrutural forte; não parecer apenas “metal marrom”.

**Desgaste, sujeira e dano.** Ferrugem escorrida sob parafusos, bordas esfareladas e articulações escuras.

**Compatibilidade recomendada.** `machine, salvaged, abandoned.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_18` — Perna remendada

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Forma humanoide irregular, com materiais diferentes formando a mesma perna.

**Construção exata.** Tecido original, faixa de pano, grampos metálicos, pequena placa e costuras/rebites improvisados.

**Materiais e leitura de superfície.** Tecido industrial + sucata + fita/grampos.

**Parâmetros procedurais permitidos.** 2–5 remendos; cores incompatíveis; placa 0–2; fita 0–3 voltas; costura grossa.

**Características obrigatórias.** Deve parecer reparada em campo, não projetada assim de fábrica.

**Desgaste, sujeira e dano.** Sujeira nas sobreposições, fios soltos, bordas de tecido desfiadas.

**Compatibilidade recomendada.** `worker, salvaged, technical.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_19` — Perna com fluido

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Perna média com pequeno reservatório visível ligado por tubo à articulação.

**Construção exata.** Reservatório cilíndrico/retangular, visor de nível, mangueira e conector no joelho ou panturrilha.

**Materiais e leitura de superfície.** Metal + vidro/policarbonato + fluido colorido.

**Parâmetros procedurais permitidos.** reservatório 1–2; posição lateral/frontal; fluido verde/azul/âmbar; tubo 1–2.

**Características obrigatórias.** Reservatório e rota do fluido precisam ser legíveis.

**Desgaste, sujeira e dano.** Condensação, manchas junto à tampa, vazamento opcional, riscos no visor.

**Compatibilidade recomendada.** `laboratory, experimental, technical, biofusion.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---

### `LEG_20` — Perna de manutenção

**Slot estrutural:** quadril → solo  
**Envelope de dimensão:** 34–46% da altura total do inimigo  
**Ponto(s) de encaixe:** conector superior central no fêmur/quadril; contato inferior no pé

**Silhueta e proporção.** Mecânica robusta, porém menos blindada que LEG_05; joelho largo e pistão lateral único dominante.

**Construção exata.** Braço estrutural principal, joelho retangular, pistão lateral, cabo curto e pé industrial médio.

**Materiais e leitura de superfície.** Aço pintado, amarelo industrial, borracha.

**Parâmetros procedurais permitidos.** pistão esquerdo/direito; placa de serviço; número; 1–2 cabos; pé quadrado/garra curta.

**Características obrigatórias.** Aparência de maquinário utilitário, não militar.

**Desgaste, sujeira e dano.** Óleo, tinta raspada em pontos de manutenção e graxa nas juntas.

**Compatibilidade recomendada.** `industrial_machine, technical, worker_machine.`

**Regra de implementação.** A peça deve preservar uma linha de apoio clara. A variação de comprimento pode mudar a postura, mas nunca deve deslocar o pé para fora do plano de contato com o chão.

---


## 3. Quadris e conectores inferiores

### `HIP_01` — Cinto industrial

**Slot estrutural:** pernas → torso  
**Envelope de dimensão:** 6–12% da altura total  
**Ponto(s) de encaixe:** dois conectores inferiores para pernas e um conector superior para o torso

**Silhueta e proporção.** Faixa horizontal envolvendo 70–95% da cintura; volume baixo.

**Construção exata.** Cinto largo, fivela simples, 1–4 presilhas, bolsos pequenos assimétricos.

**Materiais e leitura de superfície.** Couro/tecido reforçado + metal na fivela.

**Parâmetros procedurais permitidos.** bolsos 0–4; chave pendurada; cor preta/marrom/cinza; faixa refletiva.

**Características obrigatórias.** Leitura de uniforme de trabalho.

**Desgaste, sujeira e dano.** Desgaste na fivela, bordas dobradas, sujeira inferior.

**Compatibilidade recomendada.** `worker, technical.`

**Regra de implementação.** É a peça de transição estrutural. Deve esconder ou justificar diferenças de largura entre pernas e torso e fornecer espaço visual para cabos, cintos ou juntas.

---

### `HIP_02` — Cinto tático

**Slot estrutural:** pernas → torso  
**Envelope de dimensão:** 6–12% da altura total  
**Ponto(s) de encaixe:** dois conectores inferiores para pernas e um conector superior para o torso

**Silhueta e proporção.** Faixa rígida e modular; bolsos mais quadrados e simétricos.

**Construção exata.** Fivela central, módulos laterais, coldre opcional e suporte de rádio.

**Materiais e leitura de superfície.** Nylon tático + polímero + metal.

**Parâmetros procedurais permitidos.** 2–6 módulos; coldre esquerdo/direito; rádio; granulação baixa.

**Características obrigatórias.** Organização militar e módulos retangulares.

**Desgaste, sujeira e dano.** Atrito nas quinas e poeira nos bolsos.

**Compatibilidade recomendada.** `security, military, ranged.`

**Regra de implementação.** É a peça de transição estrutural. Deve esconder ou justificar diferenças de largura entre pernas e torso e fornecer espaço visual para cabos, cintos ou juntas.

---

### `HIP_03` — Estrutura metálica

**Slot estrutural:** pernas → torso  
**Envelope de dimensão:** 6–12% da altura total  
**Ponto(s) de encaixe:** dois conectores inferiores para pernas e um conector superior para o torso

**Silhueta e proporção.** Ponte rígida entre pernas e torso; largura acompanha a base do torso.

**Construção exata.** Barra transversal, dois encaixes circulares de quadril e bloco central aparafusado.

**Materiais e leitura de superfície.** Aço exposto/pintado.

**Parâmetros procedurais permitidos.** barra reta/angulada; placas laterais; 4–10 parafusos; reforço inferior.

**Características obrigatórias.** Dois pontos de perna mecânicos claramente separados.

**Desgaste, sujeira e dano.** Graxa nas juntas e riscos na barra.

**Compatibilidade recomendada.** `machine, heavy, salvaged.`

**Regra de implementação.** É a peça de transição estrutural. Deve esconder ou justificar diferenças de largura entre pernas e torso e fornecer espaço visual para cabos, cintos ou juntas.

---

### `HIP_04` — Junta hidráulica

**Slot estrutural:** pernas → torso  
**Envelope de dimensão:** 6–12% da altura total  
**Ponto(s) de encaixe:** dois conectores inferiores para pernas e um conector superior para o torso

**Silhueta e proporção.** Bloco central cilíndrico/hexagonal com mangueiras e atuadores.

**Construção exata.** Cilindro central, dois pistões laterais, coletores de fluido e anéis de vedação.

**Materiais e leitura de superfície.** Aço + borracha + cobre.

**Parâmetros procedurais permitidos.** pistões 2–4; mangueiras 1–4; reservatório pequeno opcional.

**Características obrigatórias.** Sistema hidráulico visível e volumoso.

**Desgaste, sujeira e dano.** Óleo na base, brilho nas hastes e sujeira nos selos.

**Compatibilidade recomendada.** `heavy, industrial_machine, technical.`

**Regra de implementação.** É a peça de transição estrutural. Deve esconder ou justificar diferenças de largura entre pernas e torso e fornecer espaço visual para cabos, cintos ou juntas.

---

### `HIP_05` — Saia de placas

**Slot estrutural:** pernas → torso  
**Envelope de dimensão:** 6–12% da altura total  
**Ponto(s) de encaixe:** dois conectores inferiores para pernas e um conector superior para o torso

**Silhueta e proporção.** Camada segmentada cobrindo a transição do torso para as pernas.

**Construção exata.** 4–8 placas sobrepostas, presas a aro/cinto; frente e laterais mais longas.

**Materiais e leitura de superfície.** Aço/blindagem composta.

**Parâmetros procedurais permitidos.** placas 4–10; formato retangular/trapezoidal; assimetria leve; símbolos.

**Características obrigatórias.** Segmentação sobreposta deve permanecer.

**Desgaste, sujeira e dano.** Bordas gastas, impactos e ferrugem entre placas.

**Compatibilidade recomendada.** `military, heavy, salvaged.`

**Regra de implementação.** É a peça de transição estrutural. Deve esconder ou justificar diferenças de largura entre pernas e torso e fornecer espaço visual para cabos, cintos ou juntas.

---

### `HIP_06` — Junta exposta

**Slot estrutural:** pernas → torso  
**Envelope de dimensão:** 6–12% da altura total  
**Ponto(s) de encaixe:** dois conectores inferiores para pernas e um conector superior para o torso

**Silhueta e proporção.** Pouca carcaça; cabos, eixo e rolamentos formam a cintura.

**Construção exata.** Eixo central, dois rolamentos laterais, 2–6 cabos e pequenos suportes.

**Materiais e leitura de superfície.** Aço, cobre, cabos emborrachados.

**Parâmetros procedurais permitidos.** quantidade e rota de cabos; rolamentos lisos/dentados; proteção parcial opcional.

**Características obrigatórias.** Interior mecânico claramente exposto.

**Desgaste, sujeira e dano.** Graxa pesada e fios marcados por atrito.

**Compatibilidade recomendada.** `machine, damaged, experimental.`

**Regra de implementação.** É a peça de transição estrutural. Deve esconder ou justificar diferenças de largura entre pernas e torso e fornecer espaço visual para cabos, cintos ou juntas.

---

### `HIP_07` — Cinto de ferramentas

**Slot estrutural:** pernas → torso  
**Envelope de dimensão:** 6–12% da altura total  
**Ponto(s) de encaixe:** dois conectores inferiores para pernas e um conector superior para o torso

**Silhueta e proporção.** Cinto funcional com ferramentas criando irregularidade na silhueta.

**Construção exata.** Faixa grossa, 3–7 presilhas, chaves/alicate/martelo pequeno e bolsa lateral.

**Materiais e leitura de superfície.** Tecido/couro + metais diversos.

**Parâmetros procedurais permitidos.** seleção 2–5 ferramentas; lado predominante; bolsa 0–2; fita/cordão.

**Características obrigatórias.** Ferramentas visíveis e reconhecíveis.

**Desgaste, sujeira e dano.** Ferramentas riscadas, tecido sujo de óleo.

**Compatibilidade recomendada.** `worker, technical.`

**Regra de implementação.** É a peça de transição estrutural. Deve esconder ou justificar diferenças de largura entre pernas e torso e fornecer espaço visual para cabos, cintos ou juntas.

---

### `HIP_08` — Junta biomecânica

**Slot estrutural:** pernas → torso  
**Envelope de dimensão:** 6–12% da altura total  
**Ponto(s) de encaixe:** dois conectores inferiores para pernas e um conector superior para o torso

**Silhueta e proporção.** Transição orgânico-mecânica sem linha limpa de separação.

**Construção exata.** Massa orgânica envolve um anel metálico; cabos/tendões entram no torso e nas pernas.

**Materiais e leitura de superfície.** Carne/tecido mutado + aço/cabos.

**Parâmetros procedurais permitidos.** percentual orgânico 40–80%; cabos 1–5; placas 0–3; assimetria 10–40%.

**Características obrigatórias.** Fusão visual entre matéria orgânica e hardware.

**Desgaste, sujeira e dano.** Fluido, cicatriz, ferrugem perto de implantes.

**Compatibilidade recomendada.** `biofusion, experimental.`

**Regra de implementação.** É a peça de transição estrutural. Deve esconder ou justificar diferenças de largura entre pernas e torso e fornecer espaço visual para cabos, cintos ou juntas.

---

### `HIP_09` — Cinto de contenção

**Slot estrutural:** pernas → torso  
**Envelope de dimensão:** 6–12% da altura total  
**Ponto(s) de encaixe:** dois conectores inferiores para pernas e um conector superior para o torso

**Silhueta e proporção.** Faixas grossas e travas que parecem restringir o corpo.

**Construção exata.** 2–4 correias cruzadas, fivelas grandes, trava frontal e pontos de ancoragem laterais.

**Materiais e leitura de superfície.** Tecido técnico/borracha + aço.

**Parâmetros procedurais permitidos.** correias 2–5; cadeado/trava; etiqueta; uma correia solta opcional.

**Características obrigatórias.** Sensação de contenção/prisão.

**Desgaste, sujeira e dano.** Tiras tensionadas, marcas de desgaste nas fivelas.

**Compatibilidade recomendada.** `laboratory, experimental, prisoner, biofusion.`

**Regra de implementação.** É a peça de transição estrutural. Deve esconder ou justificar diferenças de largura entre pernas e torso e fornecer espaço visual para cabos, cintos ou juntas.

---

### `HIP_10` — Plataforma robótica

**Slot estrutural:** pernas → torso  
**Envelope de dimensão:** 6–12% da altura total  
**Ponto(s) de encaixe:** dois conectores inferiores para pernas e um conector superior para o torso

**Silhueta e proporção.** Bloco mecânico rígido, quase uma pequena mesa estrutural sob o torso.

**Construção exata.** Carcaça retangular/hexagonal, dois sockets de perna, engrenagem/anel central e placa inferior.

**Materiais e leitura de superfície.** Aço reforçado/blindagem.

**Parâmetros procedurais permitidos.** forma retangular/hexagonal; 1–3 painéis; 6–12 parafusos; faixa de perigo opcional.

**Características obrigatórias.** Geometria rígida e grande capacidade de carga.

**Desgaste, sujeira e dano.** Riscos nos cantos, óleo sob sockets, tinta gasta.

**Compatibilidade recomendada.** `industrial_machine, heavy, boss.`

**Regra de implementação.** É a peça de transição estrutural. Deve esconder ou justificar diferenças de largura entre pernas e torso e fornecer espaço visual para cabos, cintos ou juntas.

---


## 4. Torsos

### `TOR_01` — Macacão industrial

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Torso humanoide médio; ombros naturais; cintura visível.

**Construção exata.** Macacão com zíper central, 1–3 bolsos, costuras nos ombros e faixa de cintura.

**Materiais e leitura de superfície.** Tecido industrial.

**Parâmetros procedurais permitidos.** zíper central/lateral; bolsos 1–4; faixa refletiva; crachá; mangas integradas ou destacáveis.

**Características obrigatórias.** Leitura clara de uniforme de trabalhador.

**Desgaste, sujeira e dano.** Óleo no abdômen, poeira inferior, tecido gasto em ombros.

**Compatibilidade recomendada.** `worker, technical.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_02` — Colete de ferramentas

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Torso humanoide com camada externa volumosa no peito e cintura.

**Construção exata.** Colete aberto/fechado, 4–10 bolsos, presilhas, suportes para ferramentas e fecho frontal.

**Materiais e leitura de superfície.** Tecido reforçado + pequenos metais.

**Parâmetros procedurais permitidos.** bolsos 4–10; ferramentas 0–4; lado assimétrico; rádio opcional.

**Características obrigatórias.** Alta densidade de bolsos/presilhas.

**Desgaste, sujeira e dano.** Sujeira de graxa, bordas desfiadas, marcas de ferramentas.

**Compatibilidade recomendada.** `technical, worker, support.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_03` — Colete balístico

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Volume moderado; placa frontal retangular/trapezoidal sobre torso humano.

**Construção exata.** Porta-placas, duas placas frontais, tiras de ombro e módulos laterais.

**Materiais e leitura de superfície.** Nylon tático + blindagem composta.

**Parâmetros procedurais permitidos.** placa lisa/segmentada; 2–6 bolsos; identificação; cor grafite/azul-petróleo.

**Características obrigatórias.** Placa central e tiras táticas.

**Desgaste, sujeira e dano.** Riscos e impactos na placa, pouco desgaste orgânico.

**Compatibilidade recomendada.** `security, military.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_04` — Peitoral militar

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Silhueta angular e mais larga; peito dividido em segmentos.

**Construção exata.** Placas centrais sobrepostas, proteção lateral, gola baixa e encaixes de ombro rígidos.

**Materiais e leitura de superfície.** Blindagem composta/aço pintado.

**Parâmetros procedurais permitidos.** 3–7 segmentos; quinas 15–35°; insígnia; pequenas luzes; numeração.

**Características obrigatórias.** Arquitetura segmentada e militar.

**Desgaste, sujeira e dano.** Impactos, tinta lascada nas quinas, poeira nas juntas.

**Compatibilidade recomendada.** `military, elite, ranged, heavy.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_05` — Blindagem pesada

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Torso muito largo e profundo; cabeça pode ficar parcialmente embutida.

**Construção exata.** Placa frontal grossa em 2–4 camadas, laterais reforçadas, gola alta e suporte inferior robusto.

**Materiais e leitura de superfície.** Aço reforçado/blindagem composta.

**Parâmetros procedurais permitidos.** espessura 110–150%; placas 2–5; visor/grade opcional; alças de manutenção.

**Características obrigatórias.** Massa frontal dominante e sensação de tanque.

**Desgaste, sujeira e dano.** Grandes arranhões, amassados, impactos, ferrugem em parafusos.

**Compatibilidade recomendada.** `heavy, boss, military, industrial_machine.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_06` — Exoesqueleto simples

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Torso-base humano permanece visível, circundado por barras externas.

**Construção exata.** Duas hastes verticais nas costas/laterais, arco de ombro, suportes de cintura e cabos curtos.

**Materiais e leitura de superfície.** Tecido/metal, aço/alumínio.

**Parâmetros procedurais permitidos.** 2–6 hastes; estrutura simétrica/assimétrica; pequenos pistões; placas 0–2.

**Características obrigatórias.** O corpo interno deve continuar visível entre as barras.

**Desgaste, sujeira e dano.** Graxa nas juntas externas, tecido gasto onde a armação encosta.

**Compatibilidade recomendada.** `worker, technical, security, biofusion.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_07` — Peitoral hidráulico

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Torso mecânico pesado com pistões como motivo principal.

**Construção exata.** Placa peitoral central, 2–4 pistões laterais/diagonais, coletor de fluido e mangueiras.

**Materiais e leitura de superfície.** Aço + hidráulica.

**Parâmetros procedurais permitidos.** pistões 2–6; direção vertical/diagonal; reservatório 0–1; grade opcional.

**Características obrigatórias.** Pistões claramente visíveis na frente ou laterais.

**Desgaste, sujeira e dano.** Óleo, riscos nas hastes e escurecimento nas vedações.

**Compatibilidade recomendada.** `industrial_machine, heavy, technical.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_08` — Torso com grade

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Bloco mecânico médio com painel de ventilação frontal dominante.

**Construção exata.** Carcaça retangular, grade com 5–12 lâminas ou malha, ventoinha interna opcional, placas laterais.

**Materiais e leitura de superfície.** Aço pintado/galvanizado.

**Parâmetros procedurais permitidos.** grade horizontal/vertical/malha; ventoinha 0–2; etiquetas; parafusos.

**Características obrigatórias.** Grande região frontal ventilada.

**Desgaste, sujeira e dano.** Poeira acumulada na grade, bordas gastas e ferrugem pontual.

**Compatibilidade recomendada.** `industrial_machine, technical.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_09` — Torso com núcleo

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Torso construído em torno de cavidade central emissiva ou protegida.

**Construção exata.** Aro/placas em volta do núcleo, tampa parcial, cabos convergentes e anel estrutural.

**Materiais e leitura de superfície.** Metal + material emissivo/vidro técnico.

**Parâmetros procedurais permitidos.** núcleo circular/hexagonal/vertical; cor emissiva; cobertura 0–60%; cabos 2–6.

**Características obrigatórias.** Núcleo central deve ser o foco visual.

**Desgaste, sujeira e dano.** Fuligem/queima perto do núcleo, rachaduras no visor, vazamento de luz.

**Compatibilidade recomendada.** `experimental, machine, boss, biofusion.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_10` — Torso aberto

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Carcaça danificada ou projetada com grande abertura revelando interior.

**Construção exata.** Bordas de placa, estrutura interna, cabos, tubos, pequenos módulos e reforços cruzados.

**Materiais e leitura de superfície.** Sucata, aço, cobre, cabos.

**Parâmetros procedurais permitidos.** abertura 25–60% da frente; cabos 3–10; tubos 0–4; placa pendente.

**Características obrigatórias.** Interior visível e ausência de fachada contínua.

**Desgaste, sujeira e dano.** Bordas rasgadas, faíscas opcionais, óleo e ferrugem interna.

**Compatibilidade recomendada.** `damaged, salvaged, machine, experimental.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_11` — Torso biomecânico

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Caixa torácica deformada misturada a placas; assimetria deliberada.

**Construção exata.** Costelas orgânicas/metálicas, placa parcial no esterno, cabos como tendões e massa orgânica entre estruturas.

**Materiais e leitura de superfície.** Carne mutada + metal.

**Parâmetros procedurais permitidos.** costelas 4–10; lado mais orgânico; placas 1–4; tubos 1–5; cavidade opcional.

**Características obrigatórias.** Fusão anatômica e mecânica; não parecer armadura vestida.

**Desgaste, sujeira e dano.** Fluido, tecido cicatrizado, ferrugem junto aos implantes.

**Compatibilidade recomendada.** `biofusion, experimental, corrupted.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_12` — Torso assimétrico

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Um lado do tronco 15–45% mais volumoso ou blindado.

**Construção exata.** Base humanoide/mecânica, placa grande unilateral, ombro reforçado correspondente e conectores desiguais.

**Materiais e leitura de superfície.** Material base + blindagem/sucata.

**Parâmetros procedurais permitidos.** lado dominante; placa 1–3; braço pesado pode combinar; inclinação ±10°.

**Características obrigatórias.** Assimetria deve ser visível pela silhueta, não só textura.

**Desgaste, sujeira e dano.** Desgaste maior no lado exposto; soldas no lado reforçado.

**Compatibilidade recomendada.** `salvaged, heavy, experimental, asymmetric.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_13` — Tanque frontal

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Torso com reservatório ocupando 25–45% da área frontal.

**Construção exata.** Estrutura de suporte, tanque cilíndrico/retangular, visor de nível, duas cintas e tubulação inferior.

**Materiais e leitura de superfície.** Metal + vidro/policarbonato + fluido.

**Parâmetros procedurais permitidos.** formato do tanque; fluido; proteção parcial; válvula; tubos 1–3.

**Características obrigatórias.** Reservatório frontal claramente separado da caixa torácica.

**Desgaste, sujeira e dano.** Riscos no tanque, condensação, vazamento na válvula.

**Compatibilidade recomendada.** `technical, laboratory, experimental.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_14` — Avental pesado

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Torso humano/industrial coberto por proteção pendente da clavícula à cintura.

**Construção exata.** Avental espesso, tiras no pescoço/ombros, 1–3 placas de reforço e bolsos baixos.

**Materiais e leitura de superfície.** Couro tratado, borracha, tecido térmico, pequenas chapas.

**Parâmetros procedurais permitidos.** comprimento 55–90% do torso; queimaduras; bolsos; faixa de perigo.

**Características obrigatórias.** Camada frontal pendente com leitura de proteção de soldador.

**Desgaste, sujeira e dano.** Queimaduras, respingos metálicos, fuligem e borda inferior gasta.

**Compatibilidade recomendada.** `worker, technical, heat, welding.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_15` — Arnês técnico

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Torso relativamente aberto, organizado por correias e conectores.

**Construção exata.** 2 correias verticais, faixa horizontal, 3–8 pontos de encaixe, cabos curtos e módulos pequenos.

**Materiais e leitura de superfície.** Tecido técnico + metal/plástico.

**Parâmetros procedurais permitidos.** correias 2–5; conectores 3–10; ferramenta 0–3; assimetria leve.

**Características obrigatórias.** Rede de fixação claramente visível sobre a base.

**Desgaste, sujeira e dano.** Manchas de óleo junto aos conectores, desgaste nas correias.

**Compatibilidade recomendada.** `technical, support, laboratory.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_16` — Torso compacto

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Bloco curto e muito largo; pouca separação entre cabeça, ombros e cintura.

**Construção exata.** Carcaça em caixa, painéis frontais pequenos, ombros integrados e base curta.

**Materiais e leitura de superfície.** Aço/blindagem/plástico industrial.

**Parâmetros procedurais permitidos.** largura 120–160%; altura 70–85%; painel 1–3; visor/grade opcional.

**Características obrigatórias.** Proporção curta e larga deve dominar.

**Desgaste, sujeira e dano.** Riscos nos cantos, poeira em recessos.

**Compatibilidade recomendada.** `machine, heavy, compact.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_17` — Torso estreito

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Tronco fino com largura 65–80% de um torso humano médio; alto espaço negativo junto aos braços.

**Construção exata.** Estrutura central, duas placas estreitas e conectores de ombro pequenos.

**Materiais e leitura de superfície.** Metal leve, tecido, experimental.

**Parâmetros procedurais permitidos.** largura 65–85%; altura normal/alta; cabos 0–4; placa 1–2.

**Características obrigatórias.** Baixa massa e leitura vertical.

**Desgaste, sujeira e dano.** Pouco dano pesado; riscos finos, fios expostos opcionais.

**Compatibilidade recomendada.** `fast, experimental, machine, ambush.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_18` — Torso corcunda

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Volume traseiro sobe acima dos ombros e empurra cabeça para frente.

**Construção exata.** Base frontal média, módulo dorsal integrado, arco superior e cabos/tubos laterais.

**Materiais e leitura de superfície.** Metal, tecido, orgânico ou misto.

**Parâmetros procedurais permitidos.** corcunda 20–55% da altura do torso; conteúdo: tanque/músculo/motor; tubos 0–4.

**Características obrigatórias.** Linha dorsal elevada e cabeça projetada.

**Desgaste, sujeira e dano.** Desgaste no topo, sujeira em vales laterais, vazamento opcional.

**Compatibilidade recomendada.** `hunched, technical, biofusion, salvaged.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_19` — Placas remendadas

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Torso formado por chapas de tamanhos e materiais diferentes, unidas sem simetria de fábrica.

**Construção exata.** 3–8 placas sobrepostas, soldas visíveis, parafusos incompatíveis, suporte interno parcialmente aparente.

**Materiais e leitura de superfície.** Sucata multi-material.

**Parâmetros procedurais permitidos.** placas 3–9; metais 2–4; cores diferentes; soldas 2–6; furos tampados.

**Características obrigatórias.** Aparência de reconstrução improvisada.

**Desgaste, sujeira e dano.** Ferrugem desigual, solda recente/antiga, bordas tortas.

**Compatibilidade recomendada.** `salvaged, worker, machine.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_20` — Isolamento térmico

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Torso volumoso por camadas acolchoadas/isolantes; contorno arredondado sobre estrutura rígida.

**Construção exata.** Manta térmica, costuras ou abraçadeiras, placas refletivas e áreas carbonizadas opcionais.

**Materiais e leitura de superfície.** Tecido térmico, cerâmica, alumínio, aço.

**Parâmetros procedurais permitidos.** manta 40–90% da superfície; placas 0–4; costura vertical/horizontal; queimadura.

**Características obrigatórias.** Camada isolante deve parecer funcional e espessa.

**Desgaste, sujeira e dano.** Fuligem, descoloração por calor, bordas queimadas.

**Compatibilidade recomendada.** `technical, heat, industrial.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_21` — Proteção química

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Torso selado, de superfície mais lisa, com costuras/selos e poucos pontos de abertura.

**Construção exata.** Macacão/borracha selada, anel de pescoço, fecho estanque, 1–2 filtros/módulos e selo de cintura.

**Materiais e leitura de superfície.** Borracha técnica, polímero, tecido selado.

**Parâmetros procedurais permitidos.** cor branco sujo/cinza/verde; filtros 0–2; janela/patch; fita de vedação.

**Características obrigatórias.** Leitura de traje selado, sem muitas frestas abertas.

**Desgaste, sujeira e dano.** Manchas químicas, amarelamento, rachaduras nas dobras.

**Compatibilidade recomendada.** `laboratory, technical, contaminated.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_22` — Escudo parcial fixo

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Torso recebe uma grande placa lateral/diagonal integrada que protege 25–50% da frente.

**Construção exata.** Base média, suporte rígido, placa espessa e pequena janela/rebaixo opcional.

**Materiais e leitura de superfície.** Blindagem composta/aço.

**Parâmetros procedurais permitidos.** lado esquerdo/direito; ângulo; tamanho; janela 0–1; símbolo.

**Características obrigatórias.** Placa integrada ao torso, não segurada pela mão.

**Desgaste, sujeira e dano.** Impactos concentrados no escudo, tinta perdida nas bordas.

**Compatibilidade recomendada.** `heavy, security, military.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_23` — Máquina de carga

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Arquitetura inspirada em atuador/empilhadeira: quadro retangular, travessas e grande mecanismo central.

**Construção exata.** Duas colunas laterais, travessa superior, atuador central, placas de serviço e suportes de braço robustos.

**Materiais e leitura de superfície.** Aço industrial, amarelo segurança, preto.

**Parâmetros procedurais permitidos.** colunas 2–4; atuador único/duplo; grade; números grandes; ganchos.

**Características obrigatórias.** Leitura de equipamento de fábrica reaproveitado.

**Desgaste, sujeira e dano.** Tinta raspada, graxa, riscos profundos e ferrugem localizada.

**Compatibilidade recomendada.** `industrial_machine, heavy, boss.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---

### `TOR_24` — Torso tubular

**Slot estrutural:** quadril → ombros/pescoço  
**Envelope de dimensão:** 25–38% da altura total  
**Ponto(s) de encaixe:** conector inferior central, dois conectores laterais de ombro e um conector superior de cabeça; alguns modelos incluem conector traseiro

**Silhueta e proporção.** Cilindros e tubulações definem a forma mais que placas contínuas.

**Construção exata.** 2–6 cilindros verticais/horizontais, coletor central, tubos de conexão e cintas metálicas.

**Materiais e leitura de superfície.** Aço, cobre, borracha, vidro técnico opcional.

**Parâmetros procedurais permitidos.** cilindros 2–8; diâmetros variados; tubos 2–10; válvulas 0–3; pressão/fluido.

**Características obrigatórias.** Tubulação/cilindros devem dominar a silhueta.

**Desgaste, sujeira e dano.** Oxidação nas braçadeiras, condensação, óleo e vazamentos.

**Compatibilidade recomendada.** `technical, experimental, industrial_machine.`

**Regra de implementação.** O torso é a peça visual dominante. Deve definir origem, massa e tecnologia do inimigo antes dos detalhes menores. Variações não podem apagar a leitura da arquitetura-base.

---


## 5. Ombros

### `SHO_01` — Sem ombreira

**Slot estrutural:** torso → braço  
**Envelope de dimensão:** 5–14% da largura total do inimigo por lado  
**Ponto(s) de encaixe:** conector interno no torso e conector externo no braço

**Silhueta e proporção.** Transição direta do torso para o braço, mantendo o eixo anatômico/mecânico visível.

**Construção exata.** Apenas anel, costura ou socket mínimo.

**Materiais e leitura de superfície.** Mesmo material do torso/braço.

**Parâmetros procedurais permitidos.** anel 0–1; costura; pequena placa interna.

**Características obrigatórias.** Não criar volume externo de ombreira.

**Desgaste, sujeira e dano.** Desgaste na borda do socket.

**Compatibilidade recomendada.** `light, humanoid, fast.`

**Regra de implementação.** A ombreira deve funcionar como adaptador entre torso e braço. Pode aumentar a silhueta, mas precisa preservar o eixo da articulação.

---

### `SHO_02` — Ombreira industrial

**Slot estrutural:** torso → braço  
**Envelope de dimensão:** 5–14% da largura total do inimigo por lado  
**Ponto(s) de encaixe:** conector interno no torso e conector externo no braço

**Silhueta e proporção.** Placa pequena quadrada/trapezoidal cobrindo o topo da junta.

**Construção exata.** Chapa de 1 camada, 2–4 rebites e pequeno suporte inferior.

**Materiais e leitura de superfície.** Aço pintado.

**Parâmetros procedurais permitidos.** forma quadrada/trapezoidal; símbolo; faixa amarela; 2–6 rebites.

**Características obrigatórias.** Baixo volume e aparência utilitária.

**Desgaste, sujeira e dano.** Riscos na borda superior e ferrugem nos rebites.

**Compatibilidade recomendada.** `worker, technical, industrial.`

**Regra de implementação.** A ombreira deve funcionar como adaptador entre torso e braço. Pode aumentar a silhueta, mas precisa preservar o eixo da articulação.

---

### `SHO_03` — Ombreira tática

**Slot estrutural:** torso → braço  
**Envelope de dimensão:** 5–14% da largura total do inimigo por lado  
**Ponto(s) de encaixe:** conector interno no torso e conector externo no braço

**Silhueta e proporção.** Placa angular seguindo a curva do ombro, sem exagerar largura.

**Construção exata.** Polígono de 2–3 segmentos, tiras de fixação e borda elevada.

**Materiais e leitura de superfície.** Blindagem composta/polímero.

**Parâmetros procedurais permitidos.** ângulo 15–35°; 1–3 segmentos; identificação.

**Características obrigatórias.** Forma angular militar.

**Desgaste, sujeira e dano.** Impactos pequenos e bordas polidas.

**Compatibilidade recomendada.** `security, military.`

**Regra de implementação.** A ombreira deve funcionar como adaptador entre torso e braço. Pode aumentar a silhueta, mas precisa preservar o eixo da articulação.

---

### `SHO_04` — Ombreira pesada

**Slot estrutural:** torso → braço  
**Envelope de dimensão:** 5–14% da largura total do inimigo por lado  
**Ponto(s) de encaixe:** conector interno no torso e conector externo no braço

**Silhueta e proporção.** Grande bloco que aumenta a largura total 8–18% por lado.

**Construção exata.** 2–4 placas sobrepostas, suporte rígido, socket profundo e parafusos grandes.

**Materiais e leitura de superfície.** Aço reforçado/blindagem.

**Parâmetros procedurais permitidos.** volume 110–150%; placas; espinhos não; faixa de perigo opcional.

**Características obrigatórias.** Grande massa sobre a articulação.

**Desgaste, sujeira e dano.** Amassados, riscos e sujeira nos recessos.

**Compatibilidade recomendada.** `heavy, boss, military, machine.`

**Regra de implementação.** A ombreira deve funcionar como adaptador entre torso e braço. Pode aumentar a silhueta, mas precisa preservar o eixo da articulação.

---

### `SHO_05` — Ombreira de sucata

**Slot estrutural:** torso → braço  
**Envelope de dimensão:** 5–14% da largura total do inimigo por lado  
**Ponto(s) de encaixe:** conector interno no torso e conector externo no braço

**Silhueta e proporção.** Chapa irregular, ligeiramente torta e claramente reaproveitada.

**Construção exata.** 1–3 chapas de formatos diferentes, soldas, parafusos incompatíveis e bordas cortadas.

**Materiais e leitura de superfície.** Sucata.

**Parâmetros procedurais permitidos.** ângulo torto ±8°; 1–3 placas; cor diferente do torso; solda.

**Características obrigatórias.** Imperfeição proposital e assimetria.

**Desgaste, sujeira e dano.** Ferrugem, tinta velha, bordas serrilhadas.

**Compatibilidade recomendada.** `salvaged, worker, machine.`

**Regra de implementação.** A ombreira deve funcionar como adaptador entre torso e braço. Pode aumentar a silhueta, mas precisa preservar o eixo da articulação.

---

### `SHO_06` — Ombreira com luz

**Slot estrutural:** torso → braço  
**Envelope de dimensão:** 5–14% da largura total do inimigo por lado  
**Ponto(s) de encaixe:** conector interno no torso e conector externo no braço

**Silhueta e proporção.** Ombreira média com beacon/indicador visível no topo ou frente.

**Construção exata.** Placa base, alojamento da luz, pequena proteção e cabo curto.

**Materiais e leitura de superfície.** Metal/polímero + emissivo.

**Parâmetros procedurais permitidos.** luz vermelha/âmbar/verde/azul; fixa/pulsante; uma ou duas lentes.

**Características obrigatórias.** Ponto emissivo deve ser legível sem dominar todo o inimigo.

**Desgaste, sujeira e dano.** Riscos na proteção e visor rachado opcional.

**Compatibilidade recomendada.** `security, technical, machine, elite.`

**Regra de implementação.** A ombreira deve funcionar como adaptador entre torso e braço. Pode aumentar a silhueta, mas precisa preservar o eixo da articulação.

---

### `SHO_07` — Ombreira com mangueira

**Slot estrutural:** torso → braço  
**Envelope de dimensão:** 5–14% da largura total do inimigo por lado  
**Ponto(s) de encaixe:** conector interno no torso e conector externo no braço

**Silhueta e proporção.** Junta protegida por placa com conector tubular saindo para trás.

**Construção exata.** Placa média, flange, mangueira de 1 diâmetro dominante e 1–3 grampos.

**Materiais e leitura de superfície.** Metal + borracha.

**Parâmetros procedurais permitidos.** rota da mangueira; diâmetro; conector reto/cotovelo; cor.

**Características obrigatórias.** Mangueira deve conectar visualmente ao BACK ou torso.

**Desgaste, sujeira e dano.** Óleo no flange, abrasão nos grampos.

**Compatibilidade recomendada.** `technical, hydraulic, machine.`

**Regra de implementação.** A ombreira deve funcionar como adaptador entre torso e braço. Pode aumentar a silhueta, mas precisa preservar o eixo da articulação.

---

### `SHO_08` — Ombreira orgânica

**Slot estrutural:** torso → braço  
**Envelope de dimensão:** 5–14% da largura total do inimigo por lado  
**Ponto(s) de encaixe:** conector interno no torso e conector externo no braço

**Silhueta e proporção.** Massa assimétrica cobrindo a articulação como crescimento ou tecido endurecido.

**Construção exata.** 2–4 volumes orgânicos, placas/ossos opcionais e fibras ligando ao braço.

**Materiais e leitura de superfície.** Carne/tecido mutado.

**Parâmetros procedurais permitidos.** volume 80–140%; protuberâncias; placas implantadas 0–2.

**Características obrigatórias.** Contorno orgânico irregular e integração com braço.

**Desgaste, sujeira e dano.** Cicatrizes, manchas e áreas necrosadas.

**Compatibilidade recomendada.** `biofusion, corrupted.`

**Regra de implementação.** A ombreira deve funcionar como adaptador entre torso e braço. Pode aumentar a silhueta, mas precisa preservar o eixo da articulação.

---

### `SHO_09` — Junta exposta

**Slot estrutural:** torso → braço  
**Envelope de dimensão:** 5–14% da largura total do inimigo por lado  
**Ponto(s) de encaixe:** conector interno no torso e conector externo no braço

**Silhueta e proporção.** Sem carenagem; eixo, pistão e rolamento formam a própria silhueta.

**Construção exata.** Disco de rotação, pino central, 1–2 pistões e cabos curtos.

**Materiais e leitura de superfície.** Aço, cobre, borracha.

**Parâmetros procedurais permitidos.** discos 1–2; pistão 0–2; cabos 1–4; tamanho.

**Características obrigatórias.** Mecanismo interno visível.

**Desgaste, sujeira e dano.** Graxa escura, riscos circulares e óleo.

**Compatibilidade recomendada.** `machine, damaged, experimental.`

**Regra de implementação.** A ombreira deve funcionar como adaptador entre torso e braço. Pode aumentar a silhueta, mas precisa preservar o eixo da articulação.

---

### `SHO_10` — Ombreira unilateral

**Slot estrutural:** torso → braço  
**Envelope de dimensão:** 5–14% da largura total do inimigo por lado  
**Ponto(s) de encaixe:** conector interno no torso e conector externo no braço

**Silhueta e proporção.** Peça destinada a existir apenas em um lado do inimigo.

**Construção exata.** Pode usar arquitetura de SHO_02/03/04, mas com suporte assimétrico e acabamento deliberado do lado oposto.

**Materiais e leitura de superfície.** Variável conforme origem.

**Parâmetros procedurais permitidos.** lado; nível leve/médio/pesado; símbolo; cor.

**Características obrigatórias.** Assimetria intencional de ombros.

**Desgaste, sujeira e dano.** Conforme material; desgaste maior no lado protegido.

**Compatibilidade recomendada.** `asymmetric, salvaged, military, experimental.`

**Regra de implementação.** A ombreira deve funcionar como adaptador entre torso e braço. Pode aumentar a silhueta, mas precisa preservar o eixo da articulação.

---

### `SHO_11` — Arco protetor

**Slot estrutural:** torso → braço  
**Envelope de dimensão:** 5–14% da largura total do inimigo por lado  
**Ponto(s) de encaixe:** conector interno no torso e conector externo no braço

**Silhueta e proporção.** Estrutura sobe do ombro e curva em volta da lateral/superior da cabeça.

**Construção exata.** Duas barras ou placa arqueada, suporte na clavícula e pontos de fixação traseiros.

**Materiais e leitura de superfície.** Aço reforçado.

**Parâmetros procedurais permitidos.** arco parcial/completo; altura; barras 1–3; pequenas placas.

**Características obrigatórias.** Proteção vertical ao redor da cabeça.

**Desgaste, sujeira e dano.** Riscos no topo e impactos frontais.

**Compatibilidade recomendada.** `heavy, machine, boss.`

**Regra de implementação.** A ombreira deve funcionar como adaptador entre torso e braço. Pode aumentar a silhueta, mas precisa preservar o eixo da articulação.

---

### `SHO_12` — Suporte de ferramenta

**Slot estrutural:** torso → braço  
**Envelope de dimensão:** 5–14% da largura total do inimigo por lado  
**Ponto(s) de encaixe:** conector interno no torso e conector externo no braço

**Silhueta e proporção.** Ombreira funcional com trilho/encaixe para módulo auxiliar.

**Construção exata.** Base rígida, trilho dentado ou furação, trava e cabo/conector.

**Materiais e leitura de superfície.** Aço/alumínio.

**Parâmetros procedurais permitidos.** trilho curto/longo; ferramenta acoplada 0–1; luz; trava.

**Características obrigatórias.** Deve parecer interface de montagem, não armadura comum.

**Desgaste, sujeira e dano.** Graxa no trilho e desgaste na trava.

**Compatibilidade recomendada.** `technical, support, machine.`

**Regra de implementação.** A ombreira deve funcionar como adaptador entre torso e braço. Pode aumentar a silhueta, mas precisa preservar o eixo da articulação.

---


## 6. Braços, ferramentas e armas

### `ARM_01` — Braço humano com manga

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Braço humanoide; bíceps e antebraço proporcionais; mão normal.

**Construção exata.** Manga com costura, cotovelo têxtil, punho e mão de 4–5 dedos simplificados.

**Materiais e leitura de superfície.** Tecido + pele/luva opcional.

**Parâmetros procedurais permitidos.** manga curta/longa; bolso 0–1; luva 0–1; largura ±10%.

**Características obrigatórias.** Anatomia humana clara, sem mecanismo dominante.

**Desgaste, sujeira e dano.** Sujeira no punho/cotovelo, rasgos pequenos.

**Compatibilidade recomendada.** `worker, security, technical.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_02` — Braço com luva pesada

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Braço humano com mão ampliada por luva industrial até 120–150% da mão normal.

**Construção exata.** Manga reforçada, punho grosso, luva com 2–4 placas e dedos robustos.

**Materiais e leitura de superfície.** Tecido + borracha/couro + metal opcional.

**Parâmetros procedurais permitidos.** placas 0–3; dedos 3–5; faixa; queimadura.

**Características obrigatórias.** Mão/luva pesada deve ser o foco.

**Desgaste, sujeira e dano.** Desgaste na palma, dedos escurecidos, arranhões em placas.

**Compatibilidade recomendada.** `worker, melee, technical.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_03` — Braço tático

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Braço humano protegido por cotoveleira e manopla.

**Construção exata.** Manga tática, cotoveleira angular, luva e pequenas tiras.

**Materiais e leitura de superfície.** Tecido tático + polímero.

**Parâmetros procedurais permitidos.** cotoveleira redonda/angular; relógio/radio 0–1; luva; placa 0–2.

**Características obrigatórias.** Leitura militar limpa e proporcional.

**Desgaste, sujeira e dano.** Riscos nas proteções e poeira no antebraço.

**Compatibilidade recomendada.** `security, military, ranged.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_04` — Braço mecânico simples

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Estrutura aberta de dois segmentos, largura 60–85% de braço humano.

**Construção exata.** Haste superior, cotovelo circular, antebraço de duas barras, punho mecânico e mão simples.

**Materiais e leitura de superfície.** Aço/alumínio + cabos.

**Parâmetros procedurais permitidos.** hastes 1–2; mão 3–5 dedos; cabo 0–2; placas 0–2.

**Características obrigatórias.** Mecânica leve e articulações expostas.

**Desgaste, sujeira e dano.** Graxa nas juntas e riscos lineares.

**Compatibilidade recomendada.** `machine, biofusion, technical.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_05` — Braço hidráulico

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Braço espesso com pistões dominantes, 120–160% da espessura normal.

**Construção exata.** Ombro reforçado, cilindro superior, cotovelo encapsulado, pistão no antebraço e mão/encaixe robusto.

**Materiais e leitura de superfície.** Aço reforçado + hidráulica.

**Parâmetros procedurais permitidos.** pistões 2–5; placas 1–4; mangueiras 1–3; mão ou ferramenta.

**Características obrigatórias.** Massa alta e pelo menos um pistão grande visível.

**Desgaste, sujeira e dano.** Óleo nas hastes, tinta gasta, sujeira em vedações.

**Compatibilidade recomendada.** `heavy, industrial_machine, melee.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_06` — Garra de três dedos

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Antebraço mecânico termina em três dedos curvos separados 120°/em pinça.

**Construção exata.** Pulso rotativo, palma circular/triangular, três dedos com 2 segmentos e microatuadores.

**Materiais e leitura de superfície.** Aço escuro.

**Parâmetros procedurais permitidos.** comprimento dos dedos ±20%; pontas lisas/dentadas; cabos 0–3.

**Características obrigatórias.** Exatamente três apêndices principais claramente legíveis.

**Desgaste, sujeira e dano.** Pontas polidas, graxa no pulso.

**Compatibilidade recomendada.** `machine, technical, melee.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_07` — Garra prensora

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Ferramenta frontal grande, semelhante a pinça industrial; massa concentrada na extremidade.

**Construção exata.** Antebraço pesado, pivô central, duas mandíbulas grossas e cilindro de fechamento.

**Materiais e leitura de superfície.** Aço industrial.

**Parâmetros procedurais permitidos.** mandíbulas retas/curvas; dentes 0–6; pistão simples/duplo; faixa de perigo.

**Características obrigatórias.** Duas mandíbulas grandes e mecanismo de prensa.

**Desgaste, sujeira e dano.** Riscos internos, óleo no pivô e amassados.

**Compatibilidade recomendada.** `heavy, industrial_machine, boss, melee.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_08` — Martelo hidráulico

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Antebraço termina em cabeça de impacto maciça alinhada ao eixo do braço.

**Construção exata.** Cilindro de acionamento, haste deslizante, bloco de impacto e carcaça protetora.

**Materiais e leitura de superfície.** Aço reforçado.

**Parâmetros procedurais permitidos.** cabeça quadrada/cilíndrica; comprimento do curso; mangueiras; placa de proteção.

**Características obrigatórias.** Linha de impacto axial e grande massa distal.

**Desgaste, sujeira e dano.** Metal polido na face, fissuras, óleo no cilindro.

**Compatibilidade recomendada.** `heavy, industrial_machine, melee.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_09` — Serra circular

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Braço mecânico 120–150% da largura padrão; antebraço cresce em direção a disco de 60–80% do comprimento do antebraço.

**Construção exata.** Ombro com junta circular; braço superior com duas placas; cotovelo de discos laterais; antebraço caixa; eixo central; disco com 18–32 dentes; proteção cobre 20–45% do disco.

**Materiais e leitura de superfície.** Aço pintado/exposto + borracha/cobre.

**Parâmetros procedurais permitidos.** 18–32 dentes; proteção 20–45%; 0–2 mangueiras; faixa de perigo; diâmetro ±15%; placas/rebites.

**Características obrigatórias.** Disco exposto, antebraço pesado e articulação claramente mecânica.

**Desgaste, sujeira e dano.** Desgaste máximo nos dentes, borda da proteção e eixo; óleo no centro; ferrugem em parafusos.

**Compatibilidade recomendada.** `industrial_machine, salvaged, heavy, melee; exige ombro médio/pesado.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_10` — Maçarico

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Braço médio com bico alongado substituindo ou sendo segurado pela mão; mangueira visível até fonte de combustível.

**Construção exata.** Punho/encaixe, válvula, tubo metálico, bocal cônico e proteção térmica; mangueira segue ao BACK/TOR.

**Materiais e leitura de superfície.** Aço/cobre/borracha + proteção térmica.

**Parâmetros procedurais permitidos.** bocal curto/longo; 1–2 mangueiras; válvula; chama estreita/larga; escudo de calor.

**Características obrigatórias.** Bico, alimentação e proteção térmica legíveis.

**Desgaste, sujeira e dano.** Descoloração azul/marrom perto do bocal, fuligem e mangueira suja.

**Compatibilidade recomendada.** `technical, worker, ranged_short, requires_fuel.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_11` — Perfurador

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Antebraço robusto termina em broca curta/grossa alinhada ao eixo.

**Construção exata.** Motor/caixa, mandril, broca helicoidal ou cônica e suporte lateral.

**Materiais e leitura de superfície.** Aço industrial.

**Parâmetros procedurais permitidos.** broca 15–35% do braço; helicoidal/cônica; guarda 0–1; cabo; placa.

**Características obrigatórias.** Broca central claramente visível.

**Desgaste, sujeira e dano.** Borda metálica polida, resíduos no sulco, óleo no mandril.

**Compatibilidade recomendada.** `industrial_machine, melee, technical.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_12` — Soldador elétrico

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Braço técnico com eletrodos/bico e cabos de alta corrente, mais leve que maçarico.

**Construção exata.** Módulo de alimentação no antebraço, dois eletrodos ou ponta única, cabo grosso e isoladores cerâmicos.

**Materiais e leitura de superfície.** Metal + cobre + cerâmica + borracha.

**Parâmetros procedurais permitidos.** ponta dupla/simples; cabos 1–3; isoladores; pequena luz; proteção.

**Características obrigatórias.** Elementos condutores/isoladores e cabo de energia.

**Desgaste, sujeira e dano.** Queima nas pontas, cobre escurecido, fuligem localizada.

**Compatibilidade recomendada.** `technical, support, special.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_13` — Bastão elétrico

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Braço humano/mecânico segura ou integra bastão 45–70% do comprimento do braço.

**Construção exata.** Empunhadura, haste isolada, eletrodos frontais e pequena bateria/indicador.

**Materiais e leitura de superfície.** Polímero/metal/borracha.

**Parâmetros procedurais permitidos.** bastão reto/T; eletrodos 2–4; luz; guarda de mão.

**Características obrigatórias.** Arma separada e fina com ponta elétrica reconhecível.

**Desgaste, sujeira e dano.** Riscos no bastão e queimaduras na ponta.

**Compatibilidade recomendada.** `security, melee, light.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_14` — Pistola

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Mão humana/mecânica segura arma curta; massa baixa.

**Construção exata.** Empunhadura, guarda-mato, corpo/slide, cano curto e mira simples.

**Materiais e leitura de superfície.** Metal/polímero.

**Parâmetros procedurais permitidos.** slide quadrado/angulado; carregador; lanterna 0–1; silenciador não; cores.

**Características obrigatórias.** Leitura inequívoca de pistola compacta.

**Desgaste, sujeira e dano.** Desgaste no slide, fuligem no cano.

**Compatibilidade recomendada.** `security, ranged, light.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_15` — SMG improvisada

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Arma compacta com corpo irregular e peças reaproveitadas; 45–65% do braço em comprimento.

**Construção exata.** Receptor, carregador lateral/inferior, cano curto, empunhadura e coronha mínima ou ausente.

**Materiais e leitura de superfície.** Aço/sucata/polímero.

**Parâmetros procedurais permitidos.** carregador reto/curvo; coronha 0–1; fita/solda; mira improvisada.

**Características obrigatórias.** Silhueta de arma automática compacta e construção remendada.

**Desgaste, sujeira e dano.** Fuligem no cano, riscos e soldas gastas.

**Compatibilidade recomendada.** `security, salvaged, ranged.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_16` — Shotgun industrial

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Arma de cano grosso e curto, visualmente pesada; pode ser segurada ou integrada.

**Construção exata.** Receptor robusto, cano único/duplo, guarda, empunhadura e alimentação tubular/caixa.

**Materiais e leitura de superfície.** Aço reforçado/madeira técnica/polímero.

**Parâmetros procedurais permitidos.** 1–2 canos; comprimento 50–75% do braço; alimentação; proteção de calor.

**Características obrigatórias.** Cano de grande diâmetro e massa frontal.

**Desgaste, sujeira e dano.** Fuligem intensa, riscos e borda do cano gasta.

**Compatibilidade recomendada.** `security, heavy_ranged, salvaged.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_17` — Nailgun

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Ferramenta-armamento com caixa retangular e trilho de alimentação.

**Construção exata.** Corpo de ferramenta, nariz estreito, magazine linear/caixa, gatilho e bateria/mangueira.

**Materiais e leitura de superfície.** Metal/polímero.

**Parâmetros procedurais permitidos.** pneumática/elétrica; magazine reto/curvo; bateria 0–1; mangueira 0–1.

**Características obrigatórias.** Formato de ferramenta industrial, não de rifle convencional.

**Desgaste, sujeira e dano.** Óleo, poeira de metal, riscos no nariz.

**Compatibilidade recomendada.** `technical, worker, ranged.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_18` — Lançador pneumático

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Módulo de pressão com câmara cilíndrica e cano grosso; volume médio/pesado.

**Construção exata.** Reservatório de pressão, válvula, cano, manômetro e mangueira/linha de carga.

**Materiais e leitura de superfície.** Aço/alumínio/borracha.

**Parâmetros procedurais permitidos.** reservatório 1–2; manômetro; cano curto/longo; válvula; linha ao BACK.

**Características obrigatórias.** Câmara de pressão e manômetro/valvulagem visíveis.

**Desgaste, sujeira e dano.** Condensação, riscos no cilindro, tinta lascada.

**Compatibilidade recomendada.** `technical, ranged, industrial_machine.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_19` — Canhão integrado

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Antebraço inteiro substituído por arma pesada; braço 130–180% da massa do outro.

**Construção exata.** Socket reforçado, bloco de recuo, câmara, cano grosso, ventilação e cabos/alimentação.

**Materiais e leitura de superfície.** Aço/blindagem/cobre.

**Parâmetros procedurais permitidos.** cano único/duplo; freio de boca; grade; alimentação por cabo/cinta; tamanho.

**Características obrigatórias.** Grande massa distal e integração direta ao braço.

**Desgaste, sujeira e dano.** Fuligem, calor, riscos de recuo e desgaste nas guias.

**Compatibilidade recomendada.** `heavy, boss, military, machine; exige torso/ombro pesado.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_20` — Escudo industrial

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Braço segura placa grande cobrindo 45–75% da altura do torso.

**Construção exata.** Empunhadura traseira, suporte de antebraço, placa espessa, bordas reforçadas e janela opcional pequena.

**Materiais e leitura de superfície.** Aço/blindagem.

**Parâmetros procedurais permitidos.** retangular/trapezoidal; janela 0–1; faixa de perigo; impactos; largura.

**Características obrigatórias.** Área contínua de proteção frontal grande.

**Desgaste, sujeira e dano.** Impactos, riscos radiais e bordas amassadas.

**Compatibilidade recomendada.** `heavy, security, industrial_machine.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_21` — Escudo transparente

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Placa de policarbonato alta, mais fina que ARM_20, com moldura.

**Construção exata.** Painel transparente, borda metálica/polímero, empunhadura e pequenas fissuras opcionais.

**Materiais e leitura de superfície.** Policarbonato + polímero/metal.

**Parâmetros procedurais permitidos.** transparência 45–75%; rachaduras; forma reta/curva; inscrição.

**Características obrigatórias.** Painel translúcido deve permanecer visível.

**Desgaste, sujeira e dano.** Riscos superficiais, rachaduras e opacidade nas bordas.

**Compatibilidade recomendada.** `security, riot, defense.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_22` — Braço orgânico alongado

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Membro 115–150% do comprimento humano, fino ou irregular; mão baixa.

**Construção exata.** Segmentos musculares alongados, cotovelo saliente, tendões, mão de dedos longos.

**Materiais e leitura de superfície.** Carne/tecido mutado.

**Parâmetros procedurais permitidos.** dedos 3–6; alongamento; assimetria; placas implantadas 0–2; tendões expostos.

**Características obrigatórias.** Comprimento anormal e leitura orgânica.

**Desgaste, sujeira e dano.** Cicatrizes, manchas, áreas secas ou sangramento leve.

**Compatibilidade recomendada.** `biofusion, corrupted, melee.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_23` — Lâmina biomecânica

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Antebraço se transforma gradualmente em lâmina; não parece arma segurada.

**Construção exata.** Osso/metal alongado, fibras/cabos na base, uma aresta principal e pequenas placas de transição.

**Materiais e leitura de superfície.** Tecido orgânico + metal/cerâmica.

**Parâmetros procedurais permitidos.** lâmina reta/curva; comprimento 30–60% do braço; serrilha 0–30%; cabos.

**Características obrigatórias.** Transição contínua braço→lâmina.

**Desgaste, sujeira e dano.** Aresta polida, manchas e fissuras na base.

**Compatibilidade recomendada.** `biofusion, experimental, melee.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_24` — Tentáculo com cabos

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Membro flexível sem cotovelo rígido, composto por feixe de cabos/tubos.

**Construção exata.** 3–8 cabos principais, anéis de contenção, ponta com garra/sensor/conector.

**Materiais e leitura de superfície.** Borracha, cobre, tecido orgânico opcional.

**Parâmetros procedurais permitidos.** cabos 3–10; comprimento 90–160%; anéis 2–6; ponta 1 de 4 tipos.

**Características obrigatórias.** Forma flexível e feixe visível.

**Desgaste, sujeira e dano.** Revestimento rasgado, fios expostos, fluido/óleo.

**Compatibilidade recomendada.** `experimental, machine, biofusion.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_25` — Manipulador técnico

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Braço fino/médio projetado para precisão, termina em pinça pequena.

**Construção exata.** Articulações múltiplas, antebraço estreito, punho rotativo e pinça de 2–3 dedos.

**Materiais e leitura de superfície.** Alumínio/aço/plástico.

**Parâmetros procedurais permitidos.** articulações 3–5; pinça 2–3 dedos; luz 0–1; ferramenta fina.

**Características obrigatórias.** Aparência de braço de manutenção de precisão.

**Desgaste, sujeira e dano.** Graxa leve e marcas de ferramenta.

**Compatibilidade recomendada.** `technical, support, machine.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_26` — Kit de reparo

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Braço normal/mecânico carrega múltiplas ferramentas integradas no antebraço.

**Construção exata.** Carcaça com compartimentos, chave retrátil, sonda, pequena solda e pinça.

**Materiais e leitura de superfície.** Metal/polímero.

**Parâmetros procedurais permitidos.** 2–5 ferramentas; gavetas abertas/fechadas; luz; cabo.

**Características obrigatórias.** Mais de uma ferramenta secundária visível.

**Desgaste, sujeira e dano.** Riscos, graxa e bordas de compartimento gastas.

**Compatibilidade recomendada.** `technical, support.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_27` — Injetor/seringa

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Antebraço leve/médio termina em injetor com reservatório visível.

**Construção exata.** Cilindro transparente, êmbolo/mecanismo, agulha protegida e mangueira opcional.

**Materiais e leitura de superfície.** Policarbonato, metal, borracha, fluido.

**Parâmetros procedurais permitidos.** 1–3 reservatórios; cor do fluido; agulha única/múltipla; proteção.

**Características obrigatórias.** Reservatório + ponta de injeção inequívocos.

**Desgaste, sujeira e dano.** Condensação, manchas químicas e riscos no protetor.

**Compatibilidade recomendada.** `laboratory, experimental, support/melee.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_28` — Braço com reservatório

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Membro mecânico ou híbrido com tanque incorporado ao antebraço, sem necessariamente possuir injetor.

**Construção exata.** Carcaça, reservatório 20–45% do volume, visor, válvula e tubos até mão/ferramenta.

**Materiais e leitura de superfície.** Metal + vidro/policarbonato + fluido.

**Parâmetros procedurais permitidos.** posição; forma do tanque; fluido; tubos 1–3; ferramenta distal.

**Características obrigatórias.** Reservatório integrado ao próprio membro.

**Desgaste, sujeira e dano.** Vazamento, condensação, rachadura opcional.

**Compatibilidade recomendada.** `experimental, laboratory, technical.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_29` — Braço danificado

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Braço mecânico/híbrido incompleto, ainda funcional ou pendente.

**Construção exata.** Segmento rompido, fios/cabos, placa torta, articulação exposta e mão parcial opcional.

**Materiais e leitura de superfície.** Sucata, metal, cabos, orgânico conforme base.

**Parâmetros procedurais permitidos.** nível de perda 20–60%; fios 2–8; faísca 0–1; placa solta.

**Características obrigatórias.** Dano estrutural visível; não apenas textura de arranhão.

**Desgaste, sujeira e dano.** Bordas rasgadas, fuligem, óleo/sangue.

**Compatibilidade recomendada.** `damaged, salvaged, machine, biofusion.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---

### `ARM_30` — Braço parcialmente amputado

**Slot estrutural:** ombro → extremidade funcional  
**Envelope de dimensão:** 30–46% da altura total  
**Ponto(s) de encaixe:** conector proximal no ombro; quando houver mão/ferramenta modular, conector distal no punho

**Silhueta e proporção.** Membro orgânico/híbrido termina antes da posição normal da mão/cotovelo.

**Construção exata.** Coto coberto, implante parcial ou estrutura interna; correias/selagem opcional.

**Materiais e leitura de superfície.** Orgânico, tecido, metal de contenção.

**Parâmetros procedurais permitidos.** altura da amputação; prótese incompleta; curativo/placa; cabos.

**Características obrigatórias.** Ausência real de segmento distal deve alterar a silhueta.

**Desgaste, sujeira e dano.** Cicatriz, tecido seco, ferrugem/fluido no implante.

**Compatibilidade recomendada.** `biofusion, salvaged, damaged.`

**Regra de implementação.** Braços podem ser assimétricos. A peça deve deixar claro onde está o cotovelo e onde está a massa funcional para que a animação continue legível.

---


## 7. Cabeças e sensores

### `HEAD_01` — Rosto humano

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Cabeça humana proporcional, largura 70–85% da largura do pescoço/ombro central; queixo e testa legíveis em poucos pixels.

**Construção exata.** Crânio, cabelo opcional simplificado, olhos, nariz e boca reduzidos; o rosto deve usar contraste forte para não virar ruído.

**Materiais e leitura de superfície.** Pele/cabelo/tecido.

**Parâmetros procedurais permitidos.** cabelo 0–4 estilos; barba 0–2; expressão neutra/agressiva; sujeira; pequenas cicatrizes.

**Características obrigatórias.** Dois olhos e orientação frontal/lateral clara.

**Desgaste, sujeira e dano.** Sujeira em testa/bochecha, pequenos cortes; evitar excesso de microdetalhe.

**Compatibilidade recomendada.** `worker, security, technical.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_02` — Capacete industrial

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Capacete arredondado com aba frontal e pequena projeção lateral.

**Construção exata.** Casco, aba, cinta/encaixe e 2–4 nervuras discretas.

**Materiais e leitura de superfície.** Plástico industrial.

**Parâmetros procedurais permitidos.** cor amarelo/branco/laranja/cinza; faixa; número; rachadura 0–1.

**Características obrigatórias.** Aba frontal e casco de obra reconhecíveis.

**Desgaste, sujeira e dano.** Riscos no topo, poeira e tinta gasta na borda.

**Compatibilidade recomendada.** `worker, technical.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_03` — Capacete com lanterna

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** HEAD_02 acrescido de módulo frontal central ou levemente lateral.

**Construção exata.** Casco, suporte, lente circular/retangular, cabo curto ou bateria traseira.

**Materiais e leitura de superfície.** Plástico + metal + emissivo.

**Parâmetros procedurais permitidos.** lente 1–2; posição; cor da luz; grade/proteção.

**Características obrigatórias.** Fonte de luz frontal claramente presa ao capacete.

**Desgaste, sujeira e dano.** Lente riscada, poeira, cabo exposto opcional.

**Compatibilidade recomendada.** `worker, technical, dark_area.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_04` — Máscara de solda

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Face quase totalmente coberta por placa retangular/trapezoidal; visor estreito.

**Construção exata.** Casco/tiara, placa facial, visor escuro 15–30% da altura e dobradiças laterais.

**Materiais e leitura de superfície.** Metal/plástico térmico + vidro escuro.

**Parâmetros procedurais permitidos.** placa reta/curva; visor único/duplo; marcas de calor; número.

**Características obrigatórias.** Grande máscara plana e visor escuro pequeno.

**Desgaste, sujeira e dano.** Fuligem, respingos de solda e riscos no visor.

**Compatibilidade recomendada.** `worker, technical, welding.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_05` — Máscara levantada

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Máscara de solda articulada acima da testa, deixando rosto parcial exposto.

**Construção exata.** Rosto-base, pivôs laterais e máscara inclinada 55–85° para cima.

**Materiais e leitura de superfície.** Pele + metal/plástico.

**Parâmetros procedurais permitidos.** ângulo; visor; desgaste; rosto com/sem respirador leve.

**Características obrigatórias.** Máscara deve ser visível acima da cabeça e rosto parcial abaixo.

**Desgaste, sujeira e dano.** Fuligem na máscara e testa, pivôs gastos.

**Compatibilidade recomendada.** `worker, technical.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_06` — Respirador

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Parte inferior do rosto coberta, olhos permanecem expostos ou protegidos separadamente.

**Construção exata.** Corpo central, 1–2 filtros laterais, tiras e válvula frontal.

**Materiais e leitura de superfície.** Borracha/plástico.

**Parâmetros procedurais permitidos.** filtros 1–2; redondos/quadrados; válvula; óculos 0–1.

**Características obrigatórias.** Cobertura de nariz/boca + filtros.

**Desgaste, sujeira e dano.** Filtros sujos, borracha rachada, poeira.

**Compatibilidade recomendada.** `worker, technical, laboratory.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_07` — Máscara antigás

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Face inteira selada com duas lentes circulares e filtro central/lateral.

**Construção exata.** Capuz/borracha, lentes, filtro, válvulas e tiras.

**Materiais e leitura de superfície.** Borracha, vidro, metal.

**Parâmetros procedurais permitidos.** lentes 2; filtro 1–2; tubo ao BACK 0–1; capuz.

**Características obrigatórias.** Duas lentes e vedação facial completa.

**Desgaste, sujeira e dano.** Lentes riscadas, filtro oxidado, borracha envelhecida.

**Compatibilidade recomendada.** `laboratory, security, contaminated.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_08` — Viseira tática

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Faixa escura horizontal cobre olhos e parte superior do rosto/capacete.

**Construção exata.** Capacete/aro, visor contínuo e pequenas laterais de suporte.

**Materiais e leitura de superfície.** Polímero + policarbonato.

**Parâmetros procedurais permitidos.** largura 60–95%; cor preta/vermelha/azul; HUD emissivo leve; capacete 0–1.

**Características obrigatórias.** Faixa horizontal única e limpa.

**Desgaste, sujeira e dano.** Riscos, rachadura 0–1, opacidade nas bordas.

**Compatibilidade recomendada.** `security, military, elite.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_09` — Capacete blindado

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Cabeça encapsulada em casco pesado com pouca área de rosto.

**Construção exata.** Casco de 2–4 placas, visor estreito ou fendas, proteção de mandíbula e gola integrada.

**Materiais e leitura de superfície.** Blindagem composta/aço.

**Parâmetros procedurais permitidos.** visor horizontal/duplo; placas; parafusos; antena 0–1.

**Características obrigatórias.** Grande massa e exposição facial mínima.

**Desgaste, sujeira e dano.** Impactos, tinta lascada e visor rachado.

**Compatibilidade recomendada.** `heavy, military, security.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_10` — Headset/rádio

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Cabeça humana/capacete com módulo de comunicação claramente lateral.

**Construção exata.** Fone unilateral/bilateral, arco, microfone e pequeno cabo/módulo.

**Materiais e leitura de superfície.** Plástico, tecido, metal.

**Parâmetros procedurais permitidos.** 1–2 fones; microfone; antena; capacete base.

**Características obrigatórias.** Microfone ou módulo lateral visível.

**Desgaste, sujeira e dano.** Espuma gasta, cabo exposto, riscos.

**Compatibilidade recomendada.** `security, technical, command.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_11` — Óculos quebrados

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Rosto exposto com óculos de proteção assimétricos ou lente faltando.

**Construção exata.** Armação, lente inteira de um lado, outra rachada/ausente, tira.

**Materiais e leitura de superfície.** Policarbonato + plástico.

**Parâmetros procedurais permitidos.** lado quebrado; lente transparente/fumê; trinca 1–3; tira solta.

**Características obrigatórias.** Assimetria ocular clara.

**Desgaste, sujeira e dano.** Riscos, poeira e fragmentos.

**Compatibilidade recomendada.** `worker, technical, salvaged.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_12` — Rosto queimado

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Rosto humano ainda reconhecível, com volumes retraídos e áreas carbonizadas.

**Construção exata.** Crânio/face base, áreas de pele escurecida, uma orelha/parte opcional deformada.

**Materiais e leitura de superfície.** Pele carbonizada, tecido.

**Parâmetros procedurais permitidos.** área afetada 30–80%; lado dominante; olho coberto; cabelo queimado.

**Características obrigatórias.** Dano térmico deve alterar tonalidade e contorno local.

**Desgaste, sujeira e dano.** Fuligem, tecido seco, brilho úmido mínimo.

**Compatibilidade recomendada.** `worker, biofusion, damaged.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_13` — Rosto metálico parcial

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Uma região do rosto é substituída por placa/implante mecânico.

**Construção exata.** Rosto-base, placa na têmpora/maxilar/olho, 2–5 parafusos e 1–2 cabos.

**Materiais e leitura de superfície.** Pele + metal.

**Parâmetros procedurais permitidos.** lado; área 20–55%; sensor 0–1; placa lisa/rebitada.

**Características obrigatórias.** Transição pele-metal visível e localizada.

**Desgaste, sujeira e dano.** Cicatriz na borda, ferrugem leve, óleo.

**Compatibilidade recomendada.** `biofusion, experimental, security.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_14` — Mandíbula mecânica

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Parte superior do rosto pode ser humana; mandíbula/chin substituídos por mecanismo.

**Construção exata.** Articulação bilateral, placa mandibular, dentes/grade opcional e cabos ao pescoço.

**Materiais e leitura de superfície.** Metal + orgânico opcional.

**Parâmetros procedurais permitidos.** mandíbula lisa/dentada; abertura; cabos 0–3; placa de queixo.

**Características obrigatórias.** Mecanismo inferior separado do crânio.

**Desgaste, sujeira e dano.** Graxa nas dobradiças, cicatriz na interface.

**Compatibilidade recomendada.** `biofusion, experimental.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_15` — Sensor único

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Cabeça mecânica centrada em uma lente circular grande.

**Construção exata.** Carcaça quadrada/circular, aro, lente, obturador/anel interno e cabo traseiro.

**Materiais e leitura de superfície.** Metal + vidro/emissivo.

**Parâmetros procedurais permitidos.** lente 25–55% da face; cor; carcaça; tampa parcial; foco.

**Características obrigatórias.** Um único sensor dominante.

**Desgaste, sujeira e dano.** Riscos no aro, lente trincada opcional, óleo.

**Compatibilidade recomendada.** `machine, industrial_machine, boss.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_16` — Sensores assimétricos

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Face mecânica com duas ou mais lentes de tamanhos/posições diferentes.

**Construção exata.** Carcaça irregular, lente principal, 1–3 sensores secundários e cabos.

**Materiais e leitura de superfície.** Metal + vidro.

**Parâmetros procedurais permitidos.** 2–4 sensores; tamanhos variados; cores iguais/diferentes; lado dominante.

**Características obrigatórias.** Assimetria ocular tecnológica clara.

**Desgaste, sujeira e dano.** Lentes riscadas, um sensor apagado opcional.

**Compatibilidade recomendada.** `machine, experimental, salvaged.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_17` — Cabeça cúbica

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Bloco quase cúbico com cantos chanfrados e painel frontal.

**Construção exata.** Seis faces, painel frontal, 4–10 parafusos, ventilação/lente pequena e conector inferior.

**Materiais e leitura de superfície.** Aço/polímero.

**Parâmetros procedurais permitidos.** chanfro; sensor 1–3; grade; número; antena 0–1.

**Características obrigatórias.** Volume cúbico rígido.

**Desgaste, sujeira e dano.** Riscos nos cantos, poeira em juntas.

**Compatibilidade recomendada.** `machine, industrial_machine.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_18` — Cabeça cilíndrica

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Corpo vertical ou horizontal cilíndrico montado sobre eixo rotativo.

**Construção exata.** Cilindro, anel de base, sensor frontal, tampa superior e cabos.

**Materiais e leitura de superfície.** Aço/alumínio.

**Parâmetros procedurais permitidos.** altura 80–140%; lente 1–3; rotação; antena; ranhuras.

**Características obrigatórias.** Silhueta cilíndrica e base de rotação.

**Desgaste, sujeira e dano.** Riscos circulares, óleo no anel.

**Compatibilidade recomendada.** `machine, sensor, technical.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_19` — Câmera industrial

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Corpo de câmera de vigilância como cabeça, com lente frontal e suporte.

**Construção exata.** Caixa retangular alongada, lente, parasol, junta de giro e cabo.

**Materiais e leitura de superfície.** Metal/plástico + vidro.

**Parâmetros procedurais permitidos.** caixa curta/longa; lente 1; LED; parasol; inclinação.

**Características obrigatórias.** Leitura inequívoca de câmera de segurança.

**Desgaste, sujeira e dano.** Lente suja, caixa riscada, cabo gasto.

**Compatibilidade recomendada.** `security_machine, industrial_machine.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_20` — Conjunto de câmeras

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Cabeça formada por cluster de 2–4 módulos ópticos orientados próximos.

**Construção exata.** Suporte central, câmeras pequenas, cabos, anel de giro e pequenas proteções.

**Materiais e leitura de superfície.** Metal/plástico/vidro.

**Parâmetros procedurais permitidos.** 2–4 câmeras; direções ±20°; tamanhos; uma lente emissiva.

**Características obrigatórias.** Múltiplas lentes separadas em cluster.

**Desgaste, sujeira e dano.** Uma lente quebrada, poeira e cabos expostos.

**Compatibilidade recomendada.** `machine, experimental, surveillance.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_21` — Face lisa

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Placa frontal sem olhos/boca aparentes; direção vem da forma do casco ou luz mínima.

**Construção exata.** Máscara/carenagem contínua, poucas juntas, possível fenda microscópica.

**Materiais e leitura de superfície.** Metal/polímero/cerâmica.

**Parâmetros procedurais permitidos.** placa oval/angular; fenda 0–1; símbolo; material.

**Características obrigatórias.** Ausência deliberada de características faciais.

**Desgaste, sujeira e dano.** Riscos amplos, rachadura, tinta gasta.

**Compatibilidade recomendada.** `experimental, machine, elite.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_22` — Crânio com cabos

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Forma craniana exposta, orgânica ou mecânica, atravessada por cabos como nervos.

**Construção exata.** Crânio, cavidades oculares/sensores, 3–8 cabos saindo da nuca/têmporas e placas pequenas.

**Materiais e leitura de superfície.** Osso/carne/metal/cabos.

**Parâmetros procedurais permitidos.** cabos 3–10; olhos 0–2; placas 0–3; lado exposto; tubos.

**Características obrigatórias.** Crânio aparente + cabos integrados.

**Desgaste, sujeira e dano.** Manchas, ferrugem em grampos, isolamento rompido.

**Compatibilidade recomendada.** `biofusion, experimental, corrupted.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_23` — Máscara fundida

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Máscara e tecido facial parecem uma única superfície, sem borda limpa de vestir/remover.

**Construção exata.** Face deformada, placa/máscara parcialmente embutida, dobras orgânicas e fixações internas.

**Materiais e leitura de superfície.** Orgânico + metal/plástico.

**Parâmetros procedurais permitidos.** percentual fundido 40–90%; visor 0–2; filtros; assimetria.

**Características obrigatórias.** Transição contínua e perturbadora entre máscara e rosto.

**Desgaste, sujeira e dano.** Cicatriz, material rachado, fluido nas bordas.

**Compatibilidade recomendada.** `biofusion, experimental.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_24` — Cabeça inclinada

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Qualquer arquitetura-base montada com eixo do pescoço deslocado/inclinado permanentemente.

**Construção exata.** Conector de pescoço torto, suporte danificado ou tecido encurtado de um lado.

**Materiais e leitura de superfície.** Depende da cabeça-base.

**Parâmetros procedurais permitidos.** inclinação 8–25°; rotação 0–12°; cabos/tendões tensionados.

**Características obrigatórias.** Inclinação estrutural persistente, não animação momentânea.

**Desgaste, sujeira e dano.** Desgaste no lado comprimido, cabo tensionado.

**Compatibilidade recomendada.** `damaged, corrupted, salvaged.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_25` — Cabeça pequena

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Cabeça 70–80% da escala padrão, parcialmente cercada por ombros/torso.

**Construção exata.** Carcaça ou crânio compacto, pescoço curto, proteção ao redor.

**Materiais e leitura de superfície.** Variável.

**Parâmetros procedurais permitidos.** escala 65–85%; profundidade no torso; visor/sensor simples.

**Características obrigatórias.** Escala pequena e sensação de cabeça afundada.

**Desgaste, sujeira e dano.** Conforme material; sombra/sujeira na gola.

**Compatibilidade recomendada.** `heavy, compact, boss.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_26` — Cabeça alongada

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Volume vertical 120–170% da altura de uma cabeça padrão, largura reduzida.

**Construção exata.** Casco/crânio estreito, 2–4 segmentos verticais, sensor no terço superior ou médio.

**Materiais e leitura de superfície.** Metal, cerâmica, orgânico.

**Parâmetros procedurais permitidos.** altura; segmentos; sensor; topo plano/pontudo moderado.

**Características obrigatórias.** Proporção vertical extrema.

**Desgaste, sujeira e dano.** Riscos verticais e desgaste no topo.

**Compatibilidade recomendada.** `experimental, machine, biofusion.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_27` — Sensor em arco

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Linha curva/horizontal de múltiplos sensores sobre uma estrutura sem rosto convencional.

**Construção exata.** Arco metálico, 3–9 lentes pequenas, suporte central e cabos.

**Materiais e leitura de superfície.** Metal + vidro/emissivo.

**Parâmetros procedurais permitidos.** lentes 3–9; curvatura; uma lente dominante; cores.

**Características obrigatórias.** Faixa de sensores distribuídos em arco.

**Desgaste, sujeira e dano.** Lentes apagadas/rachadas, poeira entre módulos.

**Compatibilidade recomendada.** `machine, experimental, security_machine.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_28` — Capacete de contenção

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Casco pesado com presilhas, cintas e conectores que sugerem restrição/monitoramento.

**Construção exata.** Casco 2–4 placas, 3–8 travas, cabos ao pescoço e visor pequeno/nenhum.

**Materiais e leitura de superfície.** Metal, borracha, polímero.

**Parâmetros procedurais permitidos.** travas 3–10; cabos 1–5; visor; etiqueta; cadeado.

**Características obrigatórias.** Elementos de contenção visíveis e redundantes.

**Desgaste, sujeira e dano.** Riscos nas travas, ferrugem e marcas de pressão.

**Compatibilidade recomendada.** `laboratory, experimental, prisoner.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_29` — Visor rachado

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Cabeça-base com grande visor frontal danificado como característica principal.

**Construção exata.** Casco, visor 35–70% da face, 1–4 trincas principais e possível área faltante.

**Materiais e leitura de superfície.** Policarbonato/vidro + metal.

**Parâmetros procedurais permitidos.** padrão de trinca; opacidade; área perdida 0–20%; luz interna.

**Características obrigatórias.** Visor grande e dano legível.

**Desgaste, sujeira e dano.** Arranhões, poeira na rachadura, condensação.

**Compatibilidade recomendada.** `security, machine, damaged.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---

### `HEAD_30` — Sem cabeça convencional

**Slot estrutural:** topo do torso  
**Envelope de dimensão:** 8–18% da altura total  
**Ponto(s) de encaixe:** conector inferior de pescoço ou base mecânica; HEAD_30 usa conectores distribuídos no torso

**Silhueta e proporção.** Não há volume de cabeça acima do torso; sensores são embutidos em peito/ombros.

**Construção exata.** Placas superiores fecham o pescoço; 1–6 sensores distribuídos; possível antena curta.

**Materiais e leitura de superfície.** Metal/cerâmica/polímero.

**Parâmetros procedurais permitidos.** sensores 1–6; posição no torso; antena; emissivos; gola fechada.

**Características obrigatórias.** Ausência total de cabeça separada.

**Desgaste, sujeira e dano.** Riscos na tampa superior e sensores sujos.

**Compatibilidade recomendada.** `machine, experimental, boss.`

**Regra de implementação.** A cabeça/sensor é o ponto focal. Deve manter contraste suficiente contra o torso e possuir uma direção visual clara para indicar para onde o inimigo está olhando.

---


## 8. Equipamentos traseiros

### `BACK_01` — Sem mochila

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Costas permanecem limpas, com no máximo conectores fechados.

**Construção exata.** Placa traseira/tecido sem módulo saliente.

**Materiais e leitura de superfície.** Material do torso.

**Parâmetros procedurais permitidos.** tampa de conector; pequeno cabo 0–1.

**Características obrigatórias.** Nenhum volume traseiro relevante.

**Desgaste, sujeira e dano.** Conforme torso.

**Compatibilidade recomendada.** `light, fast, humanoid.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_02` — Mochila de ferramentas

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Módulo retangular macio/semirrígido, 20–35% da altura do torso.

**Construção exata.** Bolsa principal, 2–5 bolsos, alças e 1–4 ferramentas presas externamente.

**Materiais e leitura de superfície.** Tecido reforçado + metal.

**Parâmetros procedurais permitidos.** bolsos; ferramentas; assimetria; cor; correias.

**Características obrigatórias.** Leitura de mochila de manutenção.

**Desgaste, sujeira e dano.** Óleo, tecido gasto, ferramentas riscadas.

**Compatibilidade recomendada.** `worker, technical.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_03` — Rádio industrial

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Caixa rígida pequena/média com antena destacada.

**Construção exata.** Carcaça, grade, antena, botão/indicador e cabo até headset/torso.

**Materiais e leitura de superfície.** Metal/plástico.

**Parâmetros procedurais permitidos.** antena 1–2; comprimento; luz; grade; cabo.

**Características obrigatórias.** Antena + caixa de comunicação.

**Desgaste, sujeira e dano.** Riscos, poeira na grade, cabo gasto.

**Compatibilidade recomendada.** `security, technical, command.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_04` — Tanque de ar

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Cilindro vertical único preso por duas cintas.

**Construção exata.** Tanque, válvula superior, cintas, regulador e mangueira até cabeça/torso.

**Materiais e leitura de superfície.** Aço/alumínio + borracha.

**Parâmetros procedurais permitidos.** altura 30–70% do torso; cor; manômetro; proteção.

**Características obrigatórias.** Cilindro único e rota respiratória coerente.

**Desgaste, sujeira e dano.** Riscos longitudinais, tinta lascada, oxidação na válvula.

**Compatibilidade recomendada.** `worker, laboratory, technical.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_05` — Tanques duplos

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Dois cilindros paralelos simétricos ou levemente deslocados.

**Construção exata.** Dois tanques, suporte comum, coletor superior/inferior e 1–3 mangueiras.

**Materiais e leitura de superfície.** Aço/alumínio.

**Parâmetros procedurais permitidos.** tamanhos iguais/diferentes até 15%; coletor; válvulas; capa.

**Características obrigatórias.** Dois cilindros claramente separados.

**Desgaste, sujeira e dano.** Desgaste nas cintas, oxidação em válvulas.

**Compatibilidade recomendada.** `technical, heavy, chemical.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_06` — Bateria

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Bloco retangular compacto com contatos e cabos grossos.

**Construção exata.** Carcaça, células/painéis, indicador, fusível e 1–3 cabos de potência.

**Materiais e leitura de superfície.** Metal/plástico/cobre.

**Parâmetros procedurais permitidos.** tamanho; células 2–6; luz; cabo; alça.

**Características obrigatórias.** Leitura elétrica e cabos de alimentação.

**Desgaste, sujeira e dano.** Queima em terminais, poeira e arranhões.

**Compatibilidade recomendada.** `technical, machine, electric.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_07` — Gerador portátil

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Módulo médio/grande com motor/ventilação; altera bastante a silhueta traseira.

**Construção exata.** Carcaça, ventoinha/grade, bloco motor, alça, cabo de potência e escape pequeno.

**Materiais e leitura de superfície.** Aço pintado, cobre.

**Parâmetros procedurais permitidos.** ventoinha 1–2; grade; escape; tamanho 25–55% do torso; luz.

**Características obrigatórias.** Aparência de fonte de energia autônoma.

**Desgaste, sujeira e dano.** Fuligem no escape, poeira na grade, óleo inferior.

**Compatibilidade recomendada.** `machine, heavy, technical.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_08` — Unidade hidráulica

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Conjunto traseiro de bomba, reservatório e mangueiras grossas.

**Construção exata.** Reservatório, bomba, coletor, 2–6 mangueiras e suportes.

**Materiais e leitura de superfície.** Aço + borracha + fluido.

**Parâmetros procedurais permitidos.** mangueiras 2–8; reservatório 1–2; manômetro; cor.

**Características obrigatórias.** Rede hidráulica claramente alimentando membros.

**Desgaste, sujeira e dano.** Óleo, sujeira nos conectores, brilho nas mangueiras.

**Compatibilidade recomendada.** `hydraulic, heavy, industrial_machine.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_09` — Bobina de cabo

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Grande carretel preso vertical ou horizontalmente.

**Construção exata.** Disco lateral duplo, eixo, cabo enrolado e ponta solta/conector.

**Materiais e leitura de superfície.** Metal + cabo emborrachado.

**Parâmetros procedurais permitidos.** orientação; diâmetro; quantidade de cabo; ponta solta; manivela.

**Características obrigatórias.** Forma circular de bobina deve ser legível.

**Desgaste, sujeira e dano.** Bordas riscadas, cabo sujo e desgaste no eixo.

**Compatibilidade recomendada.** `technical, worker, support.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_10` — Reservatório químico

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Tanque protegido com indicação de risco e mangueiras.

**Construção exata.** Recipiente cilíndrico/retangular, visor de nível, tampa, válvula e 1–3 tubos.

**Materiais e leitura de superfície.** Metal/plástico técnico/policarbonato.

**Parâmetros procedurais permitidos.** fluido; forma; proteção; símbolo; tubos.

**Características obrigatórias.** Identidade química por tanque + sinalização + tubulação.

**Desgaste, sujeira e dano.** Manchas, descoloração, vazamento opcional.

**Compatibilidade recomendada.** `laboratory, contaminated, experimental.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_11` — Exaustor

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Tubos/saídas voltadas para cima ou para trás, dando silhueta vertical.

**Construção exata.** Coletor, 1–4 tubos, abraçadeiras e pequenas grades.

**Materiais e leitura de superfície.** Aço escurecido.

**Parâmetros procedurais permitidos.** tubos 1–4; altura; curvas; fumaça; proteção.

**Características obrigatórias.** Saídas de exaustão claramente abertas.

**Desgaste, sujeira e dano.** Fuligem intensa nas bocas e descoloração térmica.

**Compatibilidade recomendada.** `machine, heavy, heat.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_12` — Radiador

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Painel traseiro largo com aletas verticais/horizontais.

**Construção exata.** Moldura, 6–16 aletas, tubos laterais e ventoinha opcional.

**Materiais e leitura de superfície.** Alumínio/aço/cobre.

**Parâmetros procedurais permitidos.** aletas; largura; ventoinha 0–2; tubos; deformação.

**Características obrigatórias.** Padrão repetido de aletas deve dominar.

**Desgaste, sujeira e dano.** Poeira nas aletas, amassados e oxidação leve.

**Compatibilidade recomendada.** `machine, heat, technical.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_13` — Mochila militar

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Módulo compacto de tecido, mais organizado que BACK_02.

**Construção exata.** Bolsa principal, 2–4 bolsos, tiras MOLLE simplificadas e antena/kit opcional.

**Materiais e leitura de superfície.** Tecido tático.

**Parâmetros procedurais permitidos.** bolsos; rolo/saco; antena; cor; tiras.

**Características obrigatórias.** Leitura militar compacta.

**Desgaste, sujeira e dano.** Poeira, bordas gastas e rasgos pequenos.

**Compatibilidade recomendada.** `security, military.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_14` — Módulo de sensores

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Plataforma com antenas, câmeras ou discos orientáveis.

**Construção exata.** Base rígida, 2–6 sensores, 1–3 antenas e cabos ao torso.

**Materiais e leitura de superfície.** Metal/plástico/vidro.

**Parâmetros procedurais permitidos.** sensores; antenas; prato pequeno 0–1; luzes; assimetria.

**Características obrigatórias.** Densidade alta de componentes de leitura/sensoriamento.

**Desgaste, sujeira e dano.** Lentes sujas, antena torta, cabo exposto.

**Compatibilidade recomendada.** `support, machine, experimental.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_15` — Estrutura externa

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Armação tipo gaiola/exoesqueleto sobe pelas costas e pode conectar ombros/quadril.

**Construção exata.** 2 hastes verticais, travessas, suportes laterais e pontos de montagem vazios/ocupados.

**Materiais e leitura de superfície.** Aço/alumínio.

**Parâmetros procedurais permitidos.** hastes 2–6; largura; travessas; pequenos módulos; assimetria.

**Características obrigatórias.** Armação aberta e visível ao redor do torso.

**Desgaste, sujeira e dano.** Riscos, soldas e graxa nos pontos de montagem.

**Compatibilidade recomendada.** `technical, machine, salvaged.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_16` — Caixa de manutenção

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Caixa rígida com portas/gavetas, como kit de ferramentas acoplado.

**Construção exata.** Carcaça retangular, tampa, 1–3 gavetas, fechos e ferramentas pequenas.

**Materiais e leitura de superfície.** Metal/plástico.

**Parâmetros procedurais permitidos.** gavetas abertas/fechadas; ferramentas; cor; número; alça.

**Características obrigatórias.** Geometria de caixa de serviço.

**Desgaste, sujeira e dano.** Bordas batidas, tinta perdida e graxa.

**Compatibilidade recomendada.** `technical, worker.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_17` — Núcleo experimental

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Módulo energético visualmente incomum, com emissivo ou contenção.

**Construção exata.** Câmara central, aro/grade de contenção, cabos, isoladores e pequenas luzes.

**Materiais e leitura de superfície.** Metal/cerâmica/vidro + emissivo.

**Parâmetros procedurais permitidos.** forma circular/hexagonal; cor; 2–8 cabos; proteção 20–70%.

**Características obrigatórias.** Fonte energética deve ser o ponto focal traseiro.

**Desgaste, sujeira e dano.** Queima, rachadura, vazamento de luz, arcos opcionais.

**Compatibilidade recomendada.** `experimental, boss, machine.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_18` — Massa biomecânica

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Volume orgânico irregular aderido às costas, atravessado por tubos/cabos.

**Construção exata.** 2–5 lobos orgânicos, fibras, placas implantadas e 1–5 tubos.

**Materiais e leitura de superfície.** Carne/tecido mutado + metal.

**Parâmetros procedurais permitidos.** volume 20–60% do torso; assimetria; tubos; pulsação visual opcional.

**Características obrigatórias.** Crescimento integrado ao corpo, não mochila removível.

**Desgaste, sujeira e dano.** Fluido, cicatriz, áreas necrosadas.

**Compatibilidade recomendada.** `biofusion, corrupted.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_19` — Motor reaproveitado

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Motor industrial visível montado externamente, com carcaça incompleta.

**Construção exata.** Bloco motor, polias/ventoinha, tubos, suporte soldado e cabo/escape.

**Materiais e leitura de superfície.** Aço, cobre, sucata.

**Parâmetros procedurais permitidos.** motor compacto/largo; polia 0–2; ventoinha; escape; correia.

**Características obrigatórias.** Leitura de motor real reaproveitado.

**Desgaste, sujeira e dano.** Óleo pesado, ferrugem, fuligem e tinta velha.

**Compatibilidade recomendada.** `salvaged, industrial_machine, heavy.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---

### `BACK_20` — Reservatório rachado

**Slot estrutural:** costas do torso  
**Envelope de dimensão:** 8–35% da altura total  
**Ponto(s) de encaixe:** um ou dois pontos traseiros; módulos pesados exigem suporte inferior adicional

**Silhueta e proporção.** Tanque danificado com fissura/abertura visível e conteúdo vazando ou seco.

**Construção exata.** Recipiente, 1–3 rachaduras, proteção parcial, válvula e tubo rompido opcional.

**Materiais e leitura de superfície.** Metal/plástico/vidro + fluido.

**Parâmetros procedurais permitidos.** rachadura 1–4; vazamento; nível do fluido; fita/remendo; forma.

**Características obrigatórias.** Dano no reservatório deve alterar sua leitura estrutural.

**Desgaste, sujeira e dano.** Escorrimento, corrosão química, bordas quebradas.

**Compatibilidade recomendada.** `damaged, experimental, laboratory, salvaged.`

**Regra de implementação.** O módulo traseiro deve alterar a silhueta lateral e traseira sem encobrir completamente cabeça e ombros. Tubos e cabos devem seguir rotas coerentes até o componente alimentado.

---


## 9. Acessórios médios — overlays estruturais

Acessórios não substituem slots principais. Eles devem possuir ponto físico de fixação e nunca atravessar uma articulação sem folga ou conexão coerente.


### `ACC_01` — Cabo cruzando o peito

1 cabo de 2–5% da largura do torso, preso em 2–4 grampos; pode ligar ombro, cintura ou BACK.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_02` — Mangueira torso→braço

Mangueira grossa com curva ampla, dois conectores visíveis e folga suficiente para cotovelo/ombro.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_03` — Corrente curta

4–10 elos simplificados, pendendo de cinto/ombro; não deve atravessar articulação animada.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_04` — Cinto de munição

Sequência de 8–20 cartuchos/blocos, atravessando peito ou cintura; peso visual direcionado à arma alimentada.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_05` — Kit médico quebrado

Caixa pequena com símbolo, tampa torta e 1–3 itens expostos; apropriado a laboratório/segurança.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_06` — Ferramentas penduradas

1–4 ferramentas de silhuetas distintas; sempre presas por cinto, argola ou bolsa.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_07` — Lanterna

Cilindro curto com lente e suporte; 2–6% da altura do torso; pode emitir luz limitada.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_08` — Painel de controle

Placa 8–20% da área do torso com 2–6 botões/luzes e uma borda clara.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_09` — Crachá

Retângulo pequeno com faixa, foto abstrata e 1–2 linhas; usado como detalhe de proximidade, não leitura à distância.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_10` — Número pintado

1–3 dígitos grandes, alto contraste; ocupa 10–30% de uma placa principal.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_11` — Símbolo de perigo

Triângulo/faixa industrial simples em área plana; no máximo um símbolo dominante por peça.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_12` — Faixa refletiva

Faixa horizontal/diagonal de 3–8% da dimensão da peça; amarelo, branco ou laranja dessaturado.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_13` — Trava metálica

Pequeno fecho retangular com pino; 2–6 unidades em tampas, cintas ou carcaças.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_14` — Cobertura rasgada

Tecido/borracha sobre mecanismo, com rasgo revelando 10–35% da estrutura interna.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_15` — Tubos no pescoço

1–4 tubos curtos da nuca/gola para cabeça; curvas não devem ocultar sensor principal.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_16` — Cabos na coluna

2–6 cabos verticais seguindo o centro/lados das costas, presos por grampos.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_17` — Válvula lateral

Volante de 3–6 raios, diâmetro 5–12% do torso; ligado a tubo ou reservatório.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_18` — Ventoinha pequena

Disco 6–15% do torso, 3–8 pás e grade opcional; alojada em placa plana.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_19` — Antena

Haste fina de 8–30% da altura da cabeça/torso; base rígida; pode inclinar levemente.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_20` — Luz de emergência

Beacon cilíndrico/retangular, 3–8% da altura do torso, cor única emissiva.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_21` — Gancho

Gancho metálico de 5–15% da altura do torso, preso a cabo/corrente/suporte.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_22` — Cintas de contenção

1–4 tiras largas cruzando peça orgânica ou tampa; fivelas grandes e tensão visível.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_23` — Placa de identificação

Chapa pequena aparafusada, 1–2 linhas abstratas e número; metal mais limpo que superfície-base.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_24` — Suporte para ferramenta

Trilho, argola ou presilha vazia/ocupada; deve ter ponto físico de fixação.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_25` — Caixa elétrica na cintura

Módulo 8–18% do torso com tampa, cabo e pequena luz/fusível.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_26` — Amostra de laboratório

Tubo/ampola transparente de 3–10% da altura do torso, protegido por suporte.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_27` — Placa improvisada

Chapa irregular adicional, 8–30% da superfície local, fixada por solda/rebites.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_28` — Bolsa técnica

Bolsa semirrígida pequena, 8–18% do torso, com aba e 1–2 fechos.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_29` — Colete parcialmente rasgado

Camada têxtil com 1–3 rasgos grandes revelando base inferior; bordas desfiadas.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---

### `ACC_30` — Correia solta

Tira estreita pendente de 8–25% da altura da peça; um único ponto de fixação visível.

**Variação permitida:** escala ±20%, posição dentro da região válida e estado de conservação. **Não permitido:** aplicar sem suporte físico ou atravessar partes móveis.

---


## 10. Microdetalhes procedurais

Microdetalhes são camadas de acabamento. Eles não devem ser usados para “forçar” variedade quando a silhueta e as peças principais estão repetidas.


### `DET_01` — Arranhões horizontais

2–8 linhas de 3–20% da largura local; maior contraste no centro e falhas nas extremidades.

---

### `DET_02` — Arranhões verticais

1–6 riscos acompanhando gravidade/contato; evitar distribuição uniforme.

---

### `DET_03` — Bordas gastas

Aplicar apenas em arestas expostas: 10–45% do perímetro, 1–2 tons mais claros/metal nu.

---

### `DET_04` — Ferrugem pontual

Manchas pequenas perto de parafusos, juntas e lascas; 2–10 focos.

---

### `DET_05` — Ferrugem escorrida

Traços verticais abaixo de parafusos/fendas, comprimento 5–25% da peça.

---

### `DET_06` — Óleo

Mancha escura semitransparente perto de junta, bomba ou vazamento; borda irregular e centro mais escuro.

---

### `DET_07` — Graxa

Faixa escura concentrada em eixo/engrenagem, menor área e maior densidade que óleo.

---

### `DET_08` — Poeira

Gradiente acumulado nas superfícies voltadas para cima e no terço inferior de pernas.

---

### `DET_09` — Fuligem

Nuvem preta/marrom irradiando de escape, fogo ou área queimada; densidade diminui com distância.

---

### `DET_10` — Sangue seco

Manchas orgânicas escuras localizadas; nunca usar como ruído global.

---

### `DET_11` — Fluido industrial

Escorrimento colorido ligado a mangueira, válvula ou reservatório; caminho físico obrigatório.

---

### `DET_12` — Vazamento emissivo

Pequena linha/pingos luminosos saindo de rachadura ou núcleo; baixa área total.

---

### `DET_13` — Rebites

Cabeças circulares de tamanho consistente, alinhadas em bordas; espaçamento 1.5–3 diâmetros.

---

### `DET_14` — Parafusos

Cabeças circulares/hexagonais em pontos estruturais; 2–12 por painel.

---

### `DET_15` — Solda irregular

Cordão ondulado ao longo de junção; largura 1–3% da placa; pode ter calor residual na cor.

---

### `DET_16` — Fita adesiva

Faixa opaca cruzando rachadura/cabo; 1–3 voltas; bordas ligeiramente desalinhadas.

---

### `DET_17` — Costura

Linha pontilhada regular sobre tecido, seguindo bordas e painéis; contraste baixo.

---

### `DET_18` — Rasgo

Abertura irregular que revela camada inferior; 5–30% da área local.

---

### `DET_19` — Tecido queimado

Borda escura com centro retraído; transição marrom→preto; revela base em alguns pontos.

---

### `DET_20` — Metal amassado

Deformação de placa indicada por contorno/sombra local; 1–3 amassados grandes, não ruído.

---

### `DET_21` — Placa rachada

Fissura ramificada partindo de impacto/borda; 1 linha principal + 1–4 secundárias.

---

### `DET_22` — Tinta descascada

Ilhas de metal nu concentradas em bordas/contato; cobertura 5–35% conforme estado.

---

### `DET_23` — Numeração apagada

Número/símbolo parcialmente removido; 30–70% dos pixels da marca ainda visíveis.

---

### `DET_24` — LED

Ponto/quadrado emissivo de 1–3% da dimensão da peça, com halo mínimo.

---

### `DET_25` — Fio solto

Cabo fino saindo de abertura e terminando livre; comprimento 5–20% da peça.

---

### `DET_26` — Faísca ocasional

Efeito animado curto em fio/junta danificada; não faz parte permanente da textura-base.

---

### `DET_27` — Vapor

Partícula/efeito animado saindo de válvula/fissura; origem física definida.

---

### `DET_28` — Gotejamento

Sequência animada ou trilha vertical de fluido sob vazamento.

---

### `DET_29` — Remendo

Pequena placa/tecido sobre área danificada, com material ou cor diferente da base.

---

### `DET_30` — Marca de impacto

Cratera/lasca focal com centro escuro e borda clara; 1–6 por placa conforme estado.

---


## 11. Materiais

### `MAT_01` — Tecido industrial

Trama grossa sugerida por 1–2 níveis de ruído; baixo brilho; vincos nas articulações.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_02` — Tecido tático

Superfície mais uniforme e escura; trama quase invisível; desgaste nas bordas e joelhos.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_03` — Couro/borracha

Valor escuro, reflexo estreito nas curvas e rachaduras em áreas velhas.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_04` — Aço pintado

Cor-base sólida, sombra fria, bordas revelando aço claro e pontos de ferrugem.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_05` — Aço exposto

Cinza frio com variação sutil; riscos mais claros e óleo escuro.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_06` — Ferro oxidado

Base marrom escura, laranja em áreas ativas e cinza onde abrasão remove ferrugem.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_07` — Alumínio

Cinza claro, baixa ferrugem, riscos finos e manchas opacas.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_08` — Chapa galvanizada

Cinza claro com manchas/marmorização grandes, sem ferrugem intensa.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_09` — Plástico industrial

Cor uniforme, bordas esbranquiçadas por abrasão e rachaduras secas.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_10` — Policarbonato

Translúcido, reflexos duros, riscos claros e opacidade nas bordas.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_11` — Borracha técnica

Preto/cinza muito escuro, quase sem brilho, poeira clara acumulada.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_12` — Cerâmica técnica

Clara e fosca; rachaduras nítidas; borda quebrada revela interior semelhante.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_13` — Cobre

Laranja/marrom metálico; pode oxidar para verde em áreas úmidas.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_14` — Cabos emborrachados

Revestimento escuro com highlight fino e cobre exposto apenas em dano.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_15` — Carne pálida

Base bege/cinza-rosa, sombras frias, pequenas variações vasculares localizadas.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_16` — Carne carbonizada

Marrom muito escuro/preto, fissuras e pouco brilho úmido em áreas recentes.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_17` — Tecido orgânico mutado

Volumes irregulares com fibras, veias e transições de cor não uniformes.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_18` — Material experimental emissivo

Superfície escura/neutra com regiões emissivas controladas; emissão nunca cobre toda a peça.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_19` — Sucata multi-material

2–4 materiais em placas diferentes; cada subpeça mantém sua própria lógica de desgaste.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---

### `MAT_20` — Blindagem composta

Placas foscas, bordas chanfradas, pouco reflexo e impactos que não revelam ferrugem imediatamente.

**Aplicação:** gerar primeiro a cor-base e o volume; só depois aplicar desgaste e estado. O material controla como luz, sujeira e dano aparecem.

---


## 12. Estados de conservação

### `COND_01` — Novo

0–5% de desgaste; superfícies limpas; apenas marcas de fabricação e juntas.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_02` — Usado

5–15% de bordas gastas, 1–4 riscos e sujeira leve em articulações.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_03` — Muito usado

15–30% de desgaste, múltiplos riscos, tinta perdida em contato e pequenas deformações.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_04` — Sujo

Poeira/graxa cobre 10–35% de áreas semanticamente plausíveis; geometria intacta.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_05` — Oleoso

Óleo concentrado em 2–6 regiões mecânicas; brilho e escurecimento local.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_06` — Enferrujado

Ferrugem em 10–30% do metal, principalmente parafusos, juntas e lascas.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_07` — Muito oxidado

30–65% de corrosão, furos/escamas ocasionais e bordas degradadas.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_08` — Queimado

Fuligem, tinta carbonizada e descoloração térmica cobrindo 25–70% conforme material.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_09` — Remendado

1–4 reparos estruturais visíveis usando material diferente; soldas/fitas/grampos.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_10` — Danificado

1–3 deformações estruturais, placas rachadas, visor quebrado ou membro desalinhado dentro dos limites de animação.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_11` — Parcialmente destruído

20–50% de uma região revela interior ou perdeu cobertura; cabos/componentes expostos.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_12` — Vazando

Ao menos um sistema de fluido possui origem, trilha e acúmulo; vazamento pode ser animado.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_13` — Corrompido

5–45% da peça recebe tecido/crescimento orgânico ou distorção; respeitar origem biofusion.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_14` — Experimental

Forma/material pode quebrar padrões industriais, mas mantém conectores e função legíveis.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_15` — Contaminado

Manchas químicas/resíduos, selos degradados e descoloração; sem mutação obrigatória.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---

### `COND_16` — Abandonado

Combina poeira pesada, oxidação e ausência de manutenção; baixo brilho e peças travadas/velhas.

**Regra:** estado modifica uma peça existente; não deve substituir a estrutura-base nem apagar seus pontos de encaixe.

---


## 13. Matriz de decisão para o gerador

Para gerar uma peça, usar a sequência:

```text
ID estrutural
  → perfil corporal
  → escala dentro do envelope
  → variante de subcomponentes
  → material primário
  → material secundário
  → estado de conservação
  → acessórios válidos
  → microdetalhes localizados
  → validação de conectores
  → validação de silhueta
  → assinatura anti-repetição
```

### Critério de identidade

Dois inimigos não devem ser considerados diferentes apenas porque usam cores, ferrugem ou números diferentes. A geração deve priorizar diferença em: perfil, torso, cabeça, membros, módulos traseiros e proporções.

### Critério de rejeição visual

Se o novo inimigo compartilhar com um inimigo recente o mesmo torso, mesma cabeça e pelo menos dois membros principais, o gerador deve preferencialmente refazer a montagem antes de alterar somente acabamento.

### Critério para Boss

Bosses usam a mesma biblioteca, mas podem:

- combinar 2 módulos traseiros compatíveis;
- usar versões 120–180% de braços pesados;
- reservar um torso ou perfil de massa extrema;
- adicionar submódulos exclusivos;
- manter conectores compatíveis com a mesma lógica procedural.

Isso evita que o Boss precise ser um modelo completamente separado do sistema.

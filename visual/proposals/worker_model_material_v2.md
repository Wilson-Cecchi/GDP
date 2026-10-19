# Worker — modelo e materiais v2

**Estado: IMPLEMENTADA EM VALIDAÇÃO — variante A laranja.** Proposta
conceitual de 16/10/2026, aprovada pelo usuário em 16/10/2026 com
"gostei desse laranja no final das contas, pode implementar".
Nenhuma das imagens abaixo é captura do executável, textura carregada pelo
jogo ou prova de desempenho. A base usada foi a captura nativa offscreen
`visual/validation/medium_worker_seed101_near_idle.png`; a edição por geração
de imagem alterou também a geometria do personagem.

## Implementação e validação técnica

O Worker v2 mantém o corpo quadrado e as oito partes animadas, com placas
laranjas, juntas grafite, faixa de visor, ferramenta pesada e fixadores escuros.
A pintura geral é o bitmap
`assets/visual/worker_painted_steel_v2.png`, criado com `image_gen` a partir
da prancha aprovada; o painel frontal amostra pixels da própria prancha em
`assets/visual/worker_front_panel_source_v2.png`. A iluminação em tempo real
altera o brilho aparente, e a referência só mostra a frente. O shader escurece
o contato entre placas e acrescenta metal exposto nas arestas pintadas.

As capturas técnicas estão em
`visual/validation/worker_model_material_v2_review/`, geradas na Quadro
K2100M com seeds 101, 202 e 303 em MEDIUM/POTATO, perto/longe. O teste
`enemy_generation_systems` passou em 1.000 seeds sem fallback; os testes
OpenGL de pixels e instancing também passaram. São 20 draws e 41 primitivas
na receita visual por Worker; o halo do olho acrescenta um draw no renderer.
O centro do visor agora possui emissão âmbar independente da luz da sala e um
halo discreto. Capturas técnicas sem retículo sobre o olho estão em
`visual/validation/worker_eye_glow_clear/` e
`visual/validation/worker_eye_glow_dark/`, feitas na Quadro K2100M com luz
normal e baixa. As duas capturas da arena são do executável real, mas o
Worker ficou distante ou atrás de obstáculos; elas não permitem aprovar o
detalhe visual em partida. O usuário testou o Worker e o considerou "muito bom
até o momento"; o brilho novo do olho ainda pede revisão em jogo.

## Motivo e escopo

O Worker atual preserva a silhueta quadrada e a variação de membros aprovada
na v1, mas as partes compartilham quase o mesmo acabamento. De longe, visor,
ferramenta e juntas não se distinguem com clareza. O usuário considerou belo
o **novo modelo conceitual** e liberou explorar cores além das atuais. Depois
escolheu a variante A laranja e autorizou sua implementação no jogo.

Esta unidade reúne o desenho do **Worker/modelo v2** e sua família de
**materiais v2**. Ela não altera Guard, Officer, Engineer, Robot, armas, IA,
colisão, hitboxes, alcance ou dano. As proporções variáveis por seed do Worker
devem continuar visíveis; as imagens são apenas uma referência frontal de um
indivíduo.

## Proposta de forma

- Preservar o corpo de caixas e a leitura retro industrial. A cabeça cilíndrica
  simples recebe uma faixa de visor escura e estreita; o peito vira um painel
  largo de aço pintado com borda e fixadores grandes.
- Separar visualmente braços, quadril e pernas por juntas grafite. Luvas/botas
  continuam blocos, com cobertura suficiente para não parecer peças soltas.
- A ferramenta permanece pesada e retangular, de aço escuro. Sua posição e
  contato com a mão devem acompanhar a variação de comprimento do braço.
- Evitar microdetalhes que desapareçam no perfil POTATO ou prometam um alcance
  de ataque/hitbox diferente do atual.

O conceito adiciona painel, visor e segmentação que **não existem nessa forma
na malha atual**. Implementar a v2 exigirá adaptar a receita e a distribuição
de materiais; não basta trocar uma cor no atlas existente.

## Três direções de paleta

| Variante | Cor principal | Cor de função | Leitura pretendida |
| --- | --- | --- | --- |
| A — segurança industrial | laranja de aço pintado | juntas grafite e visor âmbar | continuidade com o Worker atual |
| B — manutenção de campo | azul petróleo | painel cobre, juntas grafite e visor claro | separação das faixas amarelas do cenário |
| C — unidade recuperada | marfim industrial | painel vermelho, juntas grafite e visor âmbar | contraste alto em salas escuras |

As três usam o mesmo modelo, acabamento fosco e desgaste localizado. **Somente
a variante A foi aprovada para implementação.** As variantes B e C continuam
como estudos conceituais. Cores
exatas são direção artística, não valores aprovados nem amostras medidas no
renderer. A variante B requer comparação cuidadosa com o Engineer, que já
pode usar azul/verde; a C requer comparação com iluminação clara e paredes
cinzas. Nenhuma variante deve virar uma classe diferente de inimigo.

### Pranchas conceituais

- [A — laranja, usado](worker_model_material_v2_orange_used.png)
- [B — azul petróleo, usado](worker_model_material_v2_petrol_used.png)
- [C — marfim/vermelho, usado](worker_model_material_v2_bone_used.png)
- [A/B/C na distância de combate simulada](worker_model_material_v2_palette_gameplay_concept.png)

Na variante A, [conservado](worker_model_material_v2_orange_maintained.png),
[usado](worker_model_material_v2_orange_used.png) e
[gasto](worker_model_material_v2_orange_worn.png) demonstram três níveis de
desgaste. A vista distante também foi **gerada como conceito**; não valida
legibilidade real no jogo. As imagens não têm seeds procedurais reproduzíveis.

## Contrato para implementação após aprovação

1. Escolher uma direção cromática ou uma combinação explícita. Traduzir a
   proposta para uma receita procedural versionada, derivada apenas de
   `visualSeed`; manter três estados de desgaste com regiões coerentes, sem
   sortear detalhes a cada frame.
2. Montar pelo menos três variantes reais de seed, com vista próxima, distância
   de combate e iluminação neutra. Verificar a identidade do Worker ao lado dos
   outros quatro arquétipos, em POTATO e MEDIUM.
3. Preservar animação, pontos de contato da ferramenta, pés no chão,
   colisão/hitboxes e determinismo. Medir custo de malha/textura e tempo de
   quadro na Quadro K2100M antes/depois em cenário equivalente.
4. Capturar dentro da partida e pedir aceitação visual final separadamente dos
   testes automatizados e da aprovação desta proposta.

Arquivos de implementação prováveis: `src/game/visual/EnemyGenerator.cpp/.hpp`,
`src/game/visual/GdpRenderer.cpp/.hpp` e testes de geração/renderização.
`GdpRenderer.cpp` permanece sob reserva do coordenador. Outras famílias de
inimigos exigirão propostas próprias.

## Prompts e origem das imagens

Ferramenta: `image_gen` embutida. A imagem A/Usado foi editada a partir da
captura offscreen do Worker seed 101 em MEDIUM, solicitando placas industriais,
juntas escuras, visor legível e ferramenta pesada; a ferramenta mudou a forma
apesar da instrução de preservá-la. A/Conservado e A/Gasto foram editadas a
partir de A/Usado, variando apenas desgaste. B e C foram editadas de A/Usado,
solicitando as paletas descritas acima e preservação de forma e cenário. A
prancha distante foi gerada usando A, B e C como referências de composição.
Nenhuma dessas imagens deve ser apresentada como resultado do gerador atual.

Conjunto final de instruções artísticas usado nas edições: "mudar somente o
Worker da captura; manter sala, câmera e pose; corpo cuboide industrial;
painel amplo de aço pintado, juntas grafite, visor estreito legível e ferramenta
retangular pesada; três direções de paleta (laranja, azul petróleo/cobre,
marfim/vermelho); desgaste concentrado em arestas; formas grandes legíveis a
distância; sem texto, rosto, anatomia realista ou peças extras". As edições
Conservado/Gasto pediram, respectivamente, pintura quase íntegra e pintura
gasta com aço exposto nas bordas. A vista de comparação pediu três cópias do
mesmo modelo lado a lado, em distância de combate simulada e luz neutra.

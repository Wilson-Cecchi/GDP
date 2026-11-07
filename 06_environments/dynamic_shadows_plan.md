# Proposta técnica — sombras dinâmicas pela geometria

> Estado em 06/11/2026: o piloto do Central Hub foi aprovado e o caminho de
> sombras por geometria foi integrado às salas e corredores da campanha nos
> perfis MEDIUM, HIGH e ULTRA. MEDIUM usa uma luminária com mapa; HIGH e ULTRA
> usam todas as luminárias ativas da sala, até oito. POTATO e LOW preservam o
> caminho leve. A opção Sombras pode ser desligada no perfil Personalizado. As
> capturas de outras salas são validação técnica; falta playtest visual da
> integração em campanha antes de considerar esta fase aprovada.

## Preenchimento físico aprovado para as salas

Em 07/11/2026, após a aprovação do piloto no Central Hub, o gerador passou a
acrescentar uma luminária fraca a cada plano de sala, respeitando o limite de
cinco luzes. Salas pequenas usam o centro como ponto de preenchimento; salas
amplas usam um ponto lateral. A Segurança em T posiciona a quarta luminária
entre as duas da galeria. Cada fonte cria um objeto de teto destrutível. A
luz ambiente considera a potência de cada fonte, para a quebra da luminária
fraca ter efeito menor que a quebra de uma luminária principal.

Os corredores continuam recebendo as luzes e o ambiente das salas conectadas;
eles ainda não possuem luminárias próprias. O orçamento por perfil limita as
fontes processadas: POTATO 2, LOW 4, MEDIUM 6 e HIGH/ULTRA 8. Assim, o
preenchimento adicional de salas com cinco fontes aparece integralmente em
MEDIUM, HIGH e ULTRA. A aprovação é da direção visual do piloto; a distribuição
das luminárias em todos os formatos requer verificação em partida.

As prévias nativas de Storage, Segurança em T, Boss, Técnica em L e corredor
iniciaram e terminaram sem erro na Quadro K2100M com `--seed 20261107` e o
perfil local Personalizado (2048 × 1152, oito luzes e oito sombras, VSync).
Em 90 quadros, a média de GPU foi 20,84 ms no Storage, 18,82 ms na Segurança
em T e 26,01 ms no Boss. Em 60 quadros, foi 18,21 ms na Técnica em L e
28,30 ms no corredor. São amostras curtas de cenas diferentes, sem comparação
controlada com a versão anterior nem aprovação de fluidez em partida.

## Piloto no Central Hub

O piloto usa uma luminária fixa do Hub e um cubemap de profundidade de
256 px por face em MEDIUM ou 512 px em HIGH/ULTRA. A malha octogonal da
coluna, as peças do Worker na pose renderizada e a barricada de teste projetam
sombra. POTATO/LOW mantêm o caminho leve. A cena de comparação
`--central-hub-shadow-legacy-preview` tem o mesmo inimigo e barricada, mas
usa as sombras anteriores. O mapa é consumido no quadro seguinte ao desenho
dos modelos; a latência de um quadro permanece como limite conhecido para
objetos em movimento rápido.

Na Quadro K2100M em MEDIUM, 1920 × 1080, VSync ligado, seed `20261106`,
240 quadros, antes do ajuste final da seleção de luminária, o log registrou
11,78 ms de GPU para a cena com sombra anterior e 12,12 ms para o piloto.
Essas duas execuções são uma amostra curta;
houve diferença no número de atualizações fixas e uma interação de tiro na
segunda execução. A captura OpenGL comparou a mesma câmera e cena e encontrou
102.354 pixels alterados entre as duas técnicas após fixar a luminária,
sem erro OpenGL. O teste
`renderer_opengl_pixels` passou. As capturas do piloto ficam em
`visual/validation/central_hub_shadow_pilot_v1/`.

## Integração nas salas

O renderer usa o mapa de profundidade em toda sala renderizada quando o perfil
é MEDIUM ou superior. Móveis sólidos, suas peças 3D, colunas, paredes, portas
na posição atual, caixas e outros props, além das peças animadas dos inimigos,
entram na lista de emissores. Inimigos e props da sala do jogador continuam na
lista mesmo quando saem da câmera, para que sua sombra apareça no piso visível.
O passe de cor e o de profundidade compartilham as matrizes da pose. Caixas
repetidas são instanciadas no passe de sombra. Em MEDIUM, a luz ativa mais
próxima da câmera é selecionada com retenção da anterior enquanto ela continuar
próxima, evitando troca de sombra a cada pequena mudança de posição. Em HIGH e
ULTRA, o orçamento é de oito mapas de profundidade, cobrindo todas as luzes
ativas permitidas na sala. O perfil Personalizado permite mudar a opção Sombras,
o limite de mapas, a escala de renderização e os materiais detalhados.

Destruir um objeto
ou mover uma porta atualiza a lista no quadro seguinte, junto com a latência
do mapa. Os perfis POTATO e LOW mantêm a sombra analítica existente.

Para comparar a mesma prévia com a técnica anterior, usar `--legacy-shadows`.
O limite de uma luz descrito nas medições abaixo pertence à versão inicial da
integração. A correção com múltiplas luzes está registrada em
`visual/validation/global_shadows_v2/`.
As capturas técnicas em `visual/validation/global_shadows_v1/` cobrem Storage,
Cafeteria, Segurança e Laboratório, com FOV 60 e ULTRA. O teste OpenGL capturou
também FOV 100 e POTATO em cada variante; os BMP temporários foram removidos
após a inspeção. As quatro comparações da variante inicial com o caminho
anterior alteraram respectivamente 164.175, 20.005, 7.461 e 62.168 pixels,
sem erro OpenGL. Esses números indicam mudança de imagem, não qualidade visual.

Na Quadro K2100M, Storage ULTRA 1920 × 1080 com VSync e seed `20261106`,
240 quadros: caminho anterior 21,34 ms de GPU e novo caminho 11,12 ms em
execuções curtas. O P95 do quadro foi 41,79 ms e 45,40 ms, respectivamente;
o VSync e o número diferente de atualizações fixas impedem tratar a diferença
como ganho comprovado. Laboratório, no novo caminho, registrou 10,81 ms de GPU
e P95 de 42,70 ms com o mesmo número de quadros. Mais amostras em gameplay real
são necessárias para definir orçamento definitivo.

## Problema observado

O renderer atual testa raios contra caixas (`Aabb`) no fragment shader. A
`Application` oferece caixas de inimigos e de alguns props visíveis, além da
coluna central; os móveis modelados nas `RoomArchitecturePart` não entram como
malhas. Há no máximo duas luzes com sombra e oito caixas ativas. Por isso a
sombra de um inimigo é quadrada mesmo quando a modelagem mostra braços, pernas,
arma e uma pose diferente a cada quadro. Acrescentar todas as caixas dos móveis
saturaria esse limite sem resolver a silhueta.

O filtro atual de inimigos e props segue a câmera. Um objeto fora da imagem
pode projetar sombra dentro dela; a seleção futura de emissores de sombra deve
considerar o volume de cada luz, além da área visível pela câmera.

## Resultado desejado

- Sombra projetada pela **malha 3D e pela pose do quadro atual** de cada
  inimigo, inclusive braços, pernas, armas e animações de ataque ou morte.
- Móveis, caixas, colunas, portas e outros objetos opacos podem projetar e
  receber sombra. Um objeto quebrado deixa de projetar a sombra intacta.
- Ao apagar uma luminária, sua contribuição e sua sombra desaparecem juntas.
- O resultado respeita os perfis gráficos e permanece jogável na Quadro K2100M.
  Sombras de contato locais podem continuar nos perfis mais leves.

## Arquitetura proposta

1. Criar no renderer um passe de profundidade para as luzes selecionadas. Como
   as luminárias atuais são *point lights*, uma sombra em todas as direções
   exige um mapa de profundidade em cubo. Começar com uma luz próxima e mapas
   pequenos; medir antes de ampliar o orçamento. O recurso OpenGL reutilizável
   pertence a `src/render`, enquanto seleção de luzes e desenho da cena ficam
   no renderer do jogo.
2. Construir uma lista de emissores a partir das **mesmas malhas e matrizes**
   usadas no passe de cor. Para inimigos, calcular a pose uma vez e reutilizar
   `rootModel * partModel` no passe de profundidade. Armas sólidas entram;
   halos de olhos, fumaça e o modelo de arma em primeira pessoa não entram.
3. Separar geometria estática de salas e móveis da geometria móvel. Reutilizar
   o mapa estático enquanto nada mudar; invalidá-lo quando portas, móveis
   quebráveis ou luminárias mudarem de estado. Atualizar as poses móveis a cada
   quadro. A cache só entra após o primeiro piloto medir custo e correção.
4. A luz com mapa usa a profundidade para atenuar a parcela direta. Após a
   migração, remover as caixas de sombra de inimigos e móveis para que uma luz
   sem mapa não volte a produzir silhuetas quadradas. Manter o sistema analítico
   apenas como fallback temporário para a migração e para casos estruturais
   específicos que ainda precisem dele.

## Orçamento inicial para teste

| Perfil | Piloto proposto | Ajuste após medição |
| --- | --- | --- |
| POTATO / LOW | sem mapas; sombra de contato barata | manter boa leitura da sala |
| MEDIUM | 1 luz, cubo 256 px por face | reduzir ou aumentar conforme custo |
| HIGH | 1 luz, cubo 512 px por face | calibrar na K2100M |
| ULTRA | até 2 luzes, cubos 512 px por face | 2 luzes só se o custo permitir |

Esses números são hipóteses de implementação, não uma promessa de desempenho.
A seleção deve ser estável entre quadros para não fazer a sombra trocar de
luminária conforme o jogador caminha.

## Referência medida antes da implementação

Benchmarks locais na NVIDIA Quadro K2100M, 1280 × 720, seed `20261030`, 240
quadros, sem VSync, build `combat-preview`:

| Perfil | média do quadro | P95 do quadro | média GPU | Cena final do benchmark |
| --- | ---: | ---: | ---: | --- |
| MEDIUM | 8,42 ms | 18,01 ms | 4,41 ms | 1 sala, 0 inimigos, 1 prop |
| ULTRA | 9,10 ms | 16,75 ms | 6,73 ms | 1 sala, 0 inimigos, 1 prop |

O benchmark padrão **não estressa móveis nem inimigos**. Antes de definir o
orçamento final, registrar uma cena fixa com diversos móveis, cinco inimigos,
quatro luminárias e estados intacto/quebrado em 720p e 1080p. Comparar o mesmo
seed, câmera e número de quadros antes e depois.

## Entrega em etapas

1. Passe de profundidade e uma luz com mapa em uma cena controlada com coluna,
   móvel e um inimigo animado; capturar sombras com e sem mapa.
2. Reutilizar a pose visual dos cinco inimigos no passe de sombra; verificar
   caminhada, tiro, ataque corpo a corpo, morte e membros em posições variadas.
3. Incluir móveis e props opacos de todas as salas, portas e estados de quebra;
   impedir sombras que atravessam salas fechadas ou permanecem após destruição.
4. Medir, aplicar cache e ajustar os perfis. Integrar em campanha e pedir
   avaliação visual em partida antes de considerar as sombras aprovadas.

## Gates de validação

- Capturas OpenGL com FOV 60/100 e perfis MEDIUM/ULTRA: a sombra do inimigo
  muda com a pose e não fica retangular; móveis mostram contorno reconhecível.
- Testes de luz apagada, objeto destruído, porta aberta/fechada, emissor fora
  da câmera e sem sombra residual de quadros anteriores.
- Benchmark GPU e P95 em cenas equivalentes, incluindo a Quadro K2100M.
- Playtest em Central Hub e uma sala com muitos móveis. Captura técnica não
  equivale a aprovação visual em partida.

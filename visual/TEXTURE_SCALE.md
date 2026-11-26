# Escala física das texturas

Regra de produção para todas as novas texturas de salas e objetos do Iron Seed:
o tamanho de um risco, veio, rebite ou painel não pode crescer só porque a
peça ficou maior. A proposta visual deve informar quantos metros cada repetição
do material representa e qual mapeamento será usado.

## Aplicação

- **Paredes, pisos e tetos:** dividir em módulos ou calcular UV pela distância
  real ao longo da face. Um vão duas vezes mais comprido recebe duas vezes mais
  repetições, com o mesmo tamanho de detalhe. Recortes de portas preservam a
  fase do UV do trecho inteiro.
- **Objetos estáticos da sala:** usar projeção plana por face em coordenadas de
  mundo ou UV local escalado pelos metros reais da face. `GdpRenderer` usa
  `uSurfaceTileMetres` para essa projeção; `uMirrorSurfaceTile` é reservado para
  albedos que precisam de espelhamento nas bordas. A estante usa 0,8 m e os
  móveis metálicos da Cafeteria usam 0,75 m. As colunas estruturais físicas
  das salas e corredores usam um tile repetível de 0,8 m, com UV por face
  calculado a partir da posição em metros.
- **Objetos móveis, quebráveis e personagens:** usar UV local da malha com
  densidade física coerente. Projeção em coordenadas de mundo faria a textura
  deslizar quando a peça se movesse ou girasse.
- **Elementos únicos:** números, letras, emendas específicas, puxadores e
  lascas de borda não entram no tile genérico. Posicionar esses detalhes em
  metros ou em UV local da peça, sem alongar a imagem de material.

Para um tile de `T` metros, o UV de uma face plana é a distância em metros
dividida por `T` nos dois eixos da face. O valor pode ultrapassar 1; a textura
se repete. Um albedo repetível precisa ter bordas compatíveis e manter cor e
granulação semelhantes nas duas extremidades.
Mesmo com UV físico, o próprio albedo não deve conter riscos ou faixas que
atravessem o tile inteiro: ao repetir, eles parecem alongados sobre mesas e
balcões. Para tiles sem emendas, deixar a coordenada UV contínua no shader e
usar a repetição da textura preserva a seleção correta de mipmaps.

## Verificação antes da aceitação em jogo

1. Mostrar no mesmo conjunto uma peça curta e uma longa com o mesmo material.
   Riscos, granulação e desgaste devem ter a mesma escala aparente.
2. Inspecionar a transição dos tiles e as junções entre faces para detectar
   emendas, espelhamentos evidentes ou textura deslizando durante movimento.
3. Capturar perto e à distância, em FOV 60 e 100, nos perfis POTATO e ULTRA.
   Captura técnica confirma a aplicação; a aceitação visual ainda depende do
   playtest do usuário.

Materiais legados que não passaram por essa revisão continuam funcionais, mas
devem receber esse contrato quando forem retrabalhados visualmente. A regra
não equivale a uma aprovação artística de materiais ainda não revisados.

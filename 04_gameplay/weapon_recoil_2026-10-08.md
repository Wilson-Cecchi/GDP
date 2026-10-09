# Recuo controlável das armas — 2026-10-08

Cada disparo de arma de fogo aceito desloca a orientação real da câmera, após
o cálculo do tiro. Assim, o primeiro tiro usa a mira anterior ao recuo e o
seguinte exige compensação do jogador. Disparos recusados por intervalo,
recarga, troca ou falta de munição não geram recuo. A faca não gera recuo.

Pistola, carabina, SMG, escopeta e sniper têm impulsos verticais distintos e
um padrão lateral determinístico para rajadas. Mirar e agachar reduzem o
impulso. Sensibilidade, inversão vertical e tipo de dispositivo de entrada não
alteram a força da arma. O deslocamento visual da arma continua independente.

A sequência de recuo usa o mesmo índice de disparo da dispersão e volta ao
primeiro passo depois de uma pausa conforme `weaponSpreadProfile`. A câmera
não se recentraliza automaticamente: isso evita que a recuperação lute contra
a correção manual do mouse ou analógico. A retícula central indica a direção
atual da câmera; a dispersão do tiro continua sendo uma mecânica adicional.

Validação automatizada: `ctest --test-dir build/release --output-on-failure
-R '^(combat_systems|gameplay_systems)$'`. Validação manual: em Test Mode,
segure o disparo da SMG ou carabina mirando uma junta de parede; a mira deve
subir e mudar lateralmente. Puxe a mira para baixo para compensar. Após parar
por um instante, a próxima rajada começa com o mesmo padrão. Teste também a
escopeta, a sniper, a faca e o controle conectado.

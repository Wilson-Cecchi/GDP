# Colisões dos modelos de referência dos inimigos

Estado em 19/10/2026: **hitboxes do preview aceitas em partida junto com a
gameplay dos cinco modelos reconstruídos**. No preview original, as
reconstruções 3D exigiam `GDP_ENEMY_REFERENCE_MODELS=1`. Na integração da
candidata 0.1.7, os modelos e estas hitboxes passaram a ser padrão;
`GDP_ENEMY_REFERENCE_MODELS=0` seleciona visual e colisões anteriores para
comparação.

| Inimigo | Corpo de referência sem arma/mochila, metros (X; Y; Z) | Navegação mantida (raio × altura) |
| --- | --- | --- |
| Worker | −0,578..0,578; 0,012..1,833; −0,212..0,340 | 0,35 × 1,80 |
| Guard | −0,475..0,475; 0,008..1,776; −0,142..0,303 | 0,35 × 1,80 |
| Officer | −0,512..0,512; 0,005..1,780; −0,150..0,298 | 0,35 × 1,80 |
| Engineer | −0,488..0,488; 0,005..1,840; −0,157..0,320 | 0,35 × 1,80 |
| IndustrialRobot | −0,785..0,859; 0..2,147; −0,272..0,448 | 0,52 × 1,90 |

`Combat.cpp` usa seis zonas locais por arquétipo (tronco, cabeça, dois braços,
duas pernas) orientadas pelo yaw. A cabeça é zona fraca. Arma, serra, antena,
mochila e módulos traseiros ficam fora do dano corporal. O Boss tem zona de
cabeça Y 1,90–2,18 m, incluindo seu sensor emissivo Y 2,000–2,074 m.
`EnemyProjectile.cpp` usa as mesmas zonas para colisão real e bloqueio por
aliado; `PlayerGrenade.cpp` as usa para contato e amostras visíveis da explosão.
As caixas têm pequena margem para a animação e projéteis.

A navegação conserva raios/alturas anteriores. A cabeça do Boss chega a
2,15 m e os módulos traseiros a 2,45 m, enquanto passagens têm 2,20 m;
aumentar o volume de movimento sem revisar portas poderia bloquear a campanha.
As zonas de tiro são rígidas e não acompanham cada rotação de braço. A normal
de quique da granada em corpos segmentados é aproximada; a explosão amostra
quatro pontos do corpo em vez de usar o mesh completo.

Gates técnicos: `combat_systems`, `enemy_sensors_systems`,
`room_traversal_systems`, `renderer_cpu_systems` e `renderer_opengl_pixels`.
As capturas de ataque estão em `visual/validation/enemy_attack_game/`.
O responsável pelo jogo aprovou a gameplay e os visuais da prévia em
19/10/2026. Os gates técnicos acima e as limitações descritas permanecem
registrados; a aprovação em partida não representa uma medição de todos os
casos de sniper, escopeta, sensor em movimento, bloqueio por aliados e
granadas em cada sala.

# Movimento das mãos — revisão aprovada provisoriamente

Data: 2026-10-04. Complementa a revisão de contatos em
`player_weapon_contacts_2026-10-04.md`. **Aprovado provisoriamente no jogo em
2026-10-05**: o usuário aprovou movimentação e visuais por enquanto. A aprovação
não encerra o refinamento futuro das animações.

## Mudança

- Os alcances entre apoio, pente, mecanismo e ferrolho deixaram de seguir a
  reta que atravessava a arma. A mão se afasta um pouco do receptor, orienta a
  palma durante a viagem e assenta no novo contato ao final. A trajetória tem
  velocidade nula nas extremidades para não dar tranco ao prender uma peça.
- Os dedos abrem antes de sair de uma pegada e fecham perto da próxima. Anelar
  e mínimo fecham um pouco antes do indicador e polegar. Na fase de operação,
  a mão continua acompanhando o deslocamento real da peça; os pontos de
  contato já verificados no modelo não foram substituídos por poses soltas.
- A mão esquerda da sniper começa a buscar o pente quando a direita termina o
  primeiro curso do ferrolho, em vez de comprimir toda a busca em 2% da recarga.
- Carabina e SMG esperam a mão alcançar a alavanca antes de movê-la. A shotgun
  espera 72 ms após o último cartucho para a mão deixar a entrada e segurar o
  pump; mantém a mão presa durante a ida e volta. O tempo total de recarga,
  munição e mecânicas de combate não mudaram.
- A faca continua com uma mão; braços e limites de punho da revisão anterior
  foram preservados. Nenhuma textura, modelo de arma ou regra de FOV foi
  alterada nesta passagem.

## Verificação

Testes novos cobrem a folga da trajetória da pistola, abertura/fechamento dos
dedos, sobreposição controlada das duas mãos na sniper e o mecanismo aguardando
a pegada na carabina, SMG e shotgun (1, 6 e 32 cartuchos). Continuam os gates
anteriores de contato contra triângulos reais, IK, continuidade de 1000 fases,
FOV, recuo, agachar, pular, equipar, faca e renderização POTATO/MEDIUM.

- Release final: **65/65** testes (`ctest --test-dir build/release`, 15,38 s).
- ASan/UBSan final: quatro testes afetados passaram; o gate CPU passou isolado
  em **8,79 s**. Em uma tentativa concorrente com testes OpenGL, ele excedeu
  o timeout de 10 s; a repetição isolada passou sem ampliar tolerância/timeout.
- Capturas finais do renderer real: Intel HD 4600 / Mesa 26.2.3. As sequências
  em GIF têm 31 poses discretas e tempo apenas ilustrativo; não são gravação
  da Application nem teste manual do movimento no jogo. Windows não foi
  empacotado nesta passagem. `git diff --check` passou.

Pré-visualizações: [pistola](../visual/validation/player_hands_motion_2026-10-04_pistol.gif),
[carabina](../visual/validation/player_hands_motion_2026-10-04_carbine.gif),
[SMG](../visual/validation/player_hands_motion_2026-10-04_smg.gif),
[sniper](../visual/validation/player_hands_motion_2026-10-04_sniper.gif) e
[shotgun](../visual/validation/player_hands_motion_2026-10-04_shotgun.gif).
Fontes BMP: `build/visual-validation/hand-motion-final-20261004-reloads` e
`build/visual-validation/hand-motion-final-20261004-states`.

Para validar em movimento no jogo, feche a instância antiga e inicie
`build/release/gdp --test-mode`. Observe as recargas de todas as armas,
principalmente a chegada ao slide/alavanca/pump, com FOV 60/100, além da faca
e dos movimentos com Ctrl/Space. Esta correção busca movimento mais crível,
mas não declara captura corporal realista ou ausência de interpenetração em
toda combinação ainda não vista pelo usuário.

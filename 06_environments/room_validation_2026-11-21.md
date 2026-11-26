# Validação técnica das salas — 21/11/2026

Escopo: os 34 módulos da biblioteca da campanha e o corredor industrial. Esta
auditoria usa o código e a build locais de 21/11/2026. A
[galeria de 34 variantes](../visual/validation/campaign_rooms_full_2026-11-20/README.md)
foi capturada em HIGH na GTX 1060 em 20/11. As alterações posteriores estão
registradas abaixo e na [galeria desta rodada](../visual/validation/rooms_finish_2026-11-21/README.md).

## Cobertura

| Família | Módulos incluídos | Resultado técnico e observação visual |
| --- | --- | --- |
| Industrial | 101 Compacta, 102 Média, 103 Spawn, 104 Hub, 105 Hall | Arquitetura válida nas quatro rotações. O Hub já tem aprovação registrada. **105:** a repetição vertical da faixa foi corrigida e capturada na campanha. A foto antiga da 102 é obstruída por um prop perto da câmera. |
| Storage | 201 Compacta, 202 Média, 203 Secreta, 204 Hall | Arquitetura e testes de estantes/caixas passaram. Hall aprovado anteriormente; as outras variantes não herdam esse aceite. |
| Office | 301 Compacta, 302 Média, 303 Hall | Arquitetura válida. Painéis mais largos, juntas menos escuras e variação de metal nas três variantes; o Hall 303 ganhou duas mesas baixas de arquivo. Revisão visual em partida pendente. |
| Laboratório | 401 Compacto, 402 Médio, 403 Secreto, 404 Boss, 405 Hall | Arquitetura válida; os vasos pequenos quebráveis do Hall passaram. O Secreto 403 ganhou dois armários de amostras junto à parede de fundo. A foto antiga da 401 é obstruída por inimigo. O compacto com texturas novas e o Hall têm aprovações próprias registradas. |
| Manutenção | 501 Compacta, 502 Média, 503 Arena, 504 Boss, 505 Hall, 506 em L, 507 Arena com galerias | A 506 tinha falha reproduzida e foi corrigida. As demais passam nos testes disponíveis. Capturas técnicas não substituem o percurso de escadas e encontros em partida. |
| Segurança | 601 Compacta, 602 Média, 603 Arena, 604 Boss, 605 Hall quadrado, 606 em T | Arquitetura válida. As variantes 605 e 606 têm aprovações anteriores. Os corpos dos monitores 601/605/606 agora são quebráveis, com estados intacto, danificado e destruído. A moldura da janela de observação 605 permanece fixa. |
| Cafeteria | 701 Compacta, 702 Média, 703 Hall 18 × 18, 704 Alongada | Arquitetura válida. A 704 foi aprovada em prévia/campanha. As quatro variantes têm duas plantas de canto quebráveis; a 701–703 ganharam as plantas nesta rodada. |
| Corredor | Prévia industrial e corredores gerados | Prévia renderizou 60 quadros na NVIDIA sem erro; travessia e conectores passaram nos testes. Aceite visual em partida continua pendente. |

## Defeito corrigido

Na campanha com `--seed 722`, a sala 506 (Manutenção em L, rotação de 180°)
perdia todo o mobiliário e os dutos. Sua largura/comprimento derivados do AABB
podiam ser `19.999996` m em vez de `20.0` m. A comparação estrita em
`makeTechnicalPreviewArchitecture` descartava o conjunto completo.

`RoomArchitecture.cpp` agora tolera 0,001 m apenas na checagem do tamanho
nominal, em linha com a tolerância de contenção da mesma rotina. O teste novo
exercita a 506 em coordenadas fracionárias nas quatro rotações e verifica a
arquitetura dos 34 módulos nessas condições. A
[captura atual da 506 na semente 722](../visual/validation/room_audit_2026-11-21/maintenance_l_seed722.png)
mostra as bancadas e os dutos presentes.

## Correções desta rodada

1. **Industrial Hall 105:** a parede alta agora usa a metade superior do atlas
   acima de 4 m, preservando a escala das placas e deixando a faixa de
   segurança apenas na base.
2. **Office 301–303:** ajustadas juntas, relevo e variação do metal. O 303 ganhou
   duas mesas de arquivo, com circulação e zonas reservadas verificadas em
   quatro rotações e duas sementes.
3. **Laboratório Secreto 403:** dois armários rasos de amostras ocupam os lados
   da parede de fundo; o vão norte, a chave, inimigos e props continuam livres.
4. **Segurança:** o corpo dos monitores de 601/605/606 virou prop de 24 HP.
   A tela apaga após dano e vira fragmentos ao quebrar; a barra de estado
   acompanha o alerta vermelho, reinicialização amarela e verde pronto.
5. **Cafeteria:** cada variante 701–704 tem duas plantas de canto com vaso e
   folhagem 3D; um tiro pode quebrá-las. O estado destruído deixa fragmentos.

As diferenças de densidade visual entre Office 301/303 e a decoração mais
simples de algumas salas compactas são observações para avaliação em partida,
não defeitos funcionais confirmados.

## Estado de aprovação

As correções estão implementadas na biblioteca da campanha e passaram na
validação técnica. O aceite visual e de circulação das variantes alteradas
depende do [playtest desta rodada](room_finish_playtest_2026-11-21.md). A regra
de [sala pronta](room_shape_plan.md) continua por variante: aprovações antigas
não se estendem automaticamente a todas as 34 variantes. O Office deslocado é
uma ideia de formato futuro, ainda não é um módulo da biblioteca atual.

## Execução

- Build atual da GTX 1060: compilação concluída.
- Após as correções, 7 testes de integração passaram: mundo, travessia,
  materiais, props, campanha, renderização OpenGL e menus.
- 14 sementes em `--headless --frames 10`: `0`, `1`, `2`, `4`, `14`, `15`,
  `29`, `40`, `61`, `104`, `135`, `1116`, `1793`, `3710`; todas iniciaram.
- 12 prévias gráficas com `--frames 60` na NVIDIA: Storage, Arena, Boss,
  Cafeteria, Cafeteria longa, Laboratório, Técnica, Técnica em L, Segurança,
  Segurança em T, Hub e corredor; todas encerraram sem erro.
- Capturas de campanha atuais: sementes `29`, `61`, `1116` e `1793` em HIGH,
  na GTX 1060; os monitores e vasos foram capturados intactos e após dano.

Os testes provam geração, invariantes, inicialização e imagem técnica. Eles não
demonstram travessia manual nem aprovação estética das 34 variantes. A
validação em partida do usuário ainda é necessária para fechar o critério de
aprovação de cada variante alterada.

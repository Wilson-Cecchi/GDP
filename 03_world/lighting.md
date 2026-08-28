# World — Iluminação procedural

> **Estado:** planos determinísticos por sala, múltiplas luzes pontuais,
> luminárias emissivas e resposta visual a lockdown implementados na Fase 12.

## Contrato

`generateRoomLightingPlan()` recebe apenas `roomSeed`, `RoomRole`, `RoomTheme` e
os bounds já comprometidos da sala. Toda variação nasce de
`SeedDomain::Lighting`; a geração não consulta nem consome `World`, `Loot`,
`Props`, `EnemySpawn`, `Events` ou RNG global.

Cada `RoomLightingPlan` contém:

- paleta e intensidade de ambiente;
- mood `Neutral`, `Warm`, `Cool`, `Clinical`, `Dim` ou `Ominous`;
- de uma a quatro luzes pontuais dentro dos bounds e próximas ao teto;
- intensidade emissiva das luminárias;
- capacidade explícita de reagir ao alerta de lockdown;
- hash canônico do plano completo.

## Papel e tema

Secret usa uma única luz e ambiente reduzido, sem cair abaixo do limite de
legibilidade. Hub, Arena e Boss usam quatro pontos para navegação e combate.
Storage/Cafeteria favorecem tons quentes, Laboratory usa luz clínica,
Security usa tons frios e Boss recebe base ominosa. As demais salas recebem
duas ou três luzes conforme a área.

## Runtime

`Application` gera e valida o plano de todas as salas durante a inicialização.
O renderer recebe somente dados prontos, sem acessar geradores ou RNG. Quando
um evento compatível entra em lockdown, ambiente, pontos e luminárias mudam
temporariamente para alerta vermelho; a conclusão restaura o plano original.

O mundo já possui planos para todas as salas, mas a navegação física ainda
renderiza apenas a START. Nela, usar o terminal com `E` permite observar a
transição normal → alerta → normal durante a emboscada.

## Gates

O teste dedicado cobre 1.000 seeds, seis moods, papéis/temas, bounds, limites de
leitura, hash dourado `0x2D7AF8E8762A807F`, seeds padrão/12345 e estabilidade do
fingerprint estrutural. A regressão OpenGL exige mudança de pixels entre luz
padrão, plano procedural e alerta.

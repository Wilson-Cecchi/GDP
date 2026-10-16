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

Salas Boss amplas (bounds com menor eixo ≥24 m, maior ≥28 m) distribuem os quatro pontos
perto dos quartos da planta, com alcance de 11,0–11,8 m e intensidade de
1,05–1,25. Salas Normal amplas (bounds com menor eixo ≥15,5 m, maior ≥17,5 m,
incluindo módulos 16 × 18 m após o inset das paredes) usam também
quatro pontos, com alcance menor de 8,5–9,3 m e intensidade de 0,96–1,14.
O mood ominoso e a paleta do Boss permanecem. A intensidade ambiente da sala
ampla sobe de cerca de `0,14` para `0,21` porque a captura nativa em POTATO
deixava o robô quase invisível à distância; salas Boss antigas retêm a base
anterior. A captura em MEDIUM também deve ser conferida para evitar clarear
demais o confronto.
Após comparar as capturas MEDIUM/POTATO, o usuário escolheu em 2026-10-13 manter
esta iluminação, sem ajuste adicional de contraste do robô nesta fase. Essa
decisão não é uma medição de desempenho nem um playtest do combate.
Salas menores preservam a distribuição e os valores anteriores, inclusive
o hash dourado da START.

O alcance é uma escala de atenuação no shader, sem corte rígido. Os pontos
ficam afastados das paredes, mas a ausência de luz visível através de uma
sala conectada depende da geometria e das sombras do perfil gráfico. É
necessário conferir a transição de portas em MEDIUM e POTATO com capturas
nativas antes de aprovar o resultado visual. O orçamento continua em quatro
pontos por sala; o custo de sombras, sobreposição e legibilidade dos cantos
precisa ser medido em jogo.

## Runtime

`Application` gera e valida o plano de todas as salas durante a inicialização.
O renderer recebe somente dados prontos, sem acessar geradores ou RNG. Quando
um evento compatível entra em lockdown, ambiente, pontos e luminárias mudam
temporariamente para alerta vermelho; a conclusão restaura o plano original.

O mundo possui planos para todas as salas navegáveis. Na sala Terminal, usar o
console com `E` ativa o lockdown e permite observar a transição de iluminação
normal → alerta → normal após a limpeza do encontro. A START permanece sem
inimigos e sem terminal de missão.

## Gates

O teste dedicado cobre 1.000 seeds, seis moods, papéis/temas, bounds, limites de
leitura, hash dourado da START `0x9784746862ECF493`, seeds padrão/12345 e estabilidade do
fingerprint estrutural. Outro lote de 1.000 seeds confere Boss e Normal amplos,
incluindo alcance dos quatro cantos à altura da câmera e limite de pontos.
A regressão OpenGL exige mudança de pixels entre luz padrão, plano procedural
e alerta; o lote numérico não substitui inspeção nativa.

Captura técnica reproduzível:
`GDP_ROOM_CAPTURE_FRAMES=1 GDP_ROOM_CAPTURE_DIR=visual/validation/room_scale_v1 build/debug/gdp_application_menu_tests --room-scale-capture`.
Usa seed `12345`, FOV `70`, resolução `1280 × 720`, Boss módulo `504`
visto da entrada real e do flanco, e Normal ampla módulo `105`, em MEDIUM e
POTATO. Os PNGs locais em `visual/validation/room_scale_v1/` são ignorados pelo
Git e não são aprovação visual. Sem `GDP_ROOM_CAPTURE_FRAMES=1`, o comando mede
20 quadros por vista. Nesta máquina Intel HD Graphics 4600, esses quadros
estáticos por vista com VSync desligado variaram entre cerca de `19–30 ms` em
MEDIUM e `5–10 ms` em POTATO em duas execuções. O teste não mede combate,
câmera em movimento ou efeito de sombras em hardware diferente. A sensação de
escala e o equilíbrio do encontro ainda exigem playtest.

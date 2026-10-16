# Test Mode — calibração de armas

Esta etapa altera apenas a área de testes. Três placas metálicas vermelhas feitas
com o modelo de canister existente ficam junto à parede direita, a distâncias
diferentes do ponto inicial. Têm vida alta para permitir muitas rajadas sem
destruição acidental. Não introduzem texturas novas.

A arena separa a preparação do combate com dois anteparos altos em `z=-18` e
uma passagem central de quatro metros. Armas, suprimentos e placas ficam do lado
seguro; os cinco inimigos, inclusive o Boss, começam em `z=-28`. A placa mais
distante fica em `z=-14`, ainda acessível da preparação. O Boss mantém seu
alcance normal, mas só obtém linha de tiro depois que o jogador entra na zona de
combate ou se expõe pela passagem. O volume da sala vai até `z=-36` para manter
espaço de manobra atrás da linha inimiga.

O terminal atrás do ponto inicial serve como estação de suprimentos. Ao mirar
nele de perto, o HUD mostra a ação de reabastecer em inglês ou português
(Brasil). Interagir restaura HP e colete e enche o carregador e a reserva de
todas as armas de fogo possuídas, inclusive a arma guardada. Uma recarga em
curso é cancelada para evitar consumo posterior de munição. A arma selecionada,
a cadência restante, Overdrive e a faca permanecem como estavam. Se o jogador
largou todas as armas de fogo, o terminal não cria uma arma nova.

Campanha e Speedrun não recebem a estação nem as placas; dano, inimigos e
balanço das armas não mudam. A posição das placas e a ação do terminal são
determinísticas. Validação automatizada: `ctest --test-dir build/release
--output-on-failure -R '^(combat_systems|test_arena_systems)$'`.
Playtest: `build/playtest/gdp --test-mode`; olhe à direita para as placas,
dispare parado, em movimento e saltando, depois vire-se para o terminal e
pressione Interagir para reabastecer. Confira também com uma arma guardada e
outra selecionada; avance pela abertura central para enfrentar o Boss.

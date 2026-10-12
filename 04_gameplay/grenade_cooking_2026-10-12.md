# Preparo e fusível da granada

A retirada do pino leva 0,32 segundo. Segurar o disparo inicia esse processo;
soltar antes de sua conclusão registra a intenção de lançar, e o lançamento
ocorre quando o pino termina de sair. A carga de distância começa depois da
retirada do pino e atinge o máximo em 1,2 segundo. Segurar além disso não
aumenta a velocidade.

O fusível de **4,87 segundos** começa ao fim da retirada do pino, conforme o
tempo máximo indicado para esta etapa. O tempo continua passando enquanto a
granada é segurada. Ao soltar, a granada lançada recebe apenas o tempo restante;
se o prazo terminar na mão, o estado emite `DetonateInHand` uma única vez.
Lançamento e detonação são resultados terminais; uma nova granada exige
`reset()` seguido de `begin()`.

`GrenadeCookState::update(step, fireHeld)` consome o passo recebido de forma
determinística. Um passo que atravessa o fim da retirada do pino aplica sua
fração restante ao fusível enquanto o disparo continua pressionado. Se a
soltura for observada no início do passo, o lançamento tem prioridade sobre
um vencimento que ocorreria dentro desse mesmo passo; o projétil recebe o
fusível restante. Passos não positivos ou não finitos não mudam o
estado. `PlayerGrenadeSystem::throwGrenade` mantém as chamadas anteriores e
aceita, ao final, um fusível restante positivo de no máximo 4,87 segundos.
Valores inválidos são rejeitados. O tempo do fusível do projétil consome o
passo inteiro mesmo quando a simulação de movimento limita um passo grande.
`Application` encaminha a explosão na mão a `resolveGrenadeBlast`, que calcula
dano radial ao jogador e aos alvos próximos conforme sala, alcance e cobertura.
O input consome a granada no começo da retirada do pino, bloqueia a troca de
arma durante o preparo e congela o pavio quando a partida está pausada. Após
soltar, o projétil nasce com o tempo restante do pavio e a animação de soltura
termina antes de permitir outra troca de arma.

Os testes de combate cobrem liberação antecipada, força máxima, limites do
fusível antes/no/depois de 4,87 segundos, transferência do tempo restante para
o projétil, detonação única e dano radial. A integração com input, renderização
e HUD foi feita em `Application` e `GdpRenderer`. O teste de integração dos
menus da aplicação passou com `SDL_VIDEODRIVER=offscreen` e
`SDL_AUDIODRIVER=dummy` após o limite de 120 segundos do CTest ter sido
insuficiente nesta máquina. Os fixtures GL específicos do HUD, Tab e mãos
passaram; a revisão da animação em partida nativa permanece pendente.

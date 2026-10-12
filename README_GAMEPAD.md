# Controle no Iron Seed

O jogo aceita controles compatíveis com o mapeamento de gamepad do SDL3,
incluindo controles Xbox e PlayStation reconhecidos pelo sistema operacional.
USB e Bluetooth usam os mesmos comandos após o pareamento. O primeiro controle
detectado é o ativo; ao desconectá-lo, o jogo libera os comandos pressionados e
procura outro controle disponível. Teclado e mouse continuam funcionando juntos.

| Controle Xbox / PlayStation | Ação |
| --- | --- |
| Analógico esquerdo | Mover; clique para caminhar silenciosamente |
| Analógico direito | Olhar; clique para golpe rápido de faca |
| RT / R2 | Atirar; com granada equipada, segurar para carregar e soltar para lançar |
| LT / L2 | Mirar |
| A / ✕ | Pular; confirmar nos menus |
| B / ○ | Agachar; voltar nos menus |
| X / □ | Recarregar |
| Y / △ | Voltar à última arma |
| LB / L1 | Arma anterior |
| RB / R1 | Equipar granada de fragmentação (slot 4) |
| Direcional ↑, →, ↓ | Interagir, selecionar a primária, ligar/desligar a lanterna |
| Direcional ← | Largar arma |
| View / Create | Informações da partida |
| Menu / Options | Pausar ou retomar |

No menu, o direcional ou o analógico esquerdo navega. Esquerda/direita ajusta
opções; A/✕ confirma e B/○ volta. Em `Settings > Controls`, escolha
`Keyboard / Mouse` ou `Controller`. Cada dispositivo tem remapeamento e reset
independentes. Se um botão já estiver associado a outra ação, as duas ações
trocam seus comandos. Durante a captura do controle, `Options` ou `Esc`
cancela e `Delete` remove a associação. `Options` fica reservado ao menu.
O movimento analógico não é um botão: ajuste `Move Stick` no menu `Controller`
para usar o analógico esquerdo (padrão) ou direito (Southpaw); a câmera passa
automaticamente para o outro analógico. As linhas Forward/Backward/Left/Right
em `Remap Buttons` aceitam botões digitais opcionais, além do analógico escolhido.
O item especial (slot 5) permanece reservado. Com a granada equipada, R2 inicia
a retirada do pino pela outra mão. Após 0,32 s, começa o pavio de 4,87 s.
Segurar R2 recua a mão de lançamento e aumenta a distância até 1,2 s de carga;
soltar R2 arremessa com o tempo restante do pavio. Um toque curto é lançado
assim que o pino sai. Se o tempo acabar antes de soltar, a granada explode na
mão e causa dano ao jogador e a alvos próximos. A granada é consumida ao iniciar
a retirada do pino; a troca de arma fica bloqueada até o lançamento ou a explosão.
O estoque inicial é de duas granadas.

Para testar um DualSense por Bluetooth no Linux: emparelhe-o nas configurações do
sistema, inicie `build/release/gdp --test-mode` e confira a linha
`Gamepad connected: ...` no terminal. Teste o menu, os dois analógicos, os
gatilhos e a desconexão/reconexão. O teste automatizado usa um gamepad virtual;
ele não substitui a validação com seu DualSense real.

`Controller` também permite ligar/desligar a vibração e regular sua intensidade
de 0% a 100%. Disparos e dano recebido produzem pulsos de vibração comum
quando o controle e o driver oferecem suporte. A intensidade 0% não vibra,
mesmo com a opção ligada. Efeitos específicos dos gatilhos adaptativos do
DualSense continuam fora desta etapa.

As opções de câmera do controle são independentes da sensibilidade e inversão
do mouse: horizontal e vertical de 30% a 200%, sensibilidade ao mirar de 30%
a 150%, inversão vertical e zonas mortas de movimento/câmera de 0% a 40%.
O padrão é 100% para as sensibilidades, 18% para ambas as zonas mortas e
analógico esquerdo para movimento. O analógico esquerdo continua navegando
nos menus mesmo quando o layout da partida está invertido. As preferências
são salvas em `settings.toml` (schema 12); arquivos anteriores recebem esses
valores padrão sem perder os remapeamentos existentes.

O padrão de câmera agora gira até 300°/s com o analógico totalmente inclinado
(antes, 150°/s), antes dos ajustes horizontal/vertical e de mira. O layout
foi aproximado do padrão de FPS no DualSense: △ alterna para a última arma,
R3 usa a faca sem trocar a arma selecionada e R1 equipa a granada. No Iron Seed,
L3 ainda é caminhada silenciosa, não sprint; □ recarrega e a interação continua
no direcional ↑. O jogo ainda não tem sprint tático, deitar, slide, mantle ou
killstreaks. Controles salvos sem remapeamento próprio recebem o novo layout
automaticamente (schema 12); remapeamentos personalizados são preservados.
`RESET DEFAULTS` no menu de botões aplica o novo layout a qualquer momento.

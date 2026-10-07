# Controle no Iron Seed

O jogo aceita controles compatíveis com o mapeamento de gamepad do SDL3,
incluindo controles Xbox e PlayStation reconhecidos pelo sistema operacional.
USB e Bluetooth usam os mesmos comandos após o pareamento. O primeiro controle
detectado é o ativo; ao desconectá-lo, o jogo libera os comandos pressionados e
procura outro controle disponível. Teclado e mouse continuam funcionando juntos.

| Controle Xbox / PlayStation | Ação |
| --- | --- |
| Analógico esquerdo | Mover; clique para caminhar silenciosamente |
| Analógico direito | Olhar; clique para ligar/desligar a lanterna |
| RT / R2 | Atirar |
| LT / L2 | Mirar |
| A / ✕ | Pular; confirmar nos menus |
| B / ○ | Agachar; voltar nos menus |
| X / □ | Recarregar |
| Y / △ | Interagir |
| LB / L1, RB / R1 | Arma anterior, próxima arma |
| Direcional ↑, →, ↓ | Primária, secundária, faca |
| Direcional ← | Largar arma |
| View / Create | Informações da partida |
| Menu / Options | Pausar ou retomar |

No menu, o direcional ou o analógico esquerdo navega. Esquerda/direita ajusta
opções; A/✕ confirma e B/○ volta. Em `Settings > Controls`, escolha
`Keyboard / Mouse` ou `Controller`. Cada dispositivo tem remapeamento e reset
independentes. Se um botão já estiver associado a outra ação, as duas ações
trocam seus comandos. Durante a captura do controle, `Options` ou `Esc`
cancela e `Delete` remove a associação. `Options` fica reservado ao menu; os
dois analógicos mantêm movimento/visão, embora também seja possível atribuir
botões às quatro ações de movimento. Os atalhos de equipamento e item especial
seguem reservados, sem função no jogo.

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

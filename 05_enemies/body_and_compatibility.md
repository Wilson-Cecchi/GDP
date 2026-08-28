# Enemies — Body Profiles, sockets e compatibilidade

> **Estado:** catálogo do Worker validado na Fase 9 e catálogos de Guard,
> Officer, Engineer e IndustrialRobot validados na Fase 11.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Escala

O Worker usa altura-base de 1,80 m multiplicada pelo perfil. Envelopes de cabeça,
torso, pernas e acessórios são proporcionais à altura final.

## BodyProfile

Controla altura, largura de ombros, profundidade de torso, comprimento de
braços/pernas, escala da cabeça, inclinação e viés de massa esquerdo/direito. A
primeira biblioteca implementa `Standard`, `Compact`, `Lanky` e `Heavy`; os 18
perfis do guia antigo permanecem como direção de expansão.
O Guard acrescenta `Patrol`, `Tactical` e `Armored`, mapeados a envelopes
humanos Standard, Light e Heavy sem alterar os sockets compartilhados.
O Officer acrescenta `Field`, `Command` e `Veteran`, mapeados aos mesmos
envelopes Standard, Light e Heavy para preservar o contrato de sockets.
O Engineer adiciona `Maintenance`, `Diagnostics` e `Hazmat`, também mapeados a
Standard, Light e Heavy.
O IndustrialRobot usa `Sentry`, `Foundry` e `Siege`, restritos aos envelopes
Compact/Heavy e às máscaras `Heavy|Mechanical`.

## Sockets

Cada perfil materializa oito sockets tipados: `Root`, `Neck`,
`ShoulderLeft`, `ShoulderRight`, `HipLeft`, `HipRight`, `Back` e `Tool`. Cada
socket registra posição, direção, raio e máscara de compatibilidade.

## Tags

Exemplos: `worker`, `security`, `military`, `technical`, `machine`, `heavy`, `experimental`,
`biofusion`, `salvaged`, `corrupted`.

## Compatibilidade

Uma combinação precisa passar slot + socket + interseção das máscaras `Light`,
`Standard`, `Heavy` e `Mechanical`. A validação também exige todos os sockets,
receitas válidas, pés no plano, altura máxima e hash canônico íntegro.

## Assimetria

Braços e pernas esquerdos/direitos são sorteados por substreams distintos e
podem ter IDs diferentes. O `BodyProfile` mantém dimensões e encaixes dentro do
envelope validado.

## Catálogo atual

A biblioteca do Worker possui 18 definições estáveis: três alternativas para cada slot
`Torso`, `Head`, `Arm`, `Leg`, `Back` e `Tool`. Cada definição registra ID,
socket, compatibilidade, peso, primitiva Box/Cylinder, escala, material slot e
região semântica.

O Guard possui outras 18 definições nos mesmos seis slots. O slot `Tool` é
validado como `Weapon`, exige silhueta longitudinal e representa carabina,
escopeta tática ou arma pesada conforme o perfil.

O Officer possui mais 18 definições. Seu slot `Back` exige um
`MechanicalCore` vertical que representa rádio/módulo de comando; `Tool` exige
uma `Weapon` curta, diferenciada dimensionalmente das armas longas do Guard.

As 18 definições do Engineer exigem `MechanicalCore` vertical no slot `Back`
para mochila diagnóstica e `Tool` longitudinal no slot homônimo para chave,
sonda ou soldador, sem classificá-los como arma de fogo.

## Boss

IndustrialRobot possui 18 definições próprias. A montagem exige torso
`MechanicalCore`, cabeça `Lens`, dois módulos traseiros `MechanicalCore`,
braços `Piston` ampliados e `Weapon` pesada. As nove primitivas resultantes
mantêm o mesmo contrato de sockets e bounds do pipeline comum.

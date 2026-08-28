# Testing — Validação procedural e métricas

> **Estado:** especificação técnica inicial do novo GDP.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## World metrics

Rooms, loops, main path length, regeneration attempts, corridor length, secrets, enemy count,
loot e generation time.

O gate de loot executa 1.000 room seeds, exige 1.000 hashes canônicos únicos,
budget respeitado, posições dentro das zonas transformadas, separação mínima e
cobertura de Health/Ammo. Uma seed dourada protege o formato; integração de
mundo reproduz tipos, quantidades e posições e mantém o fingerprint estrutural
independente da substream `Loot`.

O gate de props executa 1.000 room seeds, exige hashes únicos, budget exato,
footprint dentro de `propZones`, separação, cobertura de crate/canister/barricade
e protege o hash dourado `0x28BB5CB31BBAF612`. As seeds padrão e 12345 validam
todas as salas; integração compara estado, HP, escala, drop, posição e
`visualSeed` sem alterar o fingerprint estrutural.

O gate de eventos percorre 1.000 room seeds, exige 1.000 hashes únicos,
cobertura de `None/TerminalAmbush/Reinforcements/Lockdown/SupplyDrop`, triggers
compatíveis, ondas completas e posições válidas. O hash dourado
`0xB04A437A8394B962` protege o contrato; seeds padrão/12345 validam todas as
salas sem consumir as streams estruturais, de loot ou props.

O gate de iluminação percorre 1.000 room seeds, exige 1.000 hashes únicos,
cobre os seis moods e valida contagem, bounds, cor, intensidade, alcance e
limite mínimo de ambiente. O hash dourado `0x2D7AF8E8762A807F` protege o
contrato; seeds padrão/12345 validam todas as salas sem alterar o fingerprint
estrutural. O OpenGL compara pixels de luz padrão, procedural e alerta.

O gate de áudio percorre 1.000 combinações de seed/kind/counter, exige hashes
únicos e cobre todas as doze categorias e os sete perfis ambientes. O hash
dourado `0xEAD1C1A48BF9FF7F` protege a receita; waveform, envelope, duração,
clipping, cache, 16 vozes, pan, distância, loop, dispositivo dummy e fallback
sem device são regressões dedicadas. A substream `Audio` não participa do
fingerprint estrutural.

O gate de progressão valida a curva 100 + 75 por nível, cap 10, XP restante,
score, contadores por fonte, eventos inválidos e ciclo fixo de nove upgrades.
Integração prova que Vitality altera HP/cura, Firepower altera dano e Capacity
altera reserva/munição nas instâncias reais, mantendo o estado canônico válido.

O gate de travessia usa as edges/corridors do mundo gerado e testa cada endpoint
nos dois sentidos. Destino, inset de entrada, facing, alcance, preservação de
estado e bounds rotacionados são invariantes; nenhuma nova stream aleatória é
consumida e o fingerprint estrutural permanece propriedade do mesmo `World`.

## Enemy metrics

Role, BodyProfile, part IDs, fallback count, materials e unique VisualGenomeHash.

O gate atual do Worker executa 1.000 seeds e exige todos os quatro BodyProfiles,
as 18 peças, as três famílias materiais, os quatro estados de desgaste,
assimetria, hashes únicos no lote e zero fallbacks. O teste de população também
prova estabilidade das `visualSeed` sem alterar o fingerprint estrutural do
World.

O gate do Guard executa outras 1.000 seeds, cobre `Patrol/Tactical/Armored`, as
18 peças próprias, três famílias táticas, quatro estados de desgaste,
assimetria, arma longa, hashes únicos e zero fallbacks. O conjunto Worker+Guard
exige 2.000 hashes sem colisão e substreams de mesh/textura distintos.

O gate do Officer executa mais 1.000 seeds, cobre `Field/Command/Veteran`, 18
peças próprias, `Fabric/Polymer/PaintedSteel`, quatro estados de desgaste,
assimetria, sidearm, módulo de comando, hashes únicos e zero fallbacks. O
conjunto dos três papéis exige 3.000 hashes sem colisão e substreams distintas.

O gate do Engineer acrescenta 1.000 seeds, cobre
`Maintenance/Diagnostics/Hazmat`, 18 peças técnicas,
`Aluminum/Polymer/PaintedSteel`, quatro estados de desgaste, assimetria,
ferramenta de reparo, mochila diagnóstica, hashes únicos e zero fallbacks. O
conjunto dos quatro papéis exige 4.000 hashes sem colisão e substreams distintas.

O gate do IndustrialRobot acrescenta 1.000 seeds, cobre
`Sentry/Foundry/Siege`, 18 peças mecânicas, `BareSteel/PaintedSteel/Aluminum`,
quatro estados de desgaste, torso e sensores especiais, braços pesados, dois
reatores, arma, emissão, assimetria, hashes únicos e zero fallbacks. O conjunto
dos cinco papéis exige 5.000 hashes sem colisão e substreams distintas.

## Texture metrics

Family, resolution, generation ms, cache hit e min/max de canais.

O gate do Industrial Corridor executa 1.000 room seeds, rasteriza Wall, Floor e
Ceiling e exige `Maintained/Used/Worn`, hashes únicos, rodapé amarelo apenas na
base, faixa central de tráfego e luminárias contrastantes. O teste OpenGL troca
a seed ativa, comprova mudança de pixels e repetição bit a bit da mesma variante.

Storage acrescenta outras 1.000 seeds e exige `Orderly/Dusty/Neglected`, hashes
únicos, oxidação apenas em metal, marca logística curta, arrasto de piso e teto
legível. O teste OpenGL também troca Industrial por Storage usando a mesma fila
de superfícies e comprova que os pixels do tema são distintos.

Combat Room acrescenta 1.000 seeds, cobre `Ready/Active/Scarred`, protege seu
hash dourado e valida blindagem, 1–3 impactos, perímetro contínuo de arena e
contraste das luminárias. A regressão OpenGL troca Storage por Combat e comprova
uma terceira saída visual distinta sem alterar a fila de superfícies.

Cafeteria acrescenta 1.000 seeds e exige `Clean/InUse/Faded`, hash dourado,
faixa vermelha contínua somente na parede, circulação polida, quatro contatos de
mobiliário e teto uniforme. O teste OpenGL confirma sua saída própria e o gate
conjunto prova isolação entre perfis.

Laboratory acrescenta 1.000 seeds `Sterile/Operational/Contained`, hash dourado,
módulo científico com dois indicadores, piso técnico preciso, mancha localizada
e ocupação luminosa efetiva de 20–40%. O OpenGL confirma sua saída própria.

Technical Area acrescenta 1.000 seeds `Serviced/Working/Overloaded`, hash
dourado, tubo volumétrico contínuo, óleo/grade controlados e uma luminária
protegida. A regressão OpenGL comprova sua variante própria.

Security acrescenta 1.000 seeds `Secured/Monitored/Alert`, hash dourado, quatro
indicadores agrupados, vermelho restrito ao console, tampa central e 20–35% de
luz no teto. O OpenGL confirma sua variante própria.

Central Hub acrescenta 1.000 seeds `Open/Active/Rerouted`, os três eixos de
rota, hash dourado, placa ciano única, caminho contínuo e iluminação alinhada.
O gate conjunto exige 8.000 hashes únicos entre os oito perfis e o OpenGL
confirma a oitava variante.

## Distribuição

Além de ranges, detectar candidatos nunca escolhidos, excesso de repetição e probabilidades
que não correspondem ao design.

## Gate

Nenhuma alteração em gerador entra sem unit tests + lote procedural + comparação de métricas
+ inspeção das seeds críticas.

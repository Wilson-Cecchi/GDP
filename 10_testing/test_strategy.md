# Testing — Estratégia

> **Estado:** especificação técnica inicial do novo GDP.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Camadas

```text
unit
→ subsystem
→ headless procedural
→ integration
→ playtest
```

## Unit

Hash/RNG, math, bounds, connectors, handles geracionais, ciclo de vida de
entidades, Weapon, hitscan, damage, Worker, Guard, Officer, texture operators e config.
Receitas procedurais também possuem validação e hashes dourados próprios.

## Subsystem

MapGraph, RoomModule, Corridor, WorldValidator, população inicial de entidades,
Combat, Worker/Guard/Officer/Engineer AI, EnemyGenerator, RoomMaterialGenerator, TextureGenerator e
AssetCache.

O gate de loot gera 1.000 layouts de sala, exige hashes únicos, orçamento sem
excesso, placement dentro de `lootZones`, separação entre drops e distribuição
de Health/Ammo. Testes adicionais cobrem Key obrigatória em Secret, escala de
dificuldade, coleta automática, capacidades máximas, distância, `roomId`,
jogador morto e destruição adiada. A regressão OpenGL diferencia os três pickups.

O gate de props gera 1.000 layouts, exige hashes únicos, orçamento exato,
footprints contidas, separação e cobertura dos três tipos. Testes funcionais
cobrem dano, reparo, três estados, colisão por sala, prioridade de hitscan,
cobertura de inimigo, proteção de terminal e drop único. A regressão OpenGL
diferencia tipo/estado e confirma a composição com inimigos e pickups.

O gate de eventos gera 1.000 planos, cobre os cinco tipos, hashes únicos,
triggers, waves, zonas e recompensa. Testes de runtime cobrem emboscada do
terminal, entrada, espera do encontro inicial, duas ondas, ownership, lockdown,
suporte, conclusão e proteção contra reativação/drop duplicado.

O gate de iluminação gera 1.000 planos, cobre seis moods, hashes únicos,
papéis/temas, bounds, ambiente legível, pontos e luminárias. As seeds críticas
validam todas as salas e preservam o fingerprint estrutural. A regressão OpenGL
confirma múltiplas luzes e a mudança temporária para alerta de lockdown.

O gate de áudio gera 1.000 receitas com hashes únicos e sintetiza as dezenove
categorias sem samples inválidos ou clipping. Testes do mixer silencioso cobrem
cache, voice budget, descarte, loop, pan, distância e os buses Effects/Ambient/UI.
`updateUi()` deve concluir a voz de interface sem mover os cursores congelados
dos outros buses. Integrações SDL usam driver dummy para saída e driver
inexistente para provar fallback não fatal.

O gate do Player também cobre passos e movimento cosmético por distância real.
Head bob exige fase determinística, amplitudes máximas, fade parado/aéreo,
proteção contra relocação e reset. O teste mantém o offset fora da câmera lógica,
preservando a origem usada por interação e combate.

O gate de progressão cobre valores por fonte, curva/limite, múltiplos níveis no
mesmo award, resto de XP, telemetria e entradas inválidas. Também exige os nove
benefícios reais de Player/Weapon no nível 10 e valida o estado após cada
sequência representativa.

O gate de sessão exige Playing inicial, Boss real e vivo antes de Victory,
estado terminal idempotente, Defeat por morte e rejeição de restart prematuro.
Também separa robôs por `roomId`, distingue vivo/morto e gera mundos válidos,
estruturalmente distintos e reproduzíveis para a seed derivada da segunda run.

O gate de travessia percorre todos os corredores de um mundo representativo nos
dois sentidos, exige edge/destino corretos, spawn dentro dos bounds, distância e
orientação válidas. Também cobre preservação de HP/inventário/upgrades,
rejeição de coordenadas inválidas e colisão nas quatro rotações. O mesmo gate
simula movimento por todos os segmentos, sem teleporte, resolve a sala destino
automaticamente e prova que porta individual fechada e lockdown impedem a saída.
O runtime cobre abertura parcial bloqueada, limiar passável, ocupação, hold,
fechamento, reversão e lock. OpenGL desloca e gira a sala, diferencia porta
aberta, em movimento e lockdown, e detecta a geometria adicional
com a câmera dentro de um corredor. A regressão multissala mantém duas chaves de
material preparadas ao mesmo tempo, troca apenas o perfil da sala conectada e
exige diferença de pixels; o mesmo teste cobre o overlay terminal no viewport.

O gate de assets gera 1.000 recipes visuais, valida pixels e regras materiais,
exercita reuse do cache e mantém o fingerprint estrutural do World separado. O
gate do Worker gera outras 1.000 seeds, valida montagem/mesh/albedo/hash e exige
cobertura de todos os quatro perfis, 18 peças, três famílias materiais, quatro
estados de desgaste, assimetria e zero fallbacks.
O gate do Guard gera mais 1.000 seeds, exige três perfis, 18 peças táticas, três
famílias materiais, quatro estados de desgaste, arma longa, assimetria e zero
fallbacks. A união dos dois papéis precisa manter 2.000 hashes únicos.
O gate do Officer acrescenta 1.000 seeds, três perfis, 18 peças de comando, três
famílias materiais, quatro estados de desgaste, sidearm e módulo de rádio. A
união dos três papéis precisa manter 3.000 hashes únicos. Testes de combate
confirmam comando limitado por range e sala, cooldown próprio e line of sight
para o disparo.
O gate do Engineer adiciona mais 1.000 seeds, três perfis, 18 peças, três
famílias materiais, quatro estados de desgaste, ferramenta técnica e mochila
diagnóstica. O lote dos quatro papéis mantém 4.000 hashes únicos; combate valida
reparo de aliado/prop, alvo mais próximo, cooldown e bloqueio por sala/sólido.
O IndustrialRobot soma 1.000 seeds, três perfis pesados, 18 peças, três
famílias metálicas, quatro estados de desgaste, núcleo/sensor, braços, dois
reatores e arma obrigatórios. O lote dos cinco papéis mantém 5.000 hashes
únicos; combate valida as três fases de HP, envelopes, dano/cadência,
line of sight, colisão, sala, cooldown e morte.
O gate de materiais industriais rasteriza Wall/Floor/Ceiling para 1.000 room
seeds, verifica todas as condições de manutenção, hashes únicos, regras de
composição e integração OpenGL determinística por pixels.
O mesmo executável rasteriza mais 1.000 seeds de Storage, cobre
`Orderly/Dusty/Neglected`, protege um hash dourado independente e testa regras
de oxidação, concreto, logística, arrasto e iluminação irregular.
Outras 1.000 seeds de Combat cobrem `Ready/Active/Scarred`, hash dourado,
blindagem, impactos localizados, delimitação de arena e teto reforçado. A
regressão OpenGL compara os perfis no mesmo framebuffer.
O lote da Cafeteria adiciona 1.000 seeds `Clean/InUse/Faded`, protege faixa
institucional, piso lavável, contatos de mobiliário e iluminação uniforme. A
isolação conjunta abrange os perfis já implementados.
Laboratory adiciona 1.000 seeds `Sterile/Operational/Contained`, hash dourado,
indicadores limitados, contaminação localizada e coverage efetivo de luz. A
isolação conjunta cobre todos os perfis implementados.
Technical Area adiciona 1.000 seeds `Serviced/Working/Overloaded`, hash dourado,
tubos, óleo, grade e teto categorizado.
Security adiciona 1.000 seeds `Secured/Monitored/Alert`, hash dourado, console
com quatro indicadores, alerta vermelho confinado, tampa central e teto com
20–35% de luz.
Central Hub adiciona 1.000 seeds `Open/Active/Rerouted`, cobre os três eixos,
protege uma placa ciano única, rotas contínuas, luzes alinhadas e ausência de
amarelo decorativo. A isolação conjunta exige 8.000 hashes distintos entre os
oito perfis.

## Headless

Gerar 100 → 1.000 → 10.000 seeds conforme maturidade. Toda falha imprime seed, tentativa,
motivo e comando de reprodução.

O teste `first_gameplay_headless` executa 900 fixed updates do loop real e só
passa se o Worker da START produzir a transição `Player died`.

## Golden seeds

Manter seeds para casos: simples, denso, loops, secret, boss, stress e falhas históricas.

## Visual regression

Salvar dumps de seeds conhecidas. Mudanças intencionais precisam ser revisadas, não aceitas automaticamente.

Regressão visual técnica e aprovação artística são gates independentes. Para
uma revisão de textura, primeiro registrar e aprovar a proposta conforme
`visual/TEXTURE_APPROVALS.md`; somente então implementar e atualizar os dumps.
Um framebuffer estável não significa que a direção visual foi aprovada, e uma
prancha aprovada não permite aceitar pixels ausentes, NaN ou perda de contraste.

O gate estrutural da Fase 20 valida os 23 módulos, as três classes de tamanho,
os papéis especiais e todos os connectors nas quatro rotações. O batch de 1.000
mundos exige AABBs sem overlap e corredores limitados; o de 500 mundos percorre
cada ligação nos dois sentidos. A regressão OpenGL inclui uma sala conectada
grande para impedir o retorno das dimensões fixas no renderer.

O gate do menu principal cobre estado inicial seguro, navegação com wrap,
seleção por ponteiro, ativação única e posição absoluta do mouse preservada
entre frames. O teste CPU valida o mesmo layout responsivo usado pelo hit test;
a regressão OpenGL diferencia `START RUN` de `EXIT GAME`. A integração
`main_menu_defers_world_generation` abre duas frames no driver offscreen e
falha caso uma seed de mundo seja gerada antes da confirmação. Os bootstraps
headless continuam criando a run diretamente para exercer o gameplay real.
Receitas distintas protegem navegação, confirmação e retorno; a Application só
emite navegação quando a seleção realmente muda e usa transições de input, sem
repetição por tecla mantida.

O gate da pausa cobre navegação e seleção direta apenas quando o estado está
ativo, rejeita opções inválidas e valida os cinco hit targets responsivos em
resoluções ampla, compacta e mínima. Os centros dos botões são consultados em
coordenadas SDL com origem superior, enquanto o renderer usa os mesmos retângulos
em coordenadas OpenGL; pontos externos não produzem seleção.

O menu de resultado possui gate próprio para estado inativo, opção segura,
wrap, seleção direta, comandos e reset. O teste CPU cobre `ResultMenuLayout` e
seus quatro hit targets nas mesmas classes de resolução; a regressão OpenGL
exige mudança de pixels ao alternar entre próxima seed, repetição e saída.

O timer de sessão é exercitado em ticks inteiros: 600 avanços equivalem a dez
segundos, estado terminal congela o total e restart zera a próxima run. O teste
de apresentação protege fronteiras de tick, segundo e hora; a regressão OpenGL
compara o mesmo HUD com e sem `TIME`, impedindo que o dado deixe de ser exibido.
O contrato de modo também exige Standard como default, seleção de Speedrun
somente antes do primeiro tick, contagem durante menus no Speedrun, bloqueio de
troca durante a run e preservação do modo no retry. O layout do título protege
os seis hit targets e seu espaçamento em resoluções ampla e compacta.

O recorde pessoal possui regressão própria para elegibilidade, primeiro tempo,
melhoria estrita, empate, resultado pior e isolamento por versão das regras. O
store cobre ausência, round trip, substituição atômica, temporário, campos
futuros, dado malformado, schema desconhecido e regras incompatíveis. A
regressão OpenGL exige mudança de pixels quando `BEST` entra no HUD de Speedrun.

O contrato de restart cobre as duas políticas: `SameSeed` incrementa somente a
tentativa, enquanto `NextSeed` incrementa o índice da sequência e deriva o mundo
seguinte. Repetir uma seed e depois avançar deve produzir exatamente
`seedForRun(initialSeed, 1)`, sem saltos. O menu emite um comando dedicado para
`REPLAY SEED`, incluindo navegação, seleção direta, mouse e pixels OpenGL.

O desafio do recorde exige que `selectRunSeed()` aceite a seed antes da
contagem e rejeite alterações depois do primeiro tick. O menu mantém `BEST SEED`
aberto para a Application validar disponibilidade; renderer e teste OpenGL
diferenciam o estado indisponível do botão ativo com tempo persistido.

O contrato da Campanha exige início no andar um, avanço exclusivo após Victory,
seeds derivadas para os andares dois e três, rejeição após Defeat e bloqueio
além do limite. Replay preserva andar/seed e nova campanha retorna ao primeiro
andar. Layout e hit test incluem as sete opções do título; a regressão OpenGL
diferencia seleção de `CAMPAIGN`, timer `FLOOR n/3` e resultado `FLOOR CLEAR`.

O carry-over da Campanha possui gate próprio para round trip de HP, Armor,
capacidades, chaves, upgrades, munição, nível, XP e score. O mesmo teste exige
que o spawn do novo andar permaneça intacto e o cooldown seja zerado. Casos com
NaN, munição acima da capacidade ou ranks inconsistentes são rejeitados antes
de alterar os sistemas de destino. Restaurar novamente o snapshot de entrada
também deve desfazer dano e munição gastos durante uma tentativa repetida.

O checkpoint persistente possui gate isolado para ausência, round trip integral,
substituição atômica, remoção idempotente, temporário, comentários/campos
futuros, valores malformados, schema desconhecido, regras incompatíveis, NaN e
seed inconsistente. `SessionSystem` aceita sua restauração apenas antes do
primeiro tick e sem mutação parcial em entradas inválidas. Layout, mouse e pixels
OpenGL diferenciam `CONTINUE --` de `CONTINUE F2/3`.

O gate da pressão de Campanha exige multiplicadores exatos de `1.00x`, `1.10x`
e `1.20x`, clamp de andares inválidos, configuração neutra idêntica ao legado e
fallback para valores não finitos. População do mundo confirma a escala da vida
sem reordenar arquétipos; o HUD OpenGL renderiza também `THREAT 110%`. A versão
2 das regras impede que checkpoints do balanceamento anterior sejam retomados.

O gate da Fase 83 exige Ammo + Armor em DeadEnd sob toda a faixa de pressão da
Campanha e observa Health, Ammo e Armor em 1.000 seeds de salas normais. A
população representativa precisa conter Armor e continuar correspondendo
exatamente aos layouts validados. Stores isolam checkpoint da regra 2 e recorde
de Speedrun da regra 1 após a atualização de balanceamento.

O gate da Fase 84 exige `Key + Health + Weapon` determinísticos em Secret e o
novo hash dourado `0x8F73F0890E80B4C9`. Equipar a Carbine valida atributos,
munição, cooldown, preservação de Firepower/Capacity e rejeição de duplicata.
Coleta, população, pixels OpenGL e carry-over cobrem o novo tipo; o store exige
schema 2 e round trip da identidade, isolando versões antigas de checkpoint e
recorde.

O gate da Fase 85 exige `Ammo + Special` determinísticos em Arena sob pressão
de Campanha, presença do pickup no mundo populado e coleta sem desperdício.
Combate valida 150% de dano, intervalo de 65%, timer pelo fixed update,
expiração e bloqueio de duplicata. Carry-over prova que Overdrive não atravessa
andares; HUD/pixels cobrem o marcador `OD`, e stores isolam as regras anteriores
4/3 das regras atuais Campaign/Speedrun 5/4.

O gate da Fase 86 executa 360 fixed updates com uma barreira sólida entre um
Worker e o Player: o inimigo precisa sair do bloqueio e repetir exatamente a
posição final com a mesma seed. As regressões anteriores continuam cobrindo
movimento direto, recuo, line of sight, sala e morte. Stores isolam as regras
5/4 das regras atuais Campaign/Speedrun 6/5.

O gate da Fase 87 inicia dois Workers na mesma coordenada, executa 360 passos e
exige separação horizontal mínima de `0,65 m`. Uma segunda fixture com as mesmas
identidades deve reproduzir ambas as posições exatamente. Campaign/Speedrun
isolam as regras anteriores 6/5 das atuais 7/6.

O gate da Fase 88 acompanha oito Workers convergindo ao mesmo alvo durante 600
passos e testa todos os pares a cada atualização. Nenhum par pode reduzir a
distância horizontal abaixo de `0,68 m`, e duas execuções devem terminar em
posições idênticas. Os stores isolam regras 7/6 das atuais Campaign/Speedrun 8/7.

O gate da Fase 89 testa a política de lançamento isoladamente: `--seed` vence
em modo gráfico e headless, headless sem argumento mantém a seed fixa e a mesma
entropia reproduz o mesmo valor. Entropias distintas precisam gerar seeds e
fingerprints estruturais diferentes. Os bootstraps headless existentes continuam
determinísticos e a geração procedural permanece inalterada.

O gate da Fase 90 amostra Idle, Walk, Attack e Dead diretamente. Ele exige
repetição exata para tick/seed iguais, defasagem entre seeds, valores finitos e
positivos, limites da respiração, passada mais legível e impulso próprio do
ataque. Também confirma que inimigos dormentes ficam estáticos e que Death
mantém a pose terminal anterior. A regressão OpenGL compara o mesmo Worker em
Idle e Attack e exige diferença material de pixels, cobrindo a aplicação real
da pose pelo renderer.

O gate da Fase 91 movimenta um Worker pelos eixos `-X` e `+X`, verifica yaw
`-90°/+90°`, exige que Attack encare o jogador mesmo sem passo de movimento e
que Idle/Death preservem a última direção. Um Guard confirma o recuo olhando
para o alvo, não para a direção de fuga. O Core valida a matriz de yaw e seu
fallback para ângulo não finito; a regressão OpenGL exige pixels distintos ao
girar o mesmo asset em 90°.

O gate da Fase 92 verifica o avanço parcial exato de Chase (`480°/s`) e Attack
(`720°/s`), convergência quando há tempo suficiente e travessia de
`-180°/180°` pelo arco mais curto. Reposition confirma que o orçamento angular
é aplicado uma única vez enquanto o inimigo recua olhando para o jogador;
Idle/Death continuam preservando a orientação.

O gate da Fase 93 exige que dano não letal inicie e reinicie a janela Hit de
`0,12 s`, que o fixed step a reduza com clamp em zero e que Death a limpe. A
amostragem CPU confirma a pose comprimida estável; a regressão OpenGL compara o
mesmo Worker em Idle e Hit e exige diferença material de pixels, cobrindo pose,
tint e emissivo no pipeline real.

O gate da Fase 94 separa receitas representativas de Worker e IndustrialRobot
em 8/9 partes, valida cada mesh e exige que a soma de vértices/índices seja
idêntica à geometria montada. Também verifica pivôs de raiz, pescoço, ombro e
quadril. A amostragem CPU cobre neutralidade, oposição dos membros em Walk,
recuo proporcional de Hit e entradas dormentes/inválidas. O Core valida pitch
de `90°` e fallback não finito; OpenGL e bootstrap offscreen cobrem o draw real
multipartes.

O gate da Fase 95 amostra todos os cinco papéis no mesmo tick e exige assinaturas
Walk distintas e ordenadas, assimetria do braço da ferramenta do Engineer,
avanço positivo da ferramenta corpo a corpo do Worker, recuo negativo da arma
do Guard e resposta de Hit mais contida no IndustrialRobot. As regressões
OpenGL e bootstrap continuam cobrindo a aplicação real dos perfis.

O gate da Fase 96 verifica inicialização sem transição sintética e durações
fixas de 6 ticks para Idle/Walk, 3 para entrada de Hit e 18 para Death. A queda
precisa terminar exatamente, escolher lados opostos para seeds de paridades
distintas e preservar escala unitária inclusive no meio do blend. O Core cobre
`rotationZMatrix` em `90°` e fallback não finito; renderer OpenGL e bootstrap
offscreen continuam validando a composição real.

O gate da Fase 97 exige escala unitária durante toda a morte, endpoints exatos
na entrada e no assentamento e movimento intermediário diferente de ambos. Ele
confirma articulação distinta de cabeça e braços, lados opostos dos membros,
variação repetível pela mesma seed e amplitude do IndustrialRobot inferior a
60% da Worker. A regressão OpenGL compara o ragdoll intermediário com o cadáver
acomodado e exige diferença material de pixels.

O gate da Fase 98 protege a transferência de peso que não era capturada apenas
por endpoints: a raiz preserva a pose até 8%; aos 12% as pernas já precisam
reagir enquanto a cabeça ainda espera; no meio da janela o torso deve ter acelerado.
Também exige 36 ticks para Worker, 52 para IndustrialRobot, conclusão exata e
as regressões CPU/OpenGL anteriores. Assim, uma futura simplificação não pode
voltar a sincronizar todas as partes em uma queda curta.

O gate da Fase 99 amostra 128 `visualSeed`s e exige quedas para frente, trás,
esquerda, direita e diagonais, repetição exata para a mesma seed e inclinação
terminal constante de `88°`. Também confirma que a ferramenta ainda difere da
pose terminal em 99%, que todas as partes coincidem com seus alvos em 100% e
que as novas durações são 48 ticks para Worker e 66 para IndustrialRobot. As
regressões OpenGL continuam comparando queda intermediária e cadáver assentado.

O gate da Fase 100 executa 240 fixed ticks de patrulha para Worker, Guard,
Officer, Engineer e IndustrialRobot. Cada papel precisa mover, alcançar ao
menos um waypoint, entrar em pausa, permanecer dentro do volume navegável e
repetir posição, yaw, índice e estados exatamente em uma segunda simulação.
Também confirma origem inicial no payload e congelamento integral quando o
Player morreu. Os testes de combate anteriores continuam protegendo detecção,
prioridade de ataque, desvio e separação.

O gate da Fase 101 cobre Worker, Guard, Officer, Engineer e IndustrialRobot com
o mesmo ciclo: aquisição em volume aberto, perda de visão por sólido, movimento
somente até a última posição observada, readquisição imediata e expiração para
Patrol/Idle. A fixture é repetida exatamente e compara posição, memória, timer e
estados; casos sem observação anterior também garantem que paredes não iniciem
Chase. Morte limpa a memória e os testes de persistência isolam regras 10/9 das
regras 9/8.

O gate da Fase 102 exige que a START populada contenha exatamente uma cobertura
industrial fixa de `0,46 × 2,25 × 2,80 m`, 180 HP, seed visual isolada e escala
horizontal coerente com a rotação da sala. Uma regressão de gameplay testa três
pontos ao longo de dois metros atrás do volume, todos sem line of sight, confirma
que sair pela borda restaura visão e que o Player percorre o lado oculto sem
colidir. O headless de 900 ticks continua inicializando e encerrando a run sem
falha; regras anteriores 10/9 permanecem isoladas.

O gate da Fase 106 fecha uma barreira em U ao redor de um Worker e exige uma
rota que inicialmente se afaste do Player, saia pela abertura e só então se
aproxime. A simulação de 720 fixed ticks precisa atravessar o lado externo,
alcançar a frente da barreira e repetir posição e decisão exatamente. Os testes
anteriores continuam protegendo o desvio local e os volumes entre inimigos.

O gate da Fase 107 posiciona o Player primeiro atrás e depois à frente de cada
um dos cinco arquétipos, com geometria livre e sem avanço de tempo. Pelas costas
nenhuma memória ou ação de combate pode surgir; no cone frontal a aquisição deve
acontecer no mesmo tick. A população também confirma que o Worker da START nasce
orientado para o Player independentemente da rotação da sala.

O gate da Fase 108 envia dois Workers de identidades consecutivas contra uma
barreira simétrica. Durante 300 fixed ticks o conjunto precisa registrar
passagem pelos lados esquerdo e direito, manter os volumes físicos e reproduzir
as posições finais e decisões laterais em uma segunda execução.

O gate da Fase 109 exige que hits em Enemy, robô, crate, terminal, sólido e fim
do volume navegável produzam posição e classe material coerentes. Miss,
cooldown e arma vazia continuam sem impacto. O teste CPU do renderer diferencia
as três assinaturas, protege o fade monotônico e sanitiza intensidade não
finita; as suítes OpenGL e bootstrap continuam cobrindo o passe real.

O gate da Fase 110 protege a curva `smoothstep` das portas em 0%, 25%, 50%,
75% e 100%, incluindo clamp e NaN. Passabilidade deve mudar exatamente quando
a abertura visual cruza 90%; uma porta em fechamento com passagem ocupada deve
emitir uma única reversão, voltar a `Opening` e aumentar sua abertura no mesmo
fixed tick. Travessia, headless e bootstrap OpenGL validam a integração.

O gate da Fase 111 valida que fundo, painel, borda e seleção possuem hierarquia
de contraste, que texto primário supera instruções secundárias e que ciano,
vermelho e verde preservam semânticas distintas. As regressões OpenGL existentes
continuam exigindo pixels diferentes para telas, seleções e valores, enquanto
os testes de layout/mouse garantem que a revisão não deslocou hit targets.

O gate da Fase 112 amostra em CPU repouso, recuo específico de Pistol/Carbine,
pico e retorno da recarga, entrada por baixo e sanitização de valores inválidos.
A regressão OpenGL compara cenário sem arma, Pistol, Carbine e disparo com flash,
exigindo mudança material de pixels e ausência de erro GL. Builds headless
continuam garantindo que o novo contrato visual não cria dependência gráfica no
combate.

O gate da Fase 113 cria uma luminária não sólida diante de um ray horizontal:
o primeiro disparo precisa produzir dano e impacto metálico, o segundo deve
destruí-la sem loot e o terceiro precisa atravessar seu volume antigo. A
regressão OpenGL compara iluminação intacta, reduzida e apagada, exigindo mudança
real de pixels nos dois estados e ausência de erro GL. Bootstrap headless cobre
a materialização determinística dos fixtures junto ao mundo.

O gate da Fase 114 renderiza a mesma sala e o mesmo plano de oito luzes com o
budget de sombras desligado e depois com um AABB conhecido. Mais de 100 canais
do framebuffer precisam mudar, sem alterar geometria, luzes ou câmera e sem erro
OpenGL. CPU/bootstrap continuam protegendo limites, defaults sem sombra e a
integração da aplicação.

O gate da Fase 115 fornece oito luzes, uma apagada, nove AABBs próximos, um fora
do alcance e um inválido. A seleção deve conservar sete luzes ativas, habilitar
sombras somente nas duas mais próximas, enviar oito volumes e repetir bytes
exatos. Um plano inteiramente apagado precisa produzir budget zero. OpenGL
solicita deliberadamente oito luzes de sombra e exige pixels idênticos ao pedido
já limitado em duas, provando o clamp defensivo do renderer.

O gate da Fase 116 adiciona `windows-release` à CI. O runner Windows x64 compila
com MSVC, executa todos os testes registrados que não dependem do backend
offscreen Unix, gera `GDP-Windows-x64.zip`, verifica a presença de `gdp.exe` e
`SDL3.dll` e inicia três frames headless usando somente o conteúdo extraído.
Assim, sucesso de compilação não é confundido com um pacote realmente executável.

O gate da Fase 117 alimenta o monitor com valores conhecidos e verifica janela
fixa, média, P95, pior frame, violações do budget, reset e sanitização de NaN,
infinito e tempos negativos. Input neutro e SDL protegem o atalho `F3`. A
regressão OpenGL exige mudança real de pixels com o overlay ativo e confirma que
draw calls, triângulos e caches são contabilizados. Os bootstraps headless passam
a imprimir um resumo de desempenho sem depender de janela ou alterar a seed.

O gate da Fase 118 valida os cinco perfis, seus budgets exatos, clamps e nomes
persistentes. A regressão OpenGL renderiza o mesmo conteúdo a 100% e em
`POTATO`/50%, exigindo mudança real de pixels e ausência de erro GL; o retorno ao
framebuffer nativo antes do HUD protege legibilidade e coordenadas de interface.
O playtest no W541 permanece o gate de calibração de FPS para `POTATO`, `LOW` e
`MEDIUM`, sem transformar uma máquina rápida de CI em referência de hardware.

O gate da Fase 119 testa câmera à frente, volume atrás, lateral fora da margem,
volume contendo a câmera e fallback para bounds inválidos. Bounds de corredor
devem incluir largura, altura e toda a linha central; caminhos vazios são
rejeitados. As 48 suítes existentes continuam protegendo geração, gameplay,
pixels OpenGL e os cinco perfis. O gate manual verifica portas durante giro e a
queda dos contadores de draw ao olhar para fora do conteúdo visível.

O gate da Fase 120 valida sanitização e média exclusiva de amostras GPU
disponíveis no monitor. A regressão OpenGL executa frames suficientes para
percorrer o anel de quatro queries, sincroniza somente pela leitura de pixels já
existente e exige tempo finito e não negativo. Debug, Release, ASan/UBSan e o
pacote Windows continuam cobrindo fallback, lifecycle e portabilidade.

O comando `MAIN MENU` é protegido nos estados puros de pausa e resultado. A
integração mantém `SessionSystem` como dono do encerramento/restart e reinicializa
o renderer no contexto existente; a validação manual deve confirmar que a janela
não fecha e que `START RUN` inicia a seed seguinte.

O gate de confirmação de saída cobre estado inativo, seleção segura inicial,
navegação normalizada, wrap, seleção direta, cancelamento e emissão única do
comando final. O teste de layout valida os dois hit targets em resoluções ampla,
compacta e mínima; a regressão OpenGL exige que o modal altere a tela do menu
principal. A Application abre o mesmo estado a partir do título, pausa e
resultado, preservando o menu de origem até cancelar ou confirmar.

O gate de configurações cobre origem título/pausa, wrap, seleção direta, retorno,
passos e clamps de todos os valores. Input neutro e adaptador SDL protegem as
novas setas laterais; gameplay confirma a aplicação da sensibilidade. O teste
CPU percorre as doze linhas do layout em resoluções ampla, compacta e mínima, e
a regressão OpenGL exige pixels distintos para tela, seleção e valor de
`VSYNC`.

A regressão de setas percorre todas as onze linhas ajustáveis e confirma que os
dois hit targets ficam dentro da linha: `<` sempre retorna direção negativa e
`>` sempre retorna positiva. Cliques no label, no valor, fora do painel ou com
dimensões inválidas não ajustam configurações. Layouts de 640×360 e 320×200
também verificam que as doze linhas não se sobrepõem quando há espaço disponível.

O gate de persistência usa diretório temporário isolado e cobre arquivo ausente,
round-trip das onze configurações/doze campos, substituição de arquivo existente,
remoção do temporário, campos ausentes/desconhecidos, corrupção parcial, clamps, versão
ausente, migração v1/v2/v3/v4/v5→v6 e versão incompatível. Gameplay compara deltas
verticais normal e invertido, garantindo yaw inalterado. Os bootstraps headless
e OpenGL confirmam que a integração não altera o ciclo de inicialização; headless
não toca nas preferências reais do usuário. Um bootstrap adicional configura um
`XDG_DATA_HOME` temporário, inicia SDL offscreen com `fullscreen = true`, exige a
flag real de fullscreen no log e remove o diretório isolado ao terminar.
Outro bootstrap isolado carrega 960×540 em modo janela e exige esse tamanho real
no log, cobrindo a aplicação do preset sem acessar preferências do usuário.
Um terceiro grava `vertical_sync = false`, inicia o contexto OpenGL offscreen e
exige no log que o intervalo de troca efetivo permaneça desativado.

O gate da Fase 132 executa três benchmarks curtos de `POTATO` em isolamento,
exige as amostras `RUN 1/3` e `RUN 3/3`, uma linha `BENCHMARK_MEDIAN` e os
contadores estruturais estáveis. O runner rejeita repetição zero/par ou perfil
desconhecido, e uma divergência de draws, triângulos, conteúdo visível, luzes,
sombras ou instancing invalida a comparação antes de publicar a mediana.

O gate da Fase 133 exige que o contexto OpenGL exponha vendor, renderer e versão
não vazios. O smoke de benchmark cobra os três tokens tanto na linha individual
quanto na mediana; o runner inclui a identidade na assinatura invariável das
repetições. Validação offscreen comprova o contrato, mas seleção Intel/NVIDIA é
confirmada somente em janela real.

O gate da Fase 134 dispara contra cabeça, torso e sensor do IndustrialRobot.
Ele exige classificação `WeakPoint`, multiplicador exato de `1,75x`, dano-base
inalterado no torso e material metálico preservado no robô. O teste de feedback
também distingue weak hit e weak kill dos marcadores comuns. A suíte completa
protege seleção do alvo mais próximo, bloqueio por parede, impacto, progressão e
pixels do HUD; o playtest confirma se o volume menor é legível durante combate.

## Sanitizers

Quando disponíveis, usar AddressSanitizer e UndefinedBehaviorSanitizer.

O preset CMake `sanitizers` ativa ambos em GCC/Clang e executa a mesma suíte de
Debug/Release. Erros de memória ou comportamento indefinido usam `halt_on_error`.
LeakSanitizer fica desativado nesse preset para não atribuir ao GDP alocações
persistentes dos drivers e bibliotecas gráficas externas.

O teste de materiais HD possui teto de 300 segundos: a instrumentação completa
de ASan/UBSan pode ultrapassar 120 segundos sem alterar o resultado funcional.
Os demais testes mantêm limites curtos e específicos para detectar travamentos.

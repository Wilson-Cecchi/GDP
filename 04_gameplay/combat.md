# Gameplay — Combate, armas e dano

> **Estado:** pistola, munição, cooldown, recarga, hitscan, dano e morte
> implementados e validados na Fase 7; Guard armado de média distância validado
> na Fase 11, junto do Officer de suporte, Engineer de reparo e IndustrialRobot
> de três fases. Props destrutíveis e cobertura dinâmica foram validados na
> Fase 12. Eventos procedurais de sala estão funcionais; Armor do Player foi
> integrada na Fase 58. A Fase 82 adicionou pressão progressiva por andar na
> Campanha, a Fase 84 integrou a Carbine encontrada em salas Secret e a Fase 85
> adicionou o Overdrive temporário encontrado em Arenas. A Fase 93 adicionou
> reação visual transitória ao dano de inimigos e a Fase 109 passou a publicar
> ponto, normal e material de cada impacto. A Fase 112 conectou eventos já
> aceitos de disparo, recarga e equip aos snapshots do viewmodel. A Fase 113
> tornou luminárias alvos metálicos destrutíveis. A Fase 134 adicionou weak
> points determinísticos e feedback próprio. A Fase 135 adicionou spread
> determinístico por rajada. A Fase 136 tornou a Carbine automática sem alterar
> a Pistol semiautomática. A Fase 137 tornou a recarga uma ação temporizada e
> autoritativa. A Fase 138 adicionou dois slots de arma com troca real. A
> Fase 139 tornou a entrada após equip/troca temporizada e autoritativa. A
> Fase 140 tornou essa seleção mais rápida, interruptível e inverteu os atalhos
> para priorizar a Carbine em `1`. A Fase 144 acrescentou o lote local de sinais
> de combate; uma fila global permanece para fases posteriores, se necessária.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Pipeline

```text
Fire input
→ Weapon valida cooldown/munição
→ raycast ou projétil
→ HitResult
→ DamageSystem
→ eventos visuais/sonoros
```

## Hitscan

O ray lógico parte da câmera/retícula e para no primeiro bloqueador válido. O cano visual não
define a direção real do tiro.

`fireHitscan()` normaliza a direção, limita pelo alcance, consulta somente
Enemies vivos e Props sólidos no `roomId` do jogador e compara o primeiro AABB
de alvo com o primeiro solid ou limite da sala. O resultado distingue cooldown,
vazio, miss, impacto no mundo, prop protegido, hit, kill e prop destruído. Todo
resultado que realmente toca uma superfície inclui `ImpactResult`: posição do
primeiro contato, normal oposta ao raio e material `Metal`, `Composite` ou
`Flesh`. Miss, cooldown e arma vazia não fabricam impacto.

Na Fase 134, `HitResult` também identifica `Standard` ou `WeakPoint`. Inimigos
orgânicos usam um volume menor sobre a cabeça; o IndustrialRobot usa o sensor
superior metálico. O ponto de impacto vem da superfície desse volume menor, e
um acerto válido aplica `1,75x` de dano. O volume permanece contido no AABB
geral, não atravessa paredes e não altera tiros no torso, Props ou luminárias.
O HUD distingue weak point com marcador dourado e segmentos internos, inclusive
quando o dano é letal, sem introduzir RNG ou alterar a direção do disparo.

Na Fase 135, todo disparo aceito passa por um padrão geométrico fixo de doze
amostras antes do raycast. O primeiro tiro é perfeitamente central; tiros
rápidos seguintes usam offsets horizontais e verticais limitados. A Pistol tem
envelope de `0,55°` e recuperação de `0,34 s`; a Carbine usa `0,90°` e `0,30 s`.
Cooldown e arma vazia não avançam o padrão. Recarga, troca de arma, restauração
de checkpoint ou o fim da janela de recuperação devolvem o próximo tiro ao
centro. Não há consumo de RNG e o resultado independe do framerate.

## WeaponDefinition

```cpp
struct WeaponDefinition {
    float damage;
    float fireInterval;
    float range;
    int magazineSize;
    int reserveMax;
    WeaponArchetype archetype;
};
```

A pistola inicial usa 34 de dano, intervalo de `0,22 s`, alcance de `30 m`,
pente de 8 e reserva de 32. Botão esquerdo dispara e `R` transfere a quantidade
necessária da reserva para o pente. Progressão pode aumentar dano em blocos de
4 e reserva máxima em blocos de 4 sem substituir a arma.

A Carbine da Fase 84 usa 26 de dano, intervalo de `0,13 s`, alcance de `34 m`,
pente de 12 e reserva de 48. Ao equipá-la, os bônus acumulados sobre o dano e a
reserva-base da Pistol são transportados; munição começa cheia e o cooldown é
zerado. Desde a Fase 138, a Pistol permanece guardada com sua munição; recolher
outra Carbine não altera nem reabastece nenhum slot.

Na Fase 136, a Pistol continua disparando somente na borda de pressionamento. A
Carbine aceita também o estado segurado e volta a tentar em cada fixed update;
`Weapon::tryFire()` mantém o intervalo de `0,13 s` como autoridade, portanto o
número de tiros não depende da taxa de renderização. Soltar, pausar, entrar em
menu ou encerrar a sessão desarma o estado segurado. Com pente vazio, repetição
automática é bloqueada, mas um novo clique ainda produz exatamente um dry fire.

Na Fase 137, `R` inicia a recarga sem alterar imediatamente o pente ou a reserva.
A Pistol conclui em `0,82 s` e a Carbine em `1,05 s`, sempre pelo fixed update.
Enquanto a ação está ativa, pedidos repetidos são recusados e `tryFire()` retorna
`Reloading` sem consumir munição, fabricar impacto, emitir ruído ou tocar dry
fire. A conclusão transfere somente a quantidade necessária; pausa congela o
timer, enquanto equip, restore e reconstrução da run o descartam. A Carbine
volta a disparar se o botão ainda estiver segurado após a conclusão.

Na Fase 138, `Weapon` mantém um slot ativo e outro guardado. `1` seleciona a
Pistol, `2` seleciona a Carbine e a roda alterna, sempre por comando pendente
consumido no fixed update. Cada slot conserva definição, pente e reserva. Troca
válida zera cooldown, spread e recarga, preserva o timer global de Overdrive e
desarma o gatilho segurado; selecionar o slot atual ou uma arma ainda não
encontrada não muda estado. Firepower e Capacity atualizam ambas as armas, mas
pickups Ammo abastecem somente a ativa.

Na Fase 139, adquirir ou selecionar um slot inicia uma entrada no próprio
`Weapon`: `0,34 s` para Pistol e `0,48 s` para Carbine. Durante esse período,
`tryFire()` retorna `Switching`, `reload()` e outra troca são recusados, e o
helper de trigger não solicita disparos. O timer avança apenas no fixed update,
portanto pausa o congela e alternar rapidamente não pode zerar cooldown para
burlar a cadência. Ao terminar, a arma já selecionada fica disponível. Restore
descarta a ação transitória, enquanto Overdrive continua preservado.

Na Fase 140, a Carbine ocupa o atalho `1` e a Pistol o `2`. As durações atuais
são `0,32 s` e `0,24 s`, respectivamente. `switchTo()` aceita o slot oposto
mesmo durante uma entrada: a arma ativa muda imediatamente e o timer reinicia
com a duração da nova arma. Selecionar o mesmo slot continua sem efeito. Como
fire e reload seguem bloqueados até o último timer terminar, scroll repetido
fica responsivo sem recuperar cadência ou produzir disparos intermediários.

O estado de spread é transitório e pertence à `Weapon`: índice da última
amostra, quantidade da rajada e timer de recuperação. Ele não integra
`WeaponCampaignProgress`, assim como cooldown e Overdrive; checkpoints entre
andares restauram a arma em repouso. O HUD recebe apenas força normalizada para
abrir a retícula enquanto a sequência estiver ativa.

O Special da Fase 85 ativa Overdrive por `12 s`. Enquanto o timer está ativo,
`Weapon::damage()` retorna 150% do dano atual e cada tiro usa 65% do intervalo
da arma equipada. O timer e o cooldown avançam pelo mesmo fixed update; ao
expirar, o perfil-base volta sem mutar `WeaponDefinition`. Um segundo Special
não reinicia nem acumula duração, evitando extensão infinita do efeito.

Na Fase 112, a `Application` traduz somente ações já aceitas em apresentação:
um tiro que consumiu munição inicia recuo e flash. Desde a Fase 137, o arco de
recarga lê o progresso autoritativo de `Weapon`; desde a Fase 139, a entrada de
equip/troca também lê a força autoritativa da arma. Apenas recuo, flash e a
entrada inicial da run mantêm timers cosméticos na aplicação. Cooldown, dry
fire e pickup recusado não fabricam movimento. Esses estados não
alteram `Weapon`, dano, cadência, munição ou direção do hitscan e congelam junto
com o gameplay quando um menu pausa a run.

## Dano

`applyDamage()` opera sobre Enemy ou Prop destrutível, ignora IDs inválidos,
dano não positivo e alvos terminais, faz clamp de HP em zero e produz uma única
transição terminal. Props passam por `Damaged`, perdem colisão em `Destroyed` e
um crate cria somente seu drop controlado. O multiplicador de weak point é
resolvido pelo hitscan antes desta função; dano direto mantém sua semântica.

`CeilingFixture` é a exceção alvo/navegação: não bloqueia personagens, mas seu
AABB centrado no modelo intercepta hitscan. Possui 24 HP, impacto metálico e não
gera drop ou progressão. Ao ser destruída, deixa de interceptar tiros seguintes
e permanece apagada durante a run; Engineers não restauram luminárias.

Acertos não letais em Enemy também reiniciam `hitReactionTime` em `0,12 s`.
Esse timer é avançado uma única vez por fixed update, não interrompe IA,
movimento, ataque nem cooldowns e é limpo pela transição Dead. A camada visual
o traduz para uma reação Hit; ele não faz parte do cálculo de dano.

Na Fase 58, a defesa do Player passou a consumir Armor ponto a ponto antes do
HP. O excedente do mesmo ataque alcança HP, Armor nunca fica negativo e zerar
somente Armor não mata. A regra independe do arquétipo atacante e não altera o
pipeline de dano de Enemy/Prop.

Na Campanha, a Fase 82 usa `1.00x`, `1.10x` e `1.20x` nos andares um a três.
O multiplicador escala vida, velocidade de movimento e dano de Worker, Guard,
Officer, Engineer e IndustrialRobot. Alcances, intervalos de ataque, comando,
reparo e transições de fase do Boss não mudam, evitando picos difíceis de ler.
Standard e Speedrun usam sempre `1.00x`.

## Eventos

`ShotResult` conserva o resultado autoritativo do hitscan e, desde a Fase 109,
inclui o impacto. Na Fase 144, `combatSignalsForShot()` traduz cada disparo
aceito em um lote local de até três sinais, na ordem `WeaponFired`, contato
(`EntityHit` ou `ImpactWorld`) e consequência (`EntityDamaged` ou
`EntityKilled`). Acerto em prop indestrutível só emite contato; `Destroyed`
usa `EntityKilled` com `ShotOutcome::Destroyed` para preservar a distinção de
áudio e HUD. Cooldown, troca, recarga e arma vazia não emitem disparo.

`Application` consome os sinais para recuo/flash ou golpe, ruído, efeito de
impacto, feedback de hit e áudio espacial. Arma vazia/recarga e outros sistemas
continuam usando seus próprios cues. O lote não é uma fila persistida ou global:
progressão, drops, colisões e log continuam ancorados no `ShotResult`. O sistema
de eventos de sala é independente e continua produzindo ondas, lockdown e
recompensa determinísticos. Nenhum cue altera resultado, RNG estrutural ou
ordem de combate.

## Testes

Implementados: sem munição, cooldown, recarga, parede bloqueia, filtro por sala,
alvo mais próximo, overkill, double death, miss por alcance, aproximação do
Guard, recuo sob pressão, envelope de tiro, cooldown e bloqueio de linha de
visão, comando do Officer, filtro de aliados por papel/range/sala e cooldown de
suporte independente, cura/restauração do Engineer, escolha do alvo reparável
mais próximo e bloqueio de reparo por sólido/sala. Weak points cobrem dano-base
inalterado no torso, multiplicador e classificação na cabeça, sensor metálico
do IndustrialRobot e feedback distinto. Spread cobre primeiro tiro central,
repetição exata do padrão, envelopes diferentes por arma, rejeições sem avanço,
reset por recarga/tempo e deslocamento efetivo do hitscan. O Boss também testa
limiares exatos de HP, três envelopes,
velocidades/danos/cadências distintos, recuo, perseguição, line of sight,
isolamento por sala, cooldown e estado Dead. A fábrica de configurações também
valida a escala neutra, o aumento e o fallback para entrada não finita. Props acrescentam prioridade do
alvo mais próximo, cobertura, três estados, reparo, colisão reconstruída, drop
único e proteção de terminal.
Overdrive acrescenta ativação única, aumento de dano, cadência, expiração no
fixed update, bloqueio de duplicata e restauração do perfil-base.
Hit acrescenta início e reinício do timer em dano não letal, avanço por fixed
step, clamp em zero e limpeza imediata na morte.
Impactos acrescentam ponto exato em parede, limite de volume, Enemy e Prop,
classe material para orgânicos, robôs, crates e terminais, além da ausência de
dados em disparos sem contato.
Trigger mode acrescenta Pistol semiautomática, Carbine automática, interrupção
ao soltar e supressão de dry fire repetido com pente vazio.
Troca temporizada acrescenta durações por arquétipo, avanço parcial, bloqueio
de disparo/recarga/segunda troca, hitscan sem impacto ou consumo, sincronização
do viewmodel e reset seguro no restore.
O gate da Fase 86 coloca uma barreira entre Worker e Player, executa 360 passos
e exige que o inimigo a contorne. A repetição com a mesma seed precisa terminar
na mesma posição; todos os cinco arquétipos usam o mesmo helper de movimento.
Na Fase 87, dois Workers iniciam sobrepostos e precisam separar-se durante a
perseguição sem perder determinismo. A regressão exige ao menos `0,65 m` entre
os centros após 360 passos e igualdade exata em uma segunda execução.

Na Fase 88, uma regressão densa posiciona oito Workers ao redor do mesmo alvo e
verifica todos os pares em cada um dos 600 passos. A distância entre centros não
pode cair abaixo de `0,68 m`, e a repetição precisa produzir posições finais
idênticas. Isso cobre o contato físico contínuo além da separação final.

Na Fase 100, os cinco updates de arquétipo também compartilham a patrulha local
quando o Player vivo está fora do alcance de detecção. Waypoints são derivados
da identidade estável, ficam ancorados no spawn e usam velocidade reduzida. A
mesma resolução de paredes, props, desvio e volumes físicos permanece
autoritativa; chegada gera pausa e timeout troca destinos bloqueados. Detectar
o Player interrompe a patrulha no mesmo update. Como a posição inicial do
encontro pode mudar, Campaign/Speedrun passam às regras 9/8.

Na Fase 101, todos os papéis exigem simultaneamente alcance e linha de visão
para adquirir ou atualizar o Player. Perder contato preserva somente a última
posição observada por uma janela limitada e entra em `Investigate`; a posição
real oculta nunca alimenta direção, ataque ou suporte do Officer. Readquirir
visão retoma o combate no mesmo tick, expirar retorna à patrulha e morte limpa
a memória transitória. A regressão executa o ciclo completo nos cinco
arquétipos duas vezes e exige resultados idênticos. Campaign/Speedrun passam às
regras 10/9.

Na Fase 102, uma cobertura larga e acima da altura da câmera entra na START para
transformar o gate automatizado de percepção em uma validação jogável imediata.
O mesmo AABB autoritativo bloqueia visão, hitscan e movimento; há espaço para o
Player percorrer dois metros pelo lado oculto e reaparecer pela extremidade. A
peça fica fora do fluxo central e pode ser destruída com 180 HP. A alteração de
conteúdo avança Campaign/Speedrun para regras 11/10.

Na Fase 103, todo resultado de disparo que consumiu munição — acerto, morte,
impacto em prop/mundo ou miss — gera um estímulo de gameplay de `18 m` na sala
ativa. Cada papel limita esse raio pela própria audição e grava a origem na
memória de investigação mesmo sem line of sight. Cooldown e arma vazia não
geram estímulo; inimigos mortos, dormentes ou de outra sala são ignorados. A
visão direta continua prioritária e corrige a memória no mesmo fixed update. A
regressão cobre os cinco papéis, isolamento, alcance, dados inválidos e
repetição determinística. Campaign/Speedrun passam às regras 12/11.

Na Fase 104, o primeiro ruído sem memória ativa inicia `Alert` por papel antes
da investigação. Nesse intervalo não há movimento, ataque, comando ou reparo;
o inimigo apenas gira para a origem e apresenta a postura visual de prontidão.
Outro disparo atualiza a posição ouvida e renova a busca, mas não reinicia a
pausa, impedindo stun lock por cadência. Morte e Player morto limpam o timer. A
regressão cobre os cinco arquétipos, orientação, imobilidade, término da janela,
prioridade visual e pose rígida. Campaign/Speedrun passam às regras 13/12.

Na Fase 105, a expiração da investigação inicia `Recover` por `0,65–1,20 s`.
Durante a varredura não há locomoção tática, ataque, comando ou reparo; visão
confirmada retoma combate imediatamente e ruído novo retoma `Investigate` sem
novo Alert. O término limpa a última posição conhecida e libera a patrulha. A
regressão cobre os cinco papéis, imobilidade, giro determinístico, interrupções,
limpeza por morte e pose rígida. Campaign/Speedrun passam às regras 14/13.

Na Fase 106, Chase, Investigate e Patrol calculam o próximo ponto por um grafo
de visibilidade restrito ao `CollisionWorld`. Cantos recebem a margem do
collider e arestas são amostradas integralmente, portanto sólidos e limites
continuam autoritativos. A regressão usa uma barreira em U que exige primeiro
afastar-se do Player para depois alcançá-lo, além de repetir a rota exatamente.
Campaign/Speedrun passam às regras 15/14.

Na Fase 107, aquisição visual exige também o cone frontal definido no perfil de
investigação. O cálculo usa yaw lógico, produto escalar e meia abertura por
papel antes de consultar line of sight. Um alvo visível pelas costas não cria
memória; à frente, todos os cinco papéis retomam seu comportamento no mesmo
tick. Audição permanece independente dessa restrição. Campaign/Speedrun passam
às regras 16/15.

Na Fase 108, o grafo ordena cantos simétricos pela preferência lateral estável
de cada entidade. Identidades vizinhas distribuem rotas equivalentes e o lado
de desvio físico substitui temporariamente essa preferência quando há bloqueio,
permitindo replanejamento sem sobreposição. A regressão exige dois lados usados
e resultado idêntico em simulações repetidas. Campaign/Speedrun passam às regras
17/16.

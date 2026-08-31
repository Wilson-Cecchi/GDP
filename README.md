# GDP Technical Bible

Pacote modular de documentação técnica do novo GDP.

`architecture.md` é o documento-mestre canônico. O arquivo
`GDP_arquitetura_tecnica.md` preserva a versão-fonte consolidada usada nesta
revisão e deve permanecer sincronizado com ele nas decisões arquiteturais.

## Ordem de leitura

1. `00_INDEX.md`
2. `architecture.md`
3. `01_core/`
4. `02_renderer/`
5. `03_world/`
6. `04_gameplay/`
7. `05_enemies/`
8. `06_environments/`
9. `07_interface/`
10. `08_audio/`
11. `09_persistence/`
12. `10_testing/`

Os guias antigos detalhados de peças e materiais foram preservados dentro das seções de
inimigos e ambientes como arquivos `*_reference.md`.

A prancha visual gerada está em `visual/reference_sheet_rooms_enemies.png`.
O gate e o registro obrigatório para futuras revisões estão em
`visual/TEXTURE_APPROVALS.md`.

Documentos `*_reference.md` preservam conteúdo e resultados históricos do GDP
anterior. Contratos atuais são definidos por `architecture.md` e pelos documentos
especializados das pastas numeradas.

## Estado da implementação

- **Fase 0 — Bootstrap:** implementada e validada.
- **Fase 1 — Core:** implementada e validada (`Time`, `Input`, `Random`, `Hash`,
  derivação de seeds e matemática).
- **Fase 2 — Renderer mínimo:** implementada e validada (`Shader`, `Mesh`,
  `Texture`, `Camera`, depth test e `Framebuffer`).
- **Fase 3 — Player:** implementada e validada (movimento, mouse look, gravidade,
  colisão e interação).
- **Fase 4 — World Module System:** implementada e validada (`RoomModule`,
  `RoomRole`, `RoomTheme`, connectors e transforms).
- **Fase 5 — World Generator:** implementada e validada (`MapGraph`, seleção,
  placement, connections, corridors e gates estruturais).
- **Fase 6 — Entity Manager:** implementada e validada (`EntityId` geracional,
  Player, Enemy, Item, Prop, queries e ciclo de vida adiado).
- **Fase 7 — Primeiro gameplay:** implementada e validada (Worker, pistola,
  hitscan, HP, dano e morte).
- **Fase 8 — Procedural Asset Foundation:** implementada e validada
  (`MeshRecipe`, `PrimitiveRecipe`, semântica, materiais, texturas e cache).
- **Fase 9 — Primeiro inimigo procedural completo:** implementada e validada
  (`BodyProfile`, peças/sockets, mesh, material, textura, desgaste e genoma).
- **Fase 10 — Materiais procedurais das salas:** implementada e validada nos
  oito perfis Industrial Corridor, Storage, Combat Room, Cafeteria, Laboratory,
  Technical Area, Security e Central Hub.
- **Fase 11 — Elenco:** implementada e validada; Guard, Officer, Engineer e
  IndustrialRobot possuem geração procedural, renderer e combate funcional.
- **Fase 12 — Procedural avançado:** implementada e validada; loot, props,
  eventos de sala, iluminação, áudio e progressão estão integrados.
- **Fase 13 — Vertical slice navegável:** concluída; portas e corredores
  Straight/L/Z conectam as salas, o IndustrialRobot encerra a run em Victory,
  morte encerra em Defeat e `Enter` inicia um novo mundo determinístico.
- **Fase 14 — Apresentação multissala:** concluída; salas adjacentes preservam
  materiais, decoração e entidades próprias antes da troca lógica, a luz
  ambiente mistura durante o corredor e objetivo/resultado aparecem no viewport.
- **Fase 15 — Portas e áudio contínuos:** concluída; `E` anima a abertura sem
  teleporte, portas bloqueiam/seguram/fecham fisicamente e a ambiência faz
  crossfade determinístico durante todo o corredor.
- **Fase 16 — Continuidade de encontros e luz:** concluída; a primeira onda dos
  eventos de entrada já existe dormente na sala adjacente, sem IA ou colisão de
  tiro antes do gatilho, e até oito luzes combinam sala atual e vizinhas sem
  aparecer somente após a troca lógica.
- **Fase 17 — HUD funcional:** concluída; HP, munição, chaves, nível, XP,
  objetivo, evento e prompts contextuais agora aparecem dentro do viewport em
  painéis responsivos que preservam o centro do combate.
- **Fase 18 — Feedback de combate:** concluída; dano recebido marca as bordas,
  acertos e eliminações confirmam na mira, e coleta, ondas, eventos e level-up
  geram avisos temporários determinísticos.
- **Fase 19 — Pausa funcional:** concluída; `Esc` congela a simulação e abre um
  menu responsivo com `RESUME`, `RESTART RUN` e `EXIT GAME`, liberando o mouse
  até o retorno ao gameplay.
- **Estabilização de travessia:** concluída; corredores não podem reentrar nas
  próprias salas, loops fecham apenas branches próximas, curvas possuem juntas
  abertas e lockdown aguarda o collider liberar o vão antes de bloquear a rota.
- **Fase 20 — Variedade estrutural de salas:** implementada e validada; a
  biblioteca possui módulos compactos, médios e grandes, com Hub, Arena, Boss e
  Secret usando dimensões próprias e placement derivado dos bounds reais.
- **Fase 21 — Menu principal:** implementada e validada; `START RUN` cria a
  dungeon somente após confirmação e `EXIT GAME` encerra pelo fluxo principal.
- **Fase 22 — Revisão visual aprovada:** concluída; a proposta
  `Industrial Corridor/Wall v1` foi recusada e substituída pela v2 HD, aceita no
  jogo com mapas `1024×1024`, Albedo/Normal/RMA, UV físico e filtragem trilinear.
- **Fase 23 — Separação teto/luminária:** concluída e aceita; os oito albedos de teto
  não desenham mais lâmpadas e não recebem emissão. A proposta
  `Lighting/CeilingFixture v1 HD` foi aprovada e implementada com carcaça, aro,
  junta, lente e fixadores, além de Albedo/Normal/RMA/Emission `1024×1024`.
- **Fase 24 — Teto HD do corredor:** concluída e aceita no jogo; a
  superfície usa Albedo/Normal/RMA `1024×1024`, UV físico e metalness
  predominante, com vigas, hatches, ventilação e três níveis de desgaste. Não
  existem luminárias ou emissão pintadas. As capturas Maintained, Used e Worn
  foram aprovadas antes do commit da fase.
- **Fase 25 — Fronteira inicial WKC Forge/GDP:** a composição concreta da
  aplicação foi retirada de `core` e passou para `game`, inclusive no namespace
  e no target executável. A mudança preserva gameplay, geração procedural,
  hashes e renderização enquanto inicia a separação incremental da infraestrutura.
- **Fase 26 — Contrato inicial de dependências:** o CMake agora declara
  explicitamente `gameplay -> world`, e uma regressão arquitetural impede que a
  aplicação retorne a `core` ou que `core` inclua módulos superiores do GDP.
- **Fase 27 — Input de plataforma:** o processamento de eventos SDL saiu de
  `core` e passou ao target `gdp_platform`, com teste próprio. `gdp_core` não
  publica mais SDL, e consumidores gráficos declaram essa dependência diretamente.
- **Fase 28 — Input independente de backend:** `platform/Input` agora expõe
  teclas, botões e transições neutros; `platform/SdlInput` traduz os eventos SDL
  em target separado. Estado e adaptador possuem regressões independentes.
- **Fase 29 — Domínios de seed do GDP:** `core/Seed` preserva somente o
  mecanismo genérico e versionado de derivação; o catálogo específico do jogo
  passou para `game/SeedDomains`. Os IDs 1–17 e os hashes dourados permanecem
  explicitamente protegidos por regressão.
- **Fase 30 — Fundação visual separada:** primitivas OpenGL e assets
  procedurais genéricos agora possuem targets próprios; receitas Worker,
  fallback industrial, luminária e demais geradores visuais do GDP ficam em
  `gdp_visual_content`. O renderer concreto apenas compõe essas camadas.
- **Fase 31 — Conteúdo visual agrupado:** os geradores de inimigos, materiais
  de sala e assets concretos passaram fisicamente para `src/game/visual`.
  `src/procedural` contém somente a fundação reutilizável, protegida por uma
  regressão arquitetural; namespaces permanecem compatíveis nesta etapa.
- **Fase 32 — Renderer concreto no jogo:** a implementação que conhece HUD,
  salas, inimigos e materiais GDP passou para `game/visual/GdpRenderer`.
  `src/render` contém apenas primitivas gráficas reutilizáveis e é protegido
  integralmente contra dependências de conteúdo do jogo.
- **Fase 33 — Nome explícito do renderer GDP:** a classe concreta agora se
  chama `gdp::render::GdpRenderer`, eliminando a aparência de renderer genérico
  sem renomear indiscriminadamente DTOs ou primitivas gráficas.
- **Fase 34 — Contrato de apresentação do GDP:** DTOs de câmera, salas,
  entidades, HUD e iluminação passaram para `game/visual/GdpRenderData`.
  `GdpRenderer` agora consome esse contrato separado, sem alterar tipos,
  namespaces, gameplay ou pixels renderizados.
- **Fase 35 — Propriedade dos dados visuais:** o contrato de apresentação
  passou de `gdp::render` para `gdp::game::visual`. Os DTOs específicos do GDP
  não ocupam mais o namespace das primitivas gráficas reutilizáveis; nomes,
  valores e representação permanecem inalterados.
- **Fase 36 — Propriedade do renderer concreto:** `GdpRenderer` também passou
  para `gdp::game::visual` e agora referencia explicitamente as primitivas de
  `gdp::render`. O namespace gráfico reutilizável deixou de possuir classes ou
  dados de apresentação específicos do GDP.
- **Fase 37 — Identidade do target visual:** o target concreto passou de
  `gdp_renderer` para `gdp_visual_renderer`. O nome agora o diferencia de
  `gdp_render_foundation`, sem antecipar o futuro rename da fundação para WKC
  Forge ou alterar nomes dos executáveis de teste.
- **Fase 38 — Fundação gráfica WKC Forge:** o target genérico passou de
  `gdp_render_foundation` para `wkc_forge_render`. Ele continua expondo
  temporariamente o namespace `gdp::render`, permitindo validar separadamente
  o primeiro nome WKC Forge antes de qualquer rename amplo de APIs.
- **Fase 39 — Core WKC Forge:** o target genérico passou de `gdp_core` para
  `wkc_forge_core`. Hash, Log, Math, Random, Seed e Time permanecem
  compatíveis em `gdp::core`, enquanto `wkc_forge_render` deixa de depender de
  qualquer target com identidade GDP.
- **Fase 40 — Plataforma WKC Forge:** input independente de backend e tradução
  SDL passaram para `wkc_forge_platform` e `wkc_forge_platform_sdl`. A cadeia
  agora é `wkc_forge_core -> wkc_forge_platform -> SDL`, mantendo
  temporariamente o namespace `gdp::platform`.
- **Fase 41 — Procedural WKC Forge:** receitas, hashing e geradores de
  mesh/textura reutilizáveis passaram de `gdp_procedural_assets` para
  `wkc_forge_procedural`. O conteúdo Worker, salas e luminária permanece nos
  targets GDP, enquanto o namespace `gdp::procedural` segue compatível.
- **Fase 42 — Resources WKC Forge:** o `AssetCache` genérico saiu de
  `src/procedural` para `src/resources` e passou a ser publicado pelo target
  header-only `wkc_forge_resources`. O renderer GDP declara esse consumo
  explicitamente; API, métricas e namespace permanecem compatíveis.

O bootstrap atual contém CMake, SDL3, janela, contexto OpenGL 3.3 core, game loop
com fixed update de 60 Hz, snapshots de input, logging e modo headless para
testes. O núcleo inclui streams aleatórios determinísticos, derivação genérica e
versionada de seeds e tipos matemáticos básicos; os domínios usados pelo GDP
pertencem à camada do jogo. Quando SDL3
não está instalado, o build baixa a versão 3.4.14 oficial com hash SHA-256
fixado.

O renderer atual desenha a sala ocupada e a geometria alcançável em até duas
conexões usando origem e rotação reais, com cubo indexado reutilizado e materiais procedurais próprios de
parede/piso/teto derivados da seed da sala, aberturas e molduras ciano nas
portas, painéis vermelhos durante lockdown, corredores com piso, teto e paredes,
até oito luzes pontuais combinadas entre a sala atual e as duas camadas vizinhas,
luminárias emissivas, ambiente por tema,
câmera em perspectiva e retícula. Parede com grandes placas e rodapé
amarelo/preto, piso de placas com
faixa de tráfego e teto com luzes lineares usam receitas e cache separados. A
cena é produzida em framebuffer próprio e apresentada na janela. O jogador
controla essa câmera com `W/A/S/D` e mouse, colide com
paredes e props, usa `E` para abrir portas fisicamente ou interagir com o
terminal, dispara com o botão esquerdo
e recarrega com `R`. Pickups próximos são coletados automaticamente; objetivo e
resultado aparecem dentro do viewport, enquanto run, HP, munição, chaves e os
mesmos estados permanecem no título como apoio. `Esc` abre a pausa durante a
run; `W/S` ou setas navegam, `Enter` confirma e `Esc` retorna
ao jogo. `EXIT GAME` encerra a aplicação pelo menu.

Em Victory ou Defeat, a simulação congela, o título mostra o resultado e
`Enter` inicia uma nova run; `Esc` continua encerrando o jogo.

Na inicialização, uma seed gera um mundo navegável validado com START, BOSS,
branches, loop controlado, salas e corredores. Cada sala recebe IDs de entidades
determinísticos: jogador, terminal e um Worker ativo na START, além de inimigos,
itens e props conforme o `RoomRole`. Cada inimigo recebe uma `visualSeed`
isolada da estrutura do mapa. O Worker visível é montado a partir de
quatro `BodyProfile` e uma biblioteca compatível de 18 peças Box/Cylinder, com
assimetria, materiais e desgaste próprios; sua mesh e seu albedo são preparados
e cacheados antes do draw. Ele persegue o jogador, ataca em alcance e pode ser
morto pela pistola hitscan. Ao caminhar por uma porta e pelo corredor, a sala
destino já permanece visível com decoração, inimigos, pickups, props e materiais
próprios; sua IA continua inativa até a entrada. `E` abre os dois painéis da
porta sem reposicionar o jogador; a colisão libera o corredor somente após 90%
de abertura, mantém a porta aberta enquanto o trajeto está ocupado e fecha após
a entrada. HP, chaves, munição e upgrades são preservados. A
entidade muda de `roomId`, e colisão, evento e ambiência passam a usar a nova
sala. Lockdown bloqueia a saída até o evento terminar.

O Guard presente nas Arenas agora possui três perfis (`Patrol`, `Tactical` e
`Armored`), biblioteca própria de 18 peças, arma longa obrigatória e paleta
tática. Sua IA procura linha de tiro entre 4–8 m, aproxima quando está longe,
recua quando pressionada e nunca dispara através de sólidos. Renderer e cache
selecionam visual de Worker ou Guard sem compartilhar substreams de papel.

O Officer completa o trio atual da Arena com perfis `Field`, `Command` e
`Veteran`, 18 peças exclusivas, sidearm curta e módulo de comando obrigatório.
Ele mantém distância de 6–10 m e, em cadência própria, reduz o cooldown de
ataque de Workers e Guards vivos dentro de 10 m na mesma sala. Comandos nunca
atravessam o limite lógico da sala; mesh, textura e cache também usam substreams
e armazenamento exclusivos do papel.

O Engineer acrescenta os perfis `Maintenance`, `Diagnostics` e `Hazmat`, com 18
peças técnicas, ferramenta de reparo e mochila diagnóstica obrigatórias. Ele
opera entre 3–6 m e usa um cooldown independente para reparar o aliado vivo ou
prop danificado mais próximo dentro de 7 m, sem atravessar sólidos ou salas. A
Arena agora reúne os quatro papéis procedurais já implementados.

O IndustrialRobot fecha o elenco da Fase 11 com perfis `Sentry`, `Foundry` e
`Siege`, biblioteca pesada de 18 peças, núcleo/sensores emissivos, braços com
pistões, dois módulos traseiros e arma pesada. Na sala Boss, seus 250 HP ativam
três comportamentos: acima de 66% mantém 8–13 m, entre 33–66% pressiona em
4–8 m e abaixo de 33% persegue para atacar até 2,25 m. Velocidade, dano e
cadência mudam por fase; sala, line of sight, colisão, cooldown e morte usam os
mesmos contratos determinísticos do restante do elenco.

O primeiro incremento da Fase 12 deriva loot exclusivamente da substream
`Loot`, combinando papel, tema, profundidade, dificuldade, orçamento e as zonas
válidas da sala. `Health`, `Ammo` e `Key` recebem quantidade e posição
determinísticas, sem ocupar connectors. A START contém uma caixa de munição
para teste imediato; recursos cheios não consomem pickups, enquanto coleta
válida atualiza Player/Weapon e conclui o ciclo de vida da entidade.

O segundo incremento usa exclusivamente a substream `Props` para distribuir
crates, canisters e barricadas nas `propZones` transformadas, com orçamento por
papel/profundidade/dificuldade e preferência por tema. Cada prop possui escala,
HP, `visualSeed` e estado `Intact/Damaged/Destroyed`; tiros respeitam o alvo
mais próximo, destruição remove a colisão e cada crate libera no máximo um
pickup determinístico. Terminal e porta permanecem protegidos.

O terceiro incremento deriva planos exclusivamente de `SeedDomain::Events`.
`TerminalAmbush`, `Reinforcements`, `Lockdown`, `SupplyDrop` e `None` combinam
papel, tema, profundidade, dificuldade e `enemyZones`. Na START, mirar no
terminal e usar `E` ativa uma emboscada adicional; limpar todos os inimigos
encerra o lockdown e cria uma única recompensa. O título e os logs expõem
ativação, ondas e conclusão.

O quarto incremento usa somente `SeedDomain::Lighting` para derivar mood,
ambiente, luminárias e até quatro pontos de luz por sala. Papel, tema e área
controlam densidade e paleta sem alterar o fingerprint estrutural nem qualquer
conteúdo de gameplay. START, Arena, Boss e Security suportam alerta: durante
lockdown, o plano muda temporariamente para vermelho e retorna ao normal após a
conclusão. Todos os planos são validados na inicialização; a START permite
testar a transição imediatamente pela emboscada do terminal.

O quinto incremento deriva receitas exclusivamente de `SeedDomain::Audio` e
sintetiza em memória disparo, arma vazia, recarga, impactos, morte, destruição,
coleta, dano recebido e sinais de evento. O mixer estéreo de 48 kHz suporta 16
vozes, cache, ambiente próprio para os sete temas, pan e distância. Falta de
dispositivo não interrompe o jogo; headless mantém síntese/mixagem silenciosa
para regressão. Nenhum arquivo WAV externo é necessário.

O sexto incremento conclui a Fase 12 com progressão funcional de sessão.
Eliminações, props destruídos, loot e eventos concedem XP e score; a curva vai
do nível 1 ao 10. Cada nível alterna automaticamente `Vitality` (+15 HP máximo
e cura), `Firepower` (+4 de dano) e `Capacity` (+4 de reserva e munição). Nível,
XP e score aparecem no título; limpar o encontro inicial e a emboscada da START
já permite alcançar o primeiro upgrade.

O primeiro incremento da Fase 13 ligou o grafo procedural ao fluxo do jogador.
Cada connector consumido por uma edge produz uma porta visível; na primeira
versão, aproximar-se, olhar e pressionar `E` resolvia o corredor correspondente
e posicionava o jogador na sala oposta. Salas e conteúdo fixo
agora respeitam origem e rotação, e a colisão usa os mesmos bounds transformados.

O segundo incremento materializa cada sequência `CorridorPath::points` como
trechos ortogonais com piso, teto e laterais. A colisão combina volumes da sala,
do corredor e da sala adjacente com sobreposição suficiente para o collider
cilíndrico atravessar retas e curvas sem teleporte. A troca de `roomId`, conteúdo,
materiais, iluminação e ambiência ocorre apenas quando o collider inteiro libera
o vão. Em lockdown, a rota permanece navegável enquanto a porta passável está
ocupada e só é removida depois que o jogador entra com segurança e o painel fecha.

O terceiro incremento conclui o vertical slice funcional. `SessionSystem`
mantém `Playing`, `Victory` e `Defeat`: entrar na Boss registra o
IndustrialRobot vivo e somente sua morte concede vitória. A morte do jogador
concede derrota; estados terminais congelam gameplay. `Enter` desmonta os
sistemas da run e gera outro mundo a partir de uma seed derivada do valor
inicial e do índice da run, reproduzível em execuções diferentes.

O incremento da Fase 14 remove a troca brusca de apresentação no limite da
sala. Todos os materiais do mundo são enviados à GPU antes do primeiro frame;
cada shell e corredor referencia uma chave estável, e conteúdo/decor da sala
adjacente entra na fila ainda durante a aproximação. No corredor, ambiente e
emissão misturam com `smoothstep`. Um passe bitmap independente da câmera 3D
mostra `REACH BOSS`, `DEFEAT BOSS`, `VICTORY` ou `DEFEAT` no viewport.

Na Fase 15, `DoorRuntime` substitui o reposicionamento interativo por estados
`Closed/Opening/Open/Closing/Locked`. Cada edge começa fechada, abre em 0,45 s,
fica livre a partir de 90%, não fecha sobre o jogador e volta a bloquear somente
depois que o corredor está vazio. Os painéis deslizam lateralmente e lockdown
solicita fechamento/bloqueio assim que o vão fica livre. O áudio mantém dois
loops cacheados no corredor e
aplica crossfade equal-power com o mesmo progresso suavizado da iluminação.

Na Fase 16, eventos acionados por entrada preparam a primeira onda durante a
inicialização do mundo. Esses inimigos são desenhados na sala vizinha, mas
permanecem dormentes: não executam IA, não contam para a limpeza e ignoram dano
até o evento liberá-los. A ativação reutiliza as mesmas entidades, evitando
duplicação e aparecimento tardio; emboscadas de terminal continuam ocultas. O
renderer também seleciona as luzes vizinhas mais próximas e combina até oito
pontos, mantendo luminárias e iluminação da sala destino visíveis pelo corredor.

Na Fase 17, `HudRenderData` recebe somente snapshots da simulação e desenha HP
com barra de risco, munição, chaves, nível e progresso de XP nas bordas. Objetivo
e estado `STANDBY/ACTIVE/CLEAR/LOCKDOWN` ficam no topo; prompts `E OPEN DOOR`,
`DOOR LOCKED`, `E USE TERMINAL` e `R RELOAD` usam a mesma detecção contextual da
ação real. O passe bitmap escala entre resoluções estreitas e normais sem mover
regras de gameplay para o renderer.

Na Fase 18, `FeedbackState` mantém timers no fixed update para dano, confirmação
de tiro e avisos. Dano acende brevemente as quatro bordas; `Hit` e `Kill` usam
marcadores distintos ao redor da mira. `SUPPLIES ACQUIRED`, `EVENT START`,
`NEW WAVE`, `SECTOR CLEAR` e `LEVEL UP` aparecem por tempo limitado. Passos
inválidos não consomem timers e reiniciar a run limpa todo feedback pendente.

Na Fase 19, `PauseMenuState` mantém seleção segura em `RESUME`, navegação com
wrap e comandos separados para retomar, reconstruir a run ou sair. Enquanto
pausado, look, input pendente, IA, portas, arma, eventos, áudio atualizado pelo
game loop e timers de HUD não avançam. O mouse relativo é liberado na abertura
e restaurado no retorno; o renderer oculta mira e feedback transitório sob o
overlay central.

Na estabilização posterior à Fase 19, `CorridorPath` passou a permitir contato
com a sala de origem apenas no primeiro stub e com a sala destino apenas no
último. Isso rejeita curvas que atravessavam novamente a própria sala. A adoção
de sala agora considera raio e altura do personagem; lockdown segura uma porta
passável ocupada e a colisão segue o estado físico de cada edge. Loops agora
fecham somente branches a dois passos no grafo, o espaçamento padrão caiu de 14
para 12 unidades e a auditoria de 10.000 mundos limita a maior rota a 16,3
unidades — a Run 2 da seed padrão caiu de 46,3 para 15,3. Nas curvas, paredes
internas são aparadas, paredes externas se encontram e piso/teto cobrem a junta.
O renderer antecipa shells, portas, corredores, entidades e luzes por duas
conexões, evitando que a parede da próxima saída desapareça somente ao trocar o
`roomId`. A regressão permanente percorre 500 mundos e as seeds anteriormente
problemáticas; auditorias ampliadas validaram 10.000 mundos gerados e 2.000
mundos atravessados.

O mesmo pipeline já oferece o tema Storage: metal amarronzado envelhecido,
marcação logística curta, concreto empoeirado com marcas de arrasto e teto
escuro com vigas. Combat Room acrescenta placas blindadas verde-oliva com 1–3
impactos, piso escuro demarcado e teto estrutural reforçado. Cafeteria fornece
painéis claros com faixa institucional vermelha, piso lavável marcado por
circulação/mobiliário e teto modular neutro.
Laboratory acrescenta painéis frios precisos, módulo científico com indicadores,
piso técnico azul e contaminação localizada, além de teto branco modular. O
perfil Technical Area usa tubos volumétricos, caixa/válvula conectadas, piso
pesado com grade e óleo e teto mecânico categorizado. Security acrescenta
console monitorado, alerta vermelho confinado, piso técnico e teto claro
regular. Central Hub fecha o conjunto com grandes superfícies neutras,
sinalização ciano única, rotas contínuas no piso e teto arquitetônico neutro.
As luminárias são modelos independentes posicionados pelo plano de iluminação e
usam material próprio, sem qualquer representação gravada nos tetos. O
renderer cacheia os oito perfis pelo hash completo e seleciona o perfil de cada
sala renderizada. O teste OpenGL offscreen também cobre sala
transladada/rotacionada, porta normal/lockdown, geometria vista do corredor,
materiais diferentes simultâneos em salas conectadas e o overlay terminal.

## Variedade estrutural de salas

A Fase 20 substituiu as sete cópias da base aproximada de `8 × 4 × 10` por 23
módulos retangulares distribuídos entre famílias compactas, médias e grandes.
Os footprints variam de `7 × 4 × 9` a `14 × 6 × 13`; Hub, Arena e Boss usam
módulos grandes próprios, enquanto Secret usa módulos compactos dedicados.

`localBounds` e `footprint` agora definem renderer, colisão, connectors, zonas
de spawn e o fingerprint determinístico. O placement calcula a separação entre
os AABBs reais das salas: o caminho principal avança a partir da borda do
módulo anterior e branches consideram todas as salas vizinhas já posicionadas.
Assim, aumentar uma sala não alonga nem quebra o corredor por manter centros em
uma grade de tamanho fixo.

Todos os módulos validam connectors e zonas nas quatro rotações. Os lotes
permanentes cobrem 1.000 mundos estruturais, 500 mundos atravessados nos dois
sentidos e uma sala grande conectada no teste OpenGL. Nenhuma receita de textura
foi alterada nesta fase; a revisão artística continua sujeita ao gate abaixo.

## Menu principal antes da run

A Fase 21 separou a inicialização da plataforma da criação da run. Em modo
gráfico, SDL, áudio, janela e renderer ficam prontos primeiro e exibem um menu
responsivo com `START RUN` e `EXIT GAME`; `W/S` ou setas mudam a seleção com
wrap, `Enter` confirma, o mouse seleciona/clica e `Esc` sai. A dungeon, suas
entidades, eventos, iluminação e materiais específicos só são preparados ao
confirmar `START RUN`.

O modo `--headless` ignora apenas essa espera interativa e inicia a run
automaticamente para preservar os testes determinísticos. A regressão OpenGL
verifica o overlay e suas duas seleções; outra integração encerra ainda no menu
e falha se encontrar o log de geração do mundo. O restart da pausa também marca
a run atual como encerrada antes de derivar a próxima seed, evitando a rejeição
de um reinício solicitado durante `Playing`.

## Gate obrigatório de aprovação visual

Os materiais implementados nas Fases 9 e 10 são baseline técnico funcional e
continuam disponíveis durante o desenvolvimento, mas ainda não são arte final
aprovada. O protocolo e o inventário ficam em
[`visual/TEXTURE_APPROVALS.md`](visual/TEXTURE_APPROVALS.md).

Para cada superfície ou família visual, o fluxo obrigatório é:

1. definir objetivo, referências, material, paleta, densidade e desgaste;
2. produzir uma prancha versionada com seeds representativas antes de editar a
   implementação definitiva;
3. aguardar aprovação visual explícita do usuário para aquela unidade;
4. revisar a proposta sem trocar o código atual quando ela não for aprovada;
5. implementar somente a versão aprovada;
6. validar no jogo contra a prancha, além dos testes técnicos automatizados.

Aprovar `Storage/Wall`, por exemplo, não aprova `Storage/Floor`, outro ambiente
ou uma versão futura. Como as recipes são procedurais, a aprovação cobre a
família e as regras demonstradas pela prancha, não cada seed possível.

## Compilar e testar

Requisitos locais: CMake 3.24+, Ninja, compilador C++20 e OpenGL.

```bash
cmake --preset debug
cmake --build --preset debug --parallel
ctest --preset debug
```

Os mesmos comandos aceitam o preset `release`; ambos compilam e executam a suíte
completa de Core, Renderer, Procedural Assets, Enemy Generation, Room Materials,
Gameplay, Session, Room Traversal, Loot, Props, Events, Lighting, Audio, Progression,
Combat, HUD Feedback, Pause Menu, Entities e World.

Validação instrumentada de memória e comportamento indefinido:

```bash
cmake --preset sanitizers
cmake --build --preset sanitizers --parallel
ctest --preset sanitizers
```

Esse preset ativa AddressSanitizer e UndefinedBehaviorSanitizer em compiladores
GCC/Clang. A detecção de leaks de bibliotecas gráficas externas fica desativada;
acessos inválidos e comportamento indefinido encerram o teste imediatamente.

Executar normalmente:

```bash
build/debug/gdp
```

Executar sem janela por uma quantidade determinada de frames:

```bash
build/debug/gdp --headless --frames 60
```

Executar uma seed específica:

```bash
build/debug/gdp --seed 12345
```

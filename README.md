# GDP Technical Bible

GDP é um FPS procedural jogável, desenvolvido em C++20, SDL3 e OpenGL sobre
uma engine própria. Este repositório reúne a implementação, os testes e a
documentação técnica modular que orienta a evolução do projeto.

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
  menu responsivo com `RESUME`, `RESTART RUN`, `SETTINGS`, `CONTROLS`,
  `MAIN MENU` e `EXIT GAME`,
  liberando o mouse até o retorno ao gameplay.
- **Estabilização de travessia:** concluída; corredores não podem reentrar nas
  próprias salas, loops fecham apenas branches próximas, curvas possuem juntas
  abertas e lockdown aguarda o collider liberar o vão antes de bloquear a rota.
- **Fase 20 — Variedade estrutural de salas:** implementada e validada; a
  biblioteca possui módulos compactos, médios e grandes, com Hub, Arena, Boss e
  Secret usando dimensões próprias e placement derivado dos bounds reais.
- **Fase 21 — Menu principal:** implementada e validada; `START RUN` cria a
  dungeon somente após confirmação e `EXIT GAME` entra no fluxo principal de saída.
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
- **Fase 43 — Fundação de áudio WKC Forge:** contratos, hash, validação e síntese
  determinística de clips passaram para `wkc_forge_audio`. Os eventos
  `WeaponFire`, `EnemyDeath`, `Pickup`, perfis de ambiência e a fachada SDL
  permanecem em `gdp_audio`, agora agrupados fisicamente em `src/game/audio`.
- **Fase 44 — Mixer WKC Forge:** cache de clips, orçamento de vozes, buses,
  espacialização, crossfade e backend SDL passaram para `AudioMixer` em
  `wkc_forge_audio`. `gdp_audio` não depende mais diretamente de SDL e apenas
  converte eventos, seeds e perfis do jogo em receitas genéricas.
- **Fase 45 — Namespace de áudio WKC Forge:** `AudioRecipe`, `AudioClip`,
  validação, síntese e `AudioMixer` passaram de `gdp::audio` para `wkc::audio`.
  `AudioEventKind`, geração de receitas e `AudioSystem` permanecem em
  `gdp::audio`, tornando explícita a direção GDP → WKC Forge na API C++.
- **Fase 46 — Namespace de Resources:** `AssetCache` e `AssetCacheStats`
  passaram de `gdp::procedural` para `wkc::resources`. O renderer GDP consome a
  API WKC explicitamente, e um teste ligado somente a `wkc_forge_resources`
  protege reutilização, métricas e limpeza do cache header-only.
- **Fase 47 — Namespace de Plataforma:** `Input`, `Key`, `MouseButton` e
  `handleSdlInputEvent` passaram de `gdp::platform` para `wkc::platform`. A
  aplicação GDP agora consome a API WKC explicitamente; snapshots, transições,
  mapeamentos e processamento de eventos SDL permanecem inalterados.
- **Fase 48 — Namespace de Render:** `Camera`, `Framebuffer`, `GlApi`, `Mesh`,
  `Shader`, `Texture` e seus dados auxiliares passaram de `gdp::render` para
  `wkc::render`. Renderer, conteúdo visual e procedural do GDP consomem agora a
  fundação WKC explicitamente, sem aliases ou alteração de pixels.
- **Fase 49 — Propriedade dos geradores visuais:** `EnemyGenerator` e
  `RoomMaterialGenerator` passaram de `gdp::procedural` para
  `gdp::game::visual`. As receitas e operações genéricas continuam
  explicitamente em `gdp::procedural`, preparando seu rename WKC sem misturar
  Worker, papéis de inimigo ou temas de sala à API reutilizável.
- **Fase 50 — Namespace Procedural WKC Forge:** `PrimitiveRecipe`,
  `MeshRecipe`, `TextureRecipe`, validação, hashing e geração passaram de
  `gdp::procedural` para `wkc::procedural`. Conteúdo visual GDP consome essa API
  explicitamente, sem aliases e com hashes, meshes e pixels preservados.
- **Fase 51 — Time no Core WKC Forge:** `FixedStepClock` passou de
  `gdp::core` para `wkc::core`. A aplicação consome o relógio WKC explicitamente
  e mantém passo fixo, clamp de frames longos e interpolação inalterados; os
  demais contratos de Core continuam compatíveis para migração incremental.
- **Fase 52 — Log no Core WKC Forge:** `LogLevel` e `log` passaram de
  `gdp::core` para `wkc::core`. Plataforma gráfica, áudio, renderer e aplicação
  consomem agora o diagnóstico genérico explicitamente, sem mudar mensagens ou
  níveis emitidos.
- **Fase 53 — Hash no Core WKC Forge:** `mix64`, `hashCombine` e `fnv1a64`
  passaram de `gdp::core` para `wkc::core`. Seeds, mundo, áudio e assets usam a
  API WKC explicitamente, preservando todos os fingerprints e goldens.
- **Fase 54 — Namespace Core concluído:** Math, Seed e Random passaram para
  `wkc::core`, concluindo a API pública de `wkc_forge_core`. Todos os
  consumidores GDP/WKC foram atualizados sem aliases; coordenadas, RNG,
  derivação de seeds, hashes e geração permanecem idênticos.
- **Fase 55 — Propriedade de Entities e World:** a análise dos contratos
  confirmou `gdp_entities` e `gdp_world` como camadas do jogo. Arquétipos,
  itens, props, papéis, temas, encontros e regras de salas permanecem em
  `gdp::entities`/`gdp::world`; a arquitetura agora impede que esses conceitos
  sejam promovidos artificialmente à WKC Forge ou virem dependências da engine.
- **Fase 56 — Fronteira WKC Forge × GDP concluída:** targets e testes exclusivos
  da engine usam identidade WKC, os namespaces reutilizáveis são
  `wkc::{core,platform,render,resources,procedural,audio}` e gates impedem
  includes ou links WKC → GDP. Gameplay, Entities, World, áudio/visual concreto
  e Application permanecem GDP. A divisão em outro repositório continua
  deliberadamente adiada; a fronteira atual já é verificável no monorepo. A
  validação manual posterior completou uma run até derrotar o boss sem regressão
  visual ou funcional observada.
- **Fase 57 — CI de regressão:** GitHub Actions configura, compila e executa a
  suíte completa nos presets Debug e Release em Linux, incluindo headless e
  OpenGL offscreen. Um job independente executa ASan + UBSan. O próprio gate
  arquitetural verifica os comandos obrigatórios e proíbe atualização automática
  de goldens.
- **Fase 58 — Armadura funcional:** o Player possui Armor/Maximum Armor,
  absorve dano ponto a ponto antes do HP e preserva o estado entre salas. Cada
  recompensa Vitality aumenta HP máximo em 15, Armor máximo em 10 e restaura
  ambos. O runtime coleta pickups Armor respeitando capacidade, o Entity Manager
  sincroniza o estado e o HUD exibe valor/barra azul. A geração procedural
  existente não foi alterada, preservando os hashes de loot e eventos.
- **Fase 59 — Áudio funcional das portas:** abrir, fechar e travar possuem
  receitas procedurais distintas e espacializadas na posição física da porta.
  Os sons são emitidos somente quando o estado realmente transita; cada edge
  mantém sua própria sequência determinística, inclusive durante lockdown.
- **Fase 60 — Passos funcionais do Player:** o deslocamento horizontal real e
  grounded acumula uma passada a cada 1,60 m. Parado, no ar, morto ou após salto
  anormal de posição não emite áudio. `Footstep` usa impacto metálico procedural,
  variação por índice e sequência própria reiniciada a cada run.
- **Fase 61 — Movimento cosmético da câmera:** `ViewMotionState` deriva head bob
  do deslocamento real grounded, com ciclo de 3,20 m, limites de 3,5 cm vertical
  e 1,8 cm lateral e blend suave. Somente a câmera enviada ao renderer recebe o
  offset; mira, hitscan, interação, IA e colisão continuam na câmera lógica.
- **Fase 62 — Áudio funcional da interface:** a WKC Forge possui bus UI com
  volume e atualização seletiva próprios. `UiNavigate`, `UiConfirm` e `UiBack`
  respondem às transições reais dos menus; durante pausa somente esse bus
  avança, mantendo ambiência, efeitos e toda a simulação congelados.
- **Fase 63 — Mouse em todos os menus funcionais:** menu principal e pausa
  aceitam hover e clique sem remover `W/S`, setas, `Enter` ou `Esc`. O menu de
  pausa compartilha o layout responsivo entre desenho e hit test, inclusive em
  framebuffer high-DPI.
- **Fase 64 — Menu funcional de resultado:** Victory e Defeat agora oferecem
  `NEW RUN/RETRY` e `EXIT GAME` com teclado e mouse. O término libera o ponteiro,
  congela a simulação e preserva o reinício pela próxima seed determinística.
- **Fase 65 — Retorno ao menu principal:** pausa e resultado agora oferecem
  `MAIN MENU`. A run ativa é encerrada, a próxima seed é preparada e todos os
  recursos da run são limpos sem fechar ou recriar a janela.
- **Fase 66 — Configurações runtime:** `SETTINGS` no título e na pausa ajusta
  Master, Effects, Ambient, UI, sensibilidade e FOV em tempo real. Teclado,
  setas e mouse compartilham layout responsivo.
- **Fase 67 — Persistência segura das configurações:** os seis valores são
  carregados de `settings.toml` no diretório de preferências da plataforma e
  salvos ao fechar o menu. O formato versionado aceita campos ausentes/futuros,
  recupera valores inválidos e só substitui o arquivo após validar o temporário.
- **Fase 68 — Inversão vertical do mouse:** `INVERT Y` integra menu, Player e
  persistência. Esquerda escolhe `OFF`, direita escolhe `ON`; o schema v2 lê
  arquivos v1 preservando os seis valores anteriores e assume `OFF` com segurança.
- **Fase 69 — Fullscreen persistente:** `FULLSCREEN` alterna a janela SDL em
  runtime sem reiniciar a run. O schema v3 migra v1/v2 com `OFF` como default, e
  um bootstrap isolado confirma que a preferência é aplicada na próxima abertura.
- **Fase 70 — Resolução de janela persistente:** `RESOLUTION` oferece 960×540,
  1280×720, 1600×900 e 1920×1080. A troca ocorre em runtime no modo janela; em
  fullscreen, o preset fica preservado para o retorno. O schema v4 migra v1–v3.
- **Fase 71 — Setas clicáveis nas configurações:** cada linha ajustável possui
  hit targets próprios para `<` e `>`. A seta esquerda sempre reduz, a direita
  sempre aumenta, e clicar no nome/valor apenas seleciona sem alterar.
- **Fase 72 — VSync persistente:** `VSYNC` alterna a sincronização vertical do
  contexto OpenGL em runtime. O padrão permanece `ON`, o schema v5 migra v1–v4
  e um bootstrap isolado confirma a aplicação de `OFF` na próxima abertura.
- **Fase 73 — Confirmação segura de saída:** `EXIT GAME` no título, pausa e
  resultado abre um diálogo compartilhado com `CANCEL` selecionado por padrão.
  WASD, setas, `Enter`, `Esc`, hover e clique usam o mesmo estado e hit test.
- **Fase 74 — Cronômetro determinístico de run:** o tempo avança nos passos
  fixos de 60 Hz, congela em menus, pausa e resultado e zera somente na próxima
  run. HUD, título e log terminal exibem o mesmo valor `MM:SS.cc`.
- **Fase 75 — Modo Speedrun selecionável:** `SPEEDRUN` inicia a mesma geração e
  simulação com identidade própria. Seu cronômetro continua durante pausa e
  configurações, congela apenas no resultado e o modo é preservado em retry.
- **Fase 76 — Recorde pessoal de Speedrun:** vitórias válidas criam ou melhoram
  o melhor tempo local. Tempo, seed e versão das regras são salvos com troca
  atômica; HUD, resultado, título e logs exibem `BEST` ou `NEW RECORD`.
- **Fase 77 — Repetição da mesma seed:** o resultado oferece `REPLAY SEED`
  separado de `NEW RUN/NEXT SEED`, com teclado e mouse. `RESTART RUN` na pausa
  também preserva o mapa, sem pular a próxima seed da sequência determinística.
- **Fase 78 — Desafio da melhor seed:** `BEST SEED` no título carrega a seed do
  recorde local e inicia um Speedrun no mesmo mapa. Sem recorde, a opção fica
  indisponível e não fecha o menu nem gera mundo.
- **Fase 79 — Fundação jogável da Campanha:** `CAMPAIGN` encadeia três andares
  determinísticos. Os dois primeiros bosses liberam `NEXT FLOOR`; o terceiro
  encerra em `CAMPAIGN CLEAR`, com andar visível no HUD e no título.
- **Fase 80 — Carry-over da Campanha:** a passagem por `NEXT FLOOR` preserva
  HP, Armor, capacidades, chaves, upgrades, munição, nível, XP, score e
  contadores. Replay/restart restaura a entrada do andar; spawn, cooldowns e
  vínculos do mundo anterior são reiniciados.
- **Fase 81 — Continue persistente da Campanha:** cada entrada de andar grava
  um checkpoint versionado e validado. `CONTINUE Fn/3` restaura a seed, o andar
  e o progresso permanente após reabrir o jogo; a vitória final remove o save.
- **Fase 82 — Pressão progressiva da Campanha:** o primeiro andar preserva o
  balanceamento original; os seguintes aplicam `THREAT 110%` e `120%` à vida,
  mobilidade e dano dos inimigos, aos eventos e aos budgets de recursos/props.
- **Fase 83 — Armor no loot procedural:** becos sem saída agora entregam Ammo e
  Armor nas dificuldades jogáveis; salas comuns e arenas podem sortear Armor
  por tema, usando a mesma substream, budget, placement e validação do loot.
- **Fase 84 — Carabina secreta:** toda sala Secret contém uma Carbine funcional
  junto de Key e Health. O pickup troca cadência, dano, alcance e capacidades,
  preserva upgrades, não recarrega duplicatas e persiste entre andares.
- **Fase 85 — Overdrive de Arena:** toda Arena contém Ammo e um pickup Special
  que ativa 12 segundos de Overdrive. O efeito aumenta dano em 50%, reduz o
  intervalo de tiro para 65%, bloqueia duplicatas e não atravessa andares.
- **Fase 86 — Desvio local dos inimigos:** Worker, Guard, Officer, Engineer e
  IndustrialRobot contornam obstáculos quando o avanço direto bloqueia. O lado
  é derivado da identidade da entidade e permanece estável por uma janela curta.
- **Fase 87 — Separação de combate:** inimigos vivos e ativos da mesma sala
  preservam espaço pessoal durante avanço e recuo. Sobreposições exatas usam
  desempate estável por `EntityId`, sem criar colisão rígida ou travar grupos.
- **Fase 88 — Volumes físicos entre inimigos:** o movimento agora testa todo o
  segmento percorrido contra os raios dos demais inimigos, impedindo que corpos
  se atravessem em grupos densos. Idle e Attack também liberam sobreposições.
- **Fase 89 — Seed inicial variável:** abrir o jogo normalmente seleciona uma
  seed nova antes do menu. `--seed`, headless, Continue, Best Seed, replay e
  restart preservam seus contratos determinísticos.
- **Fase 90 — Poses procedurais dos inimigos:** Idle, Walk, Attack e Death agora
  possuem poses de raiz distintas, amostradas pelos ticks fixos e defasadas pela
  `visualSeed`. Pausa, resultado e inimigos dormentes permanecem congelados.
- **Fase 91 — Orientação corporal dos inimigos:** Chase acompanha o deslocamento
  real, Attack e Reposition encaram o jogador, e Idle/Death preservam a última
  direção válida. O renderer aplica o yaw antes da pose de raiz.
- **Fase 92 — Giro suave dos inimigos:** o yaw converge pelo arco mais curto nos
  fixed updates, a `480°/s` em Chase e `720°/s` ao mirar/recuar, eliminando
  mudanças instantâneas sem atrasar a resposta de combate.
- **Fase 93 — Reação visual ao dano:** acertos não letais ativam por `0,12 s`
  uma compressão de raiz com tonalidade quente e emissivo reforçado. O pulso
  não interrompe IA/ataque, reinicia em novo acerto e Death sempre prevalece.
- **Fase 94 — Hierarquia rígida dos inimigos:** as receitas consolidadas agora
  são enviadas à GPU por torso, cabeça, braços, pernas e acessórios, preservando
  pivôs estáveis. Walk/Attack articulam partes com amplitudes contidas e Hit foi
  refinado para recuo mecânico sem compressão cartunesca da silhueta.
- **Fase 95 — Assinaturas de movimento por arquétipo:** Worker, Guard, Officer,
  Engineer e IndustrialRobot agora usam ciclos e amplitudes próprios. Golpes
  corpo a corpo avançam a ferramenta, armas recuam e o Boss responde com massa.
- **Fase 96 — Transições e queda rígida dos inimigos:** trocas entre Idle, Walk,
  Attack e Hit agora usam blends curtos por fixed tick. Death interrompe os
  demais estados e conclui uma queda lateral sem achatar ou alargar o modelo.
- **Fase 97 — Ragdoll procedural dos inimigos:** a queda rígida foi substituída
  por uma reação articulada de torso, cabeça, braços, pernas e equipamento, com
  atrasos, limites e amortecimento próprios por massa, sem física de gameplay.
- **Fase 98 — Queda orgânica por transferência de peso:** o ragdoll agora perde
  sustentação pelas pernas antes do torso, conserva inércia na cabeça e membros
  e termina com contato amortecido; o Boss usa uma queda mais longa e pesada.
- **Fase 99 — Direção integral e assentamento completo:** cada morte escolhe de
  forma determinística qualquer direção nos 360° ao redor do inimigo. A janela
  só termina após torso, cabeça, braços, pernas e equipamento alcançarem suas
  poses no piso, inclusive a última peça solta.
- **Fase 100 — Patrulha local determinística:** os cinco arquétipos percorrem
  waypoints curtos ao redor do spawn enquanto não detectam o jogador, pausam
  entre destinos e reutilizam colisão, desvio e separação. Combate interrompe a
  patrulha imediatamente; Campaign/Speedrun avançam para regras 9/8.
- **Fase 101 — Memória de investigação:** linha de visão passa a ser obrigatória
  para adquirir o jogador. Ao perder contato, cada papel busca por tempo
  limitado a última posição realmente observada e então retorna à patrulha;
  Campaign/Speedrun avançam para regras 10/9.
- **Fase 102 — Cobertura para validação:** a START recebe uma divisória industrial
  alta e larga, fora da rota de portas e do terminal, permitindo ocultar-se do
  primeiro Worker, mover-se atrás dela e reaparecer pela borda. Campaign e
  Speedrun avançam para regras 11/10.
- **Fase 103 — Percepção por ruído de combate:** cada disparo consumido cria na
  sala ativa um estímulo de `18 m`, limitado pela audição própria de cada papel.
  Inimigos sem visão investigam a origem; visão real continua tendo prioridade.
  Campaign/Speedrun avançam para regras 12/11.
- **Fase 104 — Alerta de prontidão:** o primeiro ruído interrompe a patrulha,
  gira o inimigo para a origem e sustenta brevemente uma postura própria antes
  da investigação. Novos tiros atualizam o alvo sem reiniciar essa reação.
  Campaign/Speedrun avançam para regras 13/12.
- **Fase 105 — Recuperação da busca:** ao esgotar a memória, cada papel executa
  uma varredura curta e determinística antes de voltar à patrulha. Visão ou novo
  ruído interrompem a saída sem repetir o alerta. Campaign/Speedrun avançam para
  regras 14/13.
- **Fase 106 — Pathfinding global local à sala:** perseguição, investigação e
  patrulha usam um grafo de visibilidade determinístico para atravessar rotas com
  múltiplos obstáculos. O desvio curto permanece como fallback físico.
  Campaign/Speedrun avançam para regras 15/14.
- **Fase 107 — Percepção direcional:** visão agora exige alcance, linha livre e
  presença no cone frontal próprio de cada papel. Patrulha e busca orientam esse
  cone, enquanto ruído continua podendo alertar pelas costas. Campaign/Speedrun
  avançam para regras 16/15.
- **Fase 108 — Navegação de grupos:** rotas simétricas distribuem inimigos por
  preferências laterais derivadas da identidade, e bloqueios físicos podem
  inverter a escolha. Grupos deixam de insistir todos no mesmo lado.
  Campaign/Speedrun avançam para regras 17/16.
- **Fase 109 — Impactos materiais:** todo acerto consumido expõe ponto, normal e
  superfície reais. Metal, compósito e tecido orgânico recebem assinaturas
  visuais distintas e transitórias, sincronizadas com o áudio espacial.
  Campaign/Speedrun avançam para regras 18/17.
- **Fase 110 — Portas contínuas refinadas:** painéis usam aceleração e frenagem
  suaves, e o mesmo vão visível governa a colisão. Player ou inimigo vivo na
  passagem interrompe o fechamento e reabre a porta com segurança.
  Campaign/Speedrun avançam para regras 19/18.
- **Fase 111 — Interface industrial revisada:** HUD, título, pausa, resultado,
  configurações e confirmação compartilham uma paleta semântica, molduras em
  camadas e seleção bilateral mais legível, preservando layouts e mouse.
- **Fase 112 — Armas em primeira pessoa:** Pistol e Carbine recebem silhuetas
  próprias no viewmodel, com entrada ao equipar, arco de recarga, recuo e flash
  de disparo ligados ao estado real da arma. Overdrive altera o detalhe emissivo.
- **Fase 113 — Luminárias destrutíveis:** cada ponto de luz possui um fixture
  metálico acertável no teto. Dano reduz luz e emissão; destruição apaga ambos
  sem criar loot, bloquear navegação ou permitir reparo por Engineer.
- **Fase 114 — Sombras dinâmicas:** luzes ativas testam visibilidade contra os
  volumes de inimigos e props opacos. A oclusão reduz somente a contribuição
  direta, preserva o ambiente mínimo e acompanha destruição e movimento.
- **Fase 115 — Orçamento e limpeza de iluminação:** o renderer limita sombras
  às duas luzes ativas mais próximas e aos oito oclusores relevantes mais
  próximos. Fixtures usam índice direto por sala, sem varrer todos os props.
- **Fase 116 — Pacote Windows x64:** preset MSVC, runtime SDL ao lado do
  executável e CPack produzem `GDP-Windows-x64.zip`. A CI compila, executa os
  testes portáveis e valida o executável extraído antes de publicar o artefato.
- **Fase 117 — Diagnóstico de desempenho:** uma janela móvel de 120 frames mede
  média, P95, pior frame, update, render e espera de apresentação. `F3` alterna
  um overlay com FPS, budget, draw calls, triângulos, entidades, luzes, sombras,
  caches e tempos de preparação; headless emite o mesmo resumo no log.
- **Fase 118 — Perfis gráficos escaláveis:** `QUALITY` oferece `POTATO`, `LOW`,
  `MEDIUM`, `HIGH` e `ULTRA`. Cada perfil aplica um orçamento determinístico de
  escala interna, luzes, sombras e materiais detalhados sem mudar HUD, menus,
  resolução da janela ou regras de gameplay. Configurações antigas migram para
  `ULTRA`, preservando exatamente o renderer anterior.
- **Fase 119 — Culling conservador por câmera:** salas vizinhas, corredores e
  entidades totalmente fora do campo visual deixam de entrar na fila de draw.
  A sala atual nunca é removida, bounds inválidos falham abertos e uma margem
  angular protege portas e continuidade durante giros rápidos.
- **Fase 120 — Tempo real de GPU:** quatro timer queries OpenGL medem o trabalho
  gráfico sem bloquear o frame. `F3` separa `CPU RENDER`, `GPU` e `WAIT`, usa
  fallback seguro quando o driver não oferece a extensão e ignora o aquecimento
  inicial ao calcular a média.
- **Fase 121 — Cache de uniforms OpenGL:** `Shader` resolve cada nome de uniform
  somente uma vez por programa e reutiliza a localização nas submissões
  seguintes. Localizações inativas também são cacheadas, o cache é invalidado
  em recompilação/reset e métricas protegem o comportamento sem mudar pixels.
- **Fase 122 — Deduplicação de uniforms:** valores idênticos consecutivos deixam
  de gerar chamadas `glUniform`. O cache tipado cobre matrizes, vetores, floats e
  inteiros, sobrevive entre frames, invalida com o programa e expõe no `F3` as
  submissões realizadas e evitadas sem alterar a qualidade visual.
- **Fase 123 — Cache de bindings de textura:** o renderer acompanha a unidade
  ativa e as texturas já ligadas durante o frame, omitindo `glActiveTexture` e
  `glBindTexture` redundantes. O `F3` expõe binds, skips e trocas de unidade;
  estado externo é invalidado conservadoramente a cada frame.
- **Fase 124 — Cache de VAO/mesh:** draws consecutivos do mesmo mesh reutilizam
  o vertex array já ligado, sem executar bind e unbind em cada peça. O caminho
  tradicional permanece disponível, e `F3` informa binds realizados e evitados.
- **Fase 125 — Shader de baixo custo:** `POTATO` e `LOW` usam uma variante
  especializada quando respeitam o orçamento de até quatro luzes e zero
  sombras. O shader remove em compilação normal mapping, especular e testes de
  oclusão que esses perfis já mantinham desligados; entradas fora do contrato
  voltam ao shader completo e o `F3` identifica `LOW SPEC` ou `FULL`.
- **Fase 126 — Back-face culling seguro:** meshes declaram explicitamente se
  permitem descartar faces traseiras. Caixas e composições só de caixas evitam
  rasterização invisível; geometrias legadas de cilindro continuam dupla face.
  Um cache troca o estado OpenGL somente quando necessário e o `F3` mostra
  quantos draws receberam culling e quantas mudanças de estado ocorreram.
- **Fase 127 — Culling dos cilindros:** o winding lateral dos cilindros foi
  corrigido para manter todas as faces orientadas para fora. Cilindros e meshes
  compostos por caixas/cilindros agora também descartam faces traseiras, com
  regressão geométrica explícita e sem alterar receitas, seeds ou hashes.
- **Fase 128 — Benchmark gráfico reproduzível:** `--benchmark PROFILE` executa
  a mesma seed e câmera inicial em passo fixo, 1280×720 e VSync desligado, sem
  ler ou gravar preferências pessoais. Um executor percorre os cinco perfis e
  coleta uma linha estável com tempos, FPS e carga real do renderer.
- **Fase 129 — Fundação de instancing:** meshes WKC aceitam lotes validados de
  matrizes, configuram atributos divisores e emitem `glDrawElementsInstanced`.
  O caminho tradicional permanece intacto e uma regressão OpenGL comprova duas
  instâncias reais antes da integração concreta do GDP.
- **Fase 130 — Instancing de salas e corredores:** superfícies opacas com o
  mesmo material, tint e UV são agrupadas em um draw. A cena de benchmark reduz
  de 265 para 185 draws em `MEDIUM`, preservando pixels e triângulos; F3 e o
  resumo registram lotes, instâncias e submissões evitadas.
- **Fase 131 — Especialização do budget de sombras:** o shader completo possui
  variantes para 3, 6 e 8 oclusores e seleciona a menor capacidade suficiente
  por frame. O caminho de referência com capacidade 8 permanece disponível à
  regressão, que exige pixels idênticos para a mesma cena.
- **Fase 132 — Benchmark estável por mediana:** o runner aceita um número ímpar
  de repetições, preserva cada amostra, rejeita mudanças nos contadores
  estruturais e publica a mediana de cada métrica. O alvo estável executa três
  matrizes sem substituir o benchmark rápido de uma passagem.
- **Fase 133 — Proveniência da GPU:** a inicialização registra vendor, renderer
  e versão OpenGL; as linhas de benchmark carregam a mesma identidade em formato
  legível por máquina. Matrizes repetidas falham se o dispositivo mudar entre
  passagens, impedindo comparar Intel, NVIDIA ou renderização por software.
- **Fase 134 — Weak points:** inimigos orgânicos recebem um volume menor na
  cabeça e o IndustrialRobot no sensor superior. Acertos aplicam `1,75x` de
  dano, publicam a zona no resultado do hitscan e usam confirmação dourada
  própria no HUD, sem RNG ou mudança em tiros no torso. Campaign/Speedrun
  avançam para regras 21/20.
- **Fase 135 — Spread determinístico:** o primeiro tiro permanece central e
  disparos rápidos seguem um padrão fixo de duas dimensões, mais estreito na
  Pistol que na Carbine. Tempo ou recarga restauram precisão; a retícula abre
  enquanto a sequência está ativa. Campaign/Speedrun avançam para regras 22/21.
- **Fase 136 — Carbine automática:** manter o botão primário pressionado repete
  tiros da Carbine pelo fixed update e pelo cooldown real da arma. A Pistol
  permanece semiautomática; pente vazio não repete dry fire. Campaign/Speedrun
  avançam para regras 23/22.
- **Fase 137 — Recarga temporizada:** `R` inicia uma transação de `0,82 s` na
  Pistol e `1,05 s` na Carbine. A munição permanece na reserva e disparos ficam
  bloqueados até o fixed update concluir a transferência; o viewmodel usa o
  mesmo progresso autoritativo. Campaign/Speedrun avançam para regras 24/23.
- **Fase 138 — Loadout de duas armas:** encontrar a Carbine preserva a Pistol
  no slot 1; `1`, `2` e roda do mouse alternam os slots pelo fixed update. Cada
  arma conserva munição própria, enquanto melhorias alcançam ambas e duplicatas
  não reabastecem. Checkpoint schema 3 grava os dois slots; regras avançam 25/24.
- **Fase 139 — Troca de arma temporizada:** Pistol entra em `0,34 s` e Carbine
  em `0,48 s`, pelo estado autoritativo de `Weapon`. Disparo, recarga e outra
  troca ficam bloqueados até a entrada terminar; o viewmodel lê o mesmo timer.
  Campaign/Speedrun avançam para regras 26/25.
- **Fase 140 — Seleção responsiva:** `1` passa a selecionar a Carbine e `2` a
  Pistol. As entradas caem para `0,32 s`/`0,24 s`; teclado ou roda podem
  interromper uma animação para exibir imediatamente o outro slot, reiniciando
  seu bloqueio de disparo. Campaign/Speedrun avançam para regras 27/26.
- **Fase 141 — Faca de emergência:** `3` seleciona uma Knife sempre disponível,
  sem munição, com alcance de `1,65 m`, 48 de dano e golpe semiautomático a cada
  `0,52 s`. A roda percorre as armas possuídas e a faca; sacar a arma branca não
  altera munição nem o slot de fogo persistido. Campaign/Speedrun avançam para
  regras 28/27.
- **Fase 142 — Agachar e pular:** segurar o comando de agachar reduz o
  collider e a velocidade,
  com câmera interpolada e bloqueio seguro ao tentar levantar sob obstáculos;
  `Espaço` aplica salto físico com gravidade, colisão de cabeça e aterrissagem
  sobre o cenário. Campaign/Speedrun avançam para regras 29/28.
- **Fase 143 — Armas em primeira pessoa, variante Used:** rifle, pistola e faca
  receberam malhas próprias com componentes mecânicos, chanfros e materiais
  metálico/polímero de desgaste moderado. A implementação e os testes técnicos
  estão concluídos; empunhaduras e enquadramento foram corrigidos após a
  primeira avaliação, mas a aceitação visual ainda depende de playtest.
- **Fase 144 — Sinais de combate:** cada disparo aceito produz um lote local
  determinístico de `WeaponFired`, contato e dano/morte. `Application` usa esse
  contrato para feedback visual, HUD e áudio, mantendo dano, drops, progressão e
  colisão no resultado autoritativo. Rejeições não produzem tiro fictício.
- **Fase 145 — HUD Rail v1:** após aprovação da variante A, o HUD de gameplay
  ganhou módulos finos separados para recursos, progresso e objetivo/evento.
  Menus e lógica de jogo não mudaram. Capturas in-engine em 1280×720, 960×540
  e pouca luz estão disponíveis; falta aceitação visual jogando.
- **Fase 146 — Agachar em Ctrl:** `Ctrl` esquerdo ou direito substitui `C`
  como comando de agachar; a física, a câmera e o salto não mudaram. As duas
  teclas são rastreadas separadamente para uma não cancelar a outra ao soltar.
- **Fase 147 — Troca direcional e última arma:** `Q` alterna com a última arma
  realmente selecionada; a roda percorre os slots disponíveis para frente ou
  para trás conforme o sentido. A memória de `Q` é transitória e não altera o
  formato do checkpoint.
- **Fase 148 — Caminhada silenciosa:** segurar qualquer `Shift` reduz a
  velocidade horizontal para 52% da normal, além do passo
  e seu volume; os inimigos investigam passos normais próximos, mas só ouvem
  passos silenciosos/agachados praticamente ao lado. Visão e disparos mantêm
  seu alcance normal.
- **Fase 149 — Mira no botão direito:** segurar `Mouse2` aproxima a câmera
  suavemente e reduz a dispersão dos tiros; a faca não usa mira. O FOV base
  configurado pelo jogador permanece como referência ao soltar o botão.
- **Fase 150 — Lanterna:** `F` liga/desliga um cone de luz preso à câmera,
  independente das luminárias do ambiente. A primeira versão não projeta
  sombras nem usa bateria; desligada, não consome um slot de luz da sala.
  Após o primeiro teste em jogo, o feixe foi reforçado a média distância e
  passou a ter a indicação `F LIGHT ON` no HUD.
- **Fase 151 — Painel rápido:** segurar `Tab` mostra placar, abates, eventos,
  loot, seed e tempo da run sem pausar a simulação. `F3` permanece como painel de
  desempenho separado.
- **Fase 152 — Soltar arma:** `G` coloca a arma de fogo selecionada à frente do
  jogador quando há espaço livre. A arma no
  chão conserva pente e reserva, pode ser recuperada por coleta automática e
  não concede XP pela segunda coleta. A faca é permanente e não pode ser solta.
  A regra atual permite largar também a última arma de fogo: o jogador fica
  apenas com a faca, sem slots fantasmas de Pistol/Carbine. O checkpoint schema
  4 grava esse estado; checkpoints schema 3 com arma continuam legíveis.
- **Fase 153 — Lanterna e percepção:** enquanto `F` mantém a lanterna ligada,
  inimigos podem adquirir o jogador até 25% mais longe, limitado a 4 m extras.
  O cone frontal, a linha de visão, a sala ativa e o alcance normal com a
  lanterna desligada não mudam. Os atalhos `4` e `5` continuam reservados, sem
  equipamento ou item especial associado por enquanto.
- **Referência de controles:** `CONTROLS` no título e na pausa lista os comandos
  implementados. `Esc`, `Enter` ou clique em `BACK` retorna ao menu de origem;
  abrir a página durante uma run não avança a simulação.
- **Bunny hop do Player:** saltos encadeados conservam impulso horizontal e
  air-strafe pode elevar a velocidade até `7,2 m/s` (`1,6×` a caminhada);
  uma janela de `0,12 s` aceita o próximo pressionamento de `Espaço` pouco
  antes de aterrissar. Caminhada, diagonal normalizada, agachamento e modo
  silencioso mantêm seus limites; não há repetição automática ao segurar a tecla.
- **Base de neuroevolução — núcleo genérico:** `wkc_forge_ai` fornece uma rede
  feedforward pequena (uma camada oculta, `tanh`) e genomas copiáveis com
  inicialização, mutação e cruzamento determinísticos por seed independente.
  A implementação usa C++20/RAII para seguir a arquitetura atual, embora a
  proposta original exemplifique C. Entradas são normalizadas em `[-1, 1]`;
  pesos gerados e mutados ficam nesse intervalo. Há testes unitários de forma,
  forward, clone, seed, mutação e cruzamento. Nesta entrega inicial, ainda não
  havia sensores ou integração com os inimigos; a IA da partida permaneceu
  inalterada até a etapa experimental seguinte.
- **Inimigo neural experimental:** `--experimental-neural-enemy TIPO` habilita um
  único inimigo do arquétipo escolhido por partida (`worker`, `guard`, `officer`,
  `engineer` ou `robot`). `--experimental-neural-worker` permanece como alias.
  Os sensores incluem distância/ângulo apenas sob percepção,
  visibilidade, vida e proximidade de paredes. A rede decide movimento local,
  rotação e pedido de ataque a 10 Hz; colisão, cooldown, alcance e linha de
  visão continuam obrigatórios. A seed é derivada em domínio independente.
  No jogo normal, os cinco cérebros distribuídos em `assets/ai/` são carregados
  automaticamente. Cada inimigo tem seu próprio estado de decisão, compartilhando
  os pesos treinados do seu arquétipo. Se um arquivo estiver ausente ou inválido,
  somente aquele arquétipo volta à IA anterior. `--scripted-enemies` força a IA
  anterior para todos; a opção experimental continua isolando um único inimigo.
  Para testar uma coleção completa sem alterar `assets/ai/`, use
  `--test-brain-dir DIR`: a pasta precisa conter `worker.brain`, `guard.brain`,
  `officer.brain`, `engineer.brain` e `robot.brain`, todos válidos e do tipo
  correto. Se faltar algum, o jogo interrompe a inicialização da partida em
  vez de misturar silenciosamente cérebros antigos e novos. O modo de teste
  não pode ser combinado com `--scripted-enemies` ou com a opção experimental
  de um único inimigo; sem a flag, o jogo volta aos cérebros distribuídos.
  No jogo normal, a navegação e a mira têm salvaguardas para manter o combate
  legível; se um cérebro não progride nem consegue atacar por dois segundos,
  o inimigo usa temporariamente a IA anterior.
  As habilidades especiais de suporte/reparo continuam sob suas regras
  existentes; a rede controla movimento, rotação e ataque.
  A contagem de suporte/reparo aparece no log de treino, mas não entra no
  fitness porque essas ações ainda não são decisões da rede.
  Nos cérebros legados, o movimento lateral continua limitado a um ajuste
  curto de direção e um pedido apenas lateral vira um passo cauteloso para
  frente. Cérebros táticos v3 usam frente e lateral independentes, com
  normalização da diagonal. Assim os arquivos antigos continuam carregando
  sem reinterpretar seus pesos.
- **Treinamento automático dos cinco arquétipos:** `--ai-training TIPO` executa
  simulações determinísticas sem abrir janela ou inicializar renderização/áudio.
  O currículo 8 libera, ao longo das gerações, sete estágios: locomoção, mira,
  controle de tiro, esquiva, duelo, posicionamento e mapas gerados. Cada estágio
  usa duas seeds de treino por arquétipo; o holdout avalia os sete estágios com
  seeds independentes. `--all-scenarios` aumenta para quatro seeds por estágio
  (currículo 9). Dois candidatos iniciais são reintroduzidos ao abrir cada
  estágio, para que uma fase sem combate não elimine todas as políticas que
  sabem atirar. Use pelo menos sete gerações para completar os sete estágios;
  treinos mais curtos são diagnósticos e exibem `stages_completed`.
  O treino usa `controlAll=true`, como o jogo, com estado motor e fallback por
  inimigo; `runtime_control_all=1` no log confirma esse contrato.
  Worker pratica perseguição e ataque próximo; Guard, alinhamento
  de mira e faixa de tiro; Officer, posicionamento junto a aliados; Engineer,
  sobrevivência e alcance de reparo; IndustrialRobot, as três faixas de HP e
  combate mais longo. Movimento do jogador, orientação inicial, cobertura e
  cadência dos disparos recebidos variam. Um dos cenários de treino usa
  `Player::fixedUpdate` para saltos/Bhop reais; a etapa final também pode usar
  Bhop em um mundo gerado com corredor. Os demais usam trajetórias roteirizadas.
  As regras de recompensa `reward_rules=3` pontuam apenas dano,
  acertos e eliminações de autoria neural; dano durante fallback e suporte
  automático aparecem no log, mas não rendem pontos ofensivos. Dano recebido
  penaliza o episódio mesmo durante fallback. Eliminar o Player vale
  `+10`, morrer vale `-10`, e a soma dos termos auxiliares fica entre `-4` e
  `+4` por episódio. O progresso na faixa ideal é líquido e limitado contra
  ciclos de aproximação/recuo; reversões improdutivas são punidas, mas uma
  reação a ameaça de tiro iminente não é. Avanço, recuo e strafe recebem
  atribuição diagnóstica proporcional à intenção, sem recompensa por apenas
  comandar uma direção. Os disparos neurais
  remotos exigem alinhamento de 12°; Worker usa 35° para ataque próximo. A
  população inicial inclui uma política simples que percebe, gira e tenta
  atacar, além de indivíduos aleatórios; a evolução pode superá-la. A seleção
  combina média e pior cenário para reduzir especialização excessiva. O log
  mostra `aim_s`, suporte e demais métricas do campeão. Comando e reparo ainda
  são ações roteirizadas: a rede aprende somente o posicionamento que pode
  facilitar essas ações, não decide quando executá-las. Sem visão atual do
  Player, a navegação volta aos estados normais de alerta/investigação em vez
  de repetir comandos neurais sem alvo. O treino preserva os
  melhores e cria descendentes por torneio e mutação.
  `--save-dir` escreve um arquivo versionado com arquétipo e checksum por
  geração, sem substituir arquivos
  existentes. `--resume` inicia outra sequência a partir de um desses campeões
  do mesmo arquétipo (não reproduz exatamente a população anterior). Um treino
  novo não substitui automaticamente os assets distribuídos: escolha, teste e
  promova manualmente os novos campeões a `assets/ai/`. Arquivos
  antigos de Worker ainda podem ser carregados no jogo. Treinos novos usam o
  contrato tático de cérebro (`GDP_ENEMY_BRAIN 3`): sensores de velocidade e
  ameaça do Player, munição e recarga, além de strafe independente e uma ação
  de recarga. Os arquivos anteriores continuam jogáveis, mas não podem ser
  usados com `--resume` no contrato novo, pois seus pesos têm dimensões e
  significados diferentes. `--ai-training all` treina os
  cinco arquétipos em sequência e salva cada um em uma subpasta de `--save-dir`;
  `--resume` exige um tipo específico. O treino ainda usa
  arenas simplificadas nos seis primeiros estágios e não ensina coordenação
  entre inimigos; melhora de fitness não equivale a aprovação de gameplay,
  que ainda requer playtest.
  Em notebooks que aquecem sob carga contínua, `--cooldown-ms N` faz uma pausa
  de N milissegundos entre gerações, sem alterar os pesos ou a pontuação.
  A pausa não substitui o monitoramento da temperatura: interrompa o treino
  se a CPU se aproximar do limite térmico informado pelo hardware.
  Pontuações de `reward_rules=3` não são comparáveis numericamente às de runs
  anteriores. `--resume` aceita apenas um checkpoint do contrato tático novo;
  um cérebro antigo continua jogável, mas o treino novo precisa começar fresco.
  Os checkpoints gerados não substituem os ativos sem
  avaliação comparativa e aprovação visual. Inimigos de alcance agora lançam
  projéteis visíveis com trajetória fixa, colisão com cobertura, carregador e
  recarga; Worker continua com ataque corpo a corpo. O treino registra tiros
  disparados, acertos reais, tiros bloqueados e tiros evitados pelo Player.
  O Player ainda usa hitscan: a defesa da IA contra ele depende de antecipar
  mira e buscar cobertura, não de desviar depois que o tiro foi disparado.
  A etapa final usa a navegação de mundos gerados e posições em corredor;
  treino e holdout variam as seeds. Portas fechadas e múltiplos cômodos em
  sequência ainda não são cobertos por todos os exercícios.
  Para aproveitar a política inicial e a mira mais exigente, comece uma
  população nova, usando um diretório de saída novo e vazio.

  Exemplo após compilar o preset Debug (use um diretório novo ou vazio):

  ```bash
  build/debug/gdp --ai-training all --population 24 --generations 30 --seed 42 \
    --save-dir build/ai-runs/all-42
  build/release/gdp --ai-training all --all-scenarios --population 24 \
    --generations 1200 --seed 424242 --cooldown-ms 1000 \
    --save-dir build/ai-runs/all-scenarios-424242
  build/debug/gdp --experimental-neural-enemy guard \
    --neural-brain build/ai-runs/all-42/guard/guard_generation_000030.brain
  build/debug/gdp --scripted-enemies # comparação com a IA anterior
  ```

  Para continuar um único tipo, combine `--ai-training TIPO`,
  `--resume CAMINHO_DO_BRAIN`, `--generations N` e
  `--save-dir NOVO_DIRETORIO`.

- **Avaliação reproduzível de gameplay da IA:** `gdp_ai_gameplay_eval` compara a
  IA anterior (`scripted`) e os cérebros carregados (`neural`) em cinco situações:
  espaço aberto, cobertura central, jogador elevado, grupo e cobertura do boss
  (esta última apenas para o robô). Usa salas Start/Boss de um mundo gerado pela
  seed indicada. Obstáculos e posições de teste variam de forma determinística
  com a seed; os inimigos são atualizados a 60 Hz e o jogador segue o mesmo
  movimento nas duas políticas. O CSV informa dano
  causado ao jogador, acertos, deslocamento dos inimigos, menor distância,
  tempo até derrota, ticks controlados pela rede, projéteis disparados,
  bloqueados e esquivados, além das recargas.
  Pode-se avaliar um checkpoint ainda não promovido sem alterar os assets:

  ```bash
  cmake --build --preset debug --target gdp_ai_gameplay_eval
  build/debug/gdp_ai_gameplay_eval --seed 12345 --frames 360
  build/debug/gdp_ai_gameplay_eval --seed 12345 --frames 360 \
    --brain robot=build/ai-runs/EXEMPLO/robot/robot_generation_010000.brain
  ```

  Para comparar checkpoints, execute com as mesmas seeds e a mesma quantidade
  de frames; `--brain-dir` aponta para outro conjunto completo de cinco cérebros.
  Esta é uma medição controlada, não uma partida completa: usa os limites das
  salas geradas, mas substitui a decoração/colisão interna por obstáculos de
  teste, não simula tiros do jogador nem progressão de campanha. Dano maior ao
  jogador, isoladamente, não aprova um cérebro; cobertura, navegação, legibilidade
  e equilíbrio ainda precisam de playtest.

  Para uma primeira comparação em **duelo bilateral**, adicione `--duel`:

  ```bash
  build/debug/gdp_ai_gameplay_eval --duel --seed 12345 --frames 360
  ```

  Esta opção mantém o CSV antigo inalterado quando ausente. No duelo, um bot
  fixo controla o Player pela física real em solo ou com saltos/Bhop encadeados,
  mira no centro do inimigo e atira usando `Weapon`/`fireHitscan`. O CSV registra
  vitória, derrota ou limite de tempo, tiros e acertos do Player, acertos do
  inimigo, dano bilateral, saltos, pico de velocidade e deslocamentos. A arma
  fraca do bot é apenas um instrumento de avaliação, não um ajuste de balanceamento
  da campanha. O espaço é aberto e não contém objetos/portas: a mira automática
  e esse cenário sintético não comprovam qualidade de navegação, precisão contra
  jogadores humanos ou desempenho em uma partida completa. Compare políticas
  nas mesmas seeds e valide visualmente antes de promover qualquer cérebro.

O bootstrap atual contém CMake, SDL3, janela, contexto OpenGL 3.3 core, game loop
com fixed update de 60 Hz, snapshots de input, logging e modo headless para
testes. O núcleo inclui streams aleatórios determinísticos, derivação genérica e
versionada de seeds e tipos matemáticos básicos; os domínios usados pelo GDP
pertencem à camada do jogo. Quando SDL3
não está instalado, o build baixa a versão 3.4.14 oficial com hash SHA-256
fixado.

O renderer atual desenha a sala ocupada e a geometria alcançável em até duas
conexões usando origem e rotação reais, com cubo indexado reutilizado e materiais 
procedurais próprios de parede/piso/teto derivados da seed da sala, aberturas e 
molduras ciano nas portas, painéis vermelhos durante lockdown, corredores com piso, 
teto e paredes, até oito luzes pontuais combinadas entre a sala atual e as duas 
camadas vizinhas, luminárias emissivas, ambiente por tema,
câmera em perspectiva e retícula. Parede com grandes placas e rodapé
amarelo/preto, piso de placas com
faixa de tráfego e teto com luzes lineares usam receitas e cache separados. A
cena é produzida em framebuffer próprio e apresentada na janela. O jogador
controla essa câmera com `W/A/S/D` e mouse, colide com
paredes e props, usa `E` para abrir portas fisicamente ou interagir com o
terminal, segura `Ctrl` para agachar, pula com `Espaço`, dispara com o botão esquerdo
e recarrega com `R`; após encontrar a Carbine, `1` seleciona a Carbine, `2`
seleciona a Pistol, `3` seleciona a Knife, `Q` retorna à última arma, e a roda
percorre as armas disponíveis nos dois sentidos. `G` solta a arma de fogo
selecionada mesmo quando é a última; a faca não pode ser solta. `F` alterna a
lanterna e `Tab` mostra o painel rápido; `4` e `5` não executam ação por
enquanto. `F3` alterna o
diagnóstico de desempenho. Pickups próximos
são coletados automaticamente; objetivo e
resultado aparecem dentro do viewport, enquanto run, HP, munição, chaves e os
mesmos estados permanecem no título como apoio. `Esc` abre a pausa durante a
run; `W/S` ou setas navegam, `Enter` confirma, o mouse seleciona/clica e `Esc`
retorna ao jogo. `EXIT GAME` encerra a aplicação pelo menu.

Em Victory ou Defeat, a simulação congela e o menu de resultado oferece
`NEW RUN/NEXT SEED`, `REPLAY SEED`, `MAIN MENU` e `EXIT GAME`; teclado e mouse
usam a mesma navegação.

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
válidas da sala. `Health`, `Ammo`, `Key`, `Armor`, `Weapon` e `Special` recebem 
quantidade e posição determinísticas, sem ocupar connectors. A START contém uma 
caixa de munição para teste imediato; recursos cheios não consomem pickups, 
enquanto coleta válida atualiza Player/Weapon e conclui o ciclo de vida da entidade.

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

Na Fase 59, as transições das portas ganharam leitura sonora própria. Interação
e reversão disparam `DoorOpen`, fechamento automático ou por lockdown dispara
`DoorClose`, e a conclusão do bloqueio dispara `DoorLock`. Cada cue é
espacializada no connector da edge mais próximo do jogador e usa um contador
por porta, preservando determinismo mesmo quando várias edges mudam no mesmo
fixed update.

Na Fase 60, `FootstepState` passou a acumular somente a distância horizontal
realmente resolvida pela colisão. Cada 1,60 m grounded emite um `Footstep`
procedural curto e metálico na posição do Player. Movimento bloqueado, estado
aéreo/morto e segmentos acima de 0,50 m não produzem passos; o último caso evita
rajadas falsas após reposicionamento. O índice dedicado dá variação reproduzível
sem consumir a sequência dos demais eventos de áudio.

Na Fase 61, `ViewMotionState` passou a produzir um head bob discreto a partir do
mesmo deslocamento efetivo pós-colisão. Um ciclo cobre 3,20 m, alternando a
oscilação lateral entre os pés; amplitude máxima é 3,5 cm vertical e 1,8 cm
lateral. Entrada e parada convergem suavemente, estado aéreo perde peso sem
avançar fase e segmentos acima de 0,50 m zeram o efeito. O offset existe somente
na `CameraData` de renderização, nunca na origem usada por tiro ou interação.

Na Fase 62, menu principal e pausa ganharam feedback procedural para mudança de
seleção, confirmação e retorno por `Esc`. As ações usam `wasKeyPressed` e mudança
efetiva de opção, portanto tecla mantida ou hover estacionário não repete cue. O
mixer genérico ganhou bus `UI`, volume independente e `updateUi()`: enquanto o
overlay bloqueia o fixed update do jogo, apenas vozes da interface avançam;
Effects e Ambient preservam seus cursores até retomar.

Na Fase 63, o menu de pausa recebeu a mesma paridade de ponteiro já presente no
menu principal. Hover escolhe `RESUME`, `RESTART RUN` ou `EXIT GAME`; o clique
primário confirma somente quando aponta para um botão. `PauseMenuLayout` é a
fonte única para desenho e hit test, e a Application converte coordenadas da
janela para o framebuffer antes da consulta, preservando escala high-DPI.

Na Fase 64, o banner terminal foi promovido a menu funcional. Victory seleciona
`NEW RUN` e Defeat seleciona `RETRY` como ação segura; a alternativa é
`EXIT GAME`. `W/S`, setas, `Enter`, `Esc`, hover e clique usam o mesmo estado
puro, e `ResultMenuLayout` mantém desenho e hit test alinhados. A transição de
sessão libera imediatamente o mouse sem alterar a regra determinística de
`SessionSystem::restart`.

Na Fase 65, pausa e resultado ganharam `MAIN MENU`. Ao confirmar, uma run em
andamento é encerrada antes de `SessionSystem::restart` preparar a próxima seed.
Entidades, mundo, eventos, colisão, feedback, áudio e caches gráficos da run são
limpos; o renderer é reinicializado no contexto OpenGL existente e a janela
permanece aberta. Iniciar novamente pelo título usa a seed já preparada.

Na Fase 66, `SettingsMenuState` passou a oferecer seis ajustes funcionais:
volumes Master/Effects/Ambient/UI de 0–100%, sensibilidade de 30–200% e FOV de
60–100 graus. A/D ou setas laterais alteram o valor; W/S ou setas verticais
navegam; mouse seleciona a linha e ajusta pelas setas `<` e `>`. Mudanças
são aplicadas imediatamente e sobrevivem a novas runs dentro do processo.

Na Fase 67, `SettingsStore` passou a carregar e salvar esses valores em um TOML
simples de versão 1 no diretório retornado por `SDL_GetPrefPath`. O carregamento
aceita campos ausentes e desconhecidos; números fora da faixa são clampados e
campos conhecidos malformados preservam o default. Ao sair de `SETTINGS`, a
Application escreve `settings.toml.tmp`, faz flush, relê e valida o conteúdo e
só então executa a troca no mesmo diretório. Falhas preservam o arquivo anterior
e o shutdown repete uma gravação ainda pendente. O modo headless não consulta
nem altera preferências do usuário.

Na Fase 68, a sétima opção funcional `INVERT Y` passou a controlar somente o
sinal do delta vertical aplicado por `Player::applyLook`; yaw e sensibilidade
permanecem idênticos. A linha usa `ON/OFF` e mantém os mesmos controles de teclado
e mouse. O arquivo evoluiu para versão 2 com `invert_mouse_y = true/false`;
arquivos v1 continuam legíveis, carregam a opção nova como `OFF` e só são
reescritos após uma alteração real do usuário.

Na Fase 69, `FULLSCREEN` tornou-se a oitava configuração funcional. Esquerda
seleciona `OFF` e direita seleciona `ON`; a Application compara o estado desejado
com as flags reais da janela e chama `SDL_SetWindowFullscreen` somente quando
necessário. A troca mantém run, contexto e renderer, cujo framebuffer já acompanha
o tamanho atual a cada quadro. O schema v3 adiciona `fullscreen = true/false` e
migra v1/v2 com `OFF`; falha do backend é não fatal, restaura o estado real e
fica registrada no log.

Na Fase 70, `RESOLUTION` adicionou quatro presets 16:9, navegados sem wrap entre
960×540 e 1920×1080. A janela inicia diretamente no preset persistido e
`SDL_SetWindowSize` aplica mudanças em runtime, recentralizando-a sem reconstruir
a run ou o contexto. Em fullscreen, o tamanho lógico fica guardado e é aplicado
ao retornar ao modo janela; o framebuffer continua seguindo o tamanho real em
pixels. O schema v4 salva `window_width/window_height`, migra v1–v3 para
1280×720 e rejeita pares que não pertencem ao catálogo.

Na Fase 71, o ajuste por mouse deixou de inferir a direção pela metade global do
framebuffer. `SettingsMenuLayout` passou a fornecer botões de decremento e
incremento para cada linha, usados tanto pelo desenho quanto pelo hit test. O
valor fica centralizado entre `<` e `>`; clicar na seta esquerda reduz, clicar na
direita aumenta e clicar no label/valor somente seleciona a opção. O espaçamento
vertical também se adapta a alturas compactas sem sobrepor linhas quando há
espaço físico suficiente.

Na Fase 72, `VSYNC` tornou a sincronização vertical uma preferência explícita,
aplicada pelo SDL ao contexto OpenGL ativo sem recriar janela, renderer ou run.
Esquerda seleciona `OFF`, direita seleciona `ON`, e falhas do backend são não
fatais. O schema v5 adiciona `vertical_sync = true/false`, migra v1–v4 com `ON`
e preserva a configuração entre execuções. O layout passou a acomodar onze
linhas e reduz apenas a altura dos hit targets em framebuffers muito compactos.

Na Fase 73, toda saída iniciada pelos menus passou por
`ExitConfirmationState`. O diálogo preserva o título, pausa ou resultado por
baixo, começa em `CANCEL` e só encerra o processo depois da escolha explícita de
`EXIT`. `Esc` cancela, WASD e todas as setas alternam as duas opções, e mouse,
teclado, desenho e hit test compartilham o mesmo estado. O fechamento solicitado
pelo sistema operacional continua imediato.

Na Fase 74, `SessionState` passou a possuir `elapsedRunTicks`, avançado somente
uma vez por fixed update jogável. Pausa, configurações, título e resultado não
consomem tempo; o passo que decide Victory/Defeat é incluído, o resultado fica
congelado e `restart()` zera a nova run. O renderer formata os ticks de 60 Hz em
`MM:SS.cc` ou `HH:MM:SS.cc`, exibindo o tempo no HUD enquanto Application usa o
mesmo texto no título e no log terminal. Essa é a base determinística de
Speedrun e ranking, ainda sem criar um modo separado.

Na Fase 75, o título passou a oferecer `START RUN`, `SPEEDRUN`, `SETTINGS` e
`EXIT GAME`. A primeira opção seleciona `Standard`; a segunda registra
`RunMode::Speedrun` antes da geração, sem alterar seed, combate ou estrutura do
mundo. Standard congela seu tempo em pausa/configurações; Speedrun mantém o
cronômetro avançando nesses menus para impedir vantagem pelo pause. Victory e
Defeat congelam ambos, retry preserva o modo e voltar ao título permite escolher
outro. HUD, título e logs identificam Speedrun explicitamente.

Na Fase 76, uma vitória em Speedrun passou a ser comparada ao melhor tempo local.
O primeiro resultado válido cria o recorde e apenas um tempo estritamente menor
o substitui; Standard, Defeat, zero ticks e empates não alteram o dado. O arquivo
`speedrun_record.toml`, separado das configurações, registra schema, versão das
regras, ticks e seed por meio de temporário validado antes do rename. Recordes de
regras incompatíveis são ignorados sem serem apagados. Durante Speedrun, o HUD e
o título mostram `BEST`; o resultado destaca `NEW RECORD` quando aplicável.

Na Fase 77, o menu de resultado passou a separar avanço e repetição. `NEW RUN`
após Victory ou `NEXT SEED` após Defeat avança a sequência determinística;
`REPLAY SEED` reinicia todo o runtime com a seed atual, permitindo repetir o
mesmo mapa e comparar rotas. `SessionState` mantém `runIndex` para tentativas e
`seedIndex` para a sequência de mundos, portanto uma repetição não pula a seed
seguinte. Pela mesma coerência, `RESTART RUN` da pausa agora preserva a seed.

Na Fase 78, o menu principal ganhou `BEST SEED`. Quando há recorde local, a
opção mostra seu tempo, seleciona a seed registrada e inicia diretamente em
Speedrun; isso permite retomar o desafio mesmo depois de fechar o jogo. Sem
recorde, o botão exibe `BEST SEED --`, usa cor inativa e rejeita a confirmação
sem fechar o menu ou gerar um mundo. A troca de seed só é aceita antes do
primeiro tick, preservando o estado da run em andamento.

Na Fase 79, `CAMPAIGN` passou a iniciar uma sequência fechada de três andares.
Cada vitória intermediária apresenta `FLOOR CLEAR` e `NEXT FLOOR`; a seed
seguinte usa a mesma derivação determinística protegida das runs, sem aceitar
avanço após Defeat ou além do terceiro andar. O HUD mostra `FLOOR n/3` junto ao
tempo e a vitória final usa `CAMPAIGN CLEAR`. Nesta fundação, cada andar começa
com recursos básicos; o transporte de atributos, munição e progressão será um
incremento próprio, com snapshot e regressão de fronteira entre andares.

Na Fase 80, essa fronteira ganhou `CampaignCarryover`. O snapshot é criado
somente ao confirmar `NEXT FLOOR` e transporta saúde/armadura atuais e máximas,
chaves, definição e munição da arma, além do estado integral de progressão.
Antes da restauração, capacidades, valores finitos, vida, munição e ranks são
validados em conjunto. O novo andar mantém seu próprio spawn e entidades;
velocidade, cooldown da arma, feedback e handles antigos são descartados.
Cada entrada de andar cria ainda uma base própria: `REPLAY FLOOR` e o restart da
pausa restauram essa base, descartando apenas dano e munição gastos na tentativa.
Voltar ao título ou começar outro modo elimina ambos os snapshots.

Na Fase 81, a entrada segura do andar também passou a ser persistida em
`campaign_checkpoint.toml`. O arquivo guarda as versões do schema e das regras,
seed inicial/atual, índices determinísticos, andar e o `CampaignCarryover`; um
temporário é relido e comparado antes da troca atômica. `CONTINUE Fn/3` fica
ativo somente quando todo o conjunto é válido e regenera o mesmo andar desde o
início, sem serializar portas, inimigos ou eventos no meio do combate. Defeat,
replay, retorno ao título e encerramento conservam esse ponto; iniciar uma nova
Campaign o substitui depois que o novo andar termina de inicializar. A vitória
do terceiro boss remove o checkpoint concluído.

Na Fase 82, `campaignDifficultyMultiplier()` passou a derivar uma pressão
determinística exclusivamente do andar: `1.00x`, `1.10x` e `1.20x`. O valor é
aplicado à vida de inimigos fixos e procedurais, velocidade e dano de todos os
arquétipos, além dos budgets já parametrizados de eventos, loot e props. Range,
cadência de ataque, suporte e reparo permanecem inalterados para conservar a
leitura dos encontros. Standard e Speedrun continuam em `1.00x`; Campanha exibe
`THREAT` no HUD e título. Como a regra altera o balanceamento reproduzível, a
versão das regras da Campanha avançou para 2 e checkpoints anteriores são
rejeitados com segurança.

Na Fase 83, Armor deixou de existir apenas como recompensa de Vitality e entrou
na tabela procedural. `DeadEnd` prioriza a combinação Ammo + Armor dentro do
budget atual; Normal e Arena podem selecionar Armor com pesos por tema, maiores
em Security, Storage e Maintenance. A quantidade continua escalando por
profundidade e a coleta cheia preserva o pickup no chão. Como a distribuição
afeta rotas e tempo, `currentCampaignRulesVersion` avançou para 3 e
`currentSpeedrunRulesVersion` para 2; saves e recordes antigos ficam isolados.

Na Fase 84, a sala Secret passou a garantir `Key + Health + Weapon`. O primeiro
pickup Weapon equipa a Carbine: 26 de dano, intervalo de `0,13 s`, alcance de
34 m, pente de 12 e reserva de 48. Bônus já conquistados de Firepower e
Capacity são transferidos para a nova base; o equipamento começa abastecido e
zera apenas o cooldown transitório. Uma Carbine duplicada permanece no chão,
evitando recarga infinita. HUD, título e pickup usam identidade visual própria.
O checkpoint schema v2 serializa o arquétipo, as regras da Campanha avançam
para 4 e as de Speedrun para 3.

Na Fase 85, Arena passou a garantir `Ammo + Special` mesmo sob a maior pressão
da Campanha. Special ativa 12 segundos de Overdrive na arma equipada: o dano
efetivo sobe 50% e o intervalo entre tiros cai para 65% do valor-base. O efeito
usa somente o fixed update, aparece como `OD` no HUD e com segundos restantes
no título; enquanto ativo, outro pickup permanece no chão. Overdrive é estado
transitório: expira deterministicamente e é descartado ao repetir, reiniciar ou
avançar de andar. As regras da Campanha/Speedrun avançaram para 5/4.

Na Fase 86, o movimento dos cinco arquétipos passou a compartilhar um desvio
local determinístico. Quando a direção desejada perde progresso contra um
sólido, o inimigo escolhe um lado pela combinação de `visualSeed` e `EntityId`
e mantém essa decisão por `1,6 s`, evitando oscilação a cada frame. Se o lado
também bloquear, tenta o oposto sem atravessar colisão, bounds ou sala. O estado
é descartado na morte e nunca entra em save; caminhos e tempos de combate podem
mudar, portanto as regras da Campanha/Speedrun avançaram para 6/5.

Na Fase 87, o mesmo movimento passou a combinar a direção tática com repulsão
dos aliados próximos. A distância desejada soma os raios dos dois inimigos e
`0,20 m`; o IndustrialRobot contribui com seu raio maior. Apenas inimigos vivos,
ativos e no mesmo `roomId` participam. A resposta é steering, não sólido rígido,
permitindo que grupos ainda atravessem passagens sem formar uma barreira. Como
o resultado altera posicionamento e tempo, Campaign/Speedrun avançaram para 7/6.

Na Fase 88, o contato entre inimigos deixou de depender somente dessa repulsão
suave. Cada tentativa de movimento testa o segmento percorrido contra o volume
circular dos inimigos vivos da sala, bloqueando tanto penetração quanto troca de
lados no mesmo passo. Uma sobreposição já existente só aceita movimento que
aumente a distância; inimigos em Idle ou Attack também executam uma correção
lenta para abrir espaço sem abandonar `CollisionWorld`. A regra permanece no
gameplay concreto do GDP e usa ordem estável de entidades. Como posições e
tempos podem mudar, Campaign/Speedrun avançaram para 8/7.

Na Fase 89, a entrada concreta do executável passou a selecionar uma seed nova
para cada abertura gráfica sem `--seed`, combinando relógios do sistema com a
fonte de entropia disponível. Depois dessa escolha, a seed numérica continua
imutável e todo o mundo permanece reproduzível. Uma seed explícita sempre vence;
headless sem seed conserva o valor fixo dos testes. Continue, Best Seed,
`REPLAY SEED` e `RESTART RUN` continuam substituindo ou preservando a seed pelas
regras anteriores. A geração não mudou, portanto Campaign/Speedrun permanecem
nas regras 8/7.

Na Fase 90, o estado de comportamento de cada inimigo passou a alimentar uma
pose visual de raiz: Idle respira discretamente, Chase/Reposition usam a
cadência Walk, Attack aplica um alongamento curto e Dead conserva a silhueta
achatada anterior. O ciclo usa somente ticks fixos e uma fase derivada da
`visualSeed`, evitando grupos perfeitamente sincronizados sem consumir RNG. O
contador é reiniciado com a run e não avança em pausa ou resultado; inimigos
dormentes ficam estáticos. Esta primeira camada transforma o modelo consolidado
inteiro. A hierarquia rígida de torso, cabeça e membros, além do refinamento da
pose Hit por partes, permanece para a etapa de revisão dos assets. Colliders,
IA, dano e geração não
mudaram, por isso Campaign/Speedrun continuam nas regras 8/7.

Na Fase 91, a direção visual deixou de permanecer presa ao eixo original do
asset. Depois de cada deslocamento de Chase, o yaw é calculado pelo vetor
realmente percorrido, incluindo desvios de obstáculos e separação do grupo.
Attack encara a posição atual do jogador; Guard, Officer, Engineer e
IndustrialRobot recuam em Reposition sem dar as costas ao alvo. Idle e Death
preservam a última orientação, e deslocamentos nulos não introduzem rotação
inválida. A `Application` envia o yaw no snapshot e o renderer compõe
translação × rotação × escala. Trajetória, colisores e combate permanecem
inalterados, portanto Campaign/Speedrun continuam nas regras 8/7.

Na Fase 92, a orientação passou a convergir deterministicamente em vez de saltar
para o yaw-alvo. Chase usa um limite de `480°/s`; Attack e Reposition usam
`720°/s` para continuar responsivos. A diferença angular usa o arco mais curto,
inclusive ao atravessar `-180°/180°`, e o orçamento é consumido uma única vez
por fixed update. Reposition move para trás, mas gira somente em direção ao
jogador. Pausa, Idle, Death e deslocamentos nulos não avançam a rotação. Como o
resultado é exclusivamente visual, Campaign/Speedrun permanecem nas regras 8/7.

Na Fase 93, `applyDamage()` passou a iniciar um timer visual de `0,12 s` em
acertos não letais. A `Application` avança todos esses timers uma única vez por
fixed update e converte a janela ativa em Hit, com prioridade abaixo apenas de
Death. O renderer aplica uma compressão curta da raiz, tonalidade quente e
emissivo moderadamente reforçado; movimento, ataque, cooldowns e colisores
continuam avançando sem interrupção. Novo dano reinicia a janela, morte a limpa
e o valor não consome RNG. Como esse feedback é transitório e não altera regras,
Campaign/Speedrun permanecem nas versões 8/7.

Na Fase 94, cada primitiva da receita visual passou a ser preservada como uma
parte rígida independente na GPU: torso, cabeça, braços, pernas e acessórios
mantêm ordem e pivôs estáveis, sem regenerar ou perder geometria. O renderer
compõe transform da entidade, pose de raiz e pose local por parte. Walk usa
balanço oposto e discreto de braços/pernas, Attack recua braços e ferramenta, e
Idle limita-se a uma pequena leitura de cabeça. O feedback Hit aprovado como
funcional foi refinado: não comprime nem rebaixa mais o corpo; aplica um recuo
mecânico curto e progressivamente reduz tint/emissivo. A WKC Forge recebeu
somente `rotationXMatrix`, primitiva matemática genérica; estados, amplitudes e
hierarquia continuam concretos do GDP. Gameplay, RNG, colisores e regras 8/7
permanecem inalterados.

Na Fase 95, a pose local passou a escolher um perfil pelo papel visual já
presente no snapshot. O Worker usa passada mais marcada e avanço da ferramenta
no ataque corpo a corpo; Guard e Officer usam balanço e recuo controlados; o
Engineer enfatiza assimetricamente o braço da ferramenta; o IndustrialRobot
tem ciclos mais longos, amplitudes menores e absorve Hit com mais massa. Idle,
Walk, Attack e Hit possuem períodos e amplitudes próprios, mas continuam
amostrados apenas de tick fixo, `visualSeed` e força restante do impacto. Não há
RNG adicional, mutação de gameplay ou mudança nas regras 8/7.

Na Fase 96, a `Application` passou a conservar para cada `EntityId` o estado
visual anterior, o atual e o avanço da transição. Idle/Walk usam 6 ticks,
Attack e a saída de Hit usam 4, a entrada de Hit usa 3 e Death conclui a queda
em 18 ticks. O renderer interpola raiz e partes rígidas com `smoothstep` antes
de compor as matrizes; a pose terminal mantém escala `1:1:1` e gira lateralmente 
em `82°`, com lado derivado da `visualSeed`. Durante Victory/Defeat somente a 
acomodação de inimigos mortos pode avançar; IA, combate, timers da run e inimigos 
vivos continuam congelados. A WKC Forge recebeu apenas `rotationZMatrix`, operação
matemática genérica. RNG, gameplay, persistência e regras 8/7 não mudaram.

Na Fase 97, Death passou a usar um ragdoll procedural visual sobre os pivôs já
preservados. A raiz continua fazendo contato aproximado com o piso, enquanto
cabeça, braços, pernas, acessórios e ferramenta recebem alvos angulares,
atrasos curtos e uma oscilação amortecida independentes. Worker se desarticula
mais; Guard e Officer permanecem contidos; Engineer conserva assimetria; e o
IndustrialRobot limita amplitudes para comunicar massa. Direção, variações e
ordem de acomodação derivam exclusivamente da `visualSeed`, sem RNG novo. Não
há solver físico, colisão de cadáver com paredes/props nem alteração de
collider: a morte continua imediata para IA e combate. Regras 8/7 e saves não
mudaram.

Na Fase 98, o feedback visual expôs que a primeira versão ainda sincronizava
demais a raiz e os membros. Death passou a transportar progresso linear próprio
e aplicar curvas diferentes por estágio: pernas cedem primeiro, a raiz mantém
um intervalo curto de inércia, o torso acelera entre 10–78% e cabeça, braços e
equipamento entram depois. O contato final usa duas oscilações pequenas com
decaimento quadrático. Worker/Engineer levam 36 ticks, Guard 38, Officer 40 e
IndustrialRobot 52, em vez dos 18 ticks comuns anteriores. A alteração continua
exclusivamente visual e determinística; gameplay, RNG, colliders, saves e regras
8/7 permanecem inalterados.

Na Fase 99, a direção binária esquerda/direita foi substituída por um azimute
contínuo derivado por hash da `visualSeed`. A raiz inclina `88°` em direção a
esse vetor e transporta impulso nos dois eixos do piso, permitindo quedas para
frente, trás, lados e diagonais sem consumir o RNG estrutural. Cada parte passou
a ter instantes próprios de primeiro contato e repouso; pernas e torso assentam
antes, enquanto cabeça, braços e a peça de equipamento continuam amortecendo.
A transição só chega a 100% quando a última peça alcança exatamente a pose
terminal. As janelas são 48 ticks para Worker/Engineer, 50 para Guard, 52 para
Officer e 66 para IndustrialRobot. A mudança permanece estritamente visual:
gameplay, colisores, saves e regras 8/7 não mudaram.

Na Fase 100, inimigos ativos fora do alcance de detecção deixam de permanecer
completamente imóveis. `EnemyData` conserva origem, índice do waypoint, tempo
de tentativa e pausa; os destinos formam uma rota curta ao redor do spawn com
ângulo e raio derivados de `EntityId`, `roomId` e índice, sem consumir RNG.
Chegada produz uma pausa breve; um destino bloqueado expira e avança, impedindo
tentativas eternas contra paredes. O movimento usa os mesmos colliders, desvio
e volumes físicos do combate, e o estado `Patrol` alimenta a pose Walk. Player
morto, inimigo dormente, outra sala e estado Dead continuam congelados. Como a
patrulha altera posição e tempo de encontro, `currentCampaignRulesVersion`
avança para 9 e `currentSpeedrunRulesVersion` para 8; saves e recordes antigos
ficam isolados.

Na Fase 101, distância sozinha deixa de revelar o jogador através de paredes,
portas e cobertura. Os cinco arquétipos só entram em combate após confirmar
linha de visão; cada confirmação atualiza uma posição conhecida e reinicia uma
janela própria por papel. Ao perder contato, o estado `Investigate` avança até
esse ponto usando colisão, desvio, separação e giro já existentes, sem consultar
a posição atual escondida. Chegar ao destino mantém observação até o timer
terminar; depois a patrulha retorna. Readquirir visão substitui imediatamente a
memória, e Player morto ou Enemy morto limpa a busca. Como essa percepção muda
rotas e tempos, as regras passam a Campaign 10 e Speedrun 9; o schema de save
continua 2.

Na Fase 102, a sala START recebe uma barricada industrial fixa de `2,25 m` de
altura, `2,80 m` de extensão e `0,46 m` de espessura. Ela fica no lado oposto ao
primeiro Worker, preserva o eixo central entre portas e deixa `1,5 m` livres
entre a face externa e a parede para o Player circular. A extensão bloqueia a
linha de visão durante cerca de `2 m` de deslocamento lateral e a borda devolve
uma visão direta, tornando aquisição, investigação e readquisição observáveis
logo no começo da run. Rotação de sala troca os eixos físicos da escala; colisão,
hitscan e render usam o mesmo volume. O visual funcional de Barricade já
existente é reutilizado; o acabamento HD definitivo continua sujeito à aprovação
visual antes de qualquer textura nova. Como a cobertura altera conteúdo e rota,
Campaign/Speedrun passam às regras 11/10.

Na Fase 103, um disparo que realmente consome munição publica um estímulo de
gameplay na posição do Player. O raio-base de `18 m` é limitado pela audição de
Worker (`10 m`), Guard (`15 m`), Officer (`17 m`), Engineer (`13 m`) e
IndustrialRobot (`22 m`); cooldown e clique seco não revelam posição. Somente
inimigos vivos e ativos da sala atual recebem a memória, sem exigir linha de
visão através da cobertura. No fixed update seguinte eles usam `Investigate`
até a origem ou até o timer expirar, enquanto uma observação visual válida
substitui imediatamente o ruído. O sinal pertence ao gameplay e não depende do
sistema de áudio, RNG ou renderer. Campaign/Speedrun passam às regras 12/11; o
schema do checkpoint permanece 2.

Na Fase 104, o primeiro estímulo ouvido passa por `Alert` antes de
`Investigate`. Worker reage por `0,18 s`, Guard por `0,24 s`, Officer por
`0,20 s`, Engineer por `0,28 s` e IndustrialRobot por `0,34 s`. Durante essa
janela o inimigo interrompe movimento, ataque, comando e reparo, gira suavemente
para a origem e assume uma postura rígida de prontidão com intensidade reduzida
nos papéis mais pesados. Disparos seguintes ainda atualizam a posição ouvida e
renovam a investigação, mas não reiniciam `Alert`, evitando stun lock. Morte ou
Player morto limpam o timer; Hit visual e visão real preservam suas prioridades.
O estado é transitório, não usa RNG e não altera o schema do checkpoint.
Campaign/Speedrun passam às regras 13/12.

Na Fase 105, o fim de `Investigate` entra em `Recover` em vez de retornar
diretamente à patrulha. Worker varre o ambiente por `0,65 s`, Guard por
`0,85 s`, Officer por `1 s`, Engineer por `0,75 s` e IndustrialRobot por
`1,20 s`. A entidade interrompe locomoção tática e ações, gira primeiro para um
lado derivado de `visualSeed + EntityId` e então retorna, enquanto cabeça,
braços e equipamento usam uma pose rígida de busca com menor amplitude nos
papéis pesados. Visão confirmada cancela Recover e retoma combate no mesmo tick;
novo ruído cancela a varredura e volta a `Investigate` sem repetir `Alert`.
Ao terminar, a memória é apagada e a patrulha retorna. Não há consumo de RNG,
mudança de collider ou schema novo. Campaign/Speedrun passam às regras 14/13.

Na Fase 106, todo deslocamento dirigido a um ponto consulta uma rota global
dentro dos volumes navegáveis da sala. O grafo é formado pela origem, destino e
cantos seguros de sólidos/volumes; conexões só existem quando o collider cabe em
todo o segmento. Dijkstra escolhe o menor caminho com ordem estável e entrega o
próximo ponto visível. Isso permite sair inclusive de obstáculos em U, sem RNG,
navmesh universal ou mudança nos fingerprints do mundo. O steering local,
separação e colisão contínua continuam autoritativos como acabamento e fallback.
Campaign/Speedrun passam às regras 15/14.

Na Fase 107, os cinco papéis deixam de possuir visão de 360°. O vetor frontal é
derivado do yaw lógico e comparado ao alvo antes do raycast: Worker usa meia
abertura de `65°`, Guard `75°`, Officer `80°`, Engineer `70°` e
IndustrialRobot `85°`. Ruído continua omnidirecional dentro do alcance auditivo
e pode girar o inimigo para uma futura aquisição visual. O Worker inicial da
START nasce orientado para o Player, preservando o tutorial e o teste headless.
Campaign/Speedrun passam às regras 16/15.

Na Fase 108, rotas de custo equivalente deixam de concentrar todo o grupo no
mesmo canto. A preferência esquerda/direita vem de `visualSeed + EntityId` e
ordena os candidatos do grafo sem consumir RNG ou escolher um caminho mais
longo quando não há empate útil. Se a colisão ativa o desvio local, essa escolha
passa a seguir o lado de evasão e pode replanejar no tick seguinte. A regressão
coloca dois Workers atrás da mesma barreira e exige uso dos dois lados, volumes
físicos e repetição exata. Campaign/Speedrun passam às regras 17/16.

Na Fase 109, `ShotResult` transporta um `ImpactResult` independente do alvo,
com posição real do raycast, normal contrária ao disparo e classe `Metal`,
`Composite` ou `Flesh`. A Application conserva cada impacto por `0,32 s`, filtra
pelo `roomId` atual e entrega ao renderer um snapshot sem regra de dano. Metal
gera fagulhas quentes e mais emissivas, compósitos usam fragmentos ocres e
inimigos orgânicos recebem uma marca vermelha curta; robôs e terminais usam
metal. O áudio de parede passou a usar o mesmo ponto real, em vez de uma
distância fixa diante da câmera. O efeito usa seed visual derivada e não consome
RNG estrutural. Campaign/Speedrun passam às regras 18/17.

Na Fase 110, o progresso lógico linear de `DoorRuntime` é convertido por uma
curva `smoothstep` única antes de mover os dois painéis. O limiar físico de 90%
consulta essa mesma abertura visual, evitando liberar um corredor cuja imagem
ainda parece fechada. A ocupação agora inclui o Player no corredor/connector e
todo Enemy ativo e vivo próximo de qualquer lado da edge; fechamento normal
reverte para Opening e lockdown espera o vão ficar livre. O teste protege
endpoints, aceleração, frenagem, NaN, limiar compartilhado e reversão ocupada.
Campaign/Speedrun passam às regras 19/18; não há RNG ou alteração na WKC Forge.

Na Fase 111, `UiVisualTheme` centraliza fundo, painel, borda, seleção, texto e
cores semânticas de alerta, perigo e sucesso. Um único compositor de moldura
industrial aplica sombra, aro técnico, faixa superior e cantos marcados a todos
os menus e blocos do HUD; linhas selecionadas recebem marcadores nos dois lados.
O menu principal ganha trilhos laterais e instruções secundárias usam contraste
próprio. Geometria de `*MenuLayout`, hit tests, inputs, gameplay e versões de
Campaign/Speedrun permanecem inalterados. CPU protege a hierarquia da paleta e
OpenGL continua cobrindo cada tela e seleção.

Na Fase 112, `WeaponViewmodelRenderData` leva ao renderer somente a identidade
da Pistol/Carbine e frações transitórias de recuo, recarga, equip e flash. A
`Application` inicia essas frações quando o evento lógico já foi aceito e as
avança no fixed update; o renderer amostra uma pose limitada e desenha volumes
metálicos após o mundo, com depth limpo, antes do HUD. Assim, o cano visual nunca
altera o hitscan da câmera. Pistol e Carbine têm comprimentos, empunhaduras e
detalhes próprios; Overdrive troca apenas o detalhe emissivo para magenta. Seed
visual varia discretamente o metal sem consumir RNG estrutural. Gameplay,
persistência, WKC Forge e regras de Campaign/Speedrun permanecem inalterados.

Na Fase 113, cada `RoomPointLight` ganha uma entidade `CeilingFixture` não
sólida, com AABB centrado no modelo, 24 HP e seed visual derivada do domínio de
iluminação. O hitscan considera esse tipo mesmo sem colisão de navegação: dano
abaixo de 50% reduz intensidade/emissão para 45%, e destruição apaga a luz e
escurece a carcaça pelo restante da run. O alvo usa impacto e áudio metálicos,
não gera drop, XP ou reparo de Engineer. A luz ambiente mínima continua ativa
para impedir softlock visual. Por afetar decisões e tempo da run, Campaign e
Speedrun avançam para regras 20/19; RNG estrutural e WKC Forge não mudam.

Na Fase 114, o contrato de iluminação recebe até 16 AABBs de oclusão derivados
de inimigos vivos/ativos e props não destruídos das salas já visíveis. Para cada
luz ativa, o fragment shader testa o segmento superfície–luminária contra esses
volumes e reduz a contribuição difusa/especular bloqueada para 16%; ambiente e
emissão permanecem independentes. As sombras são analíticas, determinísticas e
acompanham movimento sem framebuffer ou RNG adicional. Uma luminária apagada já
tem intensidade zero, portanto deixa de produzir luz e sombra no mesmo frame.
Gameplay, Campaign/Speedrun e WKC Forge permanecem inalterados.

Na Fase 115, `optimizeShadowBudget()` ordena luzes ativas pela distância ao
observador, descarta oclusores inválidos ou fora do alcance e conserva no máximo
duas luzes e oito AABBs relevantes. O teto teórico cai de 128 para 16 testes de
segmento por fragmento iluminado; o renderer repete esses clamps como barreira
de segurança e pula luzes apagadas ou superfícies sem difusa. A reconciliação
de luminárias deixou de alocar/varrer todos os props: `roomFixtureEntities_`
mantém IDs alinhados ao plano de cada sala. Testes protegem seleção, repetição,
fallback escuro e equivalência de pixels sob entrada acima do budget. Não houve
mudança visual intencional, de gameplay, regras ou WKC Forge.

Na Fase 116, `windows-release` configura MSVC x64 e testa os sistemas que não
dependem de uma sessão gráfica. O runtime MSVC é ligado estaticamente e o CMake
copia/instala `SDL3.dll` junto de `gdp.exe`; CPack gera um ZIP autocontido. O job
Windows da CI extrai esse ZIP e executa um smoke test headless a partir do pacote,
impedindo a publicação de um artefato sem executável ou DLL. O código-fonte
continua portátil e as rotas Linux permanecem inalteradas. Instruções de uso e
diagnóstico estão em `README_WINDOWS.md`.

Na Fase 117, `PerformanceMonitor` recebe amostras sanitizadas da orquestração,
mantém uma janela fixa de 120 frames e calcula média, P95, pior caso, FPS e
percentual acima do budget de 60 Hz. A `Application` mede processamento/update,
submissão do renderer e espera de apresentação separadamente; o renderer expõe
somente contadores observáveis de draw calls, triângulos e caches. `F3` alterna
o painel sem pausar ou alterar a simulação. O resumo headless usa seed e número
de frames explícitos para comparações reproduzíveis. A instrumentação ainda não
reduz qualidade: os presets gráficos serão definidos na fase seguinte a partir
de medições em hardware real.

Na Fase 118, uma coleta no ThinkPad W541, ainda sem registrar a GPU ativa,
marcou aproximadamente 9 FPS, P95 de 127 ms, `UPDATE` de 2 ms, `RENDER` de
84 ms e `WAIT` de 29 ms
com oito luzes, duas luzes de sombra e oito oclusores. A medição confirmou
pressão gráfica, não de simulação. `QUALITY` passou a selecionar cinco budgets:
`POTATO` usa escala interna de 50%, duas luzes, nenhuma sombra e materiais
simples, mantendo a sala vizinha direta; `LOW`, 67%, quatro luzes e o mesmo
alcance de salas; `MEDIUM`, 85%, seis luzes e sombra 1×3; `HIGH`, resolução
interna integral e sombra 1×6; `ULTRA`
preserva o caminho anterior, integral e 2×8. O mundo é ampliado para o
framebuffer real antes de HUD e menus, que continuam nítidos e com hit tests na
resolução nativa. O painel `F3` identifica o perfil e a escala ativos.

Na Fase 119, a mesma profundidade de salas do perfil gráfico governa shells,
inimigos, pickups e props. Antes de formar a fila visual, bounds de salas e
corredores são convertidos em esferas conservadoras e comparados à direção e ao
FOV da câmera com uma margem de 14 graus. Objetos individuais passam pelo mesmo
filtro; a sala ocupada é sempre preservada, volumes que contêm a câmera nunca
são removidos e dados inválidos mantêm o draw como fallback seguro. Luzes não
são descartadas por direção, pois ainda podem iluminar superfícies visíveis.
Essa etapa reduz submissão fora da tela sem alterar geração, colisão, IA ou RNG.

Na Fase 120, `wkc::render::GpuTimer` mantém quatro consultas `GL_TIME_ELAPSED`
em rotação. O renderer nunca espera um resultado: se a consulta que seria
reutilizada ainda estiver ocupada, aquele frame não recebe nova amostra. Valores
prontos chegam ao `PerformanceMonitor` com disponibilidade explícita, de modo
que frames de aquecimento e drivers sem suporte não entram como zero na média.
O painel `F3` passa a exibir `CPU RENDER`, `GPU` e `WAIT` separadamente. Assim,
o próximo perfil de otimização pode distinguir custo real da placa, submissão
da CPU e sincronização da apresentação sem alterar pixels ou simulação.

Na Fase 121, `wkc::render::ShaderUniformLocationCache` elimina consultas
repetidas de `glGetUniformLocation` no caminho de draw. O primeiro uso de cada
nome consulta o driver; usos seguintes reutilizam a localização, inclusive
quando ela é `-1`. O cache pertence ao programa, é limpo em reset/recompilação e
preserva o fallback funcional caso não consiga armazenar uma nova entrada. Uma
regressão isolada verifica requests, hits, consultas reais e invalidação, e o
teste OpenGL por pixels protege a aparência já aprovada. Nenhum preset, material,
luz, textura, regra de gameplay ou stream de RNG foi alterado.

Na Fase 122, `wkc::render::ShaderUniformValueCache` compara o último valor
enviado a cada localização antes de atravessar a API OpenGL. Matrizes, vetores,
floats e inteiros mantêm tipos separados e comparação exata; valores realmente
novos continuam sendo enviados, enquanto repetições são omitidas. Os valores
permanecem válidos entre frames, as métricas reiniciam por frame e recompilar ou
destruir o programa invalida tudo. `F3` mostra `UNIFORM UP` e `SKIP`, permitindo
medir o ganho no hardware real. Regressões cobrem mudança de tipo, persistência,
invalidação, contadores e pixels OpenGL. Os cinco perfis gráficos, materiais,
iluminação, gameplay e RNG permanecem iguais.

Na Fase 123, `wkc::render::TextureBindingCache` mantém o estado conhecido das
16 primeiras unidades de textura, acima das quatro usadas pelo renderer atual.
Cada pedido informa separadamente se precisa ativar a unidade e ligar o objeto;
repetições na mesma unidade não atravessam a API OpenGL. Unidades acima do limite
continuam corretas pelo fallback que sempre executa o bind. O começo do frame
invalida todo estado conhecido depois de operações externas de framebuffer e
reinicia as métricas. `F3` mostra `TEXTURE BIND`, `SKIP` e `UNIT`; testes cobrem
troca de unidade, troca de textura, repetição, fallback, invalidação, métricas e
pixels OpenGL. Perfis, imagens, amostragem, iluminação, gameplay e RNG não mudam.

Na Fase 124, `wkc::render::MeshBindingCache` acompanha o vertex array atualmente
ligado. O renderer GDP usa o novo caminho de draw cacheado e mantém o VAO entre
peças consecutivas do mesmo mesh; a API tradicional de `Mesh::draw` continua
fazendo bind/unbind para consumidores que não controlam estado. O começo de cada
frame invalida o conhecimento do cache para tolerar operações OpenGL externas.
`F3` mostra `MESH BIND` e `SKIP`; testes cobrem repetição, troca, invalidação,
métricas e pixels reais. Geometria, ordem dos draws, materiais, perfis gráficos,
gameplay e RNG permanecem inalterados.

Na Fase 125, o GDP mantém dois programas sobre a mesma infraestrutura genérica
de `wkc::render::Shader`. A variante `LOW SPEC` limita o laço a quatro luzes e
retira do fragment shader, por compilação condicional, os caminhos de normal
mapping, reflexão especular e interseção de sombras que já não participavam do
resultado de `POTATO` e `LOW`. A seleção só acontece com materiais detalhados
desligados, até quatro luzes e contadores de sombra zerados; qualquer chamada
inconsistente falha conservadoramente para `FULL`. Uma regressão OpenGL desenha
a mesma cena pelos dois programas e exige pixels byte a byte idênticos. Os
orçamentos, texturas, cores, iluminação efetiva, gameplay e RNG não mudam.

Na Fase 126, `wkc::render::MeshData` passa a transportar a permissão explícita
de back-face culling até o objeto GPU. `makeBoxMesh` marca sua geometria fechada
e com winding consistente como segura; o gerador procedural conserva a marca
somente quando todas as primitivas de uma composição são seguras. Cilindros
legados permanecem deliberadamente dupla face, evitando tornar invisíveis seus
lados até que o winding deles seja revisado em uma etapa própria. O cache
genérico `BackfaceCullingStateCache` invalida o conhecimento a cada frame e
omite `glEnable`/`glDisable` repetidos. O GDP conta draws protegidos no `F3`,
enquanto a suíte OpenGL continua verificando salas, corredores, inimigos,
objetos, arma, HUD e menus. Ordem, triângulos submetidos, texturas, perfis,
gameplay e RNG permanecem iguais; a GPU apenas deixa de rasterizar faces
traseiras de volumes declarados seguros.

Na Fase 127, a lateral de `makeCylinderMesh` passa a usar winding anti-horário
visto pelo lado externo, alinhado às normais já existentes e às tampas. Como as
receitas aceitam somente escalas e dimensões positivas, suas transformações
preservam essa orientação. O cilindro passa a declarar back-face culling seguro,
permitindo que composições de inimigos feitas de caixas e cilindros usem o mesmo
caminho otimizado. Um teste percorre cada triângulo e exige produto escalar
positivo entre a normal geométrica e as normais dos vértices; as regressões
OpenGL protegem o resultado final. Contagem de vértices/índices, bounds, receitas,
seeds, hashes, materiais e gameplay permanecem inalterados.

Na Fase 128, a medição deixa de depender de navegar manualmente até uma cena
parecida. O argumento `--benchmark PROFILE` inicia diretamente a seed solicitada,
descarta input de gameplay, avança exatamente um fixed update por frame e força
janela 1280×720, modo janela e VSync desligado. Se `--frames` não for informado,
240 frames são executados: os 120 primeiros aquecem caches e queries, enquanto a
janela móvel final conserva as 120 amostras comparáveis. O modo não consulta nem
altera `settings.toml`, recordes ou checkpoints.

Ao terminar, uma linha `BENCHMARK` informa perfil, seed, resolução, frames,
amostras, média, P95, pior frame, FPS, percentual acima de 16,67 ms, tempos de
update/render/present/GPU e contadores da última cena. A execução automatizada
dos cinco perfis usa a mesma seed e os mesmos frames; ela estabelece um baseline
repetível da vista inicial, mas não substitui os stress cases de sala cheia,
maior encontro e Boss. O driver offscreen serve somente para validar o pipeline:
seus tempos não representam uma GPU física.

O baseline real de 2026-09-20 no ThinkPad W541 confirmou
`POTATO` em 79,44 FPS (P95 15,13 ms), `LOW` em 74,76 FPS (P95 16,96 ms) e
`MEDIUM` em 29,35 FPS (P95 40,41 ms). Como o update ficou abaixo de 0,11 ms e o
tempo cresceu na GPU/apresentação, a próxima otimização deve atacar submissões e
geometria repetida. A tabela completa está em
[`10_testing/performance_budgets.md`](10_testing/performance_budgets.md).
Essa versão do protocolo ainda não registrava o renderer OpenGL; o W541 possui
uma Quadro K2100M, mas esses números não podem ser atribuídos retroativamente a
ela.

Na Fase 129, `wkc::render::Mesh` recebeu um buffer opcional de transforms por
instância. Matrizes finitas usam quatro atributos consecutivos a partir da
location 4, cada coluna com divisor 1; lotes vazios desativam o draw e entradas
não finitas são rejeitadas sem substituir o lote válido anterior. O carregador
OpenGL 3.3 passa a exigir `glVertexAttribDivisor` e
`glDrawElementsInstanced`, enquanto `MeshBindingCache::invalidate()` mantém o
cache coerente quando a configuração do VAO muda fora do draw comum.

A regressão offscreen da fundação desenha duas caixas separadas com uma única
submissão, verifica pixels nas duas posições e ausência no intervalo, testa o
lote vazio e preserva o caminho não instanciado existente. O renderer concreto
do GDP ainda não usa o novo draw nesta fase; imagens, contadores, gameplay e RNG
permanecem iguais. O agrupamento das peças repetidas de salas e corredores fica
para a Fase 130, sobre uma API já validada isoladamente.

Na Fase 130, o renderer concreto reúne primeiro as superfícies de shells,
corredores e peças estruturais na ordem tradicional. A execução otimizada agrupa
somente comandos com o mesmo material GPU, modo detalhado, tint, escala de UV e
offset; apenas a matriz varia por instância. Portas, luminárias, entidades,
impactos e viewmodel continuam no caminho individual. Um fallback interno
mantém a ordem e os draws anteriores para a regressão comparar os dois caminhos.

Na seed `12345`, os perfis `POTATO/LOW` evitam 13 submissões e passam de 80/96
para 67/83 draws. `MEDIUM/HIGH/ULTRA` evitam 80 e passam de 265/281/281 para
185/201/201, sem alterar a contagem de 3.360/3.552 triângulos. O teste OpenGL
exige pixels byte a byte idênticos entre referência e instancing. O F3 mostra
`INSTANCE DRAW`, `COUNT` e `SAVE`; a linha `BENCHMARK` publica os mesmos campos
como `instanced_draws`, `instances` e `saved_draws`. Gameplay, visibilidade,
materiais, RNG e conteúdo permanecem inalterados.

A repetição em hardware real no W541 confirmou todos os contadores. Nessa
passagem, `MEDIUM` avançou de 29,35 para 30,14 FPS e `HIGH` de 20,19 para 21,02,
mas `LOW` e `ULTRA` oscilaram para pior. O resultado comprova a redução de draws,
não um ganho uniforme de frame time; o gargalo principal continua em GPU e
apresentação e exige amostras repetidas para comparar otimizações pequenas.

Na Fase 131, o limite estático de 16 oclusores no fragment shader foi alinhado
ao teto real de 8 do renderer. O shader completo passou a ter variantes com
capacidade 3, 6 e 8; o renderer escolhe a menor que comporta o frame, enquanto
`POTATO/LOW` continuam no shader simplificado sem sombras. Isso permite que o
driver compile laços e arrays menores para os budgets comuns sem remover luz,
sombra ou material. A regressão OpenGL força a variante de capacidade 8 como
referência e exige framebuffer byte a byte idêntico à variante 3, além de
executar casos reais das capacidades 6 e 8. F3 mostra `SHADOW CAP`, e o resumo
de benchmark publica `shadow_capacity`. O impacto real no W541 continua sujeito
à matriz de hardware; os tempos offscreen não são usados como evidência de FPS.

Na Fase 132, `tools/run_graphics_benchmark.cmake` aceita `GDP_REPEATS` ímpar e
executa matrizes completas na mesma ordem. Cada amostra continua disponível em
uma linha `RUN N/TOTAL BENCHMARK`; ao final, `BENCHMARK_MEDIAN` publica a mediana
independente de média, P95, pior frame, FPS, percentual fora do budget e tempos
de update/render/present/GPU. Draws, triângulos, visibilidade, luzes, sombras e
contadores de instancing precisam permanecer idênticos entre as passagens ou o
runner falha, evitando comparar cenas diferentes. O alvo
`graphics-benchmark-stable` usa três repetições; o alvo anterior conserva uma
para iteração rápida. Essa fase altera apenas medição, sem tocar em renderização,
gameplay, RNG ou qualidade visual.

Na Fase 133, o renderer captura `GL_VENDOR`, `GL_RENDERER` e `GL_VERSION` logo
após criar o contexto. A linha comum e a mediana publicam `gpu_vendor`,
`gpu_renderer` e `gpu_version` como tokens estáveis; o runner inclui esses campos
na assinatura estrutural e recusa uma matriz que troque de dispositivo entre
passagens. No W541 auditado, a sessão padrão usa `Intel HD Graphics 4600`; em
janela real, `DRI_PRIME=1` seleciona `Mesa/NVE6`, correspondente à Quadro K2100M
com `nouveau`. O backend SDL offscreen continua escolhendo a Intel e não deve ser
usado para validar seleção de GPU ou desempenho físico.

A primeira passagem explícita na Quadro ficou entre 10,96 e 12,00 FPS, com custo
GPU quase plano de 81,99–89,77 ms entre `POTATO` e `ULTRA`. Ao terminar, o pacote
da CPU estava em 85 °C, acima do limite `high` de 84 °C, e o `nouveau` não expôs
temperatura ou clock da GPU. O resultado confirma a seleção `NVE6`, mas não é
baseline válido para reduzir qualidade: driver, reclocking e condição térmica
precisam ser resolvidos ou isolados primeiro.

Na Fase 134, o hitscan passou a classificar o contato de Enemy como `Standard`
ou `WeakPoint`. O volume fraco é deliberadamente menor que o collider geral:
cabeça nos quatro papéis orgânicos e sensor superior metálico no
IndustrialRobot. A distância e o ponto do impacto usam a superfície desse
volume, continuam competindo normalmente com paredes e outros alvos e aplicam
`1,75x` de dano somente após um disparo aceito. O HUD traduz weak hit e weak
kill em um marcador dourado com detalhe interno; torso, Props, luminárias,
munição, cooldown, áudio, progressão e RNG preservam os contratos anteriores.
Como isso afeta estratégia e tempo, Campaign/Speedrun avançam para regras 21/20.

Na Fase 135, cada arma recebe um perfil de spread derivado somente do arquétipo.
Uma tabela fixa de doze amostras move o ray lógico em dois eixos após o primeiro
tiro da rajada: a Pistol limita o envelope a `0,55°`, enquanto a Carbine chega a
`0,90°`. A janela de recuperação é avançada no fixed update e zerada por tempo,
recarga, equip ou restore; cooldown e dry fire não avançam a sequência. O HUD
abre a retícula de forma proporcional para tornar a perda de precisão legível.
Nenhum stream aleatório, seed estrutural ou formato de save foi alterado. Como
o balanceamento e os tempos mudam, Campaign/Speedrun avançam para regras 22/21.

Na Fase 136, o input distingue a borda de clique do estado segurado. Qualquer
arma aceita a borda; somente a Carbine transforma o estado segurado em novos
pedidos de disparo. As tentativas passam pelo mesmo `Weapon::tryFire()`, então
cooldown, consumo de munição, spread, Overdrive, ruído, feedback e dano seguem
um único pipeline no fixed update. Menus, pausa, término de sessão e soltura do
botão desarmam o hold. Quando o pente zera, o hold deixa de pedir disparos para
não repetir áudio de arma vazia; outro clique ainda gera um dry fire legítimo.
Campaign/Speedrun avançam para regras 23/22.

Na Fase 137, `Weapon::reload()` deixa de transferir munição instantaneamente e
passa a iniciar uma ação temporizada: `0,82 s` para a Pistol e `1,05 s` para a
Carbine. Durante esse intervalo, pente e reserva não mudam, novos pedidos de
recarga são rejeitados e nenhum disparo, impacto, ruído ou dry fire é produzido.
O fixed update conclui a transferência de uma única vez e o progresso entregue
ao viewmodel vem diretamente desse estado, eliminando o antigo timer cosmético
duplicado na `Application`. Pausa congela a ação; equip, restore e nova run a
cancelam. O estado transitório não entra no checkpoint, enquanto o novo tempo de
combate avança Campaign/Speedrun para regras 24/23.

Na Fase 138, o pickup da Carbine deixa de substituir a Pistol: ele cria um
loadout de dois slots, equipa a arma nova e conserva munição da sidearm. `1`
seleciona Pistol, `2` seleciona Carbine e qualquer passo da roda alterna entre
as duas; pedidos entram no fixed update e só funcionam para armas possuídas.
Trocar cancela cooldown, spread e recarga do slot que sai, preserva Overdrive e
desarma o gatilho segurado para evitar um tiro involuntário. Cada slot mantém
pente e reserva próprios; Ammo abastece somente a arma ativa, melhorias de dano
e capacidade alcançam as duas e uma Carbine duplicada permanece no chão sem
reabastecer. Campaign/Speedrun avançam para regras 25/24. O checkpoint schema 3
grava arma ativa e guardada; schemas anteriores não são reinterpretados.

Na Fase 139, equipar ou selecionar uma arma inicia sua entrada autoritativa:
`0,34 s` para a Pistol e `0,48 s` para a Carbine. Enquanto esse timer avança no
fixed update, `tryFire()` retorna `Switching`, recarga e outra troca são
rejeitadas e nenhum cartucho, hitscan, ruído, recuo, flash ou dry fire é criado.
Isso impede reiniciar cooldown pela alternância rápida de slots. A pausa congela
a ação e o viewmodel recebe diretamente a força restante do mesmo estado, sem
um segundo timer de troca na `Application`. A entrada inicial da run continua
somente cosmética. O timer não entra no checkpoint; restore devolve o loadout
em repouso. Campaign/Speedrun avançam para regras 26/25, sem mudar o schema 3.

Na Fase 140, os atalhos passam a seguir a prioridade do loadout final: `1`
seleciona a Carbine e `2` a Pistol. A roda continua alternando. A entrada fica
mais ágil, com `0,32 s` para a Carbine e `0,24 s` para a Pistol. Uma seleção
oposta durante o movimento troca o slot imediatamente e reinicia a entrada da
nova arma, em vez de descartar o comando até a animação anterior acabar.
Disparo e recarga permanecem bloqueados até o último movimento concluir, então
rolar repetidamente não permite burlar cadência; apenas acompanha cada escolha.
Campaign/Speedrun avançam para regras 27/26, mantendo checkpoint schema 3.

Na Fase 141, a Knife entra como fallback permanente do loadout no slot `3` e
na sequência da roda. Ela não consome nem exibe munição: cada clique inicia um
golpe semiautomático de 48 de dano, alcance de `1,65 m` e intervalo de `0,52 s`.
O ataque reutiliza a seleção autoritativa do hitscan, limitada pelo alcance
curto, mas possui arco de viewmodel, áudio e ruído de apenas `4 m`, sem recuo ou
flash de disparo. O modelo procedural usa lâmina metálica independente, sem
criar ou aprovar uma nova textura. A seleção da faca é transitória; checkpoints
continuam no schema 3 e preservam apenas os dois slots de fogo e suas munições,
restaurando a arma de fogo ativa. Campaign/Speedrun avançam para regras 28/27.

Na Fase 142, segurar o comando de agachar (atualmente `Ctrl`) interpola a
câmera de `1,62 m` para `1,02 m`, reduz o
collider de `1,80 m` para `1,15 m` e limita a velocidade a 58% da caminhada.
Soltar a tecla restaura a postura somente quando o volume em pé está livre,
evitando atravessar tetos ou props baixos. `Espaço` aplica impulso vertical de
`6,2 m/s` apenas no chão; a gravidade permanece no fixed update, colisões
interrompem a cabeça em tetos e permitem aterrissar sobre superfícies sólidas.
A trajetória real move a câmera, enquanto o viewmodel recebe respostas sutis
de postura e estado aéreo sem deslocar a mira lógica. Campaign/Speedrun avançam
para regras 29/28, mantendo o checkpoint schema 3 porque postura, velocidade e
posição continuam transitórias.

Na Fase 143, a variante `Used` da proposta
[`Weapon/first-person presentation v1`](visual/proposals/weapon_first_person_proposal_v1.md)
aprovada pelo usuário substitui as peças simples do rifle, da pistola e da
faca. As malhas são agrupadas por material para limitar draws; superfícies
principais usam mapas procedurais de `512×512` com albedo, normal e RMA, e
detalhes menores usam `128×128`. A iluminação de preenchimento é restrita ao
viewmodel. Lógica de tiro, recarga, seleção, munição, dano e checkpoints não
mudaram. As [capturas in-engine](visual/validation/weapon_first_person_v1_used_comparison.png)
e o perfil POTATO estão registrados para aceitação visual; a prancha conceitual
não é uma captura da build nem inclui aprovação de mãos/braços novos.
Após o primeiro feedback, a inclinação das empunhaduras da pistola e do rifle
foi corrigida para que suas bases recuem em direção à coronha. O enquadramento
dos três modelos ficou menor e mais periférico; a comparação
[antes/depois](visual/validation/weapon_first_person_v1_used_pose_correction_comparison.png)
registra a correção em 1280×720.

Na Fase 144, `combatSignalsForShot()` transforma `ShotResult` em um lote de até
três sinais ordenados. Flash/recuo, ruído, impacto, hitmarker e áudio consomem
esse lote sem alterar munição, dano ou RNG estrutural. É uma separação local de
apresentação por disparo, não uma fila global nem save de eventos. O checkpoint
continua registrando a entrada do andar, não um estado intermediário.

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
game loop e timers de HUD não avançam. Somente o bus UI continua para responder
à navegação, sem avançar efeitos ou ambiência. O mouse relativo é liberado na
abertura e restaurado no retorno; o renderer oculta mira e feedback transitório
sob o overlay central.

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

Em combate entre salas, uma porta aberta agora expõe os inimigos à visão do
Player e vice-versa: eles podem atacar e perseguir pelos corredores, mas não
abrem portas fechadas. A porta ocupada permanece transitável até o inimigo
liberar a passagem; o collider maior do boss também cabe nas juntas entre sala
e corredor. Matar o boss pela passagem aberta encerra a run imediatamente.
Como esse combate altera tempos comparáveis, recordes de Speedrun usam regras
`32`; checkpoints de Campaign permanecem na versão `31`.

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
responsivo com `START RUN`, `CONTINUE`, `CAMPAIGN`, `SPEEDRUN`, `BEST SEED`,
`SETTINGS` e `EXIT GAME`;
`W/S` ou setas mudam a
seleção com wrap, `Enter` confirma, o mouse seleciona/clica e `Esc` sai. A dungeon, 
suas entidades, eventos, iluminação e materiais específicos só são preparados ao
confirmar `START RUN`.

O modo `--headless` ignora apenas essa espera interativa e inicia a run
automaticamente para preservar os testes determinísticos. A regressão OpenGL
verifica o overlay e suas sete seleções; outra integração encerra ainda no menu
e falha se encontrar o log de geração do mundo. O restart da pausa também marca
a run atual como encerrada antes de repetir a seed, evitando a rejeição de um
reinício solicitado durante `Playing`.

## Gate obrigatório de aprovação visual

Os materiais implementados nas Fases 9 e 10 são baseline técnico funcional e
continuam disponíveis durante o desenvolvimento, mas ainda não são arte final
aprovada. O protocolo e o inventário ficam em
[`visual/TEXTURE_APPROVALS.md`](visual/TEXTURE_APPROVALS.md).

A opção [HUD v2 A Precision](visual/proposals/hud_visual_proposal_v2.md) e a
[arma caída v2](visual/proposals/ground_weapon_pickup_proposal_v2.md) foram
aprovadas para implementação e já estão na build. As
[capturas reais do HUD](visual/validation/hud_visual_v2_a_implemented_1280x720_neutral.png)
e [das armas no piso](visual/validation/ground_weapon_pickup_v2_gameplay.png)
documentam o estado atual; a aceitação visual em partida ainda está pendente.

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
Combat, HUD Feedback, Menus, Persistência, Entities e World.

Validação instrumentada de memória e comportamento indefinido:

```bash
cmake --preset sanitizers
cmake --build --preset sanitizers --parallel
ctest --preset sanitizers
```

Esse preset ativa AddressSanitizer e UndefinedBehaviorSanitizer em compiladores
GCC/Clang. A detecção de leaks de bibliotecas gráficas externas fica desativada;
acessos inválidos e comportamento indefinido encerram o teste imediatamente.

Os mesmos três presets são executados automaticamente por
`.github/workflows/ci.yml` em pushes, pull requests e disparos manuais. A CI não
aceita nem regrava hashes ou imagens golden; qualquer divergência interrompe o
job para investigação.

Executar normalmente:

```bash
build/debug/gdp
```

Depois de atualizar o repositório, recompile o preset do executável que você
usa (`cmake --build --preset debug --target gdp` ou
`cmake --build --preset release --target gdp`). Um binário anterior em `build/`
não recebe os controles novos apenas com `git pull`.

No Windows 11 x64, use o pacote `GDP-Windows-x64` produzido pela CI ou siga o
guia de compilação em [`README_WINDOWS.md`](README_WINDOWS.md). O ZIP contém
`gdp.exe`, `SDL3.dll` e o próprio guia, sem exigir CMake ou Visual Studio no
computador que apenas executará o jogo.

Cada nova abertura gráfica sem argumentos escolhe uma seed inicial diferente.
O valor usado aparece no log e pode ser reproduzido com `--seed`.

O mapa mantém oito salas e a rota garantida START→BOSS, mas o caminho principal
agora muda de direção e faz uma ou duas curvas em posições diferentes, enquanto
Hub, Arena e branches variam
de ordem e ligação. A sala Secret — e sua Carbine garantida — nem sempre fica
perto do início. Salas comuns têm um ou dois inimigos, a Arena
começa com dois a quatro adversários (mais sua onda de evento), e o boss recebe
360–420 HP conforme a seed. Campanha e Speedrun usam versões de regras novas
para não misturar checkpoints ou recordes de layouts anteriores.

O canto superior esquerdo da HUD agora traz um minimapa: ele acompanha sua
posição e direção, guarda salas visitadas e mostra apenas as saídas já
descobertas, seguindo a geometria real dos corredores. Salas usam suas dimensões
reais no mapa, e tiros disparados de um corredor podem acertar alvos na sala
vizinha quando a porta estiver aberta. Reiniciar a run ou avançar de andar
limpa essa exploração.
Recordes Speedrun anteriores a essa regra de combate são mantidos no arquivo,
mas não são comparados com os novos tempos.

Executar sem janela por uma quantidade determinada de frames:

```bash
build/debug/gdp --headless --frames 60
```

Executar uma seed específica:

```bash
build/debug/gdp --seed 12345
```

Medir um perfil gráfico isolado com o protocolo reproduzível:

```bash
build/release/gdp --benchmark POTATO --seed 12345
```

Medir `POTATO`, `LOW`, `MEDIUM`, `HIGH` e `ULTRA` em sequência:

```bash
cmake --preset release
cmake --build --preset release --target graphics-benchmark
```

Para uma comparação de desempenho mais resistente a picos, executar três
matrizes e usar as linhas finais `BENCHMARK_MEDIAN`:

```bash
cmake --build --preset release --target graphics-benchmark-stable
```

Em Linux híbrido, conferir primeiro o renderer padrão e o discreto:

```bash
glxinfo -B
DRI_PRIME=1 glxinfo -B
```

Para medir explicitamente a GPU discreta, mantendo a identidade registrada em
cada linha:

```bash
DRI_PRIME=1 cmake --build --preset release --target graphics-benchmark-stable
```

Esse comando escolhe o dispositivo; não garante que o driver aberto `nouveau`
consiga usar clocks altos. A comparação deve considerar as medianas e os campos
`gpu_vendor/gpu_renderer`, não apenas o nome físico da placa.

Durante essa execução, entradas de gameplay são descartadas; o evento de fechar
a janela ainda é respeitado. Para validar apenas a automação em ambiente sem
display, sem tratar os números como resultado de hardware:

```bash
cmake -DGDP_EXECUTABLE=build/release/gdp -DGDP_OFFSCREEN=ON \
  -P tools/run_graphics_benchmark.cmake
```

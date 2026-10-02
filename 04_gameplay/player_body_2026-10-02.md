# Player Usado — corpo inferior v1

Data: 2026-10-02. Estado: **IMPLEMENTAÇÃO PARCIAL EM VALIDAÇÃO**.

Primeira entrega do [player aprovado](../visual/proposals/player_body_proposal_v1.md):
quadril, calça industrial grafite, reforços discretos nos joelhos e botas.
Braços, luvas, dedos e mãos executando as recargas continuam aprovados,
mas **não estão implementados nesta entrega**. Não há cabeça nem terceira pessoa.
Tecido, couro e borracha são foscos e não metálicos; apenas Usado foi aplicado.

## Contrato e ownership

- `PlayerBodyAssets` pertence ao GDP. Malhas, materiais, movimento visual e
  pose não introduzem conceitos de personagem na WKC Forge.
- `Application` publica `PlayerBodyRenderData` a partir dos pés, yaw,
  crouchAmount e histórico visual do movimento pós-colisão. Pitch não faz
  parte da pose: olhar para baixo não gira as pernas junto com a câmera.
- A fase avança pela distância XZ realmente percorrida (ciclo de 3,20 m).
  Movimento contra parede não avança passos. Avanço/recuo/strafe seguem
  direção local ao yaw e convergem suavemente ao mudar de direção.
- No ar, passos cessam e as pernas recolhem; ao pousar, a flexão converge
  para a postura em solo. Agachamento usa o blend autoritativo existente.
  As duas pernas usam IK de dois segmentos com comprimentos preservados,
  dobrando os joelhos para frente. Não são blocos rígidos soltos.
- Pausa congela a simulação/pose. Spawn, retorno ao menu e shutdown limpam
  o histórico; deltas grandes de relocação também o descartam. Valores
  não finitos no DTO produzem pose segura, sem contaminar matrizes.
- Câmera, colisões, bhop, movimento físico, origem do tiro, munição, IA,
  geração do mundo, dificuldade e saves **não foram alterados**.

## Renderer e orçamento

O corpo usa a projeção do mundo e seu depth buffer, antes do passe próximo
da arma limpar profundidade. Objetos/paredes o ocultam normalmente. O rig
acompanha os pés e o yaw, com a cintura naturalmente atrás da posição dos
olhos, sem mudar a câmera lógica. Não desenha tampa artificial de pescoço.
A arma mantém a projeção própria de 68 graus e seu enquadramento existente.
Menus iniciais/resultados não desenham o corpo; pausa conserva o fundo parado.

São sete matrizes de skinning, quatro materiais/draw calls e **2096 triângulos**.
Buffers de malha e texturas são enviados uma vez na inicialização, não
reconstruídos a cada frame. A GPU deforma as superfícies com dois pesos por
vértice; o GDP reutiliza os atributos existentes para sua codificação, sem
alterar o formato de Vertex/Mesh da engine. Somente suas malhas ativam esse
modo no shader; todos os outros desenhos voltam ao modo normal.

Mapas: tecido 512², reforços/couro 256², borracha 128²; albedo/normal/propriedades
determinísticos. POTATO conserva a silhueta/articulação, usando o shader e
escala de mundo existentes. Não adiciona luzes, sombras ou uma segunda cena.
O custo geométrico foi medido pela fixture; **não há promessa de FPS ou de
custo zero**, nem benchmark comparativo representativo de gameplay nesta entrega.

## Evidências nativas

Capturas da fixture OpenGL offscreen, não playtest humano. BMP → PNG/GIF,
sem retoque. Ambiente e armas existentes; iluminação neutra para inspeção.

- [Em pé, MEDIUM/FOV 60](../visual/validation/player_body_v1_standing.png).
- [Caminhando, 960×600](../visual/validation/player_body_v1_walking.png).
- [Ciclo de passada](../visual/validation/player_body_v1_gait.gif).
- [Agachado](../visual/validation/player_body_v1_crouched.png).
- [No ar](../visual/validation/player_body_v1_airborne.png).
- [Com carabina/FOV 100](../visual/validation/player_body_v1_armed_fov100.png).
- [POTATO](../visual/validation/player_body_v1_potato.png).

Capturas iniciais revelaram que uma tampa de cintura escondia as pernas;
ela foi removida e a posição do quadril corrigida antes destas evidências.
Capturas validam enquadramento, não aprovação artística final automática.

## Validação

CPU: malhas/índices/pesos válidos, orçamento, determinismo, matrizes finitas,
comprimentos dos segmentos em caminhada/recuo/agachamento/ar, joelhos para
frente, chão, materiais não metálicos e relógio inválido/relocação.

OpenGL: diferença de pixels em pé/caminhando/agachado/no ar, reprodução
determinística, recuo/strafe, ausência na linha do horizonte, oclusão por
caixa sólida, arma/mira e exclusão no menu. MEDIUM/POTATO, FOV 60/100,
16:9, 16:10, janela compacta e ultrawide; capturas 1280×720/960×600/320×200.
`ApplicationMenuTestAccess` verifica andamento pós-colisão, pausa e reset
na integração real. Resultados finais dos comandos estão abaixo.

- Build Release e sanitizers: concluídas, sem novos avisos.
- `ctest --preset release --output-on-failure`: **64/64**, 90,96 s na revisão final.
- `ctest --preset sanitizers -R '^(application_menu_integration|renderer_cpu_systems|renderer_opengl_pixels|weapon_reload_opengl|renderer_instancing_opengl|architecture_boundaries|gameplay_systems|combat_systems|room_traversal_systems|campaign_checkpoint_persistence_systems|loot_systems)$' --output-on-failure`:
  **11/11**, 322,15 s (integração da aplicação 248,88 s), sem diagnósticos
  ASan/UBSan. Preset mantém `detect_leaks=0` conforme restrição de ptrace.
- Fixture isolada: passou na NVIDIA Quadro K2100M, driver 470.256.02,
  e no Mesa llvmpipe (LLVM 21.1.8, Mesa 26.2.3).
- `git diff --check`: sem erros de whitespace.

Alterações paralelas de treinamento presentes no checkout foram preservadas
e não entram nesta publicação. Os testes locais acima foram executados nesse
checkout; nada foi treinado/promovido/substituído por esta entrega visual.

## Ajuste de portabilidade encontrado

O CI do commit anterior (`9b0eae3`, run `37071469683`) passou Linux
Debug/Release/ASan+UBSan, mas Windows falhou em
`campaign_checkpoint_persistence_systems`: `schema six legacy fixture saves`.
O teste mantinha streams de leitura abertos ao substituir o mesmo arquivo.
Agora a leitura fica em helper com lifetime limitado antes de salvar; falha
de save é reportada sem tentar substituir texto inexistente. Não muda formato
ou comportamento de produção. A confirmação Windows depende do próximo CI.

## Teste manual

```bash
cmake --build --preset release --parallel 2
build/release/gdp --test-mode
```

Olhe para baixo e caminhe com WASD (ou bindings remapeados), para trás e
de lado. Pare contra uma caixa: não deve continuar marchando. Teste Ctrl,
Space, aterrissagem, FOV 60/100 e POTATO/MEDIUM. Pause andando e retome;
reinicie a arena. Olhando para frente, o corpo não deve ocupar a mira.
Confira também a sniper ao segurar Mouse2. Validação humana de conforto e
naturalidade permanece pendente; mãos nas recargas serão a entrega seguinte.

Fixture isolada: `SDL_VIDEODRIVER=offscreen build/release/gdp_renderer_gl_tests --player-body-only`.
Capturas: definir `GDP_PLAYER_BODY_CAPTURE_DIR` para uma pasta local em `build/`.

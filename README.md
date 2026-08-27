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
- **Fase 11 — Elenco:** em andamento; Guard e Officer procedurais, combate de
  média distância e comando tático implementados e validados.
- **Próxima atualização:** expandir a Fase 11 para o inimigo Engineer.

O bootstrap atual contém CMake, SDL3, janela, contexto OpenGL 3.3 core, game loop
com fixed update de 60 Hz, snapshots de input, logging e modo headless para
testes. O núcleo inclui streams aleatórios determinísticos e independentes por
domínio, geração versionada de seeds e tipos matemáticos básicos. Quando SDL3
não está instalado, o build baixa a versão 3.4.14 oficial com hash SHA-256
fixado.

O renderer atual desenha uma sala industrial fixa com cubo indexado reutilizado,
materiais procedurais de parede/piso/teto derivados da seed real da START,
iluminação pontual simples, luminárias emissivas, câmera em perspectiva e
retícula. Parede com grandes placas e rodapé amarelo/preto, piso de placas com
faixa de tráfego e teto com luzes lineares usam receitas e cache separados. A
cena é produzida em framebuffer próprio e apresentada na janela. O jogador
controla essa câmera com `W/A/S/D` e mouse, colide com
paredes e props, interage com o terminal usando `E`, dispara com o botão esquerdo
e recarrega com `R`. HP e munição aparecem no título da janela; `Esc` encerra a
aplicação enquanto o sistema de menus ainda não existe.

Na inicialização, uma seed gera um mundo lógico validado com START, BOSS,
branches, loop controlado, salas e corredores. Cada sala recebe IDs de entidades
determinísticos: jogador, terminal e um Worker ativo na START, além de inimigos,
itens e props conforme o `RoomRole`. Cada inimigo recebe uma `visualSeed`
isolada da estrutura do mapa. O Worker visível é montado a partir de
quatro `BodyProfile` e uma biblioteca compatível de 18 peças Box/Cylinder, com
assimetria, materiais e desgaste próprios; sua mesh e seu albedo são preparados
e cacheados antes do draw. Ele persegue o jogador, ataca em alcance e pode ser
morto pela pistola hitscan. O renderer ainda apresenta somente a sala START
fixa.

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

O mesmo pipeline já oferece o tema Storage: metal amarronzado envelhecido,
marcação logística curta, concreto empoeirado com marcas de arrasto e teto com
iluminação mais fraca/irregular. Combat Room acrescenta placas blindadas verde-
oliva com 1–3 impactos, piso escuro demarcado e teto reforçado com luzes
espaçadas. Cafeteria fornece painéis claros com faixa institucional vermelha,
piso lavável marcado por circulação/mobiliário e teto regular de luz uniforme.
Laboratory acrescenta painéis frios precisos, módulo científico com indicadores,
piso técnico azul e contaminação localizada, além de teto branco modular. O
perfil Technical Area usa tubos volumétricos, caixa/válvula conectadas, piso
pesado com grade e óleo e teto mecânico categorizado. Security acrescenta
console monitorado, alerta vermelho confinado, piso técnico e teto claro
regular. Central Hub fecha o conjunto com grandes superfícies neutras,
sinalização ciano única, rotas contínuas no piso e luminárias alinhadas. O
renderer cacheia os oito perfis pelo hash completo; como a cena navegável ainda
é apenas a START industrial, os demais ambientes são exercitados pelo teste
OpenGL offscreen.

## Compilar e testar

Requisitos locais: CMake 3.24+, Ninja, compilador C++20 e OpenGL.

```bash
cmake --preset debug
cmake --build --preset debug --parallel
ctest --preset debug
```

Os mesmos comandos aceitam o preset `release`; ambos compilam e executam a suíte
completa de Core, Renderer, Procedural Assets, Enemy Generation, Room Materials,
Gameplay, Combat, Entities e World.

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

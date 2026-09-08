# Renderer — Pipeline de renderização

> **Estado:** renderer mínimo da Fase 2 implementado e validado; variantes de
> Worker e materiais industriais preparados/cacheados antes do draw nas Fases 9
> e 10; iluminação procedural integrada na Fase 12; transform da sala ativa e
> portas, shells adjacentes e corredores conectados integrados na Fase 13;
> materiais/decor multissala e overlay bitmap integrados na Fase 14. Filas completas,
> culling, transparência, HUD final e pós-processamento
> permanecem planejados.
> A Fase 30 separou os targets de fundação gráfica, assets procedurais, conteúdo
> visual GDP e renderer concreto sem alterar a imagem produzida.
> A Fase 31 alinhou a estrutura física, movendo o conteúdo visual concreto para
> `src/game/visual` e reservando `src/procedural` à fundação reutilizável.
> A Fase 32 moveu o renderer concreto para `game/visual/GdpRenderer`; a pasta
> `src/render` ficou restrita às primitivas gráficas reutilizáveis.
> A Fase 34 extraiu seus DTOs para `game/visual/GdpRenderData`, formando um
> contrato de apresentação independente da classe concreta.
> A Fase 35 moveu esse contrato para `gdp::game::visual`; `gdp::render` deixa
> de ser proprietário de tipos específicos de HUD, entidades e salas do GDP.
> A Fase 36 moveu a própria classe `GdpRenderer` para `gdp::game::visual`,
> mantendo as primitivas reutilizáveis qualificadas em `gdp::render`.
> A Fase 37 renomeou o target concreto para `gdp_visual_renderer`, distinguindo
> a composição GDP de `gdp_render_foundation`.
> A Fase 38 renomeou a fundação para `wkc_forge_render`; o namespace
> `gdp::render` permanece temporariamente compatível.
> A Fase 48 concluiu essa migração: a API da fundação passou para
> `wkc::render`, sem aliases reversos ou mudança de pixels.
> A Fase 49 moveu geradores de inimigos e materiais de sala para
> `gdp::game::visual`, deixando `gdp::procedural` restrito à fundação genérica.
> A Fase 50 concluiu a fronteira: receitas, hashing e geração genérica passaram
> para `wkc::procedural`, sem aliases ou mudança de saída.
> A Fase 56 auditou a direção final de dependências: a fundação gráfica WKC não
> inclui camadas GDP, enquanto o renderer e o conteúdo concretos continuam no
> jogo. A separação permanece no monorepo e é protegida por regressão própria.
> A Fase 94 passou a enviar cada primitiva dos inimigos como parte rígida
> independente e a compor poses locais nos pivôs, sem promover animação ao WKC.
> A Fase 95 adicionou perfis locais distintos por papel visual, ainda dentro do
> renderer concreto do GDP.
> A Fase 96 adicionou blends de pose por fixed tick e queda rígida de Death;
> somente a matriz `rotationZMatrix` foi acrescentada à matemática WKC.
> A Fase 97 reutilizou essa fundação para um ragdoll procedural inteiramente
> concreto do GDP, sem promover juntas ou simulação física à engine.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `architecture.md — Renderer/OpenGL`
- `README(6).md — passe visual`

## Objetivo

Renderizar o estado do jogo sem possuir regras de gameplay.

## Entrada

```cpp
struct RenderFrame {
    CameraData camera;
    std::span<const RenderInstance> opaque;
    std::span<const RenderInstance> transparent;
    std::span<const LightData> lights;
    UiRenderData ui;
};
```

## Pipeline

```text
GAME STATE
→ visibility/culling
→ render queue
→ opaque world
→ opaque entities
→ transparent/emissive
→ post-process opcional
→ viewmodel
→ HUD/UI
→ present
```

## Implementação da Fase 2

`GdpRenderer` recebe dimensões do alvo e `CameraData` pronto do gameplay e mantém
shader, box GPU, texturas procedurais e framebuffer. O fluxo atual é:

```text
room transforms + corridor paths + per-room material keys + lighting plan
→ current/adjacent room shells + corridor segments + doors/fixtures/decor
→ enemy variants → pickup markers → props
→ framebuffer color+depth → blit → objective/result overlay → SDL window
```

O resize recria os anexos quando necessário. O depth test usa `GL_LESS`, erros
OpenGL encerram o frame com log e recursos GPU são destruídos antes do contexto.
Camera, Framebuffer, GlApi, Mesh, Shader e Texture são compilados em
`wkc_forge_render`. Receitas e geradores reutilizáveis ficam em
`wkc_forge_procedural`, enquanto `wkc_forge_resources` publica o `AssetCache`;
definições Worker, materiais de sala e assets concretos do GDP ficam em
`gdp_visual_content`; `gdp_visual_renderer` realiza a composição final.
Desde a Fase 46, o cache é exposto como `wkc::resources::AssetCache`.
Desde a Fase 48, as primitivas gráficas são expostas em `wkc::render`.
Sua implementação está em `game/visual/GdpRenderer.*`; o nome de classe
passou a `gdp::render::GdpRenderer` na Fase 33. Na Fase 34, os DTOs de
apresentação passaram ao contrato `game/visual/GdpRenderData.*`; seus nomes e
representação permanecem compatíveis. Na Fase 35, o namespace do contrato
passou a `gdp::game::visual`, enquanto as primitivas reutilizáveis continuam em
`wkc::render` desde a Fase 48. Na Fase 36, a classe concreta também passou a
`gdp::game::visual::GdpRenderer`, consumindo a fundação gráfica por nomes
qualificados.
`prepareIndustrialRoomMaterials()`, `prepareStorageRoomMaterials()` e
`prepareCombatRoomMaterials()`, `prepareCafeteriaRoomMaterials()` e
`prepareLaboratoryRoomMaterials()`, `prepareTechnicalRoomMaterials()` e
`prepareSecurityRoomMaterials()` e `prepareCentralHubRoomMaterials()` geram uma
chave pelo hash integral do perfil e selecionam a variante. `render()` apenas
escolhe os canais Wall/Floor/Ceiling já enviados à GPU. A START usa Industrial;
o teste offscreen alterna pelos outros sete ambientes, confirma mudança de
pixels entre os oito perfis e repetição exata da mesma variante Industrial.
`prepareWorkerVisual()`, `prepareGuardVisual()`, `prepareOfficerVisual()`,
`prepareEngineerVisual()` e `prepareIndustrialRobotVisual()` usam mapas GPU separados por papel;
`EnemyRenderData::role` seleciona mesh/textura sem geração durante draw. Desde a
Fase 94, cada primitiva é cacheada e enviada como mesh rígida independente. O
draw compõe a pose da raiz com transform local ao redor do pivô de cada parte;
estado, timing e amplitudes permanecem no renderer concreto do GDP. Na Fase 95,
o papel visual seleciona a assinatura de movimento sem consultar IA. O teste
offscreen posiciona os cinco no mesmo ponto e exige pixels distintos. Na Fase
96, o snapshot também informa estado anterior e fração de blend; raiz e partes
são interpoladas antes do draw. Death mantém escala unitária, perde emissão
progressivamente e usa uma rotação lateral em vez de deformar a malha. Na Fase
97, cada parte recebe rotação também no eixo Z e uma trajetória procedural
própria com atraso e amortecimento. A raiz compensa aproximadamente o contato
com o piso; colisões detalhadas de cadáver não fazem parte do renderer.
`PickupRenderData` desenha Health, Ammo e Key com forma/tint/emissão próprios;
a regressão também exige contraste separado e composição com inimigos.
`PropRenderData` desenha crate, canister e barricade com escala/tint derivados
do papel e da `visualSeed`; `Damaged` escurece e `Destroyed` achata o volume. A
regressão offscreen cobre os três papéis, os três estados e a composição final.

`RoomLightingRenderData` fornece ambiente e até quatro pontos com posição, cor,
intensidade e alcance. O shader acumula Lambert difuso com atenuação por alcance
e nunca consulta o gerador. As luminárias são volumes emissivos nas posições do
plano. O flag `alert` mistura ambiente, pontos e fixtures com vermelho enquanto
o evento mantém a sala em lockdown; a regressão offscreen exige saídas distintas
para iluminação padrão, procedural e alerta.

`RoomRenderData` aplica origem, quarter-turn e uma chave de material à shell da
sala ocupada e das salas diretamente adjacentes. Todos os perfis do mundo são
preparados antes do primeiro frame; por isso a sala destino mantém shell,
decoração, inimigos, pickups e props próprios antes da troca de `roomId`, sem
ativar sua IA antecipadamente. As paredes são divididas ao redor da abertura de
cada connector. `DoorRenderData` usa `openFraction` para deslizar dois painéis
lateralmente, moldura ciano no fluxo normal e vermelho durante lockdown.
`CorridorRenderData` referencia os pontos do mundo e
desenha cada trecho ortogonal com piso, teto e laterais. A regressão offscreen
coloca uma sala fora da origem, gira 90°, diferencia porta aberta, em movimento
e lockdown, e
exige mudança de pixels com a câmera dentro de um corredor conectado e também
ao trocar somente a chave de material da sala adjacente.

Após o blit, `HudRenderData` seleciona overlays bitmap independentes de FOV:
objetivo no topo e menu central para Victory/Defeat. O estado terminal oferece
jogar novamente, retornar ao título ou sair, compartilhando `ResultMenuLayout`
entre desenho e hit test. A regressão OpenGL exige alteração de pixels para o
estado terminal e ao trocar sua seleção.

## Separação

O renderer recebe `MeshHandle`, `MaterialHandle` e transforms. Ele nunca consulta `EnemyGenerator`, `WorldGenerator` ou RNG.

## Depth e transparência

Mundo/entidades opacas usam depth test. Transparência é exceção e fila separada. UI é passe separado.

## Culling

Primeira versão: frustum culling por AABB. Occlusion culling é opcional futuro.

## Viewmodel

Pode possuir FOV/projeção própria, mas hitscan usa a câmera lógica de gameplay.

## Retro visual

Pode usar resolução interna reduzida, quantização, dithering e gradação, desde que não prejudique leitura.

## Debug

Wireframe, normals, AABB, connector volumes, spawn zones, material IDs, semantic regions e light bounds.

## Falha

Shader/material ausente usa material de erro visível e gera log. Não falhar silenciosamente.

## Testes

Resize, fullscreen/windowed, mesh vazia, material inválido, zero luzes, alto número de objetos e HUD independente do FOV 3D.

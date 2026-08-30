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

`Renderer` recebe dimensões do alvo e `CameraData` pronto do gameplay e mantém
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
`gdp_render_foundation`. Receitas e geradores reutilizáveis ficam em
`gdp_procedural_assets`; definições Worker, materiais de sala e assets concretos
do GDP ficam em `gdp_visual_content`; `gdp_renderer` realiza a composição final.
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
`EnemyRenderData::role` seleciona mesh/textura sem geração durante draw. O teste
offscreen posiciona os cinco no mesmo ponto e exige pixels distintos.
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

Após o blit, `HudRenderData` seleciona um overlay bitmap provisório independente
de FOV: objetivo no topo e banner central para Victory/Defeat. A regressão
OpenGL exige alteração de pixels para o estado terminal.

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

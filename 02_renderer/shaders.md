# Renderer — Shaders

> **Estado:** shader básico equivalente ao `world_lit` implementado na Fase 2;
> demais programas e hot reload permanecem planejados.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `architecture.md — OpenGL/Renderer`

## Conjunto inicial

```text
world_lit
entity_lit
unlit
emissive
ui
debug
```

`world_lit` e `entity_lit` podem compartilhar implementação no início.

## Vertex shader

Entradas: position, normal, uv. Uniforms: model, view, projection. Saídas: world position, normal e uv.

A implementação atual usa GLSL 330 core, layouts fixos `0/1/2` para essas
entradas e uniforms centralizados com prefixo `u`.

## Fragment shader

Primeira versão suporta albedo, roughness aproximada, metallic aproximado, normal e emissive. PBR completo não é requisito do vertical slice.

Na Fase 2, o fragment shader cobre albedo, tint, ambiente, luz pontual difusa e
atenuação. Roughness, metallic, normal map e emissive entram nas próximas
iterações de materiais.

## Variantes

Evitar explosão de defines. Preferir poucos programas estáveis.

## Bindings

Uniform names, texture units e layouts ficam centralizados.

## Hot reload

Opcional. Se recompilação falhar, manter shader anterior e logar erro completo.

## Debug

OpenGL debug callback habilitado em Debug.

## Testes

Compile/link, uniform ausente, material fallback, normal mapping off e emissive.

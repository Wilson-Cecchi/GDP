# Environment — Combat Room / Arena

> **Estado:** materiais de parede, piso e teto implementados e validados na
> Fase 10; geometria modular, coberturas e navegação entre salas continuam em
> expansão.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Função

Encontro controlado.

## Identidade

- espaço aberto
- cobertura planejada
- linhas de visão
- iluminação contrastada

## Gameplay

Combate; spawn seguro; rota de movimento.

## Geração

Geometria principal vem do `RoomModule`. Props, materiais, desgaste, luzes e estados variam
com sub-seeds independentes e respeitam zonas reservadas/connectors.

## Implementação atual

`generateCombatRoomMaterials(roomSeed)` escolhe deterministicamente `Ready`,
`Active` ou `Scarred` e deriva três receitas independentes:

- **Wall:** PaintedSteel verde-oliva, placas grandes 1–4 × 2, moldura reforçada,
  sinalização amarela curta e 1–3 impactos localizados conforme o desgaste;
- **Floor:** Ceramic representando concreto/metal escuro em quatro grandes
  lajes, perímetro de arena amarelo dessaturado e marcas de passagem esparsas;
- **Ceiling:** BareSteel escuro, vigas espessas e luminárias espaçadas que
  preservam a leitura de navegação.

Condição, desgaste e impactos variam por seed sem transformar a arena em espaço
abandonado. IDs locais 200–203 do domínio `RoomMaterial` isolam o perfil dos
outros ambientes. Cada receita e o conjunto completo possuem hashes canônicos,
usados pelo cache do renderer antes do draw. A API `prepareCombatRoomMaterials`
deixa o perfil pronto para a futura seleção da sala ativa; enquanto a navegação
renderizada permanece limitada à START industrial, a variante é exercitada
pela regressão OpenGL offscreen.

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate gera e rasteriza as três superfícies para 1.000 room seeds, cobre as três
condições, exige hashes únicos, 1–3 núcleos de impacto, perímetro contínuo no
piso e contraste mínimo entre vigas e luminárias. A regressão OpenGL confirma
que o perfil altera o framebuffer em relação aos demais ambientes implementados.

# Paredes atuais — referência para novas texturas

> Estado do jogo em 27/10/2026. Este inventário descreve o que o código usa
> hoje; não fixa as cores nem os desenhos de uma futura revisão visual.
> A parede Industrial Corridor v2 HD foi aceita em jogo. As outras sete
> famílias são perfis técnicos implementados, ainda sem aprovação de arte final
> no [registro visual](../visual/TEXTURE_APPROVALS.md).

## Onde cada parede aparece

O jogo prepara e guarda um conjunto de materiais por sala, usando a seed da
sala. O papel da sala tem prioridade sobre o tema: **Hub** usa Central Hub;
**Arena** e **Boss** usam Combat Room. Nas demais salas, o tema seleciona
Storage, Laboratory, Maintenance/Technical, Security, Cafeteria ou Industrial.
O tema **Office** também usa Industrial; ainda não possui uma parede própria.
Portas são aberturas na geometria da parede e têm material separado.

Essa seleção está em [`Application.cpp`](../src/game/Application.cpp) na função
`prepareRoomMaterials`. O renderer usa a chave de material da sala ao desenhar
paredes e corredores em [`GdpRenderer.cpp`](../src/game/visual/GdpRenderer.cpp).
Por isso, as frases antigas que limitavam estes perfis à sala inicial foram
removidas dos documentos individuais.

## Inventário visual

| Ambiente | Parede que existe hoje | Variação por seed | Situação visual |
| --- | --- | --- | --- |
| [Industrial Corridor](industrial_corridor.md) | Aço pintado cinza frio, painéis grandes 2×2 com molduras chanfradas, juntas escuras recuadas, fixadores, nervuras discretas, riscos e desgaste concentrado nas bordas e na base. Faixa âmbar/preta somente no rodapé. | `Maintained`, `Used`, `Worn`: da tinta mais íntegra ao desgaste mais forte, sem cobrir toda a parede de ferrugem. | **v2 HD aceita em jogo**; a sala inicial e salas Industrial/Office usam o perfil. |
| [Storage](storage.md) | Aço pintado amarronzado, 2–3 × 2 placas grandes, uma possível placa substituída, oxidação localizada nas juntas e na base, riscos baixos e marcação logística curta. | `Orderly`, `Dusty`, `Neglected`. | Perfil técnico em uso; arte final não revisada. |
| [Combat Room](combat_room.md) | Aço pintado verde oliva, 1–4 × 2 placas grandes, molduras reforçadas, sinalização amarela curta e 1–3 impactos localizados. Também é a parede das salas Boss. | `Ready`, `Active`, `Scarred`. | Perfil técnico em uso; arte final não revisada. |
| [Cafeteria](cafeteria.md) | Painéis cerâmicos claros em grade de 2–4 × 2, uma faixa vermelha contínua e controlada, com desgaste discreto de uso. | `Clean`, `InUse`, `Faded`. | Perfil técnico em uso; arte final não revisada. |
| [Laboratory](laboratory.md) | Painéis cerâmicos branco frio em grade de 2–6 × 2, alinhados com precisão; um módulo científico azul escuro com dois indicadores verdes. | `Sterile`, `Operational`, `Contained`. | Perfil técnico em uso; arte final não revisada. |
| [Technical / Maintenance](technical.md) | Aço pintado com base 2×2 mais simples, 1–2 tubos verticais com volume e sombra, caixa técnica e válvula ligadas aos tubos. | `Serviced`, `Working`, `Overloaded`; sujeira e óleo crescem sem esconder a base. | Perfil técnico em uso; arte final não revisada. |
| [Security](security.md) | Aço pintado em 2–6 × 2 módulos organizados, com um console de tela abstrata, quatro indicadores agrupados e cabo saindo de baixo. O vermelho fica dentro do console no estado de alerta. | `Secured`, `Monitored`, `Alert`. | Perfil técnico em uso; arte final não revisada. |
| [Central Hub](central_hub.md) | Aço pintado com 1–3 divisões amplas, uma coluna arquitetônica e uma única placa ciano compacta. Sem faixa amarelo/preto decorativa. | `Open`, `Active`, `Rerouted`, com eixo de circulação horizontal, vertical ou cruzado. | Perfil técnico em uso; arte final não revisada. |

O material Industrial v2 HD usa mapas compartilhados de **albedo, normal e
propriedades (rugosidade, metalicidade e oclusão ambiente)**, cada um com
1024×1024 px, repetidos conforme o tamanho da parede. As outras sete paredes
ainda são geradas como **albedo procedural de 64×64 px**. Essa diferença é do
renderizador atual; uma proposta nova pode ser HD mesmo para uma dessas sete
salas, mas sua integração exigirá adaptar o material no jogo.

## Como criar um exemplo para me enviar

Você pode começar por **uma parede de um ambiente**, sem refazer o conjunto
todo. Há duas formas úteis de referência:

1. **Imagem de conceito:** mostre a parede de frente e, se quiser, um recorte
   próximo e uma cena em perspectiva. Ela comunica formas, cor e desgaste;
   não precisa ser repetível nem ter mapas técnicos.
2. **Textura candidata:** envie o albedo visto de frente, sem perspectiva,
   com as bordas preparadas para repetição horizontal. PNG 1024×1024 é um bom
   ponto de partida. Se tiver normal e mapas de rugosidade/metalicidade,
   envie-os separadamente; não são obrigatórios para discutir a aparência.

Para qualquer formato, identifique **ambiente**, **variante ou condição** e o
que deseja **preservar ou mudar** em relação à parede atual. Uma única versão
já é suficiente para discutir a direção visual. Se gerar estados limpo, usado
e gasto, mantenha a mesma construção e varie principalmente o desgaste.

Ao montar uma textura candidata, deixe juntas, painéis e fixadores legíveis na
distância de jogo; reserve os detalhes pequenos para a vista próxima. Evite
sombras de uma luz específica pintadas no albedo, letras indispensáveis para a
leitura e detalhes importantes atravessando as bordas de repetição. O ambiente
precisa manter portas, inimigos e itens fáceis de localizar. A paleta atual é
referência, não obrigação para a nova proposta.

As imagens aprovadas da parede industrial estão listadas no
[registro visual](../visual/TEXTURE_APPROVALS.md), e o contrato detalhado da
[v2 HD](../visual/proposals/industrial_corridor_wall_proposal_v2_hd.md) serve
de exemplo técnico. O documento
[room_materials_reference.md](room_materials_reference.md) conserva ideias
históricas de composição; use este inventário para o estado do jogo hoje.

### Texto base opcional para um gerador de imagens

```text
Crie uma referência frontal para a parede de [AMBIENTE] de um FPS industrial.
Material principal: [MATERIAL]. Estrutura: [PAINÉIS, JUNTAS E PEÇAS].
Detalhe funcional que identifica a sala: [DETALHE]. Estado de conservação:
[LIMPO, EM USO OU GASTO]. Mostre as formas grandes com clareza à distância
e os detalhes de superfície apenas de perto. Preserve áreas neutras para
destacar portas e inimigos. Sem personagem, arma, HUD ou iluminação dramática
pintada na superfície. [SE FOR TEXTURA: vista ortogonal, bordas repetíveis.]
```

Substitua os campos com a linha do ambiente na tabela. A primeira imagem pode
ser só uma proposta de aparência; não precisa acertar os mapas técnicos agora.

# Renderer — Materiais

> **Estado:** albedo texturizado e tint existem desde a Fase 2; famílias, slots,
> propriedades-base e cache CPU foram implementados na Fase 8; primeiro conjunto
> de materiais Industrial Corridor, Storage, Combat Room, Cafeteria, Laboratory,
> Technical Area, Security e Central Hub implementados na Fase 10.
> `MaterialInstance` completo e mapas PBR permanecem planejados.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `architecture.md — MaterialDefinition/MaterialFamily`

## Material × textura

Material define resposta à luz; textura fornece mapas espaciais.

```cpp
struct MaterialInstance {
    MaterialFamily family;
    TextureSet textures;
    Vec3 tint;
    float roughnessScale;
    float metallicScale;
    float emissiveScale;
};
```

## Famílias iniciais

`PaintedSteel`, `BareSteel`, `Aluminum`, `Fabric`, `Rubber`, `Polymer`, `Polycarbonate`, `Ceramic`, `OrganicSkin`, `Bone`, `CorruptedOrganic`.

`materialProperties()` fornece roughness, metallic e permissões de oxidação e
emissivo para todas as famílias. Isso já impede, por exemplo, ferrugem em Fabric
e habilita emissão apenas onde a família admite.

## Regras

- tecido, pele e borracha não metálicos;
- metal exposto pode ser metálico;
- tinta sobre metal reduz leitura metálica;
- graxa/óleo reduzem roughness;
- poeira aumenta roughness.

## Slots

`Primary`, `Secondary`, `Armor`, `Fabric`, `Rubber`, `Lens`, `Organic`, `Emissive`.

## Cache/lifetime

Instâncias idênticas compartilham recursos CPU imutáveis pelo hash canônico.
`AssetCache` contabiliza hits, misses e entradas; o renderer gera em cache e faz
upload durante `initialize()`, nunca durante `draw()`.

Na Fase 10, `IndustrialRoomMaterialSet` agrupa três receitas versionadas — Wall,
Floor e Ceiling — derivadas por `RoomMaterial` da seed da sala. Cada canal possui
família, paleta, grade estrutural, desgaste, sujeira e regras funcionais; o teto
também fornece intensidade emissiva. O hash do conjunto inclui integralmente as
três receitas.

`StorageRoomMaterialSet` reutiliza os canais Wall/Floor/Ceiling, mas possui
condições, regras, paleta e prefixos de hash próprios. PaintedSteel envelhecido,
Ceramic como concreto e BareSteel de teto não compartilham regras funcionais
indevidas; por exemplo, o concreto rejeita oxidação e somente o teto admite
emissão. O renderer indexa variantes pelo hash completo do perfil, evitando
colisão entre temas que usam a mesma room seed.

`CombatRoomMaterialSet` mantém prefixos de hash e substreams próprios, com
PaintedSteel blindado na parede, Ceramic no piso demarcado e BareSteel emissivo
no teto. Suas regras rejeitam parede sem impactos/reforço, piso com reforço de
parede e teto sem emissão. Industrial, Storage e Combat compartilham apenas os
três canais e o cache de GPU; identidade, validação e hash permanecem separados.

`CafeteriaRoomMaterialSet` usa Ceramic para parede/piso laváveis e Aluminum no
teto claro. Regras próprias obrigam faixa vermelha de 8–12% somente na parede,
grade quadrada 2×2/4×4 e contatos de mobiliário somente no piso e emissão
somente no teto. O quarto prefixo de hash mantém receitas idênticas em seed
isoladas dos demais perfis.

`LaboratoryRoomMaterialSet` usa Ceramic frio na parede, Polymer modular no piso
e Aluminum no teto. Contaminação é permitida apenas no piso, enquanto coverage
e emissão pertencem exclusivamente ao teto; o módulo científico da parede usa
um budget de detalhe separado. Prefixos e substreams 400–403 isolam o quinto
perfil sem alterar a seed estrutural do World.

`TechnicalRoomMaterialSet` usa PaintedSteel na parede e BareSteel no piso/teto.
Pipe é proibido no piso, oil é exclusivo do piso e emission é exclusiva do
teto; mechanical controla apenas detalhes conectados ou emoldurados. O sexto
prefixo canônico impede colisão com os demais perfis mesmo usando a mesma seed.

`SecurityRoomMaterialSet` separa monitor/status/alert na parede, access no piso
e status/emission no teto. A validação rejeita vermelho operacional fora do
console, detalhes de acesso em superfícies erradas e teto sem emissão. O sétimo
prefixo e os substreams 600–603 completam a isolação atual.

`CentralHubRoomMaterialSet` compartilha um `CentralHubRouteAxis` entre as três
superfícies. Structure/focal pertencem à parede, navigation ao piso e emissão ao
teto; o ciano pode orientar, mas acentos quentes são rejeitados. O oitavo
prefixo e os substreams 700–703 encerram a isolação da Fase 10.

## Fallback

Slot inválido usa material magenta/xadrez de debug.

## Testes

Material incompleto, textura ausente, múltiplos slots, cache, emissivo zero e família inválida.

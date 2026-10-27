# Environment — Security

> **Estado:** materiais de parede, piso e teto implementados e validados na
> Fase 10; consoles 3D, portas e navegação entre salas continuam em expansão.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Função

Controle e restrição.

## Identidade

- monitores pontuais
- consoles
- vermelho de alerta
- portas reforçadas
- barricadas

## Gameplay

Guard/officer; terminais; objetivos.

## Geração

Geometria principal vem do `RoomModule`. Props, materiais, desgaste, luzes e estados variam
com sub-seeds independentes e respeitam zonas reservadas/connectors.

## Implementação atual

`generateSecurityRoomMaterials(roomSeed)` seleciona `Secured`, `Monitored` ou
`Alert`. A condição usa o ID local 600; Wall/Floor/Ceiling usam 601–603:

- **Wall:** PaintedSteel organizado em 2–6 × 2 módulos, um único console com
  tela abstrata, quatro indicadores agrupados e cabo ligado à parte inferior;
- **Floor:** BareSteel técnico 2×2 ou 4×4, juntas limpas, travas, uma tampa de
  acesso central e um risco semicircular associado à posição de cadeira;
- **Ceiling:** Aluminum modular com luminárias brancas regulares ocupando
  efetivamente 20–35% da textura e um sensor escuro fora da área luminosa.

Vermelho aparece somente dentro do console na condição `Alert`; o gate verifica
essa posição pixel a pixel. Receitas e conjunto possuem hashes canônicos e o
renderer prepara/cacheia o perfil via `prepareSecurityRoomMaterials`. Salas
com tema Security usam este perfil no jogo; a regressão OpenGL offscreen
também verifica a saída visual.

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate rasteriza 1.000 room seeds, cobre os três estados, exige hashes únicos,
quatro indicadores agrupados, alerta confinado, tampa única e coverage luminoso
de 20–35%. A regressão OpenGL confirma uma saída visual distinta para o perfil.

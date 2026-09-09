# Renderer — Iluminação

> **Estado:** luz pontual mínima implementada na Fase 2; planos procedurais por
> sala, alerta e budgets integrados na Fase 12; mistura ambiente entre salas
> conectadas integrada na Fase 14. A Fase 113 vinculou cada ponto a uma
> luminária acertável com estados persistentes de dano. Flicker temporal
> permanece planejado.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `architecture.md — Lighting`
- `Guia_de_texturas(1).md`

## Tipos mínimos

```cpp
enum class LightType { Point, Spot };
struct Light { LightType type; Vec3 position,direction,color; float intensity,radius; };
```

Interiores não exigem luz direcional global no primeiro vertical slice.

A sala fixa usa uma luz pontual com cor, posição, difusa e atenuação quadrática
aproximada, além de ambiente mínimo para legibilidade. Desde a Fase 10, o teto
industrial também contém luminárias lineares com contribuição emissiva por
máscara de luminância; elas ainda não criam luzes pontuais adicionais.
Storage usa menor intensidade emissiva e decide falha visual por fixture usando
a seed da receita, sem RNG consumido por frame e sem reduzir a luz ambiente a
um nível que impeça navegação.
Combat Room usa emissão mais forte e espaçada entre vigas pesadas; a máscara
continua apenas visual, mas seu contraste mínimo é validado para manter a arena
navegável em todas as condições de desgaste.
Cafeteria usa luminárias largas, claras e regulares em todos os módulos do teto.
Seu budget emissivo é uniforme e mantém o
contraste civil/humano sem criar luzes pontuais adicionais.
Laboratory ilumina deterministicamente 20–40% dos módulos de teto com branco
frio intenso; os demais recebem filtros/ventilação simples. O gate verifica a
ocupação real, não apenas o parâmetro da receita.
Technical Area garante uma luminária protegida em um painel de teto escolhido
pela seed; os demais painéis distribuem mecânica sem consumir RNG por frame.
Security usa iluminação branca regular ocupando 20–35% da textura e posiciona
um sensor fora das luminárias. O alerta vermelho não modifica a luz global.
Central Hub usa 135–144 pixels de luminárias lineares e alinha sua direção ao
mesmo eixo de circulação do piso, sem adicionar sinalização de risco ao teto.

Durante a passagem física por um `CorridorPath`, a posição projetada sobre seus
segmentos produz peso bidirecional com `smoothstep`. Cor/intensidade ambiente e
emissão de fixture interpolam entre origem e destino antes da troca de sala;
os pontos continuam em coordenadas de mundo e o perfil ativo muda na entrada.

## Room Lighting Profile

Define quantidade, zonas, cor/temperatura, intensidade, chance de falha e luzes funcionais.

## Stream

`LightingSeed` é independente da estrutura e do loot.

## Estados

`ON`, `OFF`, `FLICKER`, `DAMAGED`, `EMERGENCY`.

Flicker usa tempo para animação; frequência/fase podem vir da seed sem consumir RNG por frame.

Desde a Fase 113, `Intact`, `Damaged` e `Destroyed` já são estados funcionais.
Cada ponto da sala cria um `CeilingFixture` de 24 HP no mesmo transform. O alvo
não é sólido para movimento, mas o hitscan usa seu AABB metálico. `Damaged`
mantém 45% da intensidade e da emissão; `Destroyed` zera ambas e preserva uma
carcaça escura. Não há loot nem reparo por Engineer. A iluminação ambiente não
é removida, mantendo portas, objetivos e rotas legíveis mesmo após destruir
todas as luminárias.

## Legibilidade

Seed nunca pode tornar objetivo/saída obrigatória ilegível. Iluminação pode criar tensão, não softlock visual.

## Budget

Limitar luzes por sala e raio de influência.

## Emissive

Material emissivo não implica automaticamente uma luz real.

## Testes

Sala sem luz, normal, emergência, mesma seed, máximo de luzes e culling.

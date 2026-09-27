# Gameplay — Player

> **Estado:** estado lógico, spawn, vida e interação inicial implementados na
> Fase 3; binding geracional e sincronização com Entity Manager validados na
> Fase 6; chaves, coleta automática e upgrade de HP validados na Fase 12.
> Victory/Defeat e restart estão integrados na Fase 13. Armor funcional foi
> integrada na Fase 58.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Responsabilidade

`Player` contém o estado lógico do jogador. Câmera, HUD e viewmodel apenas representam esse estado.

```cpp
struct Player {
    EntityID entity;
    Transform transform;
    float health;
    float maxHealth;
    float armor;
    float maxArmor;
    WeaponID activeWeapon;
    bool alive;
};
```

## Spawn

O spawn precisa estar sobre piso navegável, com altura livre, fora de portas/props/inimigos,
conectado ao caminho obrigatório e com orientação definida pelo `RoomModule`.

Na sala fixa atual, o spawn fica em `{0, 0, 4}`, com os pés sobre o piso e yaw
inicial de `-90°`, olhando para `-Z`. `spawn()` reinicializa posição, velocidade,
orientação, vida e estado grounded; a validação procedural do ponto passa a ser
responsabilidade do World Module System.

## Interação

Um ray curto parte da câmera. Prioridade: objetivo → porta → terminal → pickup interativo.
A interação nunca deve depender da geometria visual do viewmodel.

`findInteraction()` lança o ray a partir dos olhos, limita o alcance padrão a
`2,25 m` e retorna o alvo mais próximo. O terminal da sala possui AABB lógico
alinhado à instância visual; `E` é armazenado até o próximo fixed update para
não perder o comando em frames sem update. O retorno agora é um `EntityId`
geracional do terminal, portanto uma referência stale não seleciona outra
entidade que reutilize o mesmo slot.

O Player guarda seu próprio `EntityId`; a aplicação sincroniza posição, HP,
Armor, capacidade e chaves no registro central a cada fixed update. Pickups próximos são coletados
sem interromper movimento; Health e Ammo cheios permanecem disponíveis.

## Controles de postura

Segurar `Ctrl` esquerdo ou direito agacha; soltar ambos solicita levantar.
O estado físico continua bloqueando a postura em pé quando há teto ou prop
baixo acima do Player. `C` não é mais um atalho de agachar desde a Fase 146.
`Espaço` continua pulando apenas quando a postura e o chão permitem.
Segurar `Shift` esquerdo ou direito reduz a velocidade horizontal a 52% e
seleciona passos suaves. Passos normais atraem inimigos ativos na mesma sala
até 4 m; os suaves (inclusive agachado) alcançam apenas 0,75 m. Visão, tiro
e outras fontes de alerta não são suprimidos por `Shift`.

## Armadura

O Player nasce com `0/50` de Armor. Cada ponto absorve um ponto de dano antes
do HP; excedente atravessa no mesmo update e morte continua dependente apenas de
HP zerado. `addArmor()` respeita a capacidade, valores inválidos não alteram o
estado e Armor permanece entre travessias. Vitality acrescenta 10 à capacidade
e restaura 10, além do bônus de HP já existente.

## Morte

`health <= 0` muda `alive` apenas uma vez, bloqueia combate/movimento e gera `PlayerDied`,
que conduz ao estado `GameOver`.

Na implementação atual, dano e cura respeitam clamp e morte idempotente.
`SessionSystem` transforma a primeira morte em `Defeat`, congela todos os
updates de gameplay e abre o menu de resultado para retry determinístico ou
saída. O estado terminal não pode ser sobrescrito por eventos posteriores do
mesmo frame.

## Testes

Spawn válido; clamp de HP/armadura; absorção e overflow; morte idempotente;
interação por alcance; troca de sala;
nenhum update de Player durante menu/pausa.

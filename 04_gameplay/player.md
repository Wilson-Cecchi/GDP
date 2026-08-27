# Gameplay — Player

> **Estado:** estado lógico, spawn, vida e interação inicial implementados na
> Fase 3; binding geracional e sincronização com Entity Manager validados na
> Fase 6. Armadura, armas e eventos entram nas fases correspondentes.
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

O Player guarda seu próprio `EntityId`; a aplicação sincroniza posição e HP no
registro central a cada fixed update.

## Morte

`health <= 0` muda `alive` apenas uma vez, bloqueia combate/movimento e gera `PlayerDied`,
que conduz ao estado `GameOver`.

Na Fase 3, dano e cura já respeitam clamp, morte idempotente e bloqueio de
movimento. A emissão de eventos e transição para `GameOver` aguardam o sistema
de estados.

## Testes

Spawn válido; clamp de HP/armadura; morte idempotente; interação por alcance; troca de sala;
nenhum update de Player durante menu/pausa.

# World — Corredores procedurais

> **Estado:** roteamento estrutural Straight/L/Z implementado e validado na Fase
> 5; occupancy grid detalhado, BFS/A* e geração de mesh permanecem planejados.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `README(6).md — Corredores/invariantes`
- `architecture.md`

## Entrada/saída

Entrada: Connector A/B, occupancy grid, profile e seed. Saída: lista de células + largura/altura + edge ID.

## Estratégia inicial

```text
straight → L → Z → BFS/A* fallback
```

O router atual respeita stubs, tenta Straight, as duas variantes L e duas
variantes Z, sempre com segmentos ortogonais. Cada candidato é expandido pela
metade da largura e rejeitado se atravessar bounds de uma sala não participante.
O commit também rejeita interseções ou sobreposições entre corredores distintos.
BFS/A* ainda não é necessário no layout modular inicial e não é usado como
fallback permissivo.

Cada rota é validada antes de aceitar.

## Stub

Sair reto na normal do connector antes de permitir curva.

## Occupancy

Rejeitar célula que invade sala alheia, parede, clearance ou corredor incompatível.

## Folga

`minRoomClearance`, `minCorridorClearance`, `minUnlinkedRoomGap` são parâmetros explícitos.

O projeto antigo já validou o princípio de folga entre salas/corredores em 1.000 seeds; a nova implementação preserva a regra, não os números obrigatoriamente.

## Cruzamentos

Não permitir cruzamento arbitrário no primeiro passe. Se necessário, representar como módulo/nó explícito.

## Mesh

`cells → floor/walls/ceiling → MeshData`.

## Falha

Todas as rotas falharam → rejeitar placement/candidate. Nunca atravessar geometria como fallback.

## Testes

Straight, L, Z, obstáculos, salas próximas, connectors opostos e mapa impossível.

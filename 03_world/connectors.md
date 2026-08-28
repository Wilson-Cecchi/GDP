# World — Connectors e portas

> **Estado:** contrato, transformação, compatibilidade e lifecycle implementados
> na Fase 4; porta visível, interação, lockdown e travessia entre salas ligados
> ao gameplay na Fase 13. Animação contínua permanece planejada.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `README(6).md — Connectors/portas`
- `architecture.md`

## Tipos

```cpp
enum class ConnectorType { Door, Large, Secret, Service, Elevator };
```

Service/Elevator podem ficar desativados até existir função de gameplay.

## Contrato

```cpp
struct ConnectorDefinition {
    ConnectorID id;
    ConnectorType type;
    Vec3 position, normal;
    float width, height;
    bool required;
};
```

## Compatibilidade

Tipo, dimensões, orientação, rota e clearance precisam ser compatíveis.

A Fase 4 valida tipo e dimensões nas definições. Instâncias só fazem mating se
estiverem livres, com normais opostas e dentro do gap permitido. A resolução de
rota/corredor entre posições distintas pertence à Fase 5.

## Regra herdada

```text
Room Module → Connector → Corridor → Door
```

Não criar porta procurando gargalo depois.

## DoorState

`Closed`, `Opening`, `Open`, `Closing`, `Locked`, `Disabled`.

## Segurança

Fechamento consulta volume ocupado; uma porta não pode esmagar/teleportar jogador por bug.

Na implementação atual a porta é um portal de sala deliberado: exige proximidade
e orientação, resolve apenas uma edge com corredor válido e cria o jogador para
dentro do connector oposto. Não existe fechamento sobre o volume do jogador.
Durante `RoomEvent` em lockdown, a porta fica vermelha e a travessia é recusada.

## Used connector

A instância registra qual edge consumiu o connector. `required` não resolvido invalida o mapa; opcional pode ser selado.

`assignEdge()` impede edge zero e consumo duplicado. `sealConnector()` aceita
somente connector opcional ainda livre. `hasUnresolvedRequiredConnector()` é o
gate disponível para a validação do mundo.

## Debug/testes

Normal, largura, type, edge ID, usado/livre; testar rotação, incompatibilidade, secret, block e clearance.

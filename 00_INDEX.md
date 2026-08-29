# GDP — Índice da Technical Bible

> **Estado:** índice da especificação técnica inicial do novo GDP.
>
> **Regra de leitura:** decisões herdadas dos documentos anteriores são direção de design; contratos, algoritmos e números ainda não existentes são propostas desta especificação e precisam de validação em implementação/playtest.

## Base documental

- `GDP_arquitetura_tecnica.md`
- `README(6).md`

## Objetivo

Esta pasta transforma `architecture.md` em documentação implementável. O
documento-mestre descreve a arquitetura; os arquivos especializados definem
contratos, algoritmos, falhas e testes. `GDP_arquitetura_tecnica.md` preserva a
versão-fonte consolidada e não cria uma segunda autoridade arquitetural.

## Ordem de autoridade

```text
especificação especializada mais recente
    > architecture.md
    > guias visuais
    > README legado
```

O `README(6).md` é histórico técnico: pode preservar decisões já testadas, mas não impõe código C/SDL2 à nova versão C++/SDL3/OpenGL.

Para alterações artísticas, `visual/TEXTURE_APPROVALS.md` acrescenta um gate
humano obrigatório: a decisão explícita do usuário sobre uma proposta visual
versionada é necessária antes da implementação daquela unidade.

## Referências preservadas

- `05_enemies/part_library_reference.md`: preserva o catálogo de perfis, peças e regras visuais de `Guia_de_texturas_de_Inimigos.md`.
- `06_environments/room_materials_reference.md`: preserva os materiais e a composição dos oito ambientes de `Guia_de_texturas(1).md`.
- prancha visual: referência de estilo, não layout obrigatório.
- `visual/TEXTURE_APPROVALS.md`: protocolo e registro das aprovações visuais.

## Critério de documentação suficiente

Um subsistema só está pronto para implementação quando responde: problema, entradas/saídas, ownership, algoritmo mínimo, invariantes, falhas, determinismo, debug, testes e parâmetros de balanceamento.

## Pastas

```text
01_core          fundação, tempo, RNG, matemática e configuração
02_renderer      OpenGL, meshes, materiais, texturas, luz e shaders
03_world         grafo, módulos, connectors, corredores e validação
04_gameplay      jogador, combate, armas, dano, loot e progressão
05_enemies       geração, peças, compatibilidade, IA e animação
06_environments  regras visuais e oito famílias de ambiente
07_interface     HUD, menus, configurações e UI de debug
08_audio         sistema, variação procedural e ambience
09_persistence   saves e formato de settings
10_testing       estratégia, validação procedural e budgets
```

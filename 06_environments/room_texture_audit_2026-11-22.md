# Auditoria das texturas da campanha — 22/11/2026

Escopo: os 34 módulos da biblioteca da campanha e o material exclusivo do
corredor. Comparei a [galeria de todas as variantes](../visual/validation/campaign_rooms_full_2026-11-20/README.md)
com o roteamento atual de materiais. A primeira passagem encontrou cinco
variantes com materiais incorretos, mas deixou passar a arquitetura genérica e
o piso antigo das salas Industrial. A conferência direta da sala inicial
reproduziu a falha. A galeria de 20/11 é referência do estado anterior; as
[capturas antes/depois](../visual/validation/room_texture_audit_2026-11-22/README.md)
e a [galeria atual dos 34 módulos](../visual/validation/campaign_rooms_current_2026-11-22/README.md)
registram as mudanças.

| Família | Módulos e material esperado | Resultado |
| --- | --- | --- |
| Industrial | 101, 102, 103, 105: Industrial; 104: Central Hub | **101–103 e 105 corrigidos.** O piso procedural antigo foi substituído por placas metálicas com albedo, normal e propriedades; os quatro blocos xadrez do fallback foram removidos. Duto e armário de serviço 3D seguem a rotação da sala. O Hub mantém sua composição própria. |
| Storage | 201, 202, 203, 204: Storage com chapas, piso e teto aprovados para o Hall | **201–203 corrigidos.** A restrição por tamanho deixava essas três variantes no conjunto procedural anterior. |
| Office | 301, 302, 303: Office | Conjunto próprio de parede, piso e teto e estação 3D. As estações receberam gavetas e fixadores físicos; a aceitação visual em partida continua pendente. |
| Laboratório | 401, 402, 403, 405: Laboratório; 404: Boss | As quatro variantes comuns usam os painéis atuais; a arena do Boss usa o conjunto Boss. |
| Manutenção | 501, 502, 503, 505, 506: Técnica; 504: Boss; 507: Arena com galerias | **503 corrigida.** A arena compacta herdava o material genérico de combate em vez do acabamento técnico. A 507 mantém sua arte própria. |
| Segurança | 601, 602, 603, 605, 606: Segurança; 604: Boss | **603 corrigida.** A arena compacta herdava o material genérico de combate em vez do acabamento de Segurança. |
| Cafeteria | 701, 702, 703, 704: Cafeteria | As quatro variantes usam o conjunto atual. |
| Corredor | Corredor industrial: material exclusivo v2 | Não herda o material de salas adjacentes. |

Os cinco reparos de Storage, Manutenção e Segurança reaproveitam conjuntos já
implementados. A revisão Industrial usa a receita de placas largas já presente
no renderer, com paleta fria própria, e acrescenta mobiliário de serviço e
duto físico. O piso industrial fica em validação visual; esta auditoria não lhe
atribui uma aprovação artística nova. Os materiais do Boss e da Arena com
galerias não foram alterados.

Na primeira revisão das imagens, Office 301–303 e as arenas compactas 503/603 ainda
parecem mais vazias que as variantes grandes aprovadas. Elas têm material
temático aplicado, mas detalhamento e composição são uma avaliação visual
separada. Não foram classificadas como salas prontas por esta auditoria.

Uma revisão de acabamento posterior nesta data acrescentou volume e detalhes
a essas variantes, seguindo o
[padrão compartilhado](room_art_standard.md). As
[capturas novas](../visual/validation/room_art_consistency_2026-11-22/README.md)
documentam a mudança; a aprovação visual em partida continua pendente.

## Validação

- Build de `gdp`, `gdp_application_menu_tests`, `gdp_world_tests` e
  `gdp_renderer_gl_tests` concluída.
- Cinco testes passaram: `application_menu_integration`,
  `room_material_systems`, `room_traversal_systems`,
  `world_module_systems` e `renderer_opengl_pixels`.
- Uma captura de cada um dos **34 módulos da campanha** foi refeita em HIGH,
  1280 × 720, na NVIDIA GeForce GTX 1060, a partir de 11 sementes. Todos os
  processos de captura terminaram sem erro de OpenGL. O módulo inicial 103 foi
  comparado diretamente com a imagem anterior, que ainda mostrava o piso e os
  quatro blocos antigos. Os módulos 101, 102 e 105 foram conferidos para a
  mesma receita Industrial. As demais famílias foram revistas na galeria,
  com links individuais por módulo.

As capturas e os testes são validação técnica de um ângulo por módulo. Ainda
é preciso percorrer as salas em partida para julgar a composição de todos os
ângulos, interações, rotações e iluminação. A aceitação visual em partida
das variantes alteradas cabe ao usuário, segundo o
[critério de sala pronta](room_shape_plan.md).

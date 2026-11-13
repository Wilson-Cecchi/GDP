# Técnica — albedo dos móveis v1

Tiles de aço escuro para as bancadas e carcaças dos geradores da sala Técnica, derivados da [proposta aprovada](../../../../visual/proposals/technical_material_props_v1/README.md).

| Arquivo | Condição visual | Aplicação física |
| --- | --- | --- |
| `furniture_serviced.png` | Chapa conservada, riscos discretos | Tampos: 0,5 × 0,5 m; painéis: 1 × 1 m |
| `furniture_working.png` | Aço em uso, desgaste moderado | Mesma escala |
| `furniture_overloaded.png` | Escurecido, com mais riscos e marcas quentes | Mesma escala |

Os três PNGs são albedos RGB de 512 × 512 pixels. Use UV por metros reais para manter a densidade em faces curtas e compridas. O tile pode repetir nas duas direções; as bordas opostas têm os mesmos valores RGB. Parafusos, trilhos, grelhas, puxadores, placas de risco e luzes devem ser geometria ou material separado, de modo que não se repitam em cada trecho do móvel. As variações representam condições visuais e não introduzem mecânica de dano.

O material foi deixado sem reflexos ou sombras pintados no albedo para que a iluminação do jogo defina esses efeitos. A escolha da condição para cada móvel pertence à integração da sala.

## Parede e piso v2

| Arquivo | Superfície | Escala no renderer |
| --- | --- | --- |
| `wall_steel_albedo_v2.png` | Pintura de aço cinza quente | Detalhe repetido a cada 4 m, sobre atlas de chapas largas |
| `floor_graphite_albedo_v2.png` | Aço grafite escuro | Detalhe repetido a cada 4 m, sobre atlas de placas de 4 × 3 m |

As duas fontes foram geradas a partir da [direção aprovada](../../../../visual/proposals/technical_material_v2/README.md), sem juntas, sombras ou reflexos pintados. O renderer usa mipmaps para manter a textura estável à distância; juntas, desgaste por placa e iluminação são calculados separadamente. O piso recebeu acabamento mais fosco conforme a aprovação de 13/11/2026.

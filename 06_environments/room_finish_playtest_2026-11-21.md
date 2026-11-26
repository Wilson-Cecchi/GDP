# Playtest de campanha — salas revisadas em 21/11/2026

Esta build inclui a faixa corrigida do Industrial Hall, o Office com metal
revisto e duas mesas no Hall, armários no Laboratório Secreto, monitores
quebráveis na Segurança e plantas quebráveis nas quatro Cafeterias. Tudo está
na biblioteca normal da campanha.

```bash
cd /mnt/projetos/GDP/GDP-3
cmake --build build/technical-ceiling-build --target gdp -j 4
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/technical-ceiling-build/gdp --seed 1793
```

No menu, entre em **Campanha > Novo jogo** em um slot novo. Um checkpoint antigo
mantém o mapa da partida anterior. Saia do jogo e troque a semente para testar
as outras variantes:

| Semente | Salas para conferir no primeiro andar |
| --- | --- |
| `1793` | Industrial Hall 105, Laboratório Secreto 403, Cafeteria alongada 704, Segurança em T 606. |
| `1116` | Office Hall 303 e Segurança quadrada 605. |
| `61` | Cafeterias 701, 702 e 704. |
| `29` | Cafeteria Hall 703 e Segurança compacta 601. |
| `15` | Office compacto 301 e médio 302. |

Verifique a circulação e os tiros junto às mesas do Office e armários do
Laboratório, dispare nos monitores de Segurança e nas plantas da Cafeteria,
e olhe a parede inteira do Industrial Hall para confirmar que a faixa não
reaparece no alto. Repita em POTATO e ULTRA se quiser avaliar a leitura das
superfícies nos extremos gráficos. A [galeria técnica](../visual/validation/rooms_finish_2026-11-21/README.md)
mostra os pontos principais antes do playtest.

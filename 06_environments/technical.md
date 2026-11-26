# Environment — Technical / Maintenance

> **Estado:** variante em L pronta: formato, móveis, parede, piso e teto
> aprovados em 14–15/11/2026. Integrada à campanha como sala normal grande;
> geração, conexões e travessia passaram pela validação.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

## Função

Infraestrutura operacional.

## Identidade

- tubos funcionais
- máquinas
- cabos
- bancadas
- óleo/calor localizados

## Gameplay

Worker/engineer; sistemas.

## Geração

Geometria principal vem do `RoomModule`. Props, materiais, desgaste, luzes e estados variam
com sub-seeds independentes e respeitam zonas reservadas/connectors.

## Implementação atual

### Prévia da geometria

A prévia `Technical/Maintenance` mede 16 × 20 m e tem teto a 5,5 m. Segue a
composição de [`maintence.png`](maintence.png): bancadas de manutenção,
gabinetes e geradores ocupam as laterais; tubulações ficam acima da passagem.
Duas estações de manutenção desencontradas ficam no miolo da sala, cada uma
com base, tampo e uma caixa quebrável. Elas deixam 4,4 m livres entre os tampos e
passagens laterais para contorná-las. As quatro posições possíveis de porta e
as áreas reservadas para inimigos, objetos e loot ficam livres. As caixas têm
35 HP, recebem dano de tiros e explosões e perdem a colisão ao serem destruídas;
as bancadas permanecem no lugar. As peças sólidas compartilham a mesma
geometria para desenho e colisão.

```bash
cmake --build build/playtest --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/playtest/gdp --technical-preview --seed 20261029
```

A prévia abre sem inimigos para avaliar formato e circulação. Bancadas,
gabinetes e geradores usam os três albedos metálicos da proposta v1, com
detalhes de acabamento em geometria; as duas caixas usam as variações oliva
e carvão já existentes. As
[capturas técnicas](../visual/validation/technical_geometry_preview/README.md)
mostram FOV 60 e 100 em POTATO e ULTRA; a aprovação em partida está pendente.

### Variante em L

A nova prévia `--technical-l-preview` usa o mesmo limite de 16 × 20 m e altura
de 5,5 m, retirando o canto frontal direito. O piso, o teto, as paredes e a
colisão seguem esse recorte; bancadas que cruzariam o vazio são omitidas. A
prévia original `--technical-preview` continua disponível. Ambas começam com
Sniper e sem inimigos.

```bash
cmake --build build/playtest --target gdp
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  ./build/playtest/gdp --technical-l-preview --seed 20261029
```

As [capturas técnicas](../visual/validation/shaped_rooms_v1/README.md) cobrem
FOV 60/100 e POTATO/ULTRA. O formato em L foi aprovado em partida para
implementação. Em 14/11/2026, o usuário aprovou a variante concluída; a
campanha na semente 1793 e os testes de travessia e mundo passaram.
Na campanha, o formato em L aparece como variante de Manutenção grande, com
as quatro rotações possíveis. A sala retangular permanece na biblioteca.
Consulte a [ordem de integração](room_shape_plan.md).

`generateTechnicalRoomMaterials(roomSeed)` seleciona `Serviced`, `Working` ou
`Overloaded`. A condição usa o ID local 500; as superfícies usam 501–503:

- **Wall:** chapa pintada em cinza quente, juntas e desgaste localizado;
- **Floor:** placas grafite com juntas e desgaste moderado;
- **Ceiling:** painéis grafite neutros, sem luminária ou faixa emissiva pintada.

A parede e o piso usam atlas por sala com UV por distância real: 96 × 8 m
acompanham o perímetro das paredes e 24 × 24 m cobrem o piso. A revisão v4
desenha chapas de parede de aproximadamente 3–4 m de largura em fiadas
desencontradas, uma faixa inferior contínua e placas de piso de 4 × 3 m.
O desgaste muda por chapa e manchas de óleo ficam em pontos amplos esparsos;
não há escotilhas pintadas em padrão curto. O teto continua com tile de
2 × 2 m. Tubos, dutos, bandejas de
cabos, válvulas, grelhas e luminárias pertencem à geometria. Os tampos dos
móveis usam tile de 0,5 m; os painéis longos, 1 m.
Consulte a [proposta visual v1](../visual/proposals/technical_material_props_v1/README.md).

Wear, dirt e oil crescem com carga sem transformar todas as superfícies em
detalhe. Receitas e conjunto usam hashes próprios e substreams 500–503. O
renderer prepara/cacheia o perfil via `prepareTechnicalRoomMaterials`. Salas
com tema Maintenance usam este perfil no jogo; ele também é exercitado pela
regressão OpenGL offscreen.

### Revisão de decoração

Após o primeiro playtest, a sala recebeu painéis de serviço tridimensionais
nas paredes laterais e na parede interna do L, uma linha de cabos elevada,
tubos âmbar verticais, acabamento das bancadas, bandejas de peças e frentes
mais legíveis nos geradores e instrumentos. Os painéis ficam junto às paredes
e acima da passagem; as bancadas, geradores e caixas mantêm suas áreas de
colisão. Esta revisão foi aprovada pelo usuário em 14/11/2026. As [capturas técnicas da revisão
v2](../visual/validation/technical_decor_v2/README.md) não substituem a
aprovação em partida.

Os dois dutos de teto agora terminam nas paredes. Na sala retangular, ambos
atravessam o comprimento inteiro. Na variante em L, o duto do lado do recorte
liga a parede interna à parede externa; o outro liga as duas paredes externas.
As juntas têm colares 3D. A [validação da correção](../visual/validation/technical_duct_wall_fix/README.md)
mostra as duas formas em FOV 60 e 100.

### Variação das paredes e do piso

A revisão v3 variou chapas pequenas, mas o resultado continuou repetitivo e
foi rejeitado em partida. A [proposta v2 de direção artística](../visual/proposals/technical_material_v2/README.md)
foi aprovada em 13/11/2026; a implementação das chapas largas (receita v4)
está documentada na [validação técnica](../visual/validation/technical_material_v2/README.md).
O primeiro resultado no jogo foi rejeitado por deixar piso e paredes muito
parecidos e por divergir da proposta. A revisão atual usa fontes de albedo
separadas em `assets/visual/rooms/technical/`, amostradas por distância real
com mipmaps, piso grafite mais escuro e luz de trabalho neutra. O brilho do
piso acompanha somente luminárias ativas.
Em 13/11/2026, o usuário aprovou a revisão com a condição de deixar o piso
um pouco mais fosco. A rugosidade e o brilho direto foram ajustados; a
revisão v2 de parede e piso está aprovada. O teto mantém estado próprio.
Uma revisão do teto acrescenta travessas de aço em 3D a cada 4 m sobre os
painéis grafite existentes, com pequenos fixadores nos encontros. Na variante
em L, as travessas terminam na parede de retorno em vez de atravessar o canto
ausente. Dutos e luminárias continuam peças separadas nas posições atuais.
O usuário aprovou esta revisão do teto em 14/11/2026; ela não altera o
aceite da parede e do piso.
Em 15/11/2026, o usuário aprovou também as caixas quebráveis das bancadas
com armação, tampa, fecho e fixadores em 3D. A classificação visual é própria;
colisão e resistência continuam iguais às das caixas anteriores. As caixas
das estantes do Storage permanecem para uma revisão futura.
O mesmo ritmo de travessas e fixadores do teto foi adaptado para Storage,
Cafeteria, Segurança, Laboratório e Central Hub, com metais nas cores de cada
ambiente. As peças são recortadas pelo contorno real das salas em T e octogonal
e deixam espaço em volta das luminárias. Essas adaptações têm capturas técnicas
e foram aprovadas pelo usuário em 15/11/2026 após avaliação das salas. A
aprovação se refere às travessas e não fecha as demais pendências de cada sala.
Pranchas e capturas em `visual/proposals/` e `visual/validation/` são
artefatos locais de revisão, ignorados pelo Git; os albedos usados no jogo
ficam em `assets/visual/rooms/technical/`.
As [capturas antigas da v3](../visual/validation/technical_material_variation_v3/README.md)
permanecem apenas como comparação histórica.

## Validação visual

Mesmo com detalhes opcionais desativados, o ambiente precisa continuar reconhecível pela
composição, materiais e estrutura principal.

O gate rasteriza 1.000 room seeds, cobre as três cargas, exige hashes únicos,
tubos com três tons, óleo limitado, grade única e uma luminária protegida. A
regressão OpenGL confirma uma saída visual distinta para o perfil.

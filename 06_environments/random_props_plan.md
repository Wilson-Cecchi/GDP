# Props variáveis e drops — direção visual

## Estado desta passagem

O Central Hub usa decoração fixa e deixa de gerar a caixa ou barricada
genérica de `PropGenerator`. As outras salas ainda usam os três props
funcionais atuais (caixa com recurso, barricada de cobertura e cilindro
quebrável). Removê-los globalmente agora mudaria cobertura, destruição e
disponibilidade de recursos na campanha.

Os drops de vida, munição, chave, colete e recurso especial receberam modelos
compostos em `GdpRenderer.cpp`. O formato substitui o bloco de cor único; as
cores e marcas continuam distintas para leitura rápida. Armas caídas seguem
os modelos das armas. O usuário aprovou a aparência dos drops em 14/11/2026.

## Caixas de suprimento v1

As caixas quebráveis que liberam vida ou munição compartilham o mesmo volume
de colisão e os mesmos drops. O renderer recebe o tema da sala de cada caixa
e aplica uma carcaça de aço com acabamento próprio: cintas oliva no Storage,
tampa ventilada na cafeteria, reforço azul na segurança, indicadores de
contenção no laboratório, faixas de serviço na manutenção, trilhos laranja
no industrial e gavetas no Office. Uma marca pequena na frente identifica
vida ou munição sem trocar a cor principal da caixa.

As peças visíveis permanecem dentro da caixa de colisão existente. O estado
danificado escurece o conjunto; o estado destruído mantém o entulho no tom
do ambiente. As [capturas técnicas](../visual/validation/supply_crates_by_theme_v1/README.md)
comparam os sete temas sob a mesma câmera e luz. Falta aprovação visual em
partida, especialmente nos ambientes mais escuros.

## Caixas de suprimento v2 — relevos 3D

Os painéis frontal e laterais agora ficam recuados sob uma moldura com vigas
de canto e nervuras superiores e inferiores. A tampa tem uma placa elevada;
as laterais recebem alças com pontos de fixação, e a frente ganhou fecho e
placa de identificação com profundidade real. As cores e detalhes específicos
de cada sala continuam. Todas as peças continuam dentro do volume de colisão
da caixa, sem mudança no dano ou nos recursos liberados.

As [capturas v2](../visual/validation/supply_crates_by_theme_v2/README.md)
incluem uma vista em ângulo para conferir o volume. A validação técnica passou;
a aparência em partida ainda precisa de aprovação.

## Cilindro vermelho v1

O `Canister` de proporções normais agora tem corpo facetado vermelho com
anéis metálicos, base, alça superior e placa frontal em relevo. As cores
secundárias da placa acompanham o tema da sala. O objeto continua usando a
colisão, vida e comportamento de destruição atuais. O `Canister` também é
usado como alvo retangular na arena de testes; essa variante mantém o formato
anterior para não mudar a função da arena. As [capturas técnicas](../visual/validation/canister_v1/README.md)
mostram as vistas frontal, lateral e danificada. Falta aprovação em partida.

## Barricada e inventário dos props soltos

O bloco azul quadriculado da campanha era a `Barricade`: uma cobertura sólida
de 1,16 × 0,92 × 0,34 m. Agora ela usa duas faces de metal, moldura, nervuras,
parafusos e detalhes específicos de cada tema. O modelo acompanha a rotação
de 90 graus aplicada pelas salas e também cobre as barricadas maiores da sala
do Boss. Os blocos gigantes e alvos quadrados da arena de testes preservam a
geometria anterior. A textura quadriculada também saiu dos destroços de
caixas e cilindros e dos blocos de cobertura da arena; sua função e colisão
continuam iguais. As [capturas técnicas das sete salas](../visual/validation/barricades_by_theme_v1/README.md)
incluem uma barricada girada e os estados danificado e destruído. Falta
aprovação visual em partida.

Os três props aleatórios gerados pelo mapa são `Crate` (caixa de recurso),
`Canister` (cilindro vermelho) e `Barricade` (cobertura). Os `StorageBox`
quebráveis pertencem às estantes e já usam textura própria. Terminais, portas
e móveis de arquitetura seguem sistemas específicos; não são props soltos
do gerador aleatório.

## Densidade por sala

Após o playtest da campanha, o orçamento de props soltos passou a respeitar a
decoração fixa de cada ambiente. Salas normais, do Terminal e sem saída com
tema Storage, Cafeteria, Segurança, Laboratório, Manutenção ou Office recebem
no máximo um prop solto. As industriais admitem dois; Arena e Boss admitem dois
para preservar cobertura. O Central Hub continua sem props genéricos. A
dificuldade não ultrapassa esses limites, evitando grupos de três cilindros
vermelhos ao lado das bancadas. Caixas fixas nas estantes e bancadas continuam
com sua distribuição própria.

## Próximos props por ambiente

| Ambiente | Variações propostas | Função a preservar |
| --- | --- | --- |
| Storage | caixas empilhadas em palete, carrinho de carga | quebra e drop da caixa |
| Cafeteria | carrinho de bandejas, engradado de suprimentos | quebra e passagem livre |
| Segurança | armário baixo, painel móvel de contenção | cobertura da barricada |
| Laboratório | caixa de amostras lacrada, módulo de refrigeração | recurso e obstáculo baixo |
| Técnica/Manutenção | bobina de cabo, gabinete de energia | cobertura e cilindro perigoso |
| Salas industriais | palete metálico, suporte de tubos | cobertura e quebra |
| Office | arquivo móvel, maleta de equipamentos | recurso e cobertura leve |

Cada família deve ter no mínimo duas variações de desgaste e orientação,
escolhidas pelo seed visual. O volume de colisão deve acompanhar a geometria
visível, respeitar as zonas de spawn e manter corredores, portas, inimigos e
loot livres. A troca de modelo não deve alterar o orçamento de props nem a
chance de drop sem um playtest específico de gameplay.

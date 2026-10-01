# Interface — English e Português (BR)

## Contrato — 2026-10-01

- `Settings → Language` / `Configurações → Idioma` alterna English e Português
  (BR) por teclado ou setas clicáveis. A alteração aplica e salva imediatamente.
- O idioma inicial é inglês, inclusive na migração das configurações antigas.
  `RuntimeSettings::language` é persistido pelo schema 8 como `en` / `pt-BR`.
- A tradução pertence ao jogo (`game/Localization`), não à engine. O renderer
  recebe o idioma no snapshot; não acessa arquivos de preferências.
- Menus, configurações, controles, pausa, confirmação, resultados, recursos,
  objetivos, avisos, mapa e barra do boss são traduzidos. Speedrun, XP, FOV,
  seeds e nomes literais das teclas conservam suas identificações usuais.
- Comandos de menu usam enum/identificadores, independentemente dos rótulos.
  Binding `[SPACE]`, por exemplo, permanece literal.
  CLI e logs conservam inglês para não alterar automações; diagnósticos
  preservam siglas, unidades e identificadores técnicos.
- Idioma não altera simulação, geração, saves de campanha, placar ou cérebros.

## Fonte e dimensões

O gerador offline reaproveita DejaVu Sans Mono licenciada, já aprovada para a
interface. O atlas incorporado cobre U+0020–U+00FF: 16 colunas e 14 linhas,
960×1344 pixels, células lógicas 20×32 e baseline 25. O tamanho visual da
tipografia permanece o mesmo; o espaço adicional acomoda os acentos.
O gerador verifica que cada glifo cabe na própria célula. Não existe dependência
de fonte instalada, Python ou Pillow no jogo nem no pacote Windows.

Largura é medida por caracteres UTF-8, não por bytes. Truncamento remove um
caractere inteiro; bytes inválidos/glifos não suportados têm fallback seguro.
Renderização e cliques continuam usando a mesma geometria `ListMenuLayout`.
O título compacto, o HUD tático e o fundo animado aprovado são preservados.

## Validação

- `frontend_menu_systems`: catálogo, acentos/UTF-8 inválido, medidas, comandos
  independentes do idioma, ida/volta do seletor e variante sem Test Mode.
- `settings_persistence_systems`: round-trip PT-BR, migração v7 com remapeamento,
  idioma inválido e schema futuro.
- `application_menu_integration`: troca imediata com teclado/mouse, preferência
  restaurada na reabertura, campanha/slots e outros fluxos navegados em PT-BR.
- `renderer_opengl_pixels`: 13 páginas × 4 resoluções × 2 idiomas, HUD de recursos
  em 5 resoluções por idioma e barra do boss em 4 resoluções por idioma.
- Capturas reais em `visual/validation/interface_ptbr_*.png` registram título,
  configurações, remapeamento em tela pequena, HUD e boss. A aprovação estética
  prévia continua sendo a direção visual; esta revisão adiciona idioma e glifos.

Regressão local Release: **63/63 testes aprovados em 54,81 s** (execução serial,
SDL/OpenGL offscreen, 2026-10-01). O primeiro lote focal ASan+UBSan passou
**5/5 em 157,06 s**, incluindo navegação SDL e persistência PT-BR.
Após ampliar a matriz da barra do boss e ajustar seus rótulos, a checagem
final de catálogo, configurações, persistência e pixels ASan+UBSan passou
**4/4 em 21,07 s**, sem erros dos sanitizadores.
Um ensaio Release concorrente com a navegação ASan excedeu o timeout de pixels;
a execução final isolada passou em 3,30 s, sem ampliar o timeout Release.
Build Windows/MSVC e playtest no hardware do usuário não são validações locais.

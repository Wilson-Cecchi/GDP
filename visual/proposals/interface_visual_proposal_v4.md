# Interface v4 — menus Tático minimalista e fundo vivo

Data: 2026-10-01. Refinamento solicitado diretamente pelo usuário:
“deixa o menu no modo Tatico minimanista tambem”, com botões menores e uma
tela do jogo rodando ao fundo. Referência visual da direção já apresentada:
`interface_visual_v3.html`, variante Tático minimalista.

## Escopo

Menus branco/cinza, seleção por sublinhado, transparência para contraste e
nenhuma moldura metálica. Título à esquerda nas telas largas; submenus
centralizados. Remove o cartão lateral de descrição da v3 para revelar a cena.
Botões do título: 348×30 px em 1280×720, 1920×1080 e 2560×1080.
Todos os comandos e opções continuam presentes. Renderização e mouse usam a
mesma geometria, inclusive os ajustes para redução/aumento e high-DPI.
HUD Tático, minimapa, barra do boss e comportamento da gameplay não mudam.

## Cena de demonstração

Fundo 3D renderizado em tempo real com modelos/materiais existentes, câmera
lenta e dois inimigos em patrulha visual. Componente puramente de apresentação:
não cria entidades da partida, não consome RNG e não acessa brains/saves.
Não representa um teste de inteligência dos inimigos nem uma campanha ativa.
O título/submenus avançam um relógio separado; a pausa não simula essa cena.
Usa duas luzes sem sombras e respeita os perfis de materiais/escala interna.
Não introduz arte/texturas novas, dependências runtime, vídeo ou blur.

## Validação prevista

- Geometria e cliques de todas as páginas em 320×200 até ultrawide.
- Pixels OpenGL das 13 páginas em quatro resoluções; mudança do fundo em
  dois instantes com a seleção idêntica, fora da área ocupada pelos botões.
- Integração: animação sem criar mundo, sem avançar seed/timer da sessão,
  mouse no título compacto, configurações e retorno à pausa.
- Regressão completa Release e testes focados ASan/UBSan.

Estado: tela do título aceita visualmente pelo usuário em 2026-10-01,
com a captura nativa enviada e o comentário “eu amei essa tela”. Submenus
implementados em validação; essa resposta não aprova novas unidades visuais.

Validação técnica: Release 63/63 (44,49 s). Frontend, pixels OpenGL e
integração da aplicação também passaram com ASan/UBSan, em execuções focadas
(0,08 s; 31,33 s; 139,28 s). Os primeiros prazos de 20/120/180 s foram
insuficientes para a instrumentação neste host; os limites estendidos são
exclusivos do preset com sanitizadores (60/300 s), sem mudar os de Release.
LeakSanitizer permanece desativado pelo preset existente.
Capturas reais versionadas em `visual/validation/interface_v4_*.png`.

# Interface v3 — combinação aprovada

Data: 2026-10-01. Referência visual versionada: `interface_visual_v3.html`.
Decisão explícita do usuário: “use o Tatico minimalista para a hud do jogo e o
Industrial Refinado para o menu”.

## Menu — Industrial refinado

Grafite, ciano restrito a interação, logo claro, opções verticais compactas,
seleção com faixa à esquerda e descrição da opção nas telas largas.
Submenus preservam a mesma linguagem e todos os seus comandos atuais.
A geometria compartilhada deve proteger hover, clique, setas de ajuste e high-DPI.

## HUD — Tático minimalista

Saúde/armor/chaves à esquerda; arma/munição/nível/XP à direita.
Vida e carregador recebem números maiores; máximo e reserva são secundários.
Centro livre, sem molduras metálicas ou placas opacas nos recursos.
Minimapa, boss, objetivo, evento, interação, lanterna, feedback e diagnósticos
existentes continuam disponíveis. Fundo translúcido limitado aos blocos que
precisam de contraste; vermelho/laranja/verde preservam os estados semânticos.

## Adaptação ao renderer nativo

O desenho de ambiente na proposta é esquemático, não nova arte de cenário.
O jogo mantém a cena real, as armas, materiais e geração existentes.
DejaVu Sans Mono pré-rasterizada adapta a tipografia limpa da proposta;
atlas 960×468 incorporado, recursos GPU criados somente na inicialização.
Layouts compactos continuam com fallback bitmap. Não adiciona dependência
runtime, rasterização por frame, blur ou efeito 3D novo.
Pillow é usado somente pelo gerador opcional offline.

## Validação

- Menus: todas as páginas em 1280×720, 960×540, 640×360 e 320×200.
- HUD: recursos e estados em quatro resoluções-base e 960×600.
- Layouts: também 2560×1080, hit targets sem sobreposição e cantos separados.
- Integração: teclado/mouse, remapeamento, três saves e retorno à pausa.
- Boss: vida proporcional, fases, mapa e painel técnico sem cobrir a barra.

Estado: IMPLEMENTADA EM VALIDAÇÃO; aceitação visual final em partida pendente.

Validação técnica em 2026-10-01: Release 63/63 (22,59 s); ASan/UBSan
3/3 testes focados — frontend, integração da aplicação e pixels OpenGL
(132,13 s). LeakSanitizer permanece desativado no preset existente.
Capturas versionadas em `visual/validation/interface_v3_*.png`.

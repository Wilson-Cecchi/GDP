# Revisão técnica dos cinco modelos de referência

Data da revisão técnica: 18/10/2026. Estado em 19/10/2026: **cinco modelos
reconstruídos aceitos em partida pelo responsável pelo jogo**. A prévia
original era habilitada por `GDP_ENEMY_REFERENCE_MODELS=1`. Na integração da
candidata 0.1.7, os modelos e as hitboxes correspondentes passaram a ser
padrão; `GDP_ENEMY_REFERENCE_MODELS=0` seleciona o conjunto anterior para
comparação. As hitboxes foram alinhadas aos modelos no preview, conforme
`visual/validation/enemy_hitbox_alignment.md`.

| Papel | Submeshes | Triângulos | Principal diferença ainda visível perante a prancha |
| --- | ---: | ---: | --- |
| Worker | 29 | 50.784 | Chapa do peito, juntas e serra têm formas mais simples |
| Guard | 32 | 28.624 | Silhueta e rifle ainda têm menos camadas mecânicas |
| Officer | 32 | 33.556 | Peito, membros e equipamento traseiro são menos densos |
| Engineer | 32 | 46.888 | Ferramenta e bobina traseira estão representadas, mas com acabamento simplificado |
| IndustrialRobot | 30 | 44.056 | Canhão, garra e módulos são legíveis; massa e acabamento seguem mais simples |

Validação executada: todos os cinco `ISEMESH1` passam no parser, com partes
animadas e escala em metros; build Release e build da arena compilaram; cinco
testes CTest focados passaram; as capturas foram geradas na Quadro K2100M.
Guard e Officer produziram o mesmo SHA-256 em duas exportações consecutivas
após estabilizar os UVs dos materiais sem textura. As capturas em `*.png`
são testes OpenGL de cenário fixo, não imagens de uma partida controlada.

Após esta revisão técnica, o preview recebeu hitboxes por papel, inclusive
a cabeça fraca do Boss alinhada ao sensor alto. O usuário testou e aprovou
os visuais e a gameplay em 19/10/2026. As diferenças de acabamento listadas
acima seguem conhecidas e não impediram essa aprovação. A ativação padrão
foi integrada e o smoke OpenGL carregou os cinco modelos sem definir a
variável de ambiente; restam os testes de distribuição aplicáveis e o
empacotamento. A variação v1 de tamanho dos membros do Worker é uma
proposta anterior; este registro não afirma que ela foi reaplicada ao mesh
reconstruído.

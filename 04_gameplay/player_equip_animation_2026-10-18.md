# Player — guardar e sacar equipamentos

Data: 2026-10-18. Estado: **IMPLEMENTADO EM VALIDAÇÃO VISUAL**.

Uma troca apresenta duas fases: a arma anterior se move até seu suporte e sai
do quadro; somente então a próxima sai do suporte e chega à empunhadura.
Primárias usam a região das costas, pistola o quadril direito, faca o quadril
esquerdo, e granada a frente esquerda do cinto. O renderer seleciona a mesh
anterior na primeira fase e a nova na segunda; a transição não desenha as
duas simultaneamente. Arma, mãos e granada compartilham a mesma raiz de
movimento, preservando contatos.

O relógio de troca de `Weapon` mantém seus valores originais e seus contratos
de munição, recarga e cooldown. A apresentação tem 0,55–0,82 s conforme o
equipamento; disparo, recarga, drop e início do preparo da granada aguardam o
fim do saque. O primeiro equipamento da partida também entra pelo seu suporte.
O pavio da granada continua a iniciar após a retirada do pino,
quando o jogador aperta o disparo já com a granada na mão. Entradas de troca
sucessivas reiniciam a apresentação para a seleção mais recente.

O corpo v1 ainda não tem coldre, bainha ou suporte físico da granada. Portanto
as poses usam pontos fora do enquadramento que correspondem aos locais
solicitados; a modelagem visível desses suportes faz parte da revisão visual
pendente, sem alterar a regra de posse das armas.

Validação: testes CPU das fases e quatro pontos de saque; integração da
aplicação com troca, bloqueio de disparo, saque inicial e pino da granada;
renderer OpenGL em FOV/qualidade variados e
[capturas técnicas](../visual/validation/player_motion_game/README.md).
O playtest deve conferir sequência, conforto, rapidez percebida, troca durante
movimento, recarga interrompida da escopeta e granada antes/depois do clique.

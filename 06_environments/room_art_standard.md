# Padrão visual compartilhado das salas

Direção para a campanha: **ficção científica industrial com materiais de
aparência física**, no nível de acabamento do Central Hub, Storage Hall e
Laboratório grande. As cores identificam a função de cada sala, mas todas
devem parecer construídas no mesmo complexo. A proposta de arte pode ser mais
ou menos limpa conforme o ambiente; não deve alternar entre blocos de cores
chapadas, visual cartunesco e materiais realistas.

## Regras para qualquer variante

1. **Construção:** placas grandes com espessura, juntas recuadas, fixadores em
   encontros reais, colunas e rodapés compatíveis com o material da parede.
   Dutos, luminárias, armários e bancadas precisam de volume e pontos de
   conexão visíveis. Não usar cubos com textura xadrez como decoração final.
2. **Escala:** repetir superfícies por metros reais, segundo
   [a regra de UV](../visual/TEXTURE_SCALE.md). Uma peça longa pode receber
   mais tiles, mas não aumentar o tamanho de riscos, parafusos ou desgaste.
3. **Materiais:** metal pintado, metal exposto, cerâmica e vidro mantêm
   respostas diferentes à luz. Albedo, relevo e rugosidade devem trabalhar
   juntos quando a família os oferece. Evitar brilho de plástico, superfícies
   que pareçam emitir luz sem luminária e desgaste espalhado uniformemente.
   Não gravar uma grade de linhas genéricas em toda peça metálica: juntas,
   respiros e fixadores devem corresponder à construção e à função do objeto.
4. **Luz:** a luminária é a fonte visual da iluminação local. Manter sombras
   coerentes com as luzes intactas e quebradas. Telas e sinalizadores são
   acentos pequenos, não a iluminação geral da sala.
5. **Objetos:** o móvel precisa ter função e silhueta legíveis, acabamento
   equivalente ao ambiente, colisão alinhada e folga de portas, spawns e
   rotas. Salas de combate podem ter piso livre sem parecerem vazias: usar
   detalhes nas paredes e no teto que não fechem a circulação.

## Identidade de cada ambiente

| Família | Base material | Acento controlado | Elemento 3D característico |
| --- | --- | --- | --- |
| Industrial e corredor | aço grafite pintado e placas largas | sinalização de segurança | duto e armário de serviço |
| Storage | aço gasto de logística | marcação de carga | estantes e caixas |
| Office | aço neutro e piso escuro | ciano de equipamento | arquivos, gavetas e duto |
| Laboratório | esmalte/cerâmica e aço limpo | ciano clínico | bancadas e contenção |
| Manutenção | aço de oficina | âmbar de serviço | bancadas, cabos e dutos |
| Segurança | aço grafite azulado | azul de interface | consoles e monitores |
| Cafeteria | painéis claros laváveis | faixa vermelha | balcões, bancos e plantas |
| Central Hub | aço de circulação | linhas de rota | coluna central |
| Arena/Boss | aço reforçado | sinalização tática | colunas, galerias e coberturas |

## Auditoria de 22/11/2026

A [galeria das 34 variantes](../visual/validation/campaign_rooms_current_2026-11-22/README.md)
usa o mesmo ângulo aproximado, resolução e perfil gráfico. A comparação com
o Storage 204 mostrou que a família Industrial, especialmente a sala inicial
103, tinha poucas estruturas e equipamentos. Todas as variantes Industriais
101–103/105 receberam colunas com fixadores, vigas, módulos rasos de serviço
e indicação de rota sobre as portas. A 103 também ganhou dois armários de
chegada na parede, fora das zonas reservadas. O piso central permanece livre
para o spawn seguro.

Office 301–303, as arenas compactas 503/603 e as bancadas compactas de
Laboratório, Manutenção, Segurança e Cafeteria receberam frentes, puxadores
e fixadores físicos. Seus dutos têm o mesmo desenho construtivo e acabamento
adaptado ao tema. A
[comparação de capturas](../visual/validation/room_art_consistency_2026-11-22/README.md)
registra os exemplos; a galeria completa permite conferir o conjunto.

Esta auditoria verifica **coerência técnica e visual nas capturas**, não
aprovação final de cada sala. Antes de marcar uma variante como pronta,
compará-la em partida com as referências já aceitas, perto e longe, nos FOVs
60 e 100 e nos perfis POTATO e ULTRA. Se um tema parecer cartunesco ou plano
ao lado dos outros, revisar material, geometria e luz da variante sem copiar
a paleta de outra sala.

## Acabamento estrutural compartilhado

As colunas, carcaças e pequenos detalhes que usam o material estrutural
compartilhado receberam uma pintura de aço contínua, com variação suave e
desgaste localizado por estado. As antigas linhas de placa da textura se
repetiam em peças de dimensões diferentes e duplicavam as juntas em 3D.
O armário de serviço Industrial também trocou quatro barras paralelas por
duas portas de manutenção com fechos. A
[comparação técnica](../visual/validation/structural_steel_v2_2026-11-22/README.md)
mostra Industrial, Cafeteria, Segurança, Laboratório e Storage; o aceite
visual em partida continua pendente.

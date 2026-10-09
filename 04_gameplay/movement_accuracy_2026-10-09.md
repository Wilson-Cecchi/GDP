# Precisão em movimento — 2026-10-09

Esta fase complementa o recuo controlável sem mudar o dano, a cadência, o
inventário ou a IA. Parado no chão, o primeiro disparo continua exatamente no
centro da retícula. A velocidade horizontal acrescenta até 0,8° de imprecisão
ao correr no limite do bhop; caminhar normalmente acrescenta 0,5° e caminhar
silenciosamente, cerca de 0,26°. Estar no ar soma 0,8°, com limite total de
1,5°. Mirar reduz o adicional em 55%; agachar reduz mais 20%.

O padrão continua determinístico e usa a mesma sequência dos disparos. Um
primeiro tiro em movimento pode sair ligeiramente do centro. Não há RNG novo
nem alteração nos cérebros inimigos. A penalidade é calculada a partir da
velocidade real após a simulação de movimento, não apenas das teclas
pressionadas. A faca não recebe imprecisão. A retícula usa essa penalidade
junto da abertura já existente da rajada; a luneta da sniper mantém seu
retículo próprio, mas o tiro ainda segue a regra de precisão.

Validação automática: `ctest --test-dir build/release --output-on-failure -R
'^(combat_systems|gameplay_systems|first_gameplay_headless)$'`.
Para comparar em jogo: no Test Mode, dispare parado em um detalhe distante,
depois andando e saltando; repita mirando e caminhando silenciosamente. A
dispersão deve crescer nessa ordem, sem afetar a direção do primeiro tiro
quando estiver parado.

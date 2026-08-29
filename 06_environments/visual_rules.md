# Environments — Regras visuais globais

> **Estado:** orçamento global aplicado ao primeiro conjunto do Industrial
> Corridor, Storage, Combat Room, Cafeteria, Laboratory, Technical Area e
> Security, além do Central Hub, concluindo os oito perfis da Fase 10.
>
> Complementa `architecture.md`. Valores de balanceamento permanecem ajustáveis por playtest.

> **Gate visual:** os oito perfis atuais são baseline técnico, não arte final
> aprovada. Toda revisão segue `visual/TEXTURE_APPROVALS.md`; nenhuma receita
> revisada é implementada antes da aprovação explícita da sua prancha.

## Regra principal

A sala inteira tem prioridade sobre um tile isolado:

```text
RoomTheme → visual budget → structure → materials → lighting → details → wear
```

## Classes

`Base`, `Structure`, `Detail`, `Highlight`. A maioria da cena deve ficar em Base/Structure.
Highlights são raros e funcionais.

O primeiro perfil implementado materializa essa regra em três canais fixos:
placas estruturais dominam Wall/Floor/Ceiling; rodapé e luminárias são os únicos
highlights contínuos autorizados.
Storage mantém 65–80% de base simples: oxidação, logística, arrasto, rachadura e
falha de luz possuem limites explícitos, em vez de sorteios ilimitados por pixel.
Combat Room concentra os destaques na sinalização curta, no perímetro do piso e
nas luminárias; impactos ficam limitados a 1–3 pontos e não simulam loot.
Cafeteria mantém base mais clara e civil: a faixa vermelha é o único highlight
contínuo, manchas permanecem largas/dessaturadas e contatos escuros aparecem em
quatro grupos controlados associados a mobiliário.
Laboratory reduz ainda mais a microvariação: um único módulo científico e dois
indicadores identificam a parede, enquanto contaminação aparece somente como
uma mancha ampla/dessaturada no piso de condições `Contained`.
Technical Area é o perfil mais denso, mas preserva painéis simples entre grupos:
tubos têm volume e conexão, grade/óleo aparecem uma vez e o teto escolhe uma
categoria mecânica por painel em vez de sobrepor todas.
Security concentra informação em um único console: indicadores e vermelho não
podem escapar dele; piso e teto permanecem regulares para reforçar controle.
Central Hub preserva as maiores zonas neutras do conjunto: uma placa ciano na
parede, uma rota ou cruzamento no piso e luzes alinhadas compartilham um único
eixo. Amarelo/preto decorativo é rejeitado.

## Silêncio visual

Não sortear monitor, placa, mancha, luz e faixa independentemente em cada superfície. Primeiro
definir o orçamento total da sala; depois distribuir.

## Referências

Imagens representam composição, iluminação, material, densidade e linguagem visual; não
representam layout, textura, cor ou quantidade exatos.

Na revisão visual, a referência deve ser convertida em uma proposta identificada
por versão. Uma prancha aprovada autoriza somente a superfície ou família nela
nomeada; outras superfícies e novas versões continuam aguardando aprovação.

## Sinalização

Amarelo/preto é informação funcional, não moldura decorativa universal.

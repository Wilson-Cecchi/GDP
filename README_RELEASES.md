# Publicação do Iron Seed

O código-fonte está em `Wilson-Cecchi/GDP`. Os quatro pacotes de jogo são
publicados separadamente como assets de uma versão no repositório público
[`Wilson-Cecchi/Iron-Seed-Downloads`](https://github.com/Wilson-Cecchi/Iron-Seed-Downloads).
Para visitantes, a [página de downloads no portfólio](https://wkcdev.dev/projetos/gdp/#downloads)
reúne todas as plataformas; os botões dessa página apontam para os assets de
`releases/latest/download` com nomes estáveis. O workflow não publica a cada
commit: os runners de macOS só são usados em testes manuais ou em uma versão
aprovada. A CI regular executa os
testes Linux; o pacote Windows roda no lançamento ou quando a CI é iniciada
manualmente.

## Pacotes de melhorias por marco

Não existe calendário semanal obrigatório. Commits e pushes na `main` reúnem
trabalho em desenvolvimento, mas **não** atualizam o download público por si
só. Um pacote público é candidato quando uma melhoria importante ou um grupo
coeso de melhorias está completo, com testes, documentação e playtest visual
proporcionais ao impacto. Antes da tag, registrar o conjunto incluído,
validar os sistemas de destino e obter a decisão de lançamento. Correções
urgentes podem formar um pacote próprio; mudanças pequenas podem aguardar o
próximo marco. O `Latest` e o manifesto de atualização só avançam após a tag
de uma versão aprovada conforme o processo abaixo.

A versão 0.1.6 reúne as missões de campanha diversificadas, inclusive
a rota de chave e Arena, e o progresso detalhado no Tab. A granada ganhou
modelo consistente na mão e após o lançamento, animação de retirada do pino,
pavio de 4,87 s após essa retirada, fumaça de explosão e ajustes de desempenho.
Salas comuns e a sala Boss ficaram mais amplas e altas. O encontro final agora
inclui ajudantes, mais vida e ataques de maior alcance; salas inéditas concedem
uma breve janela de reação ao abrir a porta. O pacote Windows inicia sem janela
de terminal, enquanto o build local de diagnóstico mantém o console. Os cinco
cérebros ativos permanecem os mesmos da 0.1.5: os candidatos documentados em
[`05_enemies/ai_evaluation_2026-10-13.md`](05_enemies/ai_evaluation_2026-10-13.md)
não foram aprovados para promoção.

### Versão 0.1.7

O pacote reúne as cinco reconstruções visuais de inimigos (Worker,
Guard, Officer, Engineer e IndustrialRobot/Boss), com animações de ataque e
zonas de dano alinhadas aos modelos. O responsável pelo jogo aprovou os
visuais e a gameplay da prévia em partida em 19/10/2026. A integração ativou
esses modelos e as hitboxes correspondentes por padrão; definir
`GDP_ENEMY_REFERENCE_MODELS=0` recupera a aparência e a geometria de combate
anteriores para comparação. A aprovação refere-se às reconstruções testadas, que
ainda têm diferenças de acabamento perante as pranchas.

Somente o cérebro Worker da geração 280 substitui um cérebro ativo. Guard,
Officer, Engineer e Robot mantêm os cérebros da 0.1.6, pois seus candidatos
ainda apresentaram regressões. A comparação e a decisão de playtest estão em
[`05_enemies/ai_release_worker280_2026-10-19.md`](05_enemies/ai_release_worker280_2026-10-19.md).
O corpo/cinto v4 do Player ainda aguarda aprovação da prancha, e as animações
de guardar e sacar armas permanecem em validação visual; esta aprovação dos
inimigos não se estende a esses itens.

O Download público só muda após CI do SHA exato da `main`, builds das quatro
plataformas e conclusão do workflow de release.

## Preparação única

Configure no repositório do código-fonte o secret de Actions
`IRON_SEED_DOWNLOADS_TOKEN`. Use um token fine-grained ou um token de GitHub App
com permissão `Contents: Read and write` **apenas** no repositório público
`Iron-Seed-Downloads`. Não envie o valor do token no chat, no código ou em logs.
O `GITHUB_TOKEN` padrão do repositório do código-fonte não publica em outro
repositório.
Configure também `IRON_SEED_RELEASE_SIGNING_KEY` como secret de Actions no
repositório do código-fonte. O valor é o PEM da chave privada correspondente a
`packaging/linux/release-signing-public.pem`; **nunca** adicione a chave privada
ao Git, ao repositório público de downloads ou aos pacotes. Mantenha backup
seguro e acesso separado do token de downloads. O workflow falha antes de
publicar se o secret estiver ausente ou não corresponder à chave pública.
Antes de tornar o código público, confirme que a chave privada permanece fora
do Git e instale o secret de Actions sem imprimir seu conteúdo em logs ou chat.

## Versão aprovada

1. Aguarde a CI da branch `main` concluir com sucesso na revisão desejada.
   Teste o jogo visualmente nos sistemas necessários; o smoke test headless não
   substitui essa aprovação.
2. Crie uma tag nova no commit atual da `main`, por exemplo:

   ```bash
   git fetch origin main
   git tag public-v0.1.7 origin/main
   git push origin public-v0.1.7
   ```

3. O workflow `Portable downloads` verifica que a tag aponta para a `main`
   atual e que a CI **do mesmo SHA** passou. Compila e testa Windows primeiro;
   somente se ele passar, inicia Linux, macOS Intel e macOS Apple Silicon.
   Se qualquer pacote falhar, nada fica público. Não recicle uma tag de versão.
4. Todos os pacotes precisam existir e ser não vazios. O workflow publica uma
   release primeiro como rascunho e só então a marca como `Latest`. Uma falha
   após a criação do rascunho exige inspeção manual antes de nova tentativa.

Cada release contém `IronSeed-Windows-x64.zip`,
`IronSeed-Linux-x64.tar.gz`, `IronSeed-macOS-Intel.zip`,
`IronSeed-macOS-AppleSilicon.zip`, `update.json` e `update.json.sig`. O manifesto registra a
versão, o SHA do código-fonte, o tamanho, o SHA-256 e a URL versionada de cada
pacote. O executável responde a `--version` com a mesma versão da tag.
Os próximos quatro builds de distribuição usam `GDP_ENABLE_TEST_MODE=OFF`; a arena de
desenvolvimento permanece disponível nos builds locais de teste.

O manifesto dá suporte ao iniciador Linux incluído nos pacotes posteriores à
versão 0.1.2. Ele consulta atualizações **antes** da partida e exige confirmação
antes de instalar. Falhas de rede ou instalação mantêm a versão anterior; as
novas versões são guardadas em pastas separadas, sem tocar nos saves. As versões
0.1.2 e anteriores não recebem essa função retroativamente. O atualizador
Windows/macOS ainda será implementado e testado. Antes de distribuir macOS sem
avisos do Gatekeeper, ainda faltam assinatura Developer ID e notarização.
O iniciador Linux verifica a assinatura RSA-PSS/SHA-256 do manifesto com a
chave pública embutida no pacote **antes** de analisar a versão, baixar ou
executar o novo pacote. Se a assinatura faltar, for inválida ou `openssl` não
estiver disponível, ele não atualiza e abre a versão já instalada. O SHA-256
do arquivo continua protegendo o download contra divergências. Uma chave de
assinatura comprometida exige rotação e distribuição manual confiável; não
publique uma nova chave apenas no repositório de downloads. Pacotes já
distribuídos com atualizador sem assinatura também exigem atualização manual
para receber essa proteção.

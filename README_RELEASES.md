# Publicação do Iron Seed

O código continua no repositório privado `Wilson-Cecchi/GDP`. Os quatro pacotes
de jogo são publicados como assets de uma versão no repositório público
[`Wilson-Cecchi/Iron-Seed-Downloads`](https://github.com/Wilson-Cecchi/Iron-Seed-Downloads).
O portfólio pode continuar usando links `releases/latest/download` com nomes
estáveis. O workflow não publica a cada commit: os runners de macOS só são
usados em testes manuais ou em uma versão aprovada. A CI regular executa os
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

A progressão de encontros dos andares, o modelo próprio da SMG, o recuo
controlável e a precisão em movimento pertencem à fila de melhorias seguinte
à versão pública 0.1.4. A jogabilidade dos andares, o visual da SMG e o recuo
foram aprovados; a precisão em movimento e os novos recursos do Test Mode
precisam de playtest antes de entrar em um pacote público.

## Preparação única

Configure no repositório privado o secret de Actions
`IRON_SEED_DOWNLOADS_TOKEN`. Use um token fine-grained ou um token de GitHub App
com permissão `Contents: Read and write` **apenas** no repositório público
`Iron-Seed-Downloads`. Não envie o valor do token no chat, no código ou em logs.
O `GITHUB_TOKEN` padrão do repositório privado não publica em outro repositório.

## Versão aprovada

1. Aguarde a CI da branch `main` concluir com sucesso na revisão desejada.
   Teste o jogo visualmente nos sistemas necessários; o smoke test headless não
   substitui essa aprovação.
2. Crie uma tag nova no commit atual da `main`, por exemplo:

   ```bash
   git fetch origin main
   git tag public-v0.1.1 origin/main
   git push origin public-v0.1.1
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
`IronSeed-macOS-AppleSilicon.zip` e `update.json`. Este manifesto registra a
versão, o SHA da fonte privada, o tamanho, o SHA-256 e a URL imutável de cada
pacote. O executável responde a `--version` com a mesma versão da tag.

O manifesto dá suporte ao iniciador Linux incluído nos pacotes posteriores à
versão 0.1.2. Ele consulta atualizações **antes** da partida e exige confirmação
antes de instalar. Falhas de rede ou instalação mantêm a versão anterior; as
novas versões são guardadas em pastas separadas, sem tocar nos saves. As versões
0.1.2 e anteriores não recebem essa função retroativamente. O atualizador
Windows/macOS ainda será implementado e testado. Antes de distribuir macOS sem
avisos do Gatekeeper, ainda faltam assinatura Developer ID e notarização.
O iniciador Linux valida o SHA-256 do pacote anunciado pelo manifesto obtido
via HTTPS, mas o manifesto ainda não possui uma assinatura independente do
GitHub; não trate essa verificação como proteção contra comprometimento da
conta ou do repositório de downloads.

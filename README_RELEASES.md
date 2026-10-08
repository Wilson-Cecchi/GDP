# Publicação do Iron Seed

O código continua no repositório privado `Wilson-Cecchi/GDP`. Os quatro pacotes
de jogo são publicados como assets de uma versão no repositório público
[`Wilson-Cecchi/Iron-Seed-Downloads`](https://github.com/Wilson-Cecchi/Iron-Seed-Downloads).
O portfólio pode continuar usando links `releases/latest/download` com nomes
estáveis. O workflow não publica a cada commit: os runners de macOS só são
usados em testes manuais ou em uma versão aprovada. A CI regular executa os
testes Linux; o pacote Windows roda no lançamento ou quando a CI é iniciada
manualmente.

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
   atual e que a CI **do mesmo SHA** passou. Depois compila e testa Windows,
   Linux, macOS Intel e macOS Apple Silicon. Se qualquer pacote falhar, nada
   fica público. Não recicle uma tag de versão.
4. Todos os pacotes precisam existir e ser não vazios. O workflow publica uma
   release primeiro como rascunho e só então a marca como `Latest`. Uma falha
   após a criação do rascunho exige inspeção manual antes de nova tentativa.

Cada release contém `IronSeed-Windows-x64.zip`,
`IronSeed-Linux-x64.tar.gz`, `IronSeed-macOS-Intel.zip`,
`IronSeed-macOS-AppleSilicon.zip` e `update.json`. Este manifesto registra a
versão, o SHA da fonte privada, o tamanho, o SHA-256 e a URL imutável de cada
pacote. O executável responde a `--version` com a mesma versão da tag.

O manifesto **prepara** uma futura verificação de atualizações, mas os pacotes
atuais ainda **não se atualizam sozinhos**. Antes de instalar atualizações
automaticamente, faltam um launcher independente do processo do jogo,
verificação autenticada do manifesto, instalação atômica com rollback e testes
em cada sistema. No macOS também faltam assinatura Developer ID e notarização
para uma distribuição sem avisos do Gatekeeper.

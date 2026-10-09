# Iron Seed para Linux x64

O pacote `IronSeed-Linux-x64.tar.gz` contém o executável e os cinco cérebros
de IA utilizados pelo jogo. O SDL3 está incorporado ao executável; não é
necessário instalá-lo separadamente. A partir dos novos pacotes, `./gdp` é o
iniciador; o jogo real fica em `bin/gdp`.

Baixe a versão Linux pela
[página do Iron Seed](https://wkcdev.dev/projetos/gdp/#downloads).

```bash
tar -xzf IronSeed-Linux-x64.tar.gz
cd IronSeed-Linux-x64
./gdp
```

O iniciador consulta uma nova versão antes de abrir o jogo. Se houver uma,
mostra uma confirmação; **não baixa nem instala nada sem aceitar**. Ele instala
em `versions/`, verifica a assinatura do manifesto, tamanho, SHA-256 e
inicialização, e só então seleciona a nova versão. A anterior permanece no
disco para retorno; saves e configurações continuam fora da pasta do jogo.
Esse recurso exige Python 3.10+, o comando `openssl` e acesso HTTPS;
sem eles, o jogo abre normalmente na versão instalada. Uma execução com
argumentos, como `./gdp --headless --frames 3`, ignora a consulta de atualização.
Os pacotes anteriores a essa mudança não ganham o iniciador retroativamente.
Pacotes com o atualizador antigo, que não verifica assinatura, precisam ser
substituídos manualmente por um pacote confiável para receber esta proteção.
Se precisar voltar à versão anterior depois de uma atualização, execute
`./gdp --rollback-update` na pasta original; o comando não apaga nenhuma versão.
Ao voltar a um pacote antigo sem verificação de assinatura, o mecanismo de
atualização desse pacote continua sem a nova proteção.

O pacote é gerado em Ubuntu 22.04 x64, portanto requer uma distribuição
compatível com glibc 2.35 ou mais recente, além de driver com OpenGL 3.3 Core.
Outras distribuições e ambientes gráficos precisam de teste real; este pacote
não é AppImage nem Flatpak. Em hardware antigo, comece pelo perfil gráfico
`POTATO` nas configurações.

Para verificar a inicialização sem janela:

```bash
./gdp --headless --frames 3
```

Mantenha `bin/ai/` ao lado de `bin/gdp`. Saves e configurações são gravados no
diretório de preferências do usuário, não no diretório extraído. As licenças
redistribuídas estão em `licenses/`.

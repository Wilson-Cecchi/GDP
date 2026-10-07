# Iron Seed para Linux x64

O pacote `IronSeed-Linux-x64.tar.gz` contém o executável e os cinco cérebros
de IA utilizados pelo jogo. O SDL3 está incorporado ao executável; não é
necessário instalá-lo separadamente.

```bash
tar -xzf IronSeed-Linux-x64.tar.gz
cd IronSeed-Linux-x64
./gdp
```

O pacote é gerado em Ubuntu 22.04 x64, portanto requer uma distribuição
compatível com glibc 2.35 ou mais recente, além de driver com OpenGL 3.3 Core.
Outras distribuições e ambientes gráficos precisam de teste real; este pacote
não é AppImage nem Flatpak. Em hardware antigo, comece pelo perfil gráfico
`POTATO` nas configurações.

Para verificar a inicialização sem janela:

```bash
./gdp --headless --frames 3
```

Mantenha a pasta `ai/` ao lado de `gdp`. Saves e configurações são gravados no
diretório de preferências do usuário, não no diretório extraído. As licenças
redistribuídas estão em `licenses/`.

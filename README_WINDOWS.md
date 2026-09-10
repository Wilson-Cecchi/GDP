# GDP para Windows 11 x64

## Executar o pacote pronto

1. Baixe o artefato `GDP-Windows-x64` de uma execução bem-sucedida da CI no
   GitHub.
2. Extraia `GDP-Windows-x64.zip` por completo.
3. Execute `gdp.exe` dentro da pasta extraída. `SDL3.dll` deve permanecer ao
   lado do executável.

O computador precisa oferecer OpenGL 3.3 Core por meio do driver da placa de
vídeo. Em notebooks híbridos, configure `gdp.exe` para usar a GPU dedicada nas
Configurações de elementos gráficos do Windows caso o driver escolha a GPU
integrada.

O executável ainda não possui assinatura digital. O Windows pode identificar o
editor como desconhecido; confira se o ZIP veio do repositório oficial antes de
executá-lo. Políticas administrativas da faculdade ainda podem impedir programas
não assinados.

Configurações, recordes e checkpoints são gravados pelo SDL no diretório de
preferências do usuário, fora da pasta extraída.

## Compilar localmente

Requisitos:

- Windows 11 x64;
- Visual Studio 2022 ou Build Tools 2022 com `Desktop development with C++`;
- CMake 3.24+;
- Git e acesso à internet durante a primeira configuração, para obter SDL3.

No Developer PowerShell for Visual Studio:

```powershell
git clone https://github.com/Wilson-Cecchi/GDP.git
cd GDP
cmake --preset windows-release
cmake --build --preset windows-release --parallel
ctest --preset windows-release --output-on-failure
cpack --config build/windows-release/CPackConfig.cmake -C Release -B dist
```

O pacote final fica em `dist/GDP-Windows-x64.zip`. O build copia `SDL3.dll`
automaticamente para `build/windows-release/Release`, permitindo também iniciar
diretamente com:

```powershell
.\build\windows-release\Release\gdp.exe
```

## Diagnóstico

O modo sem janela valida inicialização, geração e gameplay sem depender do
driver OpenGL:

```powershell
.\build\windows-release\Release\gdp.exe --headless --frames 60
```

Se o modo headless funcionar e a janela não abrir, atualize o driver gráfico e
confirme suporte a OpenGL 3.3. Se `SDL3.dll` for removido ou separado de
`gdp.exe`, restaure os dois arquivos extraindo novamente o ZIP.

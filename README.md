# Iron Seed

[Português (BR)](README.md) · [English](README.en.md)

Iron Seed é um FPS procedural em desenvolvimento, criado em C++20, SDL3 e
OpenGL sobre a engine própria WKC Forge. A campanha tem dez andares, missões e
encontros variáveis; há também Speedrun e uma arena local para testar mecânicas.
O jogo oferece interface em português (BR) e inglês, suporte a teclado/mouse e
controles compatíveis com SDL3.

## Jogar

Baixe a versão de desenvolvimento pela
[página oficial do Iron Seed](https://wkcdev.dev/projetos/gdp/).
Ela indica a plataforma e permite escolher outra versão; os arquivos são
hospedados no GitHub Releases. Consulte o guia da sua plataforma antes de abrir:

| Plataforma | Guia |
| --- | --- |
| Windows x64 | [Instalar e executar](README_WINDOWS.md) |
| Linux x64 | [Instalar e executar](README_LINUX.md) |
| macOS Intel e Apple Silicon | [Instalar e executar](README_MACOS.md) |

São versões de desenvolvimento, não uma edição final. No jogo, o idioma pode
ser alterado em `Settings → Language` / `Configurações → Idioma`.

Para relatar um erro ou sugerir uma melhoria, use `Enviar feedback` no menu
inicial ou em Configurações durante uma partida. O envio acontece dentro do
jogo, após sua confirmação. São enviados apenas a categoria, a mensagem que
você escreveu, a versão do jogo e a plataforma; saves e logs não são anexados.
Como em qualquer envio pela internet, o serviço de formulário recebe seu IP e
pode registrar metadados da conexão.
É necessária conexão com a internet e o programa `curl` instalado no sistema.
Se o envio falhar, a mensagem fica na tela para tentar novamente.

## Compilar no Linux

Requisitos: CMake 3.24+, Ninja, compilador C++20 e driver com OpenGL 3.3 Core.
Na primeira configuração, o CMake pode baixar SDL3 se ela não estiver instalada.

```bash
cmake --preset release
cmake --build --preset release --target gdp
build/release/gdp
```

Para executar a suíte de testes após a compilação completa:

```bash
cmake --build --preset release --parallel
ctest --preset release
```

O build local inclui `build/release/gdp --test-mode` por padrão. Pacotes de
distribuição podem ser gerados sem essa arena. O executável ainda se chama
`gdp` por motivos técnicos.

## Encontrar a documentação

| Quero saber sobre… | Comece por… |
| --- | --- |
| Arquitetura e organização dos módulos | [`architecture.md`](architecture.md) e [índice técnico](00_INDEX.md) |
| Gameplay, campanha, armas e testes de mecânicas | [`04_gameplay/`](04_gameplay/) |
| Controles de Xbox e PlayStation | [Guia de controles](README_GAMEPAD.md) |
| Menus, HUD e idiomas | [`07_interface/menus_and_settings.md`](07_interface/menus_and_settings.md) |
| Revisão e aprovação visual de texturas | [`visual/TEXTURE_APPROVALS.md`](visual/TEXTURE_APPROVALS.md) |
| Treinamento opcional de IA no ZimaOS | [`tools/zimaos/README.md`](tools/zimaos/README.md) |
| Processo de build e publicação dos downloads | [Guia de releases](README_RELEASES.md) |
| Fases e decisões anteriores do projeto | [Histórico técnico](README_HISTORY.md) |

As pastas `01_core/` a `10_testing/` contêm especificações por subsistema.
`architecture.md` é o documento-mestre; o histórico registra etapas anteriores
e não substitui os contratos atuais.

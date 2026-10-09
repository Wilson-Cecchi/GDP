# Iron Seed para macOS

As duas versões estão na
[página do Iron Seed](https://wkcdev.dev/projetos/gdp/#downloads).

Escolha `IronSeed-macOS-Intel.zip` para Macs x86_64 ou
`IronSeed-macOS-AppleSilicon.zip` para Macs com chip Apple. Extraia o ZIP e abra
`IronSeed.app`. O jogo inclui SDL3 e os cinco cérebros de IA dentro do
aplicativo; mantenha o bundle intacto.

Esses pacotes são compilados para macOS 12 ou posterior e verificados em runners
macOS 15 da CI. O jogo precisa de OpenGL 3.3 Core. Ainda falta um playtest em
um Mac de usuário fora da CI.

O aplicativo é uma prévia assinada ad hoc, **não** assinada com Developer ID nem
notarizada pela Apple. O Gatekeeper pode impedir a primeira abertura. Confira
sempre que o ZIP veio da página oficial e siga apenas a orientação da
[Apple sobre abrir apps de desenvolvedores não identificados](https://support.apple.com/en-us/102445)
se decidir executá-lo. A distribuição sem esse alerta depende de credenciais
Developer ID e notarização em uma etapa futura.

Para diagnosticar pelo Terminal após extrair o ZIP:

```bash
./IronSeed.app/Contents/MacOS/IronSeed --headless --frames 3
```

Saves e configurações ficam no diretório de preferências do usuário. As
licenças redistribuídas estão em `IronSeed.app/Contents/Resources/licenses/`.

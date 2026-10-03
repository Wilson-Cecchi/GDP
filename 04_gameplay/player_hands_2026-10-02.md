# Player Usado — mãos e braços em primeira pessoa

Data: 2026-10-02. Estado: **HISTÓRICO V1 — SUBSTITUÍDO PELA IMPLEMENTAÇÃO V2**.
Unidade aprovada: braços/mãos/luvas da [proposta v1](../visual/proposals/player_body_proposal_v1.md).
Feedback durante implementação: "faltou as mãos"; "essas mãos estão fora de proporção".
Novo pedido em 2026-10-03: melhorar aparência e ajustar as mãos nas recargas.
[Proposta Hands + Reload contacts v2](../visual/proposals/player_hands_proposal_v2.md)
aprovada para implementação em 2026-10-03; contrato/capturas/testes atuais em
[mãos v2](player_hands_2026-10-03.md). O candidato v1 não recebeu aceitação
visual final nem publicação isolada. Evidências e custos abaixo são históricos
da v1, não os da implementação atual.

O primeiro rascunho tinha mãos pequenas, mangas finas e ombros encurtados
visíveis como extremidades soltas. O candidato revisa proporção palma/dedos,
volume das mangas, alcance e enquadramento. Testes técnicos não constituem
aceitação humana dessa correção. Torso/cinto da v2 **não implementados**.

## Contrato

- `PlayerHandsAssets` é apresentação GDP, sem novos conceitos na WKC Forge.
  Duas mãos humanas de cinco dedos; luvas escuras, tecido grafite, punhos e
  reforço dorsal discretos. Reutiliza os mapas não metálicos do player Usado,
  sem gerar novas texturas por frame nem redesenhar as armas aprovadas.
- Rig de 36 juntas: por braço, braço superior, antebraço, palma e três juntas
  em cada um dos cinco dedos. Superfícies arredondadas e pesos de duas juntas
  suavizam cotovelos/punhos/dedos; IK de dois segmentos resolve o alcance.
- Pose em coordenadas locais da arma. Mão direita conserva sua âncora de
  empunhadura; esquerda tem âncoras próprias de suporte, carregador e mecanismo
  para pistola/carabina/sniper/escopeta. SMG preserva a geometria provisória
  da carabina, mas usa seu perfil de recarga. Faca tem pegada/posição de prontidão.
- Arma, luvas e mangas recebem o mesmo root de recoil, troca, ataque, postura
  e mira nos contatos. Ombros permanecem ancorados no espaço da câmera;
  o rig resolve cotovelos sem girar o braço inteiro junto com o pente.
  Espaço próximo compartilhado usa escala/translação equivalentes em projeção,
  permitindo alcance humano sem mudar o tamanho aparente da arma.
  Projeção próxima fixa em 68°; o FOV do mundo não muda sua proporção.
  O enquadramento foi elevado discretamente para expor a empunhadura que antes
  ficava fora da borda inferior. Mira central permanece livre em repouso.
- `Weapon` é a única autoridade de munição/tempos; apresentação amostra
  `reloadProgress()`/quantidade planejada. Não há relógio visual adicional.
  Pausa congela a fase; cancelamento/troca/drop limpam somente a pose transitória.
- Mão esquerda extrai o pente até 32%, o solta e busca outro. Não segue a
  instância descartada até o chão. Segura o novo pente na entrada, leva a mão
  ao mecanismo e volta ao suporte. Sniper alcança seu ferrolho também no início.
- Escopeta acompanha cada intervalo de 0,35 s por cartucho planejado e a
  finalização de 0,30 s; a última inserção não busca um cartucho inexistente.
  Nenhuma pose concede balas ou muda a duração lógica da recarga.
- Mira completa da sniper oculta o passe próximo inteiro, não deixa luvas
  sobre a luneta. Sem arma visível, em título ou resultados, não há mãos soltas.
  Corpo inferior continua no passe de mundo, sem braços duplicados.

## Custo e limites

Três submissões adicionais, **2264 triângulos** e 36 matrizes de skinning.
Malhas estáticas enviadas na inicialização; GPU deforma os vértices. Codificação
GDP usa os atributos já disponíveis, sem alterar Mesh/Vertex da engine.
O shader GDP passa a reservar 36 matrizes; o corpo inferior continua usando
somente suas sete. Modo de skinning é restaurado antes de flash/desenhos normais.
Nomes de uniforms fixos, sem construir strings ou malhas a cada frame.

Reutiliza albedo/normal/propriedades de tecido, couro e reforços já residentes.
Não adiciona luzes, sombras, framebuffer, segunda cena, collider, hitbox ou
estado persistente. Rasterização próxima é dupla face por causa da base de
câmera refletida que as armas existentes usam. POTATO conserva a silhueta.
O custo de geometria foi medido; não há benchmark comparativo representativo
de gameplay nem promessa de FPS. Não são animações capturadas nem arte final.

## Evidências e validação

Capturas nativas da fixture offscreen, não ilustrações nem playtest manual;
conversão BMP → PNG/GIF sem retoque. Aceitação das proporções é do usuário.

CPU: malhas/índices/pesos, 36 juntas válidas, deformação finita/determinística,
comprimentos de IK, âncora dominante, descarte independente, seis papéis,
inserções da escopeta, limites/NaN, endpoints exatos e trajetória sem teleport.
OpenGL: custo real, contato/silhueta, fases de recarga, endpoints/cancelamento,
recoil/agachar/ar/troca/faca, mira sniper, ausência no menu/resultados,
centro livre e FOV 60/100, 16:9/16:10/compacta/ultrawide, MEDIUM/POTATO.
Integração da Application: R real, pausa sem avanço/gasto, conclusão normal.

Rodada inicial: Release **65/65**, 61,34 s. Após ajustar ancoragem, a regressão
da sniper identificou um root de mira ainda ativo em menus (64/65, 58,58 s).
O gate efetivo de mira agora alimenta tanto arma quanto mãos; assertions,
tolerâncias e timeouts foram preservados. Resultados finais registrados após
repetição. A build ainda não foi publicada.

Repetição final Release: **65/65**, 58,06 s, incluindo o gate de sniper
isolado antes da suíte, integração real de recarga/pausa e fixture das mãos.

ASan/UBSan: nove dos dez gates focados passaram na rodada final (68,99 s);
CPU atingiu timeout de 10 s durante execução concorrente de captura/llvmpipe.
Repetido isoladamente, passou em **7,77 s**, sem alteração do timeout ou
assertions. Assim, os dez gates afetados foram aprovados, sem diagnósticos
ASan/UBSan; a integração completa da Application instrumentada não foi
reexecutada nesta entrega (integração Release passou na rodada acima).
Preset mantém `detect_leaks=0` pela restrição de ptrace.

Fixture final passou na Quadro K2100M/NVIDIA 470.256.02 e no Mesa llvmpipe
(LLVM 21.1.8, Mesa 26.2.3), com MEDIUM/POTATO e ambas as faixas de FOV.
Builds Release/instrumentada concluídas sem novos avisos; `git diff --check`
passou. Não há pacote Windows desta alteração local, pois não houve publicação.

Capturas do candidato revisto:

- [Pistola](../visual/validation/player_hands_v1_revised_pistol.png).
- [Carabina](../visual/validation/player_hands_v1_revised_carbine.png).
- [Cartucho da escopeta](../visual/validation/player_hands_v1_revised_shell.png).
- [Faca](../visual/validation/player_hands_v1_revised_knife.png).
- [POTATO/16:10](../visual/validation/player_hands_v1_revised_potato.png).
- [Recarga da pistola](../visual/validation/player_hands_v1_revised_pistol_reload.gif).
- [Recarga da carabina](../visual/validation/player_hands_v1_revised_carbine_reload.gif).

As capturas anteriores sem `revised` são o rascunho, não a correção proposta.

```bash
cmake --build --preset release --parallel 2
build/release/gdp --test-mode
```

Teste empunhadura, R depois de disparar, troca por scroll/Q/1/2/3, interrupção
e G; Ctrl/Space, FOV 60/100 e Mouse2 da sniper. A correção não muda movimento,
bhop, origem do tiro, dano, reservas, saves, geração ou treinamento das IAs.

```bash
SDL_VIDEODRIVER=offscreen build/release/gdp_renderer_gl_tests --player-hands-only
GDP_HANDS_CAPTURE_DIR=build/visual-validation/player-hands-final \
SDL_VIDEODRIVER=offscreen build/release/gdp_renderer_gl_tests --player-hands-only
```

Mudanças do fluxo paralelo de IA presentes no checkout foram preservadas.
Este candidato não foi publicado isoladamente antes da revisão. A nova
aprovação autoriza a implementação v2, documentada à parte; não transforma
os resultados históricos acima em validação da v2.

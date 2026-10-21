# Uncharted para Xbox UWP — desenvolvimento experimental

**Ainda não é um port jogável.** Não inclui executáveis, dados ou shaders do jogo.
Base: SuperMedo/uncharted_pc v0.3.6, commit
`b787c1068c3a9a9ba3ec43dd8086af2bf7d35c7d`.
A adaptação usa componentes de Cosmo/gow3_xbox
`d0975c91d827045724230476c1527cd38afd72f4`, preservando os ajustes do Uncharted.

## Etapas

| Versão | Conteúdo | Validação no Xbox |
| --- | --- | --- |
| 0.1 | CPU, memória sob demanda, aliases, proteções e swap | Log do testador: resultado 0; 8242 checagens, zero corrupção |
| 0.2 | Vulkan/Dozen → imagem compartilhada D3D12 → CoreWindow | Log do testador: primeiro quadro apresentado em 1280×720; resultado 0 |
| 0.3 | Renderizador ligado ao host; testes sintéticos de shaders e texturas | Pendente |

Na versão 0.3, áudio, controle do jogo e serviços online ainda não estão ligados.
Imports de PS4 indisponíveis permanecem sem endereço e são identificados pelo
carregador. Upscalers temporais e geração de quadros estão desabilitados.
Não foram validados shaders de um jogo real, jogabilidade, uso de RAM sob carga
real ou desempenho em nenhum dos três Uncharted.

## Testar no Xbox pelo celular

1. Extraia o ZIP do pacote no celular.
2. No Device Portal do Xbox Series S/X em Modo de Desenvolvedor, use Add/Adicionar
   e selecione o `.appx` principal.
3. Caso seja solicitado, inclua o `.appx` do framework VCLibs em `Dependencies`.
4. Abra **Uncharted UWP Diagnostic**. Não precisa copiar o jogo para este teste.
5. Recupere `LocalState/loader.log` e `errors.log` pelo Device Portal.

O app usa a identidade própria `UnchartedUwpProbe` e não substitui o GoW3.
Guarde os logs anteriores antes de rodar: cada execução reescreve os logs.

Na versão 0.3, a execução padrão testa cópias de texturas RGBA e BC1/BC3,
linhas com padding, níveis mip, camadas, conversão do layout em blocos de texturas PS4,
amostragem por shader e escrita em storage images. Compara os resultados da
GPU com padrões conhecidos na CPU. Se tudo passar, apresenta o quadro azul.
O log esperado contém `Renderer texture/shader self-test: PASS`,
`CoreWindow PRESENTED` e `Diagnostic finished: 0`.
Um resultado 0 de outro modo não comprova que esses testes passaram.

## Selecionar o modo por run.txt

O arquivo é opcional e fica em `LocalState/run.txt`.

| Linha 1 | Comportamento |
| --- | --- |
| Ausente ou `renderer` | Na versão 0.3, executa os testes de texturas/shaders e apresentação |
| `graphics` | Na 0.2, apresentação; na 0.3, testes completos de texturas/shaders e apresentação |
| `memory` | Repete o diagnóstico de CPU/memória/swap |
| `imports` | Lista imports ausentes de um executável preparado |
| `boot` | Tenta iniciar um executável preparado; na 0.3 o renderizador está ligado, mas serviços ainda faltam |

Para `imports` ou `boot`, a linha 2 contém a pasta do dump original e a linha 3
seleciona `1`, `2` ou `3`. Esses modos exigem preparação privada no PC:

```sh
python xbox/scripts/prepare-game.py --help
```

Use o script com seu próprio dump descriptografado. Ele usa o linker original e
produz `Prepared/uN/boot-linked.bin` e `content.bin`. Copie a pasta `Prepared`
para `LocalState`. O script rejeita saídas existentes ou dentro do dump; não
modifica os dados originais. A preparação não comprova que o jogo iniciará.
Há um watchdog de 60 segundos nos modos experimentais do host.

## Compilar

É necessário Windows, Visual Studio com C++, SDK do Windows, clang-cl, Python
3.12 e Ninja. Para o renderizador, também é usado MSYS2 com make/diffutils/zstd.
As dependências são preparadas em `xbox/.local/deps`; não são dados do jogo.

```powershell
python -m pip install ninja==1.11.1.4 zstandard==0.23.0
./xbox/scripts/build.ps1
./xbox/scripts/package.ps1
```

Isso gera o diagnóstico básico de CPU. Para a ponte gráfica, adicione
`-GraphicsProbe -DozenDirectory <pasta-do-Dozen> -DxcDirectory <pasta-do-DXC>`.
Para ligar o renderizador, adicione também `-CompileRenderer`.
`build-deps.ps1` prepara FFmpeg, glslang e os headers necessários.

Workflows do repositório:

- **Xbox CPU diagnostic**: compila o diagnóstico e testa o backend de memória no Windows.
- **Xbox graphics build**: compila Dozen e assina a ponte gráfica 0.2.
- **Xbox renderer integration**: compila Dozen, as dependências e o renderizador;
  testa o contrato de memória, verifica imports UWP e assina o pacote 0.3.

A existência do workflow não significa que sua execução passou: confira o
resultado no Actions. Somente um pacote que passar por essas etapas deve ser
entregue para o próximo teste no console. As versões de Mesa, DXC e demais
fontes estão fixadas nos scripts/workflows. Veja `CREDITS.md` e `VALIDATION.md`.

## Verificações portáveis

```sh
python xbox/scripts/verify-portable.py
```

Os 38 testes Python, os testes C de I/O paginado e o carregador sintético
passaram no Linux. Eles cobrem regressões de CPU/arquivos e não validam o
renderizador UWP ou a execução do jogo.

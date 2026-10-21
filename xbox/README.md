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
| 0.3 | Renderizador ligado ao host; testes sintéticos de shaders e texturas | Log do testador: cópias, detile, amostragem e storage images sem erros; resultado 0 |
| 0.4 | Preparação nativa do dump no Xbox, busca no USB e menu de teste de boot | Log do testador: preparação de Uncharted 3 passou; imports pendentes |
| 0.4.1 | Registro das funções gráficas antes do diagnóstico de imports | Log do testador: pendências caíram de 124 para 59 |
| 0.5 | Controle nativo, teste físico e memória POSIX anônima | Pendente |

Na versão 0.3, áudio, controle do jogo e serviços online ainda não estão ligados.
Imports de PS4 indisponíveis permanecem sem endereço e são identificados pelo
carregador. Upscalers temporais e geração de quadros estão desabilitados.
Não foram validados shaders de um jogo real, jogabilidade, uso de RAM sob carga
real ou desempenho em nenhum dos três Uncharted.

## Preparação nativa e teste com o dump (0.4)

Veja [LEIA-ME-0.4.txt](LEIA-ME-0.4.txt) para instalação e primeiro teste pelo celular.
Coloque o dump plaintext completo em `USB:\dumps\Uncharted`, preservando
`eboot.bin`, `sce_sys`, `sce_module`, `u1data`, `u2data`, `u3data` e os demais dados originais.
Abra o app, escolha o jogo e **Conferir serviços**. A preparação roda no Xbox;
não exige Python no PC. Traga `loader.log` e `errors.log` para identificar as
próximas implementações necessárias. Resultado 20 informa imports ausentes.

**Tentar iniciar** passa o executável preparado ao runtime com o renderizador
ligado. Essa tentativa ainda não valida gameplay. Áudio continua sem implementação. A 0.5 liga o serviço de controle nativo; seu
funcionamento físico no Xbox e durante o jogo ainda precisa ser validado.

O preparador compara byte por byte com os scripts originais em fixtures sintéticas:
imports de todas as bibliotecas conhecidas, bindings nativos, TLS, relocações,
versões de módulos e conversão FS→GS. Isso valida o contrato desses casos,
não um dump real. O dump é somente lido; os resultados ficam em `LocalState/Prepared`.

## Testar no Xbox pelo celular

1. Extraia o ZIP do pacote no celular.
2. No Device Portal do Xbox Series S/X em Modo de Desenvolvedor, use Add/Adicionar
   e selecione o `.appx` principal.
3. Caso seja solicitado, inclua o `.appx` do framework VCLibs em `Dependencies`.
4. Abra **Uncharted UWP Diagnostic** e escolha **Testar renderizador**. Não precisa copiar o jogo para esse teste.
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
| Ausente ou `menu` | Na 0.4, abre seleção de jogo e modo; na 0.3, inicia o teste gráfico |
| `renderer` | Executa os testes de texturas/shaders e apresentação |
| `graphics` | Na 0.2, apresentação; na 0.3, testes completos de texturas/shaders e apresentação |
| `memory` | Repete o diagnóstico de CPU/memória/swap |
| `imports` | Prepara o dump e lista imports ausentes |
| `boot` | Prepara o dump e tenta iniciar; na 0.3 o renderizador está ligado, mas serviços ainda faltam |

Para `imports` ou `boot`, a linha 2 contém a pasta do dump original (vazia
para busca automática) e a linha 3 seleciona `1`, `2` ou `3`. Na 0.4 o app
prepara novamente os arquivos antes de cada tentativa; não usa cache antigo
quando a preparação falha. Não altera o dump.

O script `python xbox/scripts/prepare-game.py --help` continua disponível para
inspeção privada no PC, mas deixou de ser necessário para esse fluxo no Xbox.
Os watchdogs são 5 minutos para preparação e 90 segundos para diagnóstico de
imports/boot; o menu não tem limite. O teste gráfico mantém limite de 60 segundos.

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
  testa o contrato de memória, verifica imports UWP e verifica a preparação nativa contra Python e assina o pacote 0.4.

A existência do workflow não significa que sua execução passou: confira o
resultado no Actions. Somente um pacote que passar por essas etapas deve ser
entregue para o próximo teste no console. As versões de Mesa, DXC e demais
fontes estão fixadas nos scripts/workflows. Veja `CREDITS.md` e `VALIDATION.md`.

## Verificações portáveis

```sh
python xbox/scripts/verify-portable.py
```

Os 44 testes Python, os testes C de I/O paginado e o carregador sintético
passaram no Linux. Eles cobrem regressões de CPU/arquivos e não validam o
renderizador UWP ou a execução do jogo.

## Correção de imports 0.4.1

O log de Uncharted 3 fornecido pelo testador confirmou leitura do USB e
preparação nativa: boot=33.391.052 bytes, content=28 bytes. Reportou 124 de 552
imports ausentes. Desses, 65 identificadores já constavam nas tabelas do
renderizador, mas só eram registrados em `bbgpu_init`, depois do diagnóstico.

O registro de endereços agora é separado da construção do presenter/dispositivo
e do driver de vídeo. O runtime UWP registra esses endereços antes da consulta
de imports; a inicialização real continua na tentativa de boot. O caminho
desktop preserva a ordem anterior. Um contrato Windows verifica os 65 imports
pelo resolver real, sem janela nem inicialização de GPU, e mantém áudio, controle
e identificadores desconhecidos sem resolução. Isso não testa essas funções em execução.

Repita **Conferir serviços** no Xbox. Para o mesmo executável de Uncharted 3,
a expectativa é 59 imports pendentes; resultado 20 ainda é esperado.

## Controle nativo e memória 0.5

Veja [LEIA-ME-0.5.txt](LEIA-ME-0.5.txt). O menu **Diagnósticos → Testar controle**
chama o serviço de pad pela ABI guest durante 20 segundos. A/B/X/Y, ambos os
analógicos e LT/RT precisam ser exercitados para PASS; Y solicita um pulso de
vibração, cuja sensação física deve ser confirmada pelo testador.

O backend usa Windows.Gaming.Input, mantém o controle selecionado enquanto ele
permanecer conectado e acompanha substituições/reconexões. View vira o botão
do touchpad; não há gestos, giroscópio ou acelerômetro. A barra RGB é apenas
estado virtual. O código específico de câmera/callbacks do pad SDL do Uncharted
ainda não foi integrado ao novo backend. Áudio e serviços online continuam pendentes.

`mmap` anônimo agora usa memória flexível do guest, com zero inicial, limites
e erros POSIX. `msync` valida a região anônima e `munmap` retorna errno em erros.
Mapeamentos de arquivo e flags não suportadas retornam erro explícito.

Os testes portáveis passaram: 187 checagens do pad, 32.786 checagens de memória
POSIX e os 44 testes Python existentes. As entradas do teste de pad são simuladas;
a integração com o controle físico precisa ser validada no Xbox. Para o mesmo
executável U3 do log, esperam-se 50 pendências no modo imports. Isso não é
evidência de gameplay. A próxima tentativa útil é **Tentar iniciar**, após o teste do controle.

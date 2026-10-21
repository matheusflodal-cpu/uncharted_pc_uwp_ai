# Uncharted para Xbox UWP — desenvolvimento experimental

**Ainda não é um port jogável.** Não inclui executáveis, dados ou shaders do jogo.
Base: SuperMedo/uncharted_pc v0.3.6, commit
`b787c1068c3a9a9ba3ec43dd8086af2bf7d35c7d`.
A adaptação usa componentes de Cosmo/gow3_xbox
`d0975c91d827045724230476c1527cd38afd72f4`, preservando os ajustes do Uncharted.

## Teste atual (0.6.6)

Veja [LEIA-ME-0.6.6.txt](LEIA-ME-0.6.6.txt). Atualize o pacote e escolha
**Selecionar jogo → Uncharted 3 → Tentar iniciar**. Envie os dois logs dessa
tentativa. Não é necessário repetir o teste do controle nem copiar o dump.

A 0.5.1 impede que o overlay inicialize com uma tabela Vulkan incompleta.
Se uma função necessária ao menu estiver ausente, o overlay é desabilitado
com uma mensagem explícita; o renderizador continua sua inicialização.
A apresentação do Xbox continua usando a ponte D3D12/CoreWindow.
A correção do overlay foi confirmada no console: faltava vkDestroySurfaceKHR.
O jogo executou seus módulos e abriu 112 arquivos antes de solicitar áudio.
A 0.6 integra AudioOut e os decodificadores originais AJM ATRAC9/MP3.
O teste dedicado de som foi confirmado fisicamente no Xbox: 375 buffers,
48 kHz estéreo e resultado 0. No boot, três portas e o contexto AJM abriram.
O jogo solicitou shaders gráficos e compute, mas a tela permaneceu preta
até o limite de 90 segundos. No teste 0.6.1, seis pipelines foram pré-carregados,
as chamadas estabilizaram em 9 draws e 4 dispatches aos 5 segundos, sem pedido
de flip do jogo até os 300 segundos. A memória ficou perto de 2,4 GiB de 5 GiB;
não houve esgotamento registrado. O jogo também pediu uma mensagem de controle
não detectado. Isso não identifica por si só a causa da tela preta.

A 0.6.2 cobre uma lacuna: criação do pipeline de tiling/detiling usa um caminho
próprio fora das medições anteriores. Acrescenta início/retorno nesse caminho,
no registro de draws/dispatches, contadores de comandos PM4 por fila e os
operandos das esperas observados pelo processador de comandos. O monitor não
lê memória guest nem consulta a GPU. O controle registra abertura e resultados
reais do host; uma falha de conexão não é convertida em conexão fictícia.
O limite continua em 300 segundos. Não é necessário repetir áudio ou controle.
Rede/PSN e outros serviços continuam pendentes; nenhum menu ou gameplay foi
confirmado.

## Etapas

| Versão | Conteúdo | Validação no Xbox |
| --- | --- | --- |
| 0.1 | CPU, memória sob demanda, aliases, proteções e swap | Log do testador: resultado 0; 8242 checagens, zero corrupção |
| 0.2 | Vulkan/Dozen → imagem compartilhada D3D12 → CoreWindow | Log do testador: primeiro quadro apresentado em 1280×720; resultado 0 |
| 0.3 | Renderizador ligado ao host; testes sintéticos de shaders e texturas | Log do testador: cópias, detile, amostragem e storage images sem erros; resultado 0 |
| 0.4 | Preparação nativa do dump no Xbox, busca no USB e menu de teste de boot | Log do testador: preparação de Uncharted 3 passou; imports pendentes |
| 0.4.1 | Registro das funções gráficas antes do diagnóstico de imports | Log do testador: pendências caíram de 124 para 59 |
| 0.5 | Controle nativo, teste físico e memória POSIX anônima | Controle: 949 amostras, zero erros; vibração confirmada pelo testador. Boot parou na inicialização gráfica |
| 0.5.1 | Verificação do carregamento do overlay e logs de inicialização gráfica | Xbox executou código do jogo; parou em sceAudioOutInit |
| 0.6 | AudioOut via XAudio2, AJM ATRAC9/MP3 e teste de som | Tom ouvido no Xbox; boot abriu áudio/AJM e chegou a shaders reais; watchdog de 90 s encerrou |
| 0.6.1 | Medições de compilação, CPU/RAM e mensagens do jogo; teste de boot de 5 min | 9 draws/4 dispatches estáveis, zero flip do jogo, aviso de controle; watchdog de 300 s |
| 0.6.2 | Medições do driver de tiling, registro de comandos, esperas PM4 e controle durante o boot | Driver de tiling retornou em 42 ms; quarta DispatchRecord não retornou; consulta do controle vazia |
| 0.6.3 | Cópia de origem guest antes do lock de commit e controle verificado antes do boot | Quarta dispatch retornou em 36 ms; controle reconhecido; avançou até outro pipeline, que falhou com ErrorOutOfHostMemory |
| 0.6.4 | Dozen com diagnóstico nativo de DXIL/D3D12 e estado/memória de pipelines | Build Windows e contratos passaram; teste Xbox pendente |
| 0.6.5 | Leitura direta de texturas inteiras de um nivel, cache e testes de shader | Rejeicao DXIL reproduzida em SM 6.0; 80 pipelines convertidos e build completo passaram; Xbox pendente |

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
ligado. Essa tentativa ainda não valida gameplay. A 0.6 integra áudio nativo. A 0.5 liga o serviço de controle nativo; seu
funcionamento físico e a vibração foram confirmados no teste dedicado. O uso durante o jogo ainda não foi validado.

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
Os watchdogs são 5 minutos para preparação e, a partir da 0.6.1, 5 minutos para boot;
o diagnóstico de imports mantém 90 segundos; o menu não tem limite. O teste gráfico mantém limite de 60 segundos.

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

## Correcao de memoria na 0.6.3

A copia para o backing UWP lia a origem enquanto segurava o lock de commit.
Uma origem guest ainda nao carregada ou com protecao host antiga podia
provocar uma falta de pagina cujo handler adquire o mesmo lock exclusivo.
A leitura agora ocorre antes desse lock em buffer host limitado a 16 KiB,
com buffer de pilha de apenas 256 bytes para pequenos fences. O teste Windows
cobre origem lazy, cruzamento de paginas, protecao antiga e destino read-only.
Ainda e necessario confirmar se esse defeito explica o bloqueio do Xbox.
O boot exige uma deteccao real do controle em ate cinco segundos; nenhuma
conexao e fabricada. A lista inicial vazia do ultimo log deixou de ser ignorada.

## Diagnostico do driver na 0.6.4

O erro novo ocorre no pipeline 0xf43a301791304559 (FS 0x81a0a982 e VS
0xf585649d). O Dozen fixado converte falhas de nir_to_dxil, validacao DXIL
e CreatePipelineState para VK_ERROR_OUT_OF_HOST_MEMORY. Portanto esse
retorno sozinho nao prova esgotamento de RAM. O ultimo registro foi 2553
MiB de 5120 MiB, cerca de um segundo antes da parada; nao e uma medida
do pico da chamada. A 0.6.4 recompila o mesmo Dozen com um callback de
diagnostico limitado, conectado ao stderr do aplicativo, e registra estado
e memoria por chamada. Nao altera flags de compilacao, shader ou validacao.


## Texturas inteiras na 0.6.5

O teste Xbox 0.6.4 identificou rejeicao DXIL de sample.i32 no shader
0x81a0a982, pipeline 0xf43a301791304559, com memoria de 2924/5120 MiB.
A 0.6.5 converte amostragem inteira por ponto em texelFetch quando a view
tem um unico nivel. Preserva bits e enderecamento, incluindo repeticao,
espelhamento, clamp, offsets e bordas predefinidas. Mips multiplos, filtros
nao suportados, cubemaps e borda customizada mantem o caminho validado
anterior e registram os parametros. A aplicabilidade ao shader real e o
boot do jogo ainda dependem de teste Xbox. O cache de shaders/pipelines
foi versionado novamente e os estados embutidos entram na especializacao.


## Logos confirmadas e teste 0.6.6

Na 0.6.5 o testador confirmou as logos do jogo no Xbox Series S. A conversao
do shader 0x81a0a982 foi aplicada e o runtime passou da rejeicao DXIL.
O ultimo snapshot registra 741 flip requests e 743 chamadas de apresentacao
retornadas; nao equivalem a 743 imagens distintas. Menu/gameplay pendentes.
Depois houve VK_ERROR_DEVICE_LOST e escrita nula em vulkan_dzn.dll+0x4bca.
A instrucao e um preenchimento por rep stos depois de Map sem verificar o
HRESULT, consistente com o CmdFillBuffer do mesmo binario.

A 0.6.6 aplica barreiras conservadoras nos limites de renderizacao, baseadas
no GoW3, como teste da hipotese de sincronizacao. Verifica Map em Fill/Update,
preserva o erro de gravacao Vulkan e registra o motivo nativo da perda do
dispositivo. A ponte encerra o processo na perda confirmada, evitando
unwind de recursos concorrente com o guest. Isto nao prova corrigir a
perda original; a protecao da escrita nula e uma correcao separada.

O modo padrao e conservative. Para comparacao, LocalState/gpu-sync.txt pode
conter normal ou conservative. Nao ha alteracao de shaders nesta etapa; a
conversao inteira da 0.6.5 permanece. O modo pode reduzir desempenho.

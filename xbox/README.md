# Uncharted UWP — primeira etapa de desenvolvimento

**Estado: código de diagnóstico de CPU e memória. Não é um port jogável.**
Base: Uncharted for Windows **v0.3.6**, commit
`b787c1068c3a9a9ba3ec43dd8086af2bf7d35c7d`.
Referência de Xbox: Cosmo/gow3_xbox
`d0975c91d827045724230476c1527cd38afd72f4`.

## O que foi implementado

- Entrada UWP x64, manifesto com identidade própria `UnchartedUwpProbe` e logs em LocalState.
- Teste de memória executável com ABI System V, usado pelo código de PS4.
- Backend de memória esparsa, aliases físicos, proteções de páginas e paginador em disco,
  adaptados do GoW3 para Xbox, com os respectivos testes de contrato.
- Adaptadores de arquivos UWP e transferência por buffer temporário para páginas sujeitas a swap.
- Preservação da correção de leituras incompletas do Uncharted 0.3.6.
- Preparação no PC dos executáveis de Uncharted 1, 2 ou 3 usando o linker original.
- Workflow de GitHub Actions para **compilar um pacote de diagnóstico** no Windows.

As funções de GPU, áudio e controle do visitante continuam explicitamente sem implementação
neste alvo. Imports de PS4 não implementados retornam endereço zero ao resolvedor e o
carregador os identifica; não há implementações que finjam executar esses serviços.

## O que foi verificado

O carregador nativo foi compilado no Linux com os adaptadores de serviços de diagnóstico.
Passaram os 14 testes originais de executáveis sintéticos, cobrindo execução de código,
relocações, bibliotecas nativas, metadados inválidos e imports ausentes. Também passaram
os testes de leitura/escrita por buffer com leituras parciais injetadas, os testes de arquivos
de mod/salvamento e os testes Python indicados em `VALIDATION.md`.

**Ainda não foi compilado no Windows, instalado no Xbox ou executado com os jogos reais.**
Os testes portáveis não validam a compilação clang-cl nem as APIs de memória UWP.
A existência de um workflow não significa que ele já executou ou passou.

## Compilar com GitHub Actions

1. Faça um fork de https://github.com/Supermedo/uncharted_pc na sua conta.
2. Aplique esta alteração sobre a tag `v0.3.6`, numa branch `xbox/uwp-v0.3.6`.
   O arquivo `.patch` entregue com o código contém a alteração completa.
   Em um clone na tag correta: `git am /caminho/uncharted-uwp-diagnostic.patch`.
3. Envie essa branch ao seu fork. O workflow **Xbox CPU diagnostic** executa no push.
4. Se necessário, habilite Actions no fork. Também é possível executar manualmente em
   **Actions → Xbox CPU diagnostic → Run workflow**, selecionando essa branch.
5. Uma compilação que passar publica o artefato `Uncharted-UWP-CPU-diagnostic`.
   Em caso de falha, use o log do primeiro passo com erro para a próxima correção.

O workflow usa `windows-2022`, SDK do Windows, clang-cl, CMake, Ninja e winpthreads UCRT
com versão e SHA-256 fixos. A etapa de empacotamento verifica os imports do executável e da DLL
contra WindowsApp.lib antes de produzir o `.appx`. Certificados de teste são gerados durante
a compilação, com publisher separado do GoW3; não há chave privada compartilhada no projeto.
Para atualizações mantendo a mesma identidade de assinatura, forneça um certificado próprio
a `package.ps1` pelos parâmetros `CertificatePath` e `CertificatePassword`.

Este alvo não depende dos submódulos gráficos. Eles continuam referenciados no repositório
original e serão necessários quando o renderizador real for integrado.

## Testar a primeira compilação

Instale o `.appx` e a dependência VCLibs pelo Device Portal do Xbox Series S/X em Developer
Mode. Configure o tipo como **Game** para o orçamento de memória correspondente.

Ao abrir sem `run.txt`, o aplicativo executa os testes de memória e mostra um diálogo
com o resultado. Uma janela sem renderização durante o teste é esperada nesta etapa.
O watchdog encerra o diagnóstico depois de 60 segundos se ele não terminar.

Pelo explorador de arquivos do Device Portal, recupere em **LocalState**:

- `loader.log`: etapas, resultado e estatísticas de memória/swap.
- `errors.log`: mensagens de falha.

O log deve indicar `Executable System V smoke: PASS (42)` e os resultados dos testes de
memória. Qualquer código diferente de zero exige investigação; instalação bem-sucedida
não comprova memória funcionando, e memória funcionando não comprova gameplay.

## Diagnóstico de imports com seu dump

Prepare os arquivos no PC com Python 3.12 ou superior:

```powershell
python xbox/scripts/prepare-game.py "D:\MinhaCollection" --game 1 --out "D:\Preparados\u1"
```

Use `--game 2` ou `--game 3` para os outros executáveis. O script não altera o dump
e recusa uma pasta de saída dentro dele ou uma pasta de saída já existente.
O dump precisa estar descriptografado; este projeto não inclui ferramentas de descriptografia.

No Device Portal, envie **somente de forma privada** `boot-linked.bin` e `content.bin` para
`LocalState/Prepared/u1/` (ou `u2`, `u3`). Copie a Collection completa para um USB em
`E:\dumps\Uncharted`. Não inclua arquivos de jogo no repositório, nos workflows ou em releases.

Crie `LocalState/run.txt` com estas três linhas, sem aspas:

```text
imports
E:\dumps\Uncharted
1
```

O modo `imports` examina quais chamadas ainda estão ausentes e retorna **20** quando
encontra alguma, o que é esperado neste estágio sem renderizador. Esse inventário
não inicializa nem joga o título. `memory` na primeira linha repete os testes de memória.
O modo de desenvolvimento `boot` tenta executar a inicialização da CPU até a primeira
chamada ausente ou erro; ele não tem gráficos, áudio nem controle e não é um modo jogável.

## Próximas etapas, em ordem

1. Corrigir os eventuais erros de compilação Windows e validar memória/aliases/paginação no Xbox.
2. Integrar o renderizador **do Uncharted** com CoreWindow e Mesa/Dozen, preservando suas
   correções de shaders e sincronização; os dois renderizadores não são intercambiáveis.
3. Conectar controles, áudio, vídeo, salvamento e suspensão/retomada.
4. Testar cada jogo e medir memória/FPS em fases reais. Swap não remove a necessidade
   de manter código, pilhas e recursos ativos de GPU residentes na memória.

Veja `CREDITS.md` para autoria e licenças. Nenhum desempenho, resolução ou conclusão dos
jogos no Xbox foi demonstrado por esta adaptação.

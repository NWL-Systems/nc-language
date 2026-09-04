# Changelog

## v3.1 — Fusão nc-language + nc-os-extensions-files (2026)

Repositórios `nc-language` e `nc-os-extensions-files` unificados em um só,
com build único (`build_all.sh`) e os seguintes bugs corrigidos:

### Corrigido — `nc-cmd` (shell interativo)

- **Não funcionava de jeito nenhum.** Chamava um binário chamado
  `./nc_compiler` (o binário real se chama `nclang`) passando uma flag
  `-e` que **nunca existiu** no compilador. Todo comando digitado resultava
  em `Erro: nao abriu -e`. Corrigido: `nc-cmd` agora chama `./nclang`
  (com fallback pro PATH) e o compilador ganhou um modo `-e` de verdade.
- **Sem persistência de variáveis.** Cada linha rodava como processo
  isolado — uma variável criada numa linha desaparecia na próxima. Agora
  `nc-cmd` mantém uma sessão (arquivo `.nc` que cresce a cada linha) e
  reexecuta o histórico inteiro a cada comando novo, preservando estado.
  Adicionado o comando `reset` pra limpar a sessão.
- **Comandos com aspas duplas quebravam** (ex: `say = "texto"`, o uso mais
  comum da linguagem) porque o código era embutido dentro de aspas duplas
  do shell via `system()`, e a string NC já tinha aspas duplas dentro —
  colisão de quoting. Corrigido: o código passa a ser escrito num arquivo
  temporário em vez de ir por linha de comando, eliminando o problema.

### Corrigido — compilador (`nc_compiler.c`)

- **`!if!/!elif!/!else!` fechava o bloco duas vezes.** O próprio README
  documenta usar `]` antes de `!elif!`/`!else!`, mas o código dessas duas
  instruções também emitia um `}` extra por conta própria, quebrando a
  compilação de qualquer `if` com mais de um ramo. Agora `!elif!`/`!else!`
  só abrem o próximo bloco; quem fecha é sempre o `]`.
- **`!ls!`, `!mkdir!` e `!write!` não existiam no compilador**, apesar de
  serem usados no `filemanager.nc` de exemplo — viravam comentário morto
  silenciosamente (nenhum erro, mas nada acontecia). Implementados de
  verdade, com helpers portáveis (`_nc_mkdir`, `_nc_write_file`,
  `_nc_list_dir`) geradas no C de saída.
- **`!ask!` pra variáveis de texto usava `scanf("%s")`**, que corta a
  resposta no primeiro espaço. Trocado por leitura de linha inteira
  (`fgets` + remoção do `\n`), preservando frases completas.
- **Path de saída com `/` gerava arquivo `.c` intermediário inválido**
  (ex: `nclang app.nc build/app` ou `nclang app.nc /tmp/app` gerava um
  caminho tipo `/tmp/_nc_/tmp/app.c`, que não existe). Corrigido: se o
  nome de saída já contém um caminho, o `.c` intermediário fica ao lado
  dele em vez de ser concatenado com o diretório temporário.
- **Execução automática quebrava com saída em caminho absoluto** — sempre
  prefixava `./` no comando de rodar o binário gerado, o que transforma um
  caminho absoluto tipo `/tmp/app` em `.//tmp/app` (relativo à pasta
  errada). Corrigido pra só prefixar `./` quando o nome não tem caminho.
- **Novo modo `nclang -e "codigo"`** — executa código NC de uma linha
  direto, sem precisar criar um arquivo `.nc` (usado internamente pelo
  `nc-cmd`, mas também utilizável direto no terminal).
- **Novo modo `nclang --install-pkg pacote.ncpkg [destino]`** — antes,
  `.ncpkg` era só um `.zip` parado sem nenhum código no projeto pra lê-lo.
  Agora extrai o pacote, mostra o manifesto (`package.ncpkg.json`) e
  orienta como usar os headers extraídos (requer `unzip` instalado).
- Double free: `fclose(fin); fclose(fout);` era chamado duas vezes na
  função de empacotamento de arquivos genéricos.
- Path temporário fixo do Termux (`/data/data/com.termux/...`) quebrava a
  compilação em qualquer sistema fora do Termux. Agora detecta `$PREFIX`
  (Termux), depois `$TMPDIR`, depois cai pra `/tmp`.
- `clang` hardcoded quebrava em distros Linux com só `gcc` instalado (como
  este ambiente de sandbox). Agora usa `$CC` se definido, senão `cc`.
- Corrigido truncamento de buffer no `chmod_cmd` e adicionada checagem do
  retorno do `chmod` (antes falha silenciosa).
- Indentação cosmética do fechamento de bloco (`]`) corrigida pra usar o
  nível de indentação certo.
- Versão do compilador: `3.0` → `3.1`.

### Corrigido — extensões (`extensions/fs/`)

- **`nc_fs-1.c` incluía `"nc_fs.h"`, mas o header se chamava
  `nc_fs-1.h`** — o módulo nunca compilava. Renomeados para `nc_fs.c` /
  `nc_fs.h` / `nc_setup.c`.
- **`nc_setup` reportava sucesso mesmo quando a pasta base não existia**
  (ex: passar um caminho customizado inexistente por `argv[1]`): os
  `mkdir()` falhavam silenciosamente porque o diretório pai não existia,
  e o retorno nunca era checado. Agora a pasta base é criada primeiro, os
  erros de cada `mkdir` são checados e reportados, e `nc_setup` retorna
  código de saída `1` em caso de falha real.
- `fread()` sem checar retorno em `nc_read_file` e `nc_is_valid` — podiam
  ler dados não inicializados de arquivos truncados/corrompidos. Agora os
  retornos são checados.
- Normalizado o `nc-cmd.c` de CRLF pra LF (estava em quebra de linha do
  Windows, destoando do resto do projeto).

### Adicionado

- `update.sh` na raiz — pra quem clonou com `git`, atualiza o repositório
  local (`git pull --ff-only`) e recompila tudo automaticamente. Avisa
  antes de sobrescrever alterações locais não commitadas. Antes disso não
  existia nenhum mecanismo de update (o `update_package.sh` original do
  repo `nc-language` estava vazio, 0 bytes).
- `build_all.sh` único na raiz, cobrindo `nclang`, `nc-cmd` e `nc_setup`
  de uma vez, com detecção automática de compilador (`clang`/`gcc`/`cc`)
  e build opcional pra Windows via MinGW se disponível. O `build_all.sh`
  antigo (dentro de `nc-os-extensions-files`) referenciava um
  `nc_compiler.c` que não existia naquele repositório — nunca funcionava
  sozinho.
- `language-tools/language-configuration.json` — referenciado pelo
  `package.json` da extensão de VS Code, mas ausente nos dois zips
  originais (a extensão quebraria ao carregar).

### Não alterado (funciona como antes)

- Sintaxe da linguagem, tipos, extensões de arquivo (`.ncapp`, `.ncdocs`
  etc.), grammar do VS Code e entrada do Linguist.

## v3.2

- **`!mkdir!`, `!ls!` e `!write!` sem argumento (ou `!write!` sem vírgula)
  agora dão um erro de sintaxe NC claro, com número da linha**, em vez de
  gerar C quebrado e deixar o `gcc`/`clang` reclamar com uma mensagem que
  não tem nada a ver pra quem só escreve `.nc` (ex: `too few arguments to
  function call` apontando pro C gerado, que o usuário nunca viu). Agora
  `nclang` barra a compilação antes de chamar o compilador C.

## v3.3

- **App Android "NC Terminal"** (`android/`): tela preta/letras brancas
  em Kivy que roda `nclang` de verdade (binário nativo compilado via NDK,
  empacotado como `.so` pra passar pela restrição de execução do Android
  10+). Buildado automaticamente via GitHub Actions
  (`.github/workflows/android-build.yml`) — não precisa de Android
  Studio nem SDK instalado localmente. Detalhes e limitações honestas
  (o que foi testado e o que não) em `android/README.md`.
- `.ncvideo` reconhecido como tipo de arquivo pelo NCFS (`nc_get_type`,
  magic bytes `NCVD`). Só o reconhecimento — ainda não existe player de
  vídeo/GIF (precisa de uma lib gráfica tipo SDL2, fora do escopo atual).
- `build_all.sh`: build Windows agora detecta tanto `x86_64-w64-mingw32-gcc`
  (Debian/Ubuntu, pacote `mingw-w64`) quanto `x86_64-w64-mingw32-clang`
  (Termux, pacote `llvm-mingw-w64`), já que os dois ambientes usam nomes
  de compilador diferentes pro mesmo cross-compile.
- **`!mkdir!`, `!ls!` e `!write!` sem argumento (ou `!write!` sem vírgula)
  agora dão um erro de sintaxe NC claro, com número da linha**, em vez de
  gerar C quebrado e deixar o `gcc`/`clang` reclamar com uma mensagem que
  não tem nada a ver pra quem só escreve `.nc`.
- **Exit code errado**: `nclang` sempre retornava `0` mesmo quando o
  programa `.nc` executado (ou o modo `-e`) falhava, porque o valor cru
  de `system()` (um status de `wait()`, não o exit code) estava sendo
  devolvido direto como código de saída do processo — por coincidência
  de bits, isso quase sempre virava `0`. Corrigido com `WEXITSTATUS`.

### Fora do escopo (limitações honestas)

- **APK assinado pronto pra loja**: o workflow gera um APK de **debug**,
  não assinado pra distribuição. Assinatura de release precisa de
  keystore próprio, que é uma decisão sua (chave privada não pode viver
  num repositório público).
- **App macOS**: não existe cross-compile legal pra macOS fora de um Mac
  de verdade (ou GitHub Actions com runner `macos-latest`) — é limitação
  de licenciamento da Apple, não técnica.
- **Player de `.ncvideo`**: recodificação/decodificação de vídeo precisa
  de uma lib como SDL2 ou libavcodec com bindings C, não implementado.

## v3.4

- **Divisão de build reorganizada**: GitHub Actions agora builda **só o
  macOS** (`.github/workflows/macos-build.yml`, nativo via
  `macos-latest`, sem cross-compile) porque é a única peça que exige
  hardware que a gente não tem de outro jeito. Android/Linux/Windows
  passam a ser buildados no Termux (Android via Buildozer local, os
  outros dois já cobertos pelo `build_all.sh`).
- Removido `.github/workflows/android-build.yml` (a tentativa via
  `ArtemSBulgakov/buildozer-action` falhava por causa de um PPA
  descontinuado no Dockerfile da action, `ppa:openjdk-r/ppa` — nada a
  ver com o nosso código).
- `android/README.md` documenta o comando pra desligar o Gradle Daemon
  no Termux (`org.gradle.daemon=false` no `~/.gradle/gradle.properties`),
  que costuma travar em ambientes com pouca RAM/sem systemd, e o risco
  conhecido (não confirmado) de ferramentas do Android SDK distribuídas
  só em x86_64 não rodando nativamente num Termux aarch64.

## v3.5

- **Bug grave corrigido no `nc-cmd`: ele nunca achava o `nclang` de
  verdade.** A busca antiga só olhava `./nclang` (pasta atual) e depois
  caía pro PATH — então rodar `nc-cmd` de qualquer pasta que não fosse a
  mesma dos binários (ou sem `nclang` instalado globalmente) fazia
  **todo comando falhar silenciosamente** (`nclang: not found` indo pro
  stderr sem destaque nenhum). Era a causa real por trás de "o mkdir não
  faz nada". Corrigido: agora busca ao lado do **executável do próprio
  `nc-cmd`** (via `/proc/self/exe` no Linux/Android, `GetModuleFileName`
  no Windows, `_NSGetExecutablePath` no macOS) — funciona rodando de
  qualquer pasta, mesmo instalado em outro lugar do sistema. Se não
  achar em lugar nenhum, agora avisa bem claro e sai, em vez de falhar
  calado.
- **Novo comando `!pkg! nome_pacote`**: instala pacotes detectando a
  plataforma automaticamente — `pkg` no Android/Termux, `winget` no
  Windows, `brew` no macOS, e no Linux detecta em tempo de execução qual
  gerenciador existe (`apt-get`/`dnf`/`pacman`/`zypper`), usando `sudo`
  só quando não já é root. Testado de ponta a ponta (a instalação real
  chega a tentar baixar o pacote; só falha aqui no sandbox por bloqueio
  de rede, não por bug de lógica).

## v3.6

- **`nclang --open terminal` (e `nuclearcloud --open terminal`)**: o
  shell interativo agora é embutido direto no `nclang`/`nuclearcloud`,
  não precisa mais de um binário `nc-cmd` separado pra uso do dia a dia
  (o `nc-cmd` continua existindo, pra quem preferir isolado). Mesma
  lógica de sessão acumulada (variáveis persistem entre linhas).
- **Auto-inicialização do NuclearCloud OS**: se a pasta `NuclearCloud`
  ainda não existir em `$HOME` (ou `/sdcard` no Termux) quando você abre
  o terminal, o `nc_setup` roda sozinho antes de abrir o shell. Só roda
  uma vez - da segunda vez em diante (pasta já existe) pula direto pro
  shell.
- Corrigido `nc_setup.c`: a detecção Android usava `#ifdef __ANDROID__`,
  que não é definida quando compilado nativamente no Termux (só quando
  cross-compilado via NDK) - trocado pela mesma detecção em tempo de
  execução via `$PREFIX` já usada em outros pontos do compilador.
- Versão do compilador: `3.1` → `3.2`.

# NC Terminal (Android)

App mínimo pro Android: tela preta, letras brancas, um campo de texto — um
terminal de verdade pra rodar código NC Language no celular. É Python
(Kivy) por cima do `nclang` compilado nativo, não uma reimplementação da
linguagem em Python.

## Como funciona

1. `build_native_libs.sh` cross-compila `nclang`, `nc-cmd` e `nc_setup`
   pro Android usando o NDK, gerando `libnclang.so` / `libnccmd.so` /
   `libncsetup.so` em `android/libs/<abi>/`.

   **Por que `.so` e não um binário comum?** A partir do Android 10, o
   sistema só deixa executar arquivos que estão na pasta
   `nativeLibraryDir` do app — e só arquivos terminados em `.so`
   colocados lá pelo empacotador entram nessa pasta. São executáveis
   normais por dentro, só têm nome de biblioteca pra passar pelo Android.

2. `buildozer.spec` empacota o app: pega `main.py` (a interface) +
   `libs/*/*.so` (os binários) e monta o APK via `python-for-android`.

3. `main.py` é a UI: um `Label` preto com texto branco rolável + um
   `TextInput` embaixo, imitando o `nc-cmd`. Cada linha que você manda
   entra na sessão (mesma lógica de manter variáveis vivas entre
   comandos que o `nc-cmd` do terminal já usa) e chama
   `libnclang.so -e "<sessao>"` via `subprocess`.

## Divisão do trabalho: Termux vs GitHub Actions

- **Termux**: builda tudo — Linux/Android nativo (`build_all.sh` na raiz
  do repo), Windows via `llvm-mingw-w64`, e o **APK do Android** via
  Buildozer direto no aparelho (seção abaixo).
- **GitHub Actions**: só o **macOS** (`.github/workflows/macos-build.yml`),
  porque é a única peça que exige de fato um Mac — `macos-latest` roda
  num Mac de verdade, sem cross-compile, sem gambiarra.

### Por que não builda o Android via GitHub Actions também?

Já tentamos. A primeira tentativa usava a action
`ArtemSBulgakov/buildozer-action@v1`, que falhou antes de tocar no nosso
código: a imagem Docker dela tenta instalar o PPA `ppa:openjdk-r/ppa`,
que devolve 404 (parece descontinuado). Em vez de ficar caçando outro
jeito de fazer isso funcionar numa Action, decidimos builda o Android
onde você já sabe que funciona: no seu próprio Termux.

## Buildando o APK direto no Termux

O Buildozer roda no próprio aparelho, mas o **Gradle Daemon** (o processo
Java que ele deixa vivo em background entre builds pra ser mais rápido)
costuma travar/morrer no Termux — desliga ele antes:

```bash
pkg update && pkg upgrade -y
pkg install python git unzip openjdk-17 -y
pip install --upgrade pip
pip install --upgrade buildozer "cython<3.0"

# desliga o Gradle Daemon globalmente (senao trava no Termux)
mkdir -p ~/.gradle
echo "org.gradle.daemon=false" >> ~/.gradle/gradle.properties
echo "org.gradle.jvmargs=-Xmx1536m" >> ~/.gradle/gradle.properties

cd nuclearcloud-nc-language/android
yes | buildozer android debug
```

O APK final fica em `android/bin/*.apk`. Copia pro armazenamento comum
(`cp bin/*.apk /sdcard/`) e instala normal (precisa habilitar "instalar
de fontes desconhecidas", já que não está assinado pra loja).

### Risco que eu não consigo testar/confirmar daqui

Uma parte das ferramentas do Android SDK (`aapt2` principalmente)
historicamente só tinha binário `x86_64`, e não rodava nativamente num
Termux `aarch64` (que é o processador do teu celular). O Google vem
adicionando variante `linux-aarch64` pras `cmdline-tools` mais recentes,
então pode já estar resolvido — mas se o build travar com algo tipo
`aapt2: not found` ou `cannot execute binary file: Exec format error`, é
exatamente isso: o binário baixado é x86_64 e teu Termux é ARM. Nesse
caso me manda o erro que eu vejo uma saída (geralmente rodar via
`proot-distro` com uma distro x86_64 emulada, ou QEMU user-mode, resolve
— mas só vale a pena configurar isso se você bater nesse problema de
verdade).

## Buildando o macOS via GitHub Actions

Só precisa dar push — o workflow `.github/workflows/macos-build.yml`
já está configurado pra rodar sozinho:

```bash
git add .github/workflows/macos-build.yml
git commit -m "build macos nativo via actions"
git push
```

Depois, na aba **Actions** do repositório no GitHub, o workflow
"Build macOS (NC Language)" roda automaticamente num Mac de verdade
(`macos-latest`). Quando terminar, baixa o artefato `nc-language-macos`
— vem com `nclang`, `nc-cmd` e `nc_setup` compilados nativos pro macOS.

Se preferir disparar manualmente sem dar push: aba Actions → "Build
macOS (NC Language)" → "Run workflow".

## O que eu testei de verdade e o que não

✅ Testado: sintaxe do `main.py` (compila sem erro Python), sintaxe do
`build_native_libs.sh` (`bash -n`), YAML dos dois workflows válido.

❌ **Não testado**: o build do APK em si rodando no Termux, o app
rodando num Android de verdade, e o workflow do macOS rodando de
verdade. Não tenho Termux nem Mac neste ambiente pra validar isso fim a
fim — o padrão usado (NDK cross-compile + `.so` fake + Buildozer +
`pyjnius` pra achar `nativeLibraryDir` no Android; `clang` nativo no
macOS) é o jeito correto e documentado de fazer isso, mas só vai ser
validado de verdade quando você rodar. Me manda o log se travar em
alguma coisa.

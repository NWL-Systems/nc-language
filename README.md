# NC Language - NuclearCloud Language

![NC Language](icon.png)

> © 2026 NWL-Systems — Criado por NWL-Systems

NC Language é uma linguagem de programação open source criada pela **NWL-Systems**.

## Instalação do Compilador

```bash
# Baixa o compilador
git clone https://github.com/NWL-Systems/nc-language.git
cd nc-language

# Compila o compilador
clang -o nc_compiler nc_compiler.c

# Testa
./nc_compiler hello.nc
./hello
```

## Hello World

```nc
say = "Hello World!"
```

## Sintaxe Completa

```nc
!# Comentário #!

!# Variáveis #!
!num!    x = 10
!numD!   altura = 1.75
!numF!   preco = 9.99
!fra!    nome = "NuclearCloud"
!sintax! ativo = !A

!# Saída #!
say = "Texto direto"
!! nome

!# Input #!
!fra! resposta
!ask! "Qual seu nome?" -> resposta
!! resposta

!# Condicional #!
!if! ativo == 1
say = "Sistema ativo!"
!else! Sistema inativo

!# Loop #!
!loop! 3 [
say = "Repetindo!"
]


!# While #!
!while! x > 0 [
!! x
x = x - 1
]

!# Função #!
!func! saudacao() [
say = "Ola do NC!"
]

!# Chamar função #!
!jun! saudacao

!# Classe #!
MinhaClasse nClass()[
say = "Dentro da classe!"
]
```

## Tipos

| Sintaxe | Tipo | Exemplo |
|---------|------|---------|
| `!num!` | Inteiro | `!num! x = 10` |
| `!numD!` | Decimal | `!numD! pi = 3.14` |
| `!numF!` | Fração | `!numF! n = 1.5` |
| `!fra!` | Texto | `!fra! nome = "NC"` |
| `!sintax!` | Booleano | `!sintax! ok = !A` |

## Booleanos

| Sintaxe | Valor |
|---------|-------|
| `!A` ou `true` | Verdadeiro |
| `!2` ou `false` | Falso |

## Comandos

| Sintaxe | Descrição |
|---------|-----------|
| `say = "texto"` | Imprime texto |
| `!! variavel` | Imprime variável |
| `!ask! "msg" -> var` | Input do usuário |
| `!if!` / `!else!` / `]` | Condicional |
| `!loop! N [` | Repete N vezes |
| `!while! cond [` | Loop condicional |
| `!func! nome() [` | Declara função |
| `!jun! nome` | Chama função/arquivo |
| `!ret! valor` | Return |
| `!stop!` | Break |
| `!skip!` | Continue |
| `!# comentário` | Comentário |
| `nClass()[` | Classe |

## Licença

Open source — use à vontade em qualquer projeto!
Único requisito: creditar **"NC Language criada por NWL-Systems"**

Veja [LICENSE](LICENSE) e [COPYRIGHT](COPYRIGHT).

## Release 25/05/2026 


Compiladores para Linux e Android em Forma Executavel Criadas 


Windows Em Breve...
VS Code em Breve...

## Release 28/05/2026

 say, !!, !ask!

 !if!, !else!, !elif!, !loop!, !while!

 !func!, nClass()[]

 !use! — pra importar classes, web, NCD etc

 !jun!, !ret!, !stop!, !skip!

 !# comentário

 Matemática básica

 NCD.connection — única biblioteca oficial

 cp /# PASTA ONDE TA O ARQUIVO/nc_compiler.c ~/nc-language/
cd ~/nc-language
clang -o nc nc_compiler.c -lm

cd ~/nc-language

# Cria um programa
cat > ola.nc << 'EOF'
say = "Hello World!"
!fra! nome = "NuclearCloud"
!! nome
EOF

# Compila
./nc ola.nc

# Roda
./ola


NuclearCloud Extensions pode ser feita toda em .nc Veja:

# .ncapp - Aplicativo normal
./nc meuapp.nc meuapp.ncapp

# .ncdevapp - App de desenvolvedor
./nc meuapp.nc terminal.ncdevapp

# .ncprivapp - App especial
./nc meuapp.nc settings.ncprivapp

Pegue este link.             https://github.com/NWL-Systems/nc-os-extensions-files



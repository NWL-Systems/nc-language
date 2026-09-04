!# NC File Manager - NuclearCloud OS #!
!# Tipo: .ncapp #!
!# © 2026 NWL-Systems #!

!fra! opcao
!fra! path = "/sdcard/NuclearCloud"

say = "=== NC File Manager v1.0 ==="
say = "© NWL-Systems"
say = "1. Listar arquivos"
say = "2. Criar pasta"
say = "3. Criar arquivo"
say = "4. Sair"

!ask! "Escolha uma opcao:" -> opcao

!if! opcao == "1"
say = "Listando:"
!ls! path
]

!if! opcao == "2"
!fra! nome_pasta
!ask! "Nome da pasta:" -> nome_pasta
!mkdir! nome_pasta
say = "Pasta criada!"
]

!if! opcao == "3"
!fra! nome_arq
!fra! conteudo
!ask! "Nome do arquivo:" -> nome_arq
!ask! "Conteudo:" -> conteudo
!write! nome_arq, conteudo
say = "Arquivo criado!"
]

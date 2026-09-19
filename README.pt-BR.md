# SizeCheck

**Um CLI pequeno, rápido, seguro e offline para analisar o uso de espaço em disco.**

O SizeCheck examina um diretório (ou um único arquivo) e informa quanto espaço
em disco seus arquivos e diretórios ocupam. É livre de dependências, nunca
escreve nos seus arquivos, nunca segue symlinks de diretório para fora da
árvore analisada e funciona igualmente em Linux, macOS e Windows.

**Leia em outros idiomas:** [English](README.md) · [Español](README.es.md)

---

## Recursos

- **Rápido** — percorre tudo em uma única passada com listas top-N de memória
  constante.
- **Seguro** — totalmente somente leitura. Ele *nunca* modifica, move, cria
  links ou apaga nada; apenas lê metadados.
- **Consciente de symlinks** — symlinks de diretório nunca são seguidos (sem
  loops, sem contagem dupla, sem fugas para fora da árvore); symlinks de
  arquivo só são contabilizados quando o destino permanece dentro da árvore
  analisada.
- **Saída legível por máquina** — `--json` válido para ferramentas e scripts.
- **Zero dependências em tempo de execução** — apenas a biblioteca padrão
  C++20. Nada para baixar ou instalar além do executável que você compila.
- **Multiplataforma** — Linux, macOS e Windows.

---

## Pré-requisitos

Para compilar você precisa apenas de:

- Um compilador com suporte a **C++20** (GCC 11+, Clang 14+, MSVC 2022+,
  Apple Clang).
- **CMake** 3.20 ou mais recente.
- Qualquer gerador suportado pelo CMake: Ninja, Make ou o gerador do Visual
  Studio. Nenhuma biblioteca de terceiros é necessária.

---

## Compilar e instalar

### Linux / macOS

```bash
git clone https://github.com/Victordebrito2293/SizeCheck.git
cd SizeCheck

cmake -S . -B build            # configura
cmake --build build            # compila (Release por padrão)
./build/sizecheck --help       # executa
```

Para instalar no sistema (padrão: `/usr/local`):

```bash
cmake --install build
```

### Windows (Visual Studio)

```bat
git clone https://github.com/Victordebrito2293/SizeCheck.git
cd SizeCheck

cmake -S . -B build
cmake --build build --config Release
build\Release\sizecheck.exe --help
```

> O projeto gera um único executável autocontido. Depois de compilado, você
> pode copiar `sizecheck` para qualquer lugar da sua máquina (ou para outra
> máquina compatível) e executar — não é necessária instalação.

---

## Como usar

```
sizecheck [OPÇÕES] [CAMINHO]
```

`CAMINHO` é o diretório ou arquivo a ser analisado. Quando omitido, o diretório
atual (`.`) é varrido.

| Opção                           | Descrição                                                  |
| ------------------------------- | ---------------------------------------------------------- |
| `-h`, `--help`                  | Mostra a ajuda e sai.                                      |
| `-V`, `--version`               | Mostra a versão e sai.                                     |
| `--top N`                       | Mostra os `N` maiores arquivos e diretórios (padrão: 10).  |
| `--depth N`                     | Varre apenas `N` níveis de diretório abaixo de `CAMINHO` (padrão: ilimitado). `--depth 0` analisa apenas o nível superior. |
| `--exclude NOME`                | Pula qualquer item cujo nome seja `NOME`. Pode ser repetida. |
| `--hidden`                      | Inclui arquivos e diretórios ocultos (prefixo `.`).        |
| `--json`                        | Emite um relatório JSON legível por máquina.               |
| `--no-color`                    | Desativa a saída colorida.                                 |

### Exemplos

```bash
sizecheck .                          # analisa o diretório atual
sizecheck ./projeto --top 20         # mostra os 20 maiores itens
sizecheck ./projeto --depth 3        # não desce mais de 3 níveis
sizecheck ./projeto --exclude node_modules --exclude build
sizecheck ./projeto --hidden         # inclui arquivos ocultos
sizecheck ./projeto --json > relatorio.json
sizecheck ~/Downloads/video.iso      # analisa um único arquivo
```

### Códigos de saída

| Código | Significado                                                    |
| ------ | -------------------------------------------------------------- |
| `0`    | Análise concluída com sucesso.                                 |
| `1`    | A análise falhou (ex.: `CAMINHO` não existe ou não é arquivo/diretório regular). |
| `2`    | Uso inválido da linha de comando.                              |

---

## Saída

### Relatório de texto

```
SizeCheck

Scanning: ./projeto

Files:        12,842
Directories:  1,284
Total size:   3.82 GB

Largest directories:
1. node_modules/src/  842 MB
2. assets/            512 MB

Largest files:
142 MB  assets/video.mp4
91 MB   backups/db.sql.gz

Scan completed in 1.42s
```

### Relatório JSON

```json
{
  "path": "./projeto",
  "files": 12842,
  "directories": 1284,
  "total_bytes": 4103116800,
  "largest_files": [
    { "path": "assets/video.mp4", "bytes": 148897792 }
  ],
  "largest_directories": [
    { "path": "node_modules", "bytes": 883097600 }
  ],
  "errors": 0,
  "scan_duration_ms": 1420
}
```

> Todos os caminhos listados são relativos à raiz analisada e nunca expõem
> caminhos absolutos da máquina. As unidades usam fatores binários
> (1 KB = 1024 B).

---

## Notas de segurança

- **Somente leitura.** O SizeCheck nunca modifica, move, cria links nem apaga
  nada.
- **Arquivos ocultos** são excluídos por padrão (como a convenção do shell);
  use `--hidden` para incluí-los.
- **Symlinks de diretório** nunca são seguidos. Symlinks cujo destino é um
  diretório são totalmente ignorados — sem loops, sem contagem dupla, sem
  fugas para fora da árvore. Symlinks quebrados ou em loop são reportados como
  erros de acesso.
- **Erros de acesso** (ex.: arquivos que você não pode ler) são reportados,
  mas não interrompem a análise.

---

## Executando os testes

```bash
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

A suíte de testes inclui testes unitários de argumentos, varredura, formatação
e relatórios, além de testes ponta a ponta do CLI. Todos rodam em diretórios
temporários descartáveis e nunca tocam em dados reais do usuário.

### Opções de compilação opcionais

| Opção                         | Descrição                                                        |
| ----------------------------- | ---------------------------------------------------------------- |
| `-DSIZECHECK_BUILD_TESTS`     | Compila a suíte de testes (padrão: `ON`).                        |
| `-DSIZECHECK_WERROR`          | Trata avisos do compilador como erros (padrão: `OFF`).           |
| `-DSIZECHECK_SANITIZE`        | Compila com AddressSanitizer + UndefinedBehaviorSanitizer (padrão: `OFF`). |
| `-DSIZECHECK_BUILD_FUZZERS`   | Compila os harnesses libFuzzer; exige Clang + libFuzzer (padrão: `OFF`). |
| `-DSIZECHECK_ENABLE_CLANG_TIDY` | Executa clang-tidy durante a compilação (padrão: `OFF`).       |

---

## Licença

[MIT](LICENSE)
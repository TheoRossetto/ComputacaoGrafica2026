# AV1 - Computação Gráfica
## Selecionando e aplicando transformações em objetos 3D

**Grupo:**
- Bruno Groehs
- Theo Rossetto

---

## Descrição

Aplicação OpenGL que exibe múltiplos modelos 3D na cena com suporte a seleção de objetos e aplicação de transformações (translação, rotação e escala) via teclado.

---

## Pré-requisitos

- [CMake](https://cmake.org/download/) (3.10+)
- [MSYS2 / MinGW-UCRT64](https://www.msys2.org/)
- [Git](https://git-scm.com/downloads)
- GLAD (ver seção abaixo)

---

## Configuração da GLAD

Antes de compilar, os arquivos da GLAD precisam estar presentes no repositório:

1. Acesse [https://glad.dav1d.de/](https://glad.dav1d.de/)
2. Configure: **API:** OpenGL | **Version:** 4.5 | **Profile:** Core | **Language:** C/C++
3. Clique em *Generate* e baixe o zip
4. Copie os arquivos para os diretórios:
   - `glad.h` → `include/glad/`
   - `khrplatform.h` → `include/glad/KHR/`
   - `glad.c` → `Common/`

---

## Como compilar e executar

### Via VS Code (recomendado)

1. Abra a pasta do projeto no VS Code (`File → Open Folder`)
2. `Ctrl+Shift+P` → `CMake: Configure` — selecione o kit **GCC MinGW-UCRT64**
3. `Ctrl+Shift+P` → `CMake: Build`
4. No terminal, execute a partir da pasta `build/`:

```sh
./AV1.exe
```

### Via terminal

```sh
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build . --target AV1
./AV1.exe
```

> O executável deve ser rodado a partir da pasta `build/` para que os caminhos dos modelos 3D sejam resolvidos corretamente.

---

## Controles

| Tecla | Ação |
|---|---|
| `TAB` | Selecionar próximo objeto (cicla pela lista) |
| `R` | Entrar no modo **Rotação** |
| `T` | Entrar no modo **Translação** |
| `S` | Entrar no modo **Escala** |

### Modo Rotação (`R`)

| Tecla | Ação |
|---|---|
| `X` | Ativar/desativar rotação contínua no eixo X |
| `Y` | Ativar/desativar rotação contínua no eixo Y |
| `Z` | Ativar/desativar rotação contínua no eixo Z |

### Modo Translação (`T`)

| Tecla | Ação |
|---|---|
| `W` / `↑` | Mover para frente (-Z) |
| `↓` | Mover para trás (+Z) |
| `A` / `←` | Mover para esquerda (-X) |
| `D` / `→` | Mover para direita (+X) |
| `I` | Mover para cima (+Y) |
| `K` | Mover para baixo (-Y) |

### Modo Escala (`S`)

| Tecla | Ação |
|---|---|
| `]` | Aumentar escala (uniforme ou no eixo selecionado) |
| `[` ou `-` | Diminuir escala (uniforme ou no eixo selecionado) |
| `X` | Selecionar eixo X (pressione novamente para voltar ao uniforme) |
| `Y` | Selecionar eixo Y (pressione novamente para voltar ao uniforme) |
| `Z` | Selecionar eixo Z (pressione novamente para voltar ao uniforme) |

| `ESC` | Fechar a aplicação |
|---|---|

---

## Estrutura do projeto

```
📂 ComputacaoGrafica2026/
├── 📂 src/
│   ├── AV1.cpp          # Atividade Vivencial 1
│   └── M2.cpp           # Desafio Módulo 2 (base)
├── 📂 assets/
│   └── 📂 Modelos3D/    # Arquivos .OBJ utilizados
├── 📂 include/glad/     # Cabeçalhos GLAD
├── 📂 Common/           # glad.c
├── CMakeLists.txt
└── README.md
```

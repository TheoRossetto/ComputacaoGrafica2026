# Computação Gráfica 2026

Bruno Groehs e Theo Rossetto

Repositório das atividades da disciplina de Computação Gráfica. Cada módulo (M2–M6) e a atividade vivencial (AV1) são executáveis independentes.

---

## Setup

### Dependências

- CMake 3.10+ — https://cmake.org/download/
- MSYS2 com GCC ucrt64 — https://www.msys2.org/
- Git (necessário para o CMake baixar as libs automaticamente)
- GLFW, GLM e stb são baixados automaticamente pelo CMake na primeira configuração

A única dependência que precisa ser configurada manualmente é a GLAD:

1. Acesse https://glad.dav1d.de/
2. Selecione: Language C/C++, API gl versão 4.5, Profile Core
3. Clique em Generate e baixe o zip
4. Copie os arquivos:
   - `glad.h` para `include/glad/`
   - `khrplatform.h` para `include/glad/KHR/`
   - `glad.c` para `common/`

### Compilação

O caminho do repositório tem o caractere `ç` em `computaçaoGrafica`, o que quebra o mingw32-make ao tentar referenciar os arquivos fonte. O jeito que funciona é copiar os arquivos para um caminho sem caracteres especiais antes de compilar:

```bash
SRC="c:/Users/<usuario>/Documents/Applications/computaçaoGrafica/ComputacaoGrafica2026"
mkdir -p /c/tmp/cg_src
cp -r "$SRC/assets" "$SRC/src" "$SRC/include" "$SRC/common" "$SRC/CMakeLists.txt" /c/tmp/cg_src/

PATH="/c/msys64/ucrt64/bin:$PATH"
cmake -S /c/tmp/cg_src -B /c/tmp/cg_build \
      -G "MinGW Makefiles" \
      -DCMAKE_C_COMPILER="/c/msys64/ucrt64/bin/gcc.exe" \
      -DCMAKE_CXX_COMPILER="/c/msys64/ucrt64/bin/g++.exe"

cd /c/tmp/cg_build
mingw32-make M6
```

A primeira vez que o CMake roda ele clona e compila o GLFW, o que pode demorar alguns minutos.

### Execução

```bash
cd /c/tmp/cg_build
./M6.exe
```

O executável precisa ser rodado a partir da pasta `build/` porque os modelos são referenciados como `../assets/Modelos3D/`.

---

## Assets

Todos os modelos foram criados e exportados por nós no Blender 4.3.0. São primitivas padrão do próprio Blender — nada foi baixado de repositório externo.

**Suzanne.obj** — o macaco padrão do Blender (Add → Mesh → Monkey), exportado como .obj com UV unwrap padrão. A textura `Suzanne.png` foi gerada no próprio Blender.

**SuzanneSubdiv1.obj** — a mesma Suzanne com o modificador Subdivision Surface nível 1 aplicado antes do export. Usa a mesma textura.

**Cube.obj** — cubo primitivo padrão (Add → Mesh → Cube), sem textura.

**pixelWall.png** — textura de parede pixelada gerada proceduralmente, usada em exercícios anteriores.

---

## Referências

**LearnOpenGL** (Joey de Vries) — principal referência durante o semestre inteiro, especialmente os capítulos de transformações, câmera FPS, iluminação Phong e materiais.
https://learnopengl.com/

**Especificação do formato .obj e .mtl** — consultada para implementar o parser manual em `loadSimpleOBJ`.
http://paulbourke.net/dataformats/obj/

**Algoritmo de De Casteljau** — base do sistema de trajetórias Bézier implementado no M6.
https://en.wikipedia.org/wiki/De_Casteljau%27s_algorithm

**Arc-length parameterization** — técnica usada para manter velocidade constante na curva Bézier.
https://pomax.github.io/bezierinfo/#tracing

**Documentação do GLFW** — https://www.glfw.org/docs/latest/

**Documentação da GLM** — https://glm.g-truc.net/0.9.9/api/index.html

**OpenGL Reference Pages** — https://registry.khronos.org/OpenGL-Refpages/gl4/

Além disso, os slides e o código-base fornecidos pelo professor foram a referência de partida para os primeiros módulos.

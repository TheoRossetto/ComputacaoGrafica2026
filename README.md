# Computação Gráfica 2026

Bruno Groehs e Theo Rossetto

Repositório das atividades da disciplina de Computação Gráfica do semestre. Cada módulo (M2 até M6) e a atividade vivencial (AV1) são executáveis independentes, então dá pra rodar qualquer um sem depender dos outros.

O M6 especificamente tem câmera FPS, iluminação Phong 3-pontos (key/fill/back), trajetórias Bézier por objeto com De Casteljau, e um HUD de ajuda na tela.

---

## Setup

### Dependências

- CMake 3.10 ou superior — https://cmake.org/download/
- MSYS2 com GCC ucrt64 — https://www.msys2.org/
- Git (o CMake usa pra baixar as dependências automaticamente)

GLFW, GLM, stb_image e stb_easy_font são baixados e compilados pelo próprio CMake na primeira vez, sem precisar instalar nada. A única dependência que precisa de atenção manual é a GLAD:

1. Acesse https://glad.dav1d.de/
2. Selecione: Language C/C++, API gl versão 4.5, Profile Core
3. Gera e baixa o zip
4. Coloca os arquivos nos lugares certos:
   - `glad.h` → `include/glad/`
   - `khrplatform.h` → `include/glad/KHR/`
   - `glad.c` → `common/`

### Compilação

O caminho do repositório tem `ç` em `computaçaoGrafica`, o que quebra o mingw32-make na hora de referenciar os arquivos fonte. A solução que funciona é copiar tudo pra um caminho sem caracteres especiais antes de compilar:

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

Na primeira vez o CMake vai clonar e compilar o GLFW, então pode demorar alguns minutos.

### Execução

```bash
cd /c/tmp/cg_build
./M6.exe
```

O executável precisa ser rodado de dentro da pasta de build porque os modelos são referenciados com caminho relativo (`../assets/Modelos3D/`).

---

## Assets

Todos os modelos foram feitos e exportados por nós no Blender 4.3.0. São primitivas padrão do Blender mesmo, nada baixado de fora.

**Suzanne.obj** — o macaco clássico do Blender (Add → Mesh → Monkey), exportado como .obj com UV unwrap padrão. A textura `Suzanne.png` foi gerada no próprio Blender.

**SuzanneSubdiv1.obj** — a mesma Suzanne com Subdivision Surface nível 1 aplicado antes do export. Usa a mesma textura.

**Cube.obj** — cubo padrão, sem textura.

**pixelWall.png** — textura de parede pixelada gerada proceduralmente, usada em exercícios anteriores.

---

## Referências

**LearnOpenGL** (Joey de Vries) — principal referência do semestre todo, especialmente os capítulos de transformações, câmera FPS, iluminação Phong e materiais.
https://learnopengl.com/

**Especificação do .obj e .mtl** — consultada pra implementar o parser em `loadSimpleOBJ`.
http://paulbourke.net/dataformats/obj/

**Algoritmo de De Casteljau** — base da avaliação da curva Bézier no M6.
https://en.wikipedia.org/wiki/De_Casteljau%27s_algorithm

**Arc-length parameterization** — técnica usada pra estimar o comprimento da curva e manter velocidade constante.
https://pomax.github.io/bezierinfo/#tracing

**Documentação do GLFW** — https://www.glfw.org/docs/latest/

**Documentação da GLM** — https://glm.g-truc.net/0.9.9/api/index.html

**OpenGL Reference Pages** — https://registry.khronos.org/OpenGL-Refpages/gl4/

Os slides e o código-base do professor foram ponto de partida pra maioria dos módulos.

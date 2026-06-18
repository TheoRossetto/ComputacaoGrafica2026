# M6.cpp — Guia de Explicação do Código

> **Para a avaliação oral:** este documento mapeia cada tópico que pode ser perguntado a um trecho específico do arquivo `src/M6.cpp`, com número de linha e a matemática/lógica por trás.

---

## 1. Visão geral da arquitetura

O programa segue o fluxo clássico de uma aplicação OpenGL com game loop:

```
main()
 ├── inicialização (GLFW, GLAD, shaders, VAO/VBO, modelos)
 ├── loop principal (while !shouldClose)
 │    ├── poll events
 │    ├── atualiza câmera / trajetórias / transformações
 │    ├── monta matrizes Model, View, Projection
 │    ├── passa uniforms para a GPU
 │    └── draw calls (objetos 3D → waypoints/Bézier → HUD)
 └── cleanup
```

Há quatro programas de shader distintos:
| Shader | Uso | Linhas |
|---|---|---|
| `vertexShaderSource` / `fragmentShaderSource` | Objetos 3D com Phong | 77–155 |
| `lineVertexShaderSource` / `lineFragmentShaderSource` | Curva Bézier e pontos de controle | 175–192 |
| `hudVertexShaderSource` / `hudFragmentShaderSource` | Overlay 2D (HUD) | 160–172 |

---

## 2. Parser do arquivo de cena (.obj e .mtl)

**Função:** `loadSimpleOBJ()` — linha **1460**

### 2.1 Leitura do .obj

```
linha 1476  while (getline(file, line))          ← lê linha a linha
linha 1478      istringstream ss(line)
linha 1480      ss >> word                        ← lê o token inicial

linha 1482      "mtllib" → guarda nome do .mtl
linha 1483      "v"      → posição xyz → positions[]
linha 1484      "vt"     → coordenada uv → texCoords[]
linha 1485      "vn"     → normal xyz    → normals[]
linha 1486      "f"      → face (pode ser triângulo, quad ou n-gon)
```

### 2.2 Parser de face (formato `v/vt/vn`)

**Linhas 1501–1509**

Cada token de face pode ser `1/2/3`, `1//3` (sem UV) ou `1` (só posição). O código usa `getline(si, idx, '/')` para separar os três índices por `/`. Índices negativos (relativos ao fim da lista) também são resolvidos na linha 1496–1498.

### 2.3 Fan-triangulation (n-gons → triângulos)

**Linhas 1533–1538**

Um quad `[0,1,2,3]` vira dois triângulos: `(0,1,2)` e `(0,2,3)`. Qualquer n-gon convexo é convertido pelo mesmo padrão de fan.

### 2.4 Leitura do .mtl

**Linhas 1543–1560**

```
"Ka"     → mat.Ka   (coeficiente ambiente)
"Kd"     → mat.Kd   (coeficiente difuso)
"Ks"     → mat.Ks   (coeficiente especular)
"Ns"     → mat.Ns   (expoente de brilho)
"map_Kd" → texturePath (caminho da textura)
```

### 2.5 Upload para a GPU (VAO/VBO)

**Linhas 1563–1576**

O buffer intercalado tem stride de **8 floats por vértice**:
```
[x, y, z,  u, v,  nx, ny, nz]
 ↑ attrib 0  ↑ attrib 1  ↑ attrib 2
 offset 0    offset 12   offset 20
```

---

## 3. Matrizes de transformação (Model e View)

### 3.1 Matriz Model

**Linhas 627–632** (dentro do loop de renderização)

```cpp
glm::mat4 model = glm::mat4(1.0f);           // identidade
model = glm::translate(model, o.position);   // T: move o objeto no mundo
model = glm::scale(model, o.scale);          // S: escala uniforme
model = glm::rotate(model, o.rotAngle, eixo); // R: rotação no eixo escolhido
```

A ordem importa — em GLM as matrizes são pré-multiplicadas da direita para a esquerda, então a transformação aplicada primeiro ao vértice é a última escrita: **R × S × T × v**.

### 3.2 Como a Model chega ao vertex shader

**Linha 634**
```cpp
glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
```
Isso envia os 16 floats da matriz 4×4 para o uniform `model` do shader.

### 3.3 Uso no vertex shader

**Linha 93–94** (dentro do vertex shader GLSL)
```glsl
vec4 worldPos = model * vec4(position, 1.0);      // posição no espaço mundo
gl_Position   = projection * view * worldPos;      // clip space
```

A posição do vértice sai do espaço local (`position`) para o espaço mundo via `model`, depois para o espaço de câmera via `view`, e finalmente para clip space via `projection`.

### 3.4 Normal matrix

**Linha 97** (vertex shader)
```glsl
vNormal = mat3(transpose(inverse(model))) * normal;
```
A normal **não** pode ser transformada direto pela model matrix quando há escala não-uniforme — a transposta da inversa corrige a direção da normal para o espaço mundo.

### 3.5 Matriz View

**Linha 222** (classe `Camera`)
```cpp
return glm::lookAt(position, position + front, up);
```
`lookAt` constrói a matriz View que transforma do espaço mundo para o espaço da câmera. É enviada ao shader na **linha 574**.

### 3.6 Matriz Projection

**Linha 227**
```cpp
return glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
```
Cria a projeção perspectiva com campo de visão `fov`, proporção `aspect`, near plane 0.1 e far plane 100. Enviada ao shader na **linha 575**.

### 3.7 Câmera — cálculo do vetor front (yaw/pitch)

**Linhas 265–268** (`Camera::updateVectors`)
```cpp
f.x = cos(radians(yaw)) * cos(radians(pitch));
f.y = sin(radians(pitch));
f.z = sin(radians(yaw)) * cos(radians(pitch));
front = normalize(f);
```
Converte os ângulos de Euler (yaw = rotação horizontal, pitch = rotação vertical) em um vetor de direção unitário em coordenadas esféricas.

---

## 4. Passagem de uniforms para o shader

### 4.1 Obtendo os locations

**Linhas 541–550** (após o loop de carregamento de modelos)
```cpp
GLint modelLoc  = glGetUniformLocation(shader, "model");
GLint KaLoc     = glGetUniformLocation(shader, "Ka");
GLint viewLoc   = glGetUniformLocation(shader, "view");
// ... etc
```
`glGetUniformLocation` retorna um inteiro que identifica a variável uniform no programa de shader. Deve ser chamado depois do `glLinkProgram`.

### 4.2 Enviando por frame

| Uniform | Chamada | Linha |
|---|---|---|
| `model` (mat4) | `glUniformMatrix4fv` | 634 |
| `view` (mat4) | `glUniformMatrix4fv` | 574 |
| `projection` (mat4) | `glUniformMatrix4fv` | 575 |
| `Ka`, `Kd`, `Ks` (vec3) | `glUniform3fv` | 635–637 |
| `Ns` (float) | `glUniform1f` | 638 |
| `camPos` (vec3) | `glUniform3fv` | 576 |
| `useTexture` (int) | `glUniform1i` | 649 ou 654 |

### 4.3 Uniforms das luzes (3-point lighting)

**Função `uploadLights()`** — linha **929**

```cpp
for (int i = 0; i < 3; ++i) {
    glUniform3fv(glGetUniformLocation(shader, "lightPos[i]"),   ...);
    glUniform3fv(glGetUniformLocation(shader, "lightColor[i]"), ...);
    glUniform1i (glGetUniformLocation(shader, "lightOn[i]"),    ...);
}
```

Cada fonte de luz tem posição, cor e flag on/off. As teclas `1`, `2`, `3` alteram `lights[i].on` (linha **839**), e `uploadLights` sincroniza com a GPU todo frame (linha **579**).

---

## 5. Iluminação Phong no Fragment Shader

**Linhas 102–155** (string `fragmentShaderSource`)

### 5.1 Cor base

**Linha 129**
```glsl
vec3 baseColor = (useTexture == 1) ? vec3(texture(texBuff, texCoord)) : objectColor;
```
Se há textura ativa, amostra o texel; caso contrário usa a cor sólida do objeto.

### 5.2 Vetores necessários

**Linhas 131–132**
```glsl
vec3 N = normalize(vNormal);           // normal do fragmento (espaço mundo)
vec3 V = normalize(camPos - fragPos);  // vetor para a câmera
```

### 5.3 Componente Ambiente

**Linha 135**
```glsl
vec3 result = Ka * vec3(0.25) * baseColor;
```
Luz ambiente global = coeficiente Ka × intensidade ambiente (0.25) × cor base. Não depende de nenhuma fonte de luz.

### 5.4 Loop pelas 3 fontes de luz

**Linhas 138–151**

Para **cada luz ativa**:

**Difuso** (linha 142–144)
```glsl
vec3  L    = normalize(lightPos[i] - fragPos);   // vetor para a luz
float diff = max(dot(N, L), 0.0);                // cos do ângulo N·L
vec3  diffuse = Kd * diff * lightColor[i];
```
Lei de Lambert: a intensidade difusa é proporcional ao cosseno entre a normal e a direção da luz. `max(..., 0)` evita valores negativos (luz vindo de trás).

**Especular** (linhas 146–148)
```glsl
vec3  R    = normalize(reflect(-L, N));           // vetor de reflexão
float spec = pow(max(dot(R, V), 0.0), Ns);        // cos^Ns do ângulo R·V
vec3  specular = Ks * spec * lightColor[i];
```
Modelo de Phong: o brilho especular depende do cosseno elevado a `Ns` entre o vetor de reflexão `R` e o vetor para a câmera `V`. Quanto maior o `Ns`, mais concentrado e "plástico" o brilho.

**Acumulação** (linha 150)
```glsl
result += diffuse * baseColor + specular;
```
A componente difusa é modulada pela cor do objeto; a especular não (representando reflexo da luz, não da superfície).

### 5.5 Onde estão Ka, Kd, Ks, Ns?

- **Lidos do .mtl**: linhas 1554–1557 em `loadSimpleOBJ`
- **Armazenados em**: struct `Material` (linha 278)
- **Enviados à GPU**: linhas 635–638 a cada draw call
- **Usados no shader**: linhas 114–117 (declarações) e 135–148 (cálculo)

### 5.6 Como demonstrar durante a avaliação

- Pressione **M** para desligar a textura — o objeto fica com a cor sólida, mostrando o efeito puro de Ka/Kd/Ks
- Pressione **1**, **2**, **3** para ligar/desligar cada fonte de luz individualmente e ver a contribuição de cada componente
- O Cube.obj não tem .mtl — usa valores padrão `Ka=0.2, Kd=0.8, Ks=0.5, Ns=32`

---

## 6. Câmera — navegação

### 6.1 Movimento (WASD)

**Linhas 230–239** (`Camera::processKeyboard`)

```cpp
float step = speed * dt;
if (W) position += step * flatFront;   // frente projetada no plano XZ
if (S) position -= step * flatFront;
if (A) position -= step * right;
if (D) position += step * right;
```

`flatFront` é o vetor `front` com Y=0 normalizado — permite mover no plano horizontal sem subir/descer ao olhar para cima. `right = cross(front, up)` é calculado na linha 233.

### 6.2 Rotação com o mouse

**Linhas 242–253** (`Camera::processMouse`)

```cpp
float xoffset = (xpos - lastX) * sensitivity;   // delta horizontal → yaw
float yoffset = (lastY - ypos) * sensitivity;    // delta vertical   → pitch
yaw   += xoffset;
pitch = clamp(pitch + yoffset, -89°, 89°);       // evita gimbal lock extremo
```

O pitch é limitado a ±89° para evitar que a câmera "vire de cabeça para baixo".

### 6.3 Zoom (scroll)

**Linha 258**: reduz/aumenta o FOV. Quanto menor o FOV, mais "tele" é o efeito; quanto maior, mais "grande-angular".

---

## 7. Trajetória — Curva de Bézier (De Casteljau)

### 7.1 Algoritmo de De Casteljau

**Linhas 306–321** (`Trajectory::evalBezier`)

```cpp
vector<glm::vec3> pts(waypoints);   // cópia dos pontos de controle
int sz = n;
while (sz > 1) {
    for (int i = 0; i < sz - 1; ++i)
        pts[i] = glm::mix(pts[i], pts[i+1], param);  // interpolação linear
    --sz;
}
return pts[0];
```

A cada passagem do loop externo, o número de pontos diminui em 1. `glm::mix(a, b, t) = a*(1-t) + b*t` é a interpolação linear entre dois pontos. Após `n-1` passagens, sobra um único ponto: o ponto na curva para o parâmetro `t ∈ [0,1]`.

### 7.2 Velocidade constante (comprimento de arco)

**Linhas 323–338** (`Trajectory::recomputeLength`)

O parâmetro `t` da Bézier **não** distribui velocidade uniformemente — a curva pode ser mais rápida em algumas regiões. Para corrigir isso, o código estima o comprimento total da curva amostrando 128 pontos e somando os segmentos:

```
arcLen = Σ |evalBezier(i/128) - evalBezier((i-1)/128)|
```

**Linha 346** (`advance`):
```cpp
t += dt * TRAJ_SPEED / arcLen;
```

Dividir pela `arcLen` normaliza o incremento de `t` para que o objeto percorra a mesma distância por segundo independentemente do comprimento da curva.

### 7.3 Ciclo e pausa/retoma

**Linha 347**: `if (t >= 1.0) t = fmod(t, 1.0)` — ao atingir o final, volta ao início ciclicamente.

**Linha 873** (`key_callback`, tecla C): apenas alterna `obj.traj.active` sem resetar `t` — comportamento de pause/resume preservando a posição atual na curva.

### 7.4 Visualização

**Linhas 699–715**: para cada objeto com waypoints, 64 pontos são amostrados via `evalBezier` e enviados como `GL_LINE_LOOP` (curva suave em amarelo). O polígono de controle (cage) é desenhado em laranja como `GL_LINE_STRIP`.

---

## 8. Seleção e Transformação Interativa

### 8.1 Seleção (TAB)

**Linha 783**: `activeObj = (activeObj + 1) % objects.size()` — cicla pelo vetor de objetos.

### 8.2 Translação (modo T + setas/I/K)

**Linhas 594–600**: modifica `sel.position` a cada frame, escalado por `dt` para ser frame-rate independente.

### 8.3 Rotação (modo R + X/Y/Z)

**Linhas 810–812**: ativa um flag `rotX`, `rotY` ou `rotZ`. No loop de renderização, **linha 616**:
```cpp
o.rotAngle += ROT_SPEED * dt;   // incrementa ângulo continuamente
```
E na linha 630–632, `glm::rotate` usa o ângulo acumulado no eixo ativo.

### 8.4 Escala (modo F + ]/[)

**Linhas 603–610**: modifica `sel.scale` (vec3 uniforme), com mínimo `SCALE_MIN = 0.05`.

---

## 9. Materiais e Texturas

### 9.1 Carregamento da textura

**Função `loadTexture()`** — linha **1142**

Usa `stb_image` para decodificar PNG/JPG e `glTexImage2D` para enviar para a GPU. `stbi_set_flip_vertically_on_load(true)` (linha 1154) corrige o eixo Y — OpenGL espera a origem no canto inferior esquerdo, PNG usa canto superior esquerdo.

### 9.2 Toggle textura/cor (tecla M)

**Linha 824**: alterna `obj.showTexture`.

**Linha 646–657** (render loop): decide o que enviar ao shader:
```cpp
bool useTex = (o.texID != 0 && o.showTexture);
if (useTex)  glUniform1i(useTextureLoc, 1);   // usa texture()
else         glUniform1i(useTextureLoc, 0);   // usa objectColor
```

---

## 10. HUD de overlay 2D

**Função `drawHUD()`** — linha **1244**

### 10.1 Projeção ortogonal

**Linha 1247**:
```cpp
glm::mat4 ortho = glm::ortho(0, WIDTH, HEIGHT, 0, -1, 1);
```
Mapeia pixels diretamente para NDC: (0,0) = canto superior esquerdo, (WIDTH, HEIGHT) = canto inferior direito. Sem perspectiva — objetos 2D não distorcem com a distância.

### 10.2 Blending para semi-transparência

**Linhas 1250–1251**:
```cpp
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```
Fórmula: `resultado = src.rgb * src.a + dst.rgb * (1 - src.a)`. Alpha 0.88 do painel = 88% opaco.

### 10.3 Texto com stb_easy_font

**Função `hudText()`** — linha **1200**

`stb_easy_font_print` gera quads (4 vértices cada) em um buffer de bytes. O código extrai apenas X e Y de cada vértice (os outros campos são Z e RGBA que ignoramos), escala e posiciona os quads, converte para triângulos com um IBO, e chama `glDrawElements`.

---

## 11. Tabela rápida de referência para a avaliação

| Pergunta | Onde no código |
|---|---|
| Onde o .obj é parseado? | `loadSimpleOBJ()` linha 1460 |
| Onde o .mtl é lido (Ka, Kd, Ks, Ns)? | Linhas 1543–1557 |
| Onde a matriz Model é montada? | Linhas 627–632 |
| Onde Model é enviada à GPU? | Linha 634 |
| Onde View é calculada? | `Camera::getViewMatrix()` linha 222 |
| Onde View é enviada à GPU? | Linha 574 |
| Onde Projection é calculada? | `Camera::getProjectionMatrix()` linha 227 |
| Onde Projection é enviada à GPU? | Linha 575 |
| Onde Ka, Kd, Ks, Ns são enviados à GPU? | Linhas 635–638 |
| Onde a componente ambiente é calculada? | Linha 135 do fragment shader |
| Onde a componente difusa é calculada? | Linhas 142–144 do fragment shader |
| Onde a componente especular é calculada? | Linhas 146–148 do fragment shader |
| Onde as luzes são toggled? | `key_callback` linha 839 |
| Onde as luzes são enviadas à GPU? | `uploadLights()` linha 929 |
| Onde a normal matrix é calculada? | Linha 97 do vertex shader |
| Onde o vetor front da câmera é calculado? | `Camera::updateVectors()` linha 265 |
| Onde De Casteljau é implementado? | `Trajectory::evalBezier()` linha 306 |
| Onde o comprimento de arco é calculado? | `Trajectory::recomputeLength()` linha 324 |
| Onde a trajetória avança? | `Trajectory::advance()` linha 341 |
| Onde o toggle de textura ocorre (CPU)? | `key_callback` linha 824 |
| Onde o toggle de textura ocorre (GPU)? | Fragment shader linha 129 |

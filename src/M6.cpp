// M6 - trajetorias bezier
// camera: WASD + mouse + scroll
// TAB pra trocar objeto, T/R/F = translacao/rotacao/escala
// X Y Z escolhe o eixo da rotacao
// M liga/desliga textura
// 1 2 3 ligam/desligam as 3 luzes (key, fill, back)
// P = adiciona ponto na curva, C = play/pause, BACKSPACE remove ultimo
// L salva, O carrega trajectories.txt
// H mostra/esconde ajuda, ESC sai

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace std;

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_EASY_FONT_IMPLEMENTATION
#include <stb_easy_font.h>


// ─── Constantes ──────────────────────────────────────────────────────────────

const GLuint WIDTH  = 1000;
const GLuint HEIGHT = 800;

const string MODELS_DIR    = "../assets/Modelos3D/";
const string TRAJ_FILENAME = "trajectories.txt";

const float TRANSLATE_SPEED = 2.5f;
const float SCALE_SPEED     = 1.0f;
const float ROT_SPEED       = 1.5f;
const float SCALE_MIN       = 0.05f;
const float TRAJ_SPEED      = 1.5f;   // velocidade ao longo da curva

const int BEZIER_SAMPLES = 64;  // qtas amostras pra desenhar a curva


// ─── Shaders ─────────────────────────────────────────────────────────────────

// shader principal (phong com 3 luzes)
const GLchar* vertexShaderSource = R"glsl(
#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texc;
layout(location = 2) in vec3 normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 texCoord;
out vec3 vNormal;
out vec3 fragPos;

void main()
{
    vec4 worldPos = model * vec4(position, 1.0);
    gl_Position   = projection * view * worldPos;
    fragPos       = vec3(worldPos);
    texCoord      = texc;
    vNormal = mat3(transpose(inverse(model))) * normal;
}
)glsl";

// fragment shader - phong com 3 luzes (key/fill/back)
const GLchar* fragmentShaderSource = R"glsl(
#version 450
in vec2 texCoord;
in vec3 vNormal;
in vec3 fragPos;

uniform sampler2D texBuff;
uniform int       useTexture;
uniform vec3      objectColor;

uniform vec3  camPos;

uniform vec3  Ka;
uniform vec3  Kd;
uniform vec3  Ks;
uniform float Ns;

// 3 luzes
uniform vec3 lightPos[3];
uniform vec3 lightColor[3];
uniform int  lightOn[3];

out vec4 color;

void main()
{
    vec3 baseColor = (useTexture == 1) ? vec3(texture(texBuff, texCoord)) : objectColor;

    vec3 N = normalize(vNormal);
    vec3 V = normalize(camPos - fragPos);

    // ambient
    vec3 result = Ka * vec3(0.25) * baseColor;

    // soma cada luz que estiver ligada
    for (int i = 0; i < 3; ++i)
    {
        if (lightOn[i] == 0) continue;

        vec3  L    = normalize(lightPos[i] - fragPos);
        float diff = max(dot(N, L), 0.0);
        vec3  diffuse = Kd * diff * lightColor[i];

        vec3  R    = normalize(reflect(-L, N));
        float spec = pow(max(dot(R, V), 0.0), Ns);
        vec3  specular = Ks * spec * lightColor[i];

        result += diffuse * baseColor + specular;
    }

    color = vec4(result, 1.0);
}
)glsl";

// ─── Shaders 2D do HUD ───────────────────────────────────────────────────────
// coords em pixels, (0,0) = canto sup esq. matriz ortho vem como uniform.
const GLchar* hudVertexShaderSource = R"glsl(
#version 450
layout(location = 0) in vec2 pos;
uniform mat4 ortho;
void main() { gl_Position = ortho * vec4(pos, 0.0, 1.0); }
)glsl";

const GLchar* hudFragmentShaderSource = R"glsl(
#version 450
uniform vec4 hudColor;
out vec4 color;
void main() { color = hudColor; }
)glsl";

// shader simples pros pontos e curva
const GLchar* lineVertexShaderSource = R"glsl(
#version 450
layout(location = 0) in vec3 position;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    gl_Position = projection * view * vec4(position, 1.0);
    gl_PointSize = 8.0;
}
)glsl";

const GLchar* lineFragmentShaderSource = R"glsl(
#version 450
uniform vec3 lineColor;
out vec4 color;
void main() { color = vec4(lineColor, 1.0); }
)glsl";


// ─── Câmera ──────────────────────────────────────────────────────────────────

class Camera
{
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;

    float yaw, pitch, fov, speed, sensitivity;
    bool  firstMouse;
    float lastX, lastY;

    Camera(glm::vec3 startPos, float startYaw = -90.0f, float startPitch = 0.0f)
        : position(startPos)
        , yaw(startYaw), pitch(startPitch)
        , fov(45.0f), speed(5.0f), sensitivity(0.05f)
        , firstMouse(true)
        , lastX((float)WIDTH / 2.0f), lastY((float)HEIGHT / 2.0f)
    {
        front = glm::vec3(0.0f, 0.0f, -1.0f);
        up    = glm::vec3(0.0f, 1.0f,  0.0f);
        updateVectors();
    }

    glm::mat4 getViewMatrix() const
    {
        return glm::lookAt(position, position + front, up);
    }

    glm::mat4 getProjectionMatrix(float aspect) const
    {
        return glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
    }

    void processKeyboard(GLFWwindow* window, float dt)
    {
        float step  = speed * dt;
        glm::vec3 right = glm::normalize(glm::cross(front, up));
        glm::vec3 flatFront = glm::normalize(glm::vec3(front.x, 0.0f, front.z));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) position += step * flatFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) position -= step * flatFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) position -= step * right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) position += step * right;
    }

    void processMouse(double xpos, double ypos)
    {
        if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }

        float xoffset = ((float)xpos - lastX) * sensitivity;
        float yoffset = (lastY - (float)ypos) * sensitivity;
        lastX = (float)xpos;
        lastY = (float)ypos;

        yaw   += xoffset;
        pitch  = glm::clamp(pitch + yoffset, -89.0f, 89.0f);
        updateVectors();
    }

    void processScroll(double yoffset)
    {
        fov = glm::clamp(fov - (float)yoffset, 1.0f, 45.0f);
    }

private:
    void updateVectors()
    {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);

        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
        up = glm::normalize(glm::cross(right, front));
    }
};


// ─── Estruturas de dados ──────────────────────────────────────────────────────

struct Material
{
    glm::vec3 Ka = glm::vec3(0.2f);
    glm::vec3 Kd = glm::vec3(0.8f);
    glm::vec3 Ks = glm::vec3(0.5f);
    float     Ns = 32.0f;
};

struct LightSource
{
    glm::vec3 pos;
    glm::vec3 color;
    bool      on;
    string    name;
};

// curva de bezier - os waypoints sao os pontos de controle
// t vai de 0 a 1 e fica ciclando
// uso de casteljau pra avaliar
struct Trajectory
{
    vector<glm::vec3> waypoints;
    bool  active  = false;
    float t       = 0.0f;
    float arcLen  = 1.0f;   // cache do comprimento
    bool  dirty   = true;   // se true, recalcula arcLen no proximo advance

    // de casteljau
    glm::vec3 evalBezier(float param) const
    {
        int n = (int)waypoints.size();
        if (n == 0) return glm::vec3(0.0f);
        if (n == 1) return waypoints[0];

        vector<glm::vec3> pts(waypoints);
        int sz = n;
        while (sz > 1)
        {
            for (int i = 0; i < sz - 1; ++i)
                pts[i] = glm::mix(pts[i], pts[i + 1], param);
            --sz;
        }
        return pts[0];
    }

    // estima o comprimento amostrando t
    void recomputeLength(int samples = 128)
    {
        arcLen = 0.0f;
        if (waypoints.size() < 2) { dirty = false; return; }

        glm::vec3 prev = evalBezier(0.0f);
        for (int i = 1; i <= samples; ++i)
        {
            glm::vec3 cur = evalBezier((float)i / samples);
            arcLen += glm::length(cur - prev);
            prev = cur;
        }
        arcLen = max(arcLen, 1e-5f);
        dirty  = false;
    }

    // anda dt segundos e retorna a posicao
    glm::vec3 advance(float dt)
    {
        if (waypoints.size() < 2) return glm::vec3(0.0f);
        if (dirty) recomputeLength();

        t += dt * TRAJ_SPEED / arcLen;
        if (t >= 1.0f) t = fmod(t, 1.0f);

        return evalBezier(t);
    }
};

struct OBJModel
{
    GLuint    VAO        = 0;
    GLuint    VBO        = 0;   // guardado pra liberar no final
    GLuint    texID      = 0;
    int       nVertices  = 0;
    glm::vec3 position   = glm::vec3(0.0f);
    glm::vec3 scale      = glm::vec3(1.0f);
    float     rotAngle   = 0.0f;
    bool      rotX = false, rotY = false, rotZ = false;
    glm::vec3 color      = glm::vec3(1.0f);
    string    name;
    Material  mat;
    bool      showTexture = true;  // tecla M

    Trajectory traj;
};


// ─── Estado global ────────────────────────────────────────────────────────────

Camera* gCamera = nullptr;

enum TransformMode { MODE_TRANSLATE, MODE_ROTATE, MODE_SCALE };
TransformMode currentMode = MODE_TRANSLATE;

vector<OBJModel> objects;
int              activeObj = 0;
bool             keys[1024] = {};
bool             showHelp   = true;   // tecla H

// 3 luzes (key/fill/back)
LightSource lights[3] = {
    { glm::vec3( 5.0f, 6.0f,  5.0f), glm::vec3(1.00f, 0.95f, 0.90f), true,  "Key"  },
    { glm::vec3(-4.0f, 3.0f,  4.0f), glm::vec3(0.40f, 0.50f, 0.80f), true,  "Fill" },
    { glm::vec3( 0.0f, 4.0f, -6.0f), glm::vec3(0.70f, 0.60f, 0.90f), true,  "Back" },
};


// ─── Declarações ─────────────────────────────────────────────────────────────

void   key_callback   (GLFWwindow*, int, int, int, int);
void   mouse_callback (GLFWwindow*, double, double);
void   scroll_callback(GLFWwindow*, double, double);
GLuint setupShader();
GLuint setupLineShader();
GLuint setupHudShader();
int    loadSimpleOBJ(const string&, int&, string&, Material&, GLuint& outVBO);
GLuint loadTexture(const string&);
void   updateWindowTitle(GLFWwindow*);
void   saveTrajectories();
void   loadTrajectories();
void   uploadLights(GLuint shader);
void   drawHUD(GLuint hudShader, GLuint hudVAO, GLuint hudVBO, GLuint hudIBO);


// ─── main ─────────────────────────────────────────────────────────────────────

int main()
{
    if (!glfwInit()) { cerr << "Failed to initialize GLFW\n"; return -1; }

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT,
        "M6 - Bezier & 3-Point Lighting", nullptr, nullptr);
    if (!window) { cerr << "Failed to create GLFW window\n"; glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSetKeyCallback      (window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback   (window, scroll_callback);
    glfwSetWindowFocusCallback(window, [](GLFWwindow*, int focused) {
        if (!focused) fill(begin(keys), end(keys), false);
    });

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    { cerr << "Failed to initialize GLAD\n"; return -1; }

    cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
    cout << "OpenGL:   " << glGetString(GL_VERSION)  << "\n";
    cout << "\n=== M6 - Bezier & 3-Point Lighting ===\n"
         << "  WASD / Mouse   - Mover/rotacionar camera\n"
         << "  Scroll         - Zoom (FOV)\n"
         << "  TAB            - Selecionar proximo objeto\n"
         << "  T / R / F      - Modo Translacao / Rotacao / Escala\n"
         << "  M              - Ligar/desligar textura do objeto selecionado\n"
         << "  1 / 2 / 3      - Ligar/desligar Luz Key / Fill / Back\n"
         << "  P              - Adicionar ponto de controle Bezier\n"
         << "  C              - Pausar/retomar trajetoria Bezier\n"
         << "  BACKSPACE      - Remover ultimo ponto de controle\n"
         << "  L              - Salvar pontos em '" << TRAJ_FILENAME << "'\n"
         << "  O              - Carregar pontos de '" << TRAJ_FILENAME << "'\n"
         << "  ESC            - Sair\n\n";

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    GLuint shader = setupShader();
    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "texBuff"), 0);
    glActiveTexture(GL_TEXTURE0);

    // Carrega luzes iniciais
    uploadLights(shader);

    // VAO/VBO pra desenhar a curva e os waypoints
    GLuint lineShader = setupLineShader();
    GLint  lineViewLoc  = glGetUniformLocation(lineShader, "view");
    GLint  lineProjLoc  = glGetUniformLocation(lineShader, "projection");
    GLint  lineColorLoc = glGetUniformLocation(lineShader, "lineColor");

    GLuint lineVAO = 0, lineVBO = 0;
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_PROGRAM_POINT_SIZE);

    // VAO/VBO do HUD 2D (stb_easy_font solta quads, depois converto p/ triangulos)
    GLuint hudShader = setupHudShader();
    GLuint hudVAO = 0, hudVBO = 0, hudIBO = 0;
    glGenVertexArrays(1, &hudVAO);
    glGenBuffers(1, &hudVBO);
    glGenBuffers(1, &hudIBO);
    glBindVertexArray(hudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
    // o hudText extrai so xy pra um buffer proprio (stride 8). os fundos/bordas tb sao xy.
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    Camera camera(glm::vec3(0.0f, 2.0f, 7.0f));
    gCamera = &camera;

    // helper pra carregar um modelo
    auto addModel = [&](const string& file, const string& modelName,
                        glm::vec3 color, glm::vec3 pos)
    {
        OBJModel m;
        m.name     = modelName;
        m.color    = color;
        m.position = pos;

        string texPath;
        int vaoID = loadSimpleOBJ(MODELS_DIR + file, m.nVertices, texPath, m.mat, m.VBO);
        if (vaoID < 0) return;
        m.VAO = (GLuint)vaoID;

        if (!texPath.empty())
        {
            m.texID = loadTexture(texPath);
            if (m.texID) cout << "Textura carregada: " << texPath << "\n";
            else         cerr << "Falha ao carregar textura: " << texPath << "\n";
        }
        else
        {
            cout << "Sem textura para: " << modelName << " (usando cor solida)\n";
        }

        cout << "  Ka=(" << m.mat.Ka.r << "," << m.mat.Ka.g << "," << m.mat.Ka.b << ")"
             << "  Kd=(" << m.mat.Kd.r << "," << m.mat.Kd.g << "," << m.mat.Kd.b << ")"
             << "  Ks=(" << m.mat.Ks.r << "," << m.mat.Ks.g << "," << m.mat.Ks.b << ")"
             << "  Ns=" << m.mat.Ns << "\n";

        objects.push_back(m);
    };

    addModel("Suzanne.obj",        "Suzanne",        glm::vec3(1.0f, 0.35f, 0.35f), glm::vec3(-2.5f, 0.0f, 0.0f));
    addModel("SuzanneSubdiv1.obj", "SuzanneSubdiv1", glm::vec3(0.35f, 0.9f, 0.9f),  glm::vec3( 0.0f, 0.0f, 0.0f));
    addModel("Cube.obj",           "Cube",           glm::vec3(0.4f, 1.0f, 0.45f),  glm::vec3( 2.5f, 0.0f, 0.0f));

    if (objects.empty())
    {
        cerr << "Nenhum modelo carregado.\n";
        glfwTerminate();
        return -1;
    }

    loadTrajectories();

    // pega as locations dos uniforms uma vez so
    GLint modelLoc      = glGetUniformLocation(shader, "model");
    GLint colorLoc      = glGetUniformLocation(shader, "objectColor");
    GLint useTextureLoc = glGetUniformLocation(shader, "useTexture");
    GLint KaLoc         = glGetUniformLocation(shader, "Ka");
    GLint KdLoc         = glGetUniformLocation(shader, "Kd");
    GLint KsLoc         = glGetUniformLocation(shader, "Ks");
    GLint NsLoc         = glGetUniformLocation(shader, "Ns");
    GLint viewLoc       = glGetUniformLocation(shader, "view");
    GLint projLoc       = glGetUniformLocation(shader, "projection");
    GLint camPosLoc     = glGetUniformLocation(shader, "camPos");

    float aspect = (float)WIDTH / (float)HEIGHT;

    glEnable(GL_DEPTH_TEST);
    updateWindowTitle(window);

    float prevTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        float currTime = (float)glfwGetTime();
        float dt       = currTime - prevTime;
        prevTime       = currTime;

        glfwPollEvents();

        // ── camera ────────────────────────────────────────────────────────
        camera.processKeyboard(window, dt);

        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 proj = camera.getProjectionMatrix(aspect);

        glUseProgram(shader);
        glUniformMatrix4fv(viewLoc,  1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc,  1, GL_FALSE, glm::value_ptr(proj));
        glUniform3fv(camPosLoc, 1, glm::value_ptr(camera.position));

        // manda as luzes pro shader
        uploadLights(shader);

        // ── trajetorias: avanca os objetos com curva ativa ──────────────
        for (auto& obj : objects)
        {
            if (obj.traj.active && obj.traj.waypoints.size() >= 2)
                obj.position = obj.traj.advance(dt);
        }

        // ── transformacao manual (so quando a trajetoria nao esta ativa) ─
        OBJModel& sel = objects[activeObj];
        if (!sel.traj.active)
        {
            if (currentMode == MODE_TRANSLATE)
            {
                float step = TRANSLATE_SPEED * dt;
                if (keys[GLFW_KEY_UP])    sel.position.z -= step;
                if (keys[GLFW_KEY_DOWN])  sel.position.z += step;
                if (keys[GLFW_KEY_LEFT])  sel.position.x -= step;
                if (keys[GLFW_KEY_RIGHT]) sel.position.x += step;
                if (keys[GLFW_KEY_I])     sel.position.y += step;
                if (keys[GLFW_KEY_K])     sel.position.y -= step;
            }

            if (currentMode == MODE_SCALE)
            {
                float delta = 0.0f;
                if (keys[GLFW_KEY_RIGHT_BRACKET])                         delta =  SCALE_SPEED * dt;
                if (keys[GLFW_KEY_LEFT_BRACKET] || keys[GLFW_KEY_MINUS]) delta = -SCALE_SPEED * dt;
                if (delta != 0.0f)
                    sel.scale = glm::max(sel.scale + glm::vec3(delta), glm::vec3(SCALE_MIN));
            }
        }

        // rotacao automatica dos objetos com algum eixo ativo
        for (auto& o : objects)
            if (o.rotX || o.rotY || o.rotZ)
                o.rotAngle += ROT_SPEED * dt;

        // ── render ───────────────────────────────────────────────────────
        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (size_t i = 0; i < objects.size(); ++i)
        {
            const OBJModel& o        = objects[i];
            bool            selected = ((int)i == activeObj);

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, o.position);
            model = glm::scale(model, o.scale);
            if      (o.rotX) model = glm::rotate(model, o.rotAngle, glm::vec3(1, 0, 0));
            else if (o.rotY) model = glm::rotate(model, o.rotAngle, glm::vec3(0, 1, 0));
            else if (o.rotZ) model = glm::rotate(model, o.rotAngle, glm::vec3(0, 0, 1));

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform3fv(KaLoc, 1, glm::value_ptr(o.mat.Ka));
            glUniform3fv(KdLoc, 1, glm::value_ptr(o.mat.Kd));
            glUniform3fv(KsLoc, 1, glm::value_ptr(o.mat.Ks));
            glUniform1f (NsLoc, o.mat.Ns);

            glBindVertexArray(o.VAO);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.0f, 1.0f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            // usa textura ou cor solida
            bool useTex = (o.texID != 0 && o.showTexture);
            if (useTex)
            {
                glUniform1i(useTextureLoc, 1);
                glBindTexture(GL_TEXTURE_2D, o.texID);
            }
            else
            {
                glUniform1i(useTextureLoc, 0);
                glUniform3fv(colorLoc, 1, glm::value_ptr(o.color));
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            glDrawArrays(GL_TRIANGLES, 0, o.nVertices);
            glDisable(GL_POLYGON_OFFSET_FILL);

            // wireframe branco no objeto selecionado
            if (selected)
            {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glUniform1i(useTextureLoc, 0);
                glm::vec3 white(1.0f);
                glUniform3fv(colorLoc, 1, glm::value_ptr(white));
                glDrawArrays(GL_TRIANGLES, 0, o.nVertices);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }

            glBindVertexArray(0);
        }

        // ── desenha curva bezier e pontos de controle ────────────────────
        glUseProgram(lineShader);
        glUniformMatrix4fv(lineViewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(lineProjLoc, 1, GL_FALSE, glm::value_ptr(proj));
        glBindVertexArray(lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);

        for (size_t i = 0; i < objects.size(); ++i)
        {
            const auto& wps = objects[i].traj.waypoints;
            if (wps.empty()) continue;

            bool      isSel    = ((int)i == activeObj);
            glm::vec3 curveCol = isSel ? glm::vec3(1.0f, 0.85f, 0.1f)  // amarelo
                                       : glm::vec3(0.5f, 0.5f, 0.6f);  // cinza
            glm::vec3 cageCol  = isSel ? glm::vec3(1.0f, 0.5f, 0.2f)
                                       : glm::vec3(0.3f, 0.3f, 0.4f);
            glm::vec3 ptCol    = isSel ? glm::vec3(1.0f, 1.0f, 0.3f)
                                       : glm::vec3(0.7f, 0.7f, 0.8f);

            if (wps.size() >= 2)
            {
                // amostra a curva e desenha como line_loop
                vector<glm::vec3> curvePoints;
                curvePoints.reserve(BEZIER_SAMPLES);
                for (int k = 0; k < BEZIER_SAMPLES; ++k)
                    curvePoints.push_back(objects[i].traj.evalBezier((float)k / BEZIER_SAMPLES));

                glBufferData(GL_ARRAY_BUFFER,
                             curvePoints.size() * sizeof(glm::vec3),
                             curvePoints.data(), GL_DYNAMIC_DRAW);
                glUniform3fv(lineColorLoc, 1, glm::value_ptr(curveCol));
                glDrawArrays(GL_LINE_LOOP, 0, BEZIER_SAMPLES);

                // poligono de controle (cage)
                glBufferData(GL_ARRAY_BUFFER,
                             wps.size() * sizeof(glm::vec3),
                             wps.data(), GL_DYNAMIC_DRAW);
                glUniform3fv(lineColorLoc, 1, glm::value_ptr(cageCol));
                glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)wps.size());
            }
            else
            {
                // so 1 ponto - carrega mesmo assim pra desenhar o ponto
                glBufferData(GL_ARRAY_BUFFER,
                             wps.size() * sizeof(glm::vec3),
                             wps.data(), GL_DYNAMIC_DRAW);
            }

            // pontos por cima
            glBufferData(GL_ARRAY_BUFFER,
                         wps.size() * sizeof(glm::vec3),
                         wps.data(), GL_DYNAMIC_DRAW);
            glUniform3fv(lineColorLoc, 1, glm::value_ptr(ptCol));
            glDrawArrays(GL_POINTS, 0, (GLsizei)wps.size());
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // ── HUD ───────────────────────────────────────────────────────────
        drawHUD(hudShader, hudVAO, hudVBO, hudIBO);

        glfwSwapBuffers(window);
    }

    glDeleteBuffers(1, &lineVBO);
    glDeleteVertexArrays(1, &lineVAO);
    glDeleteProgram(lineShader);

    glDeleteBuffers(1, &hudVBO);
    glDeleteBuffers(1, &hudIBO);
    glDeleteVertexArrays(1, &hudVAO);
    glDeleteProgram(hudShader);

    // libera VAO/VBO/textura de cada modelo
    for (auto& o : objects)
    {
        if (o.VBO)   glDeleteBuffers(1, &o.VBO);
        if (o.VAO)   glDeleteVertexArrays(1, &o.VAO);
        if (o.texID) glDeleteTextures(1, &o.texID);
    }
    glDeleteProgram(shader);

    glfwTerminate();
    return 0;
}


// ─── Callbacks ───────────────────────────────────────────────────────────────

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
        return;
    }

    if (key >= 0 && key < 1024)
    {
        if      (action == GLFW_PRESS)   keys[key] = true;
        else if (action == GLFW_RELEASE) keys[key] = false;
    }

    if (action != GLFW_PRESS) return;

    // tecla H - mostra/esconde ajuda
    if (key == GLFW_KEY_H)
    {
        showHelp = !showHelp;
        return;
    }

    // TAB - proximo objeto
    if (key == GLFW_KEY_TAB)
    {
        activeObj = (activeObj + 1) % (int)objects.size();
        updateWindowTitle(window);
        return;
    }

    // T/R/F - modos de transformacao
    if (key == GLFW_KEY_T)
    {
        if (currentMode == MODE_ROTATE)
            objects[activeObj].rotX = objects[activeObj].rotY = objects[activeObj].rotZ = false;
        currentMode = MODE_TRANSLATE;
        updateWindowTitle(window);
        return;
    }
    if (key == GLFW_KEY_R) { currentMode = MODE_ROTATE; updateWindowTitle(window); return; }
    if (key == GLFW_KEY_F)
    {
        if (currentMode == MODE_ROTATE)
            objects[activeObj].rotX = objects[activeObj].rotY = objects[activeObj].rotZ = false;
        currentMode = MODE_SCALE;
        updateWindowTitle(window);
        return;
    }

    // dentro do modo rotacao, X/Y/Z escolhem o eixo (so um por vez)
    if (currentMode == MODE_ROTATE)
    {
        OBJModel& obj = objects[activeObj];
        if (key == GLFW_KEY_X) { obj.rotX = !obj.rotX; if (obj.rotX) { obj.rotY = obj.rotZ = false; } }
        if (key == GLFW_KEY_Y) { obj.rotY = !obj.rotY; if (obj.rotY) { obj.rotX = obj.rotZ = false; } }
        if (key == GLFW_KEY_Z) { obj.rotZ = !obj.rotZ; if (obj.rotZ) { obj.rotX = obj.rotY = false; } }
    }

    // M - liga/desliga textura
    if (key == GLFW_KEY_M)
    {
        OBJModel& obj = objects[activeObj];
        if (obj.texID == 0)
        {
            cout << "[Textura] '" << obj.name << "' nao tem textura.\n";
            return;
        }
        obj.showTexture = !obj.showTexture;
        cout << "[Textura] '" << obj.name
             << "' textura " << (obj.showTexture ? "LIGADA" : "DESLIGADA")
             << "  Ka=(" << obj.mat.Ka.r << "," << obj.mat.Ka.g << "," << obj.mat.Ka.b << ")"
             << "  Kd=(" << obj.mat.Kd.r << "," << obj.mat.Kd.g << "," << obj.mat.Kd.b << ")"
             << "  Ks=(" << obj.mat.Ks.r << "," << obj.mat.Ks.g << "," << obj.mat.Ks.b << ")"
             << "  Ns=" << obj.mat.Ns << "\n";
        updateWindowTitle(window);
        return;
    }

    // 1/2/3 - liga/desliga as luzes
    if (key == GLFW_KEY_1 || key == GLFW_KEY_2 || key == GLFW_KEY_3)
    {
        int idx = (key == GLFW_KEY_1) ? 0 : (key == GLFW_KEY_2) ? 1 : 2;
        lights[idx].on = !lights[idx].on;
        cout << "[Luz] " << lights[idx].name
             << " " << (lights[idx].on ? "LIGADA" : "DESLIGADA") << "\n";
        updateWindowTitle(window);
        return;
    }

    // ── trajetoria ───────────────────────────────────────────────────────

    // P - adiciona um ponto de controle na posicao atual
    if (key == GLFW_KEY_P)
    {
        OBJModel& obj = objects[activeObj];
        obj.traj.waypoints.push_back(obj.position);
        obj.traj.dirty = true;
        int n = (int)obj.traj.waypoints.size();
        cout << "[Bezier] Ponto de controle " << n << " adicionado para '"
             << obj.name << "': ("
             << obj.position.x << ", "
             << obj.position.y << ", "
             << obj.position.z << ")\n";
        updateWindowTitle(window);
        return;
    }

    // C - play/pause (nao reseta o t)
    if (key == GLFW_KEY_C)
    {
        OBJModel& obj = objects[activeObj];
        if (obj.traj.waypoints.size() < 2)
        {
            cout << "[Bezier] Adicione pelo menos 2 pontos de controle antes de ligar.\n";
            return;
        }
        obj.traj.active = !obj.traj.active;

        cout << "[Bezier] '" << obj.name
             << "' trajetoria " << (obj.traj.active ? "RETOMADA" : "PAUSADA")
             << " (t=" << obj.traj.t
             << ", " << obj.traj.waypoints.size() << " pontos)\n";
        updateWindowTitle(window);
        return;
    }

    // BACKSPACE - tira o ultimo ponto
    if (key == GLFW_KEY_BACKSPACE)
    {
        OBJModel& obj = objects[activeObj];
        if (!obj.traj.waypoints.empty())
        {
            obj.traj.waypoints.pop_back();
            obj.traj.dirty = true;
            if (obj.traj.waypoints.size() < 2)
            {
                if (obj.traj.active)
                    cout << "[Bezier] '" << obj.name
                         << "' trajetoria PAUSADA (pontos insuficientes)\n";
                obj.traj.active = false;
            }
            cout << "[Bezier] Ponto removido de '"
                 << obj.name << "'. Restam: " << obj.traj.waypoints.size() << "\n";
        }
        else
        {
            cout << "[Bezier] Nenhum ponto para remover.\n";
        }
        updateWindowTitle(window);
        return;
    }

    // L = salvar, O = carregar
    if (key == GLFW_KEY_L) { saveTrajectories(); return; }
    if (key == GLFW_KEY_O) { loadTrajectories(); updateWindowTitle(window); return; }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (gCamera) gCamera->processMouse(xpos, ypos);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (gCamera) gCamera->processScroll(yoffset);
}


// ─── Manda as luzes pro shader ───────────────────────────────────────────────

void uploadLights(GLuint shader)
{
    glUseProgram(shader);

    // cache: so chama glGetUniformLocation 1 vez por shader (fica caro chamar todo frame)
    static GLuint  cachedShader = 0;
    static GLint   posLoc[3], colLoc[3], onLoc[3];
    if (cachedShader != shader)
    {
        for (int i = 0; i < 3; ++i)
        {
            string base    = "lightPos["   + to_string(i) + "]";
            string colBase = "lightColor[" + to_string(i) + "]";
            string onBase  = "lightOn["    + to_string(i) + "]";
            posLoc[i] = glGetUniformLocation(shader, base.c_str());
            colLoc[i] = glGetUniformLocation(shader, colBase.c_str());
            onLoc[i]  = glGetUniformLocation(shader, onBase.c_str());
        }
        cachedShader = shader;
    }

    for (int i = 0; i < 3; ++i)
    {
        glUniform3fv(posLoc[i], 1, glm::value_ptr(lights[i].pos));
        glUniform3fv(colLoc[i], 1, glm::value_ptr(lights[i].color));
        glUniform1i (onLoc[i],  lights[i].on ? 1 : 0);
    }
}


// ─── salvar / carregar trajetorias ──────────────────────────────────────────
// formato:
//   object <nome>
//   waypoint <x> <y> <z>
//   ...

void saveTrajectories()
{
    ofstream f(TRAJ_FILENAME);
    if (!f.is_open()) { cerr << "Erro ao salvar " << TRAJ_FILENAME << "\n"; return; }

    f << fixed << setprecision(9);

    int saved = 0;
    for (const auto& obj : objects)
    {
        if (obj.traj.waypoints.empty()) continue;
        f << "object " << obj.name << "\n";
        for (const auto& wp : obj.traj.waypoints)
            f << "waypoint " << wp.x << " " << wp.y << " " << wp.z << "\n";
        ++saved;
    }
    f.close();
    cout << "[Bezier] Pontos salvos em '" << TRAJ_FILENAME
         << "' (" << saved << " objeto(s))\n";
}

void loadTrajectories()
{
    ifstream f(TRAJ_FILENAME);
    if (!f.is_open())
    {
        cout << "[Bezier] Arquivo '" << TRAJ_FILENAME << "' nao encontrado.\n";
        return;
    }

    cout << "[Bezier] Carregando '" << TRAJ_FILENAME << "'...\n";

    for (auto& obj : objects)
    {
        obj.traj.waypoints.clear();
        obj.traj.active = false;
        obj.traj.t      = 0.0f;
        obj.traj.dirty  = true;
    }

    OBJModel* current = nullptr;
    string line;
    while (getline(f, line))
    {
        istringstream ss(line);
        string word;
        ss >> word;

        if (word == "object")
        {
            string name; ss >> name;
            current = nullptr;
            for (auto& obj : objects)
                if (obj.name == name) { current = &obj; break; }
            if (!current)
                cerr << "[Bezier] Objeto '" << name << "' nao encontrado.\n";
        }
        else if (word == "waypoint" && current)
        {
            glm::vec3 wp;
            ss >> wp.x >> wp.y >> wp.z;
            current->traj.waypoints.push_back(wp);
        }
    }
    f.close();

    for (const auto& obj : objects)
        cout << "  " << obj.name << ": " << obj.traj.waypoints.size() << " pontos\n";
}


// ─── titulo da janela ───────────────────────────────────────────────────────

void updateWindowTitle(GLFWwindow* window)
{
    string modeStr;
    switch (currentMode)
    {
        case MODE_TRANSLATE: modeStr = "Trans(Setas/I/K)"; break;
        case MODE_ROTATE:    modeStr = "Rot(X/Y/Z)";       break;
        case MODE_SCALE:     modeStr = "Escala(]/[)";       break;
    }

    const OBJModel& obj = objects[activeObj];

    string texStr = (obj.texID == 0) ? " [noTex]" :
                    (obj.showTexture ? " [Tex:ON]" : " [Tex:OFF]");

    string trajInfo;
    if (obj.traj.active)
        trajInfo = " [BEZ:ON n=" + to_string(obj.traj.waypoints.size()) + "]";
    else if (!obj.traj.waypoints.empty())
        trajInfo = " [n=" + to_string(obj.traj.waypoints.size()) + "]";

    // K=key F=fill B=back (maiusculo = ligado, minusculo = desligado)
    string lightStr = " | ";
    lightStr += lights[0].on ? "K" : "k";
    lightStr += lights[1].on ? "F" : "f";
    lightStr += lights[2].on ? "B" : "b";

    string title = "M6 | " + obj.name
                 + "  [" + to_string(activeObj + 1) + "/" + to_string(objects.size()) + "]"
                 + "  " + modeStr
                 + texStr + trajInfo
                 + lightStr
                 + "  | M=tex 1/2/3=luz P=add C=play/pause";
    glfwSetWindowTitle(window, title.c_str());
}


// ─── Setup dos shaders ────────────────────────────────────────────────────────

GLuint setupShader()
{
    auto compile = [](GLenum type, const GLchar* src) -> GLuint
    {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
        GLint ok;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[512];
            glGetShaderInfoLog(s, 512, NULL, log);
            cerr << "Shader compile error: " << log << "\n";
        }
        return s;
    };

    GLuint vs   = compile(GL_VERTEX_SHADER,   vertexShaderSource);
    GLuint fs   = compile(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetProgramInfoLog(prog, 512, NULL, log);
        cerr << "Shader link error: " << log << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

GLuint setupLineShader()
{
    auto compile = [](GLenum type, const GLchar* src) -> GLuint
    {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
        GLint ok;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[512];
            glGetShaderInfoLog(s, 512, NULL, log);
            cerr << "Line shader compile error: " << log << "\n";
        }
        return s;
    };

    GLuint vs   = compile(GL_VERTEX_SHADER,   lineVertexShaderSource);
    GLuint fs   = compile(GL_FRAGMENT_SHADER, lineFragmentShaderSource);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetProgramInfoLog(prog, 512, NULL, log);
        cerr << "Line shader link error: " << log << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}


// ─── Carrega textura ─────────────────────────────────────────────────────────

GLuint loadTexture(const string& filePath)
{
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    // filtro nearest pra dar uma cara mais "pixelada" (sem mipmap)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filePath.c_str(), &w, &h, &ch, 0);

    if (data)
    {
        GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        // sem mipmap mesmo
    }
    else
    {
        cerr << "Falha ao carregar imagem: " << filePath << "\n";
        glDeleteTextures(1, &texID);
        texID = 0;
    }

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texID;
}


// ─── HUD de ajuda (usa stb_easy_font) ───────────────────────────────────────

GLuint setupHudShader()
{
    auto compile = [](GLenum type, const GLchar* src) -> GLuint
    {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
        GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) { char log[512]; glGetShaderInfoLog(s, 512, NULL, log); cerr << "HUD shader: " << log << "\n"; }
        return s;
    };
    GLuint vs = compile(GL_VERTEX_SHADER,   hudVertexShaderSource);
    GLuint fs = compile(GL_FRAGMENT_SHADER, hudFragmentShaderSource);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

// desenha uma linha de texto. vbo/vao precisam estar com stride=8 e attrib 0 = vec2.
// o ibo eh reaproveitado entre chamadas pra nao ficar gerando/deletando todo frame.
static void hudText(GLuint vbo, GLuint ibo, float px, float py, float scale, const char* text)
{
    static char buf[99999];
    int nq = stb_easy_font_print(0, 0, const_cast<char*>(text), NULL, buf, sizeof(buf));

    // o stb_easy_font solta quads (4 verts cada, stride 16: x,y,z + rgba).
    // monto um vector xy escalado e ja posicionado.
    int nv = nq * 4;
    vector<float> verts;
    verts.reserve(nv * 2);
    for (int i = 0; i < nv; ++i)
    {
        float* v = reinterpret_cast<float*>(buf + i * 16);
        verts.push_back(px + v[0] * scale);
        verts.push_back(py + v[1] * scale);
    }

    // converte cada quad em 2 triangulos
    vector<unsigned int> idx;
    idx.reserve(nq * 6);
    for (int q = 0; q < nq; ++q)
    {
        unsigned int b = q * 4;
        idx.push_back(b+0); idx.push_back(b+1); idx.push_back(b+2);
        idx.push_back(b+0); idx.push_back(b+2); idx.push_back(b+3);
    }

    // sobe os xy
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // ibo (reaproveitado)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_DYNAMIC_DRAW);

    glDrawElements(GL_TRIANGLES, (GLsizei)idx.size(), GL_UNSIGNED_INT, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void drawHUD(GLuint hudShader, GLuint hudVAO, GLuint hudVBO, GLuint hudIBO)
{
    // ortho em pixels: (0,0) = canto sup esq, y cresce pra baixo
    glm::mat4 ortho = glm::ortho(0.0f, (float)WIDTH, (float)HEIGHT, 0.0f, -1.0f, 1.0f);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(hudShader);
    GLint orthoLoc = glGetUniformLocation(hudShader, "ortho");
    GLint colLoc   = glGetUniformLocation(hudShader, "hudColor");
    glUniformMatrix4fv(orthoLoc, 1, GL_FALSE, glm::value_ptr(ortho));

    glBindVertexArray(hudVAO);

    // ── barra de status no topo (sempre visivel) ─────────────────────────
    {
        const OBJModel& obj = objects[activeObj];
        string modeStr = (currentMode == MODE_TRANSLATE) ? "TRANS" :
                         (currentMode == MODE_ROTATE)    ? "ROT"   : "SCALE";
        string texState = (obj.texID == 0) ? "no-tex" : (obj.showTexture ? "tex:ON" : "tex:OFF");
        string trajState = obj.traj.active ? "BEZIER:ON" :
                           (!obj.traj.waypoints.empty() ?
                            "pts:" + to_string(obj.traj.waypoints.size()) : "no-pts");
        string lights_s = string("Luz:") +
                          (lights[0].on ? "K" : "k") +
                          (lights[1].on ? "F" : "f") +
                          (lights[2].on ? "B" : "b");

        string statusLine = "  [" + to_string(activeObj+1) + "/" + to_string(objects.size()) + "] "
                          + obj.name + "  |  Modo:" + modeStr
                          + "  " + texState
                          + "  " + trajState
                          + "  " + lights_s
                          + "  |  H=ajuda";

        // fundo da barra
        float bh = 22.0f;
        float barVerts[] = { 0,0, (float)WIDTH,0, (float)WIDTH,bh, 0,0, (float)WIDTH,bh, 0,bh };
        glUniform4f(colLoc, 0.0f, 0.0f, 0.0f, 0.65f);
        glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(barVerts), barVerts, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glUniform4f(colLoc, 0.85f, 0.90f, 1.0f, 1.0f);
        hudText(hudVBO, hudIBO, 6.0f, 5.0f, 1.5f, statusLine.c_str());
    }

    // ── painel de ajuda (so quando showHelp == true) ─────────────────────
    if (showHelp)
    {
        struct Entry { const char* key; const char* desc; };
        struct Section { const char* title; vector<Entry> entries; };

        vector<Section> sections = {
            { "CAMERA",  {
                { "W / S",       "Mover frente / tras"       },
                { "A / D",       "Mover esquerda / direita"  },
                { "Mouse",       "Rotacionar (yaw / pitch)"  },
                { "Scroll",      "Zoom (FOV)"                },
            }},
            { "SELECAO & TRANSFORM", {
                { "TAB",         "Proximo objeto"            },
                { "T",           "Modo Translacao"           },
                { "  Setas",     "Mover X / Z"               },
                { "  I / K",     "Mover Y cima / baixo"      },
                { "R",           "Modo Rotacao"              },
                { "  X / Y / Z", "Ativar eixo de rotacao"    },
                { "F",           "Modo Escala uniforme"      },
                { "  ] / [",     "Aumentar / diminuir"       },
            }},
            { "TEXTURA & MATERIAL", {
                { "M",           "Ligar/desligar textura"    },
                { "",            "(mostra Ka Kd Ks Ns)"      },
            }},
            { "ILUMINACAO (Phong 3-pts)", {
                { "1",           "Luz Key   (principal)"     },
                { "2",           "Luz Fill  (preenchimento)" },
                { "3",           "Luz Back  (contraluz)"     },
            }},
            { "TRAJETORIA BEZIER", {
                { "P",           "Add ponto de controle"     },
                { "C",           "Play / Pause trajetoria"   },
                { "BACKSPACE",   "Remover ultimo ponto"      },
                { "L",           "Salvar em trajectories.txt"},
                { "O",           "Carregar trajectories.txt" },
            }},
            { "GERAL", {
                { "H",           "Mostrar / ocultar ajuda"   },
                { "ESC",         "Sair"                      },
            }},
        };

        // dimensoes do painel
        const float SCALE   = 1.5f;
        const float LHEIGHT = 13.0f * SCALE;  // altura de cada linha
        const float SHEIGHT = 17.0f * SCALE;  // altura do cabecalho de secao
        const float PAD     = 10.0f;
        const float COL_KEY = 110.0f;
        const float COL_DESC= 260.0f;

        int totalLines = 0;
        for (auto& s : sections) totalLines += 1 + (int)s.entries.size();
        // PAD topo + titulo (~1.1*SHEIGHT) + separador (~6) + linhas + extras dos cabecalhos + PAD baixo
        float titleBlock = SHEIGHT * 1.1f + 6.0f;
        float panelH = PAD + titleBlock + totalLines * LHEIGHT
                     + sections.size() * (SHEIGHT - LHEIGHT) + PAD;
        float panelW = PAD + COL_KEY + COL_DESC + PAD;
        float px = (float)WIDTH  - panelW - 12.0f;
        float py = 28.0f;  // logo abaixo da barra de status

        // sombra
        {
            float sx = px + 4, sy = py + 4;
            float sv[] = { sx,sy, sx+panelW,sy, sx+panelW,sy+panelH,
                           sx,sy, sx+panelW,sy+panelH, sx,sy+panelH };
            glUniform4f(colLoc, 0.0f, 0.0f, 0.0f, 0.45f);
            glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(sv), sv, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // fundo do painel
        {
            float bv[] = { px,py, px+panelW,py, px+panelW,py+panelH,
                           px,py, px+panelW,py+panelH, px,py+panelH };
            glUniform4f(colLoc, 0.05f, 0.07f, 0.12f, 0.88f);
            glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(bv), bv, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // borda (4 retangulos finos)
        {
            float t = 2.0f;
            float borders[][12] = {
                { px,py, px+panelW,py, px+panelW,py+t,  px,py, px+panelW,py+t, px,py+t },  // top
                { px,py+panelH-t, px+panelW,py+panelH-t, px+panelW,py+panelH, px,py+panelH-t, px+panelW,py+panelH, px,py+panelH }, // bot
                { px,py, px+t,py, px+t,py+panelH, px,py, px+t,py+panelH, px,py+panelH },  // left
                { px+panelW-t,py, px+panelW,py, px+panelW,py+panelH, px+panelW-t,py, px+panelW,py+panelH, px+panelW-t,py+panelH }, // right
            };
            glUniform4f(colLoc, 0.3f, 0.5f, 0.9f, 0.9f);
            for (auto& b : borders)
            {
                glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
                glBufferData(GL_ARRAY_BUFFER, sizeof(b), b, GL_DYNAMIC_DRAW);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }

        // titulo
        float cy = py + PAD;
        glUniform4f(colLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        hudText(hudVBO, hudIBO, px + PAD, cy, SCALE, "CONTROLES  (H = fechar)");
        cy += SHEIGHT * 1.1f;

        // separador
        {
            float sep[] = { px+PAD, cy, px+panelW-PAD, cy, px+panelW-PAD, cy+1.5f,
                            px+PAD, cy, px+panelW-PAD, cy+1.5f, px+PAD, cy+1.5f };
            glUniform4f(colLoc, 0.3f, 0.5f, 0.9f, 0.7f);
            glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(sep), sep, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        cy += 6.0f;

        // secoes
        for (const auto& sec : sections)
        {
            // fundo do cabecalho da secao
            {
                float sh = SHEIGHT;
                float sv[] = { px+PAD-2,cy-2, px+panelW-PAD+2,cy-2, px+panelW-PAD+2,cy+sh-2,
                               px+PAD-2,cy-2, px+panelW-PAD+2,cy+sh-2, px+PAD-2,cy+sh-2 };
                glUniform4f(colLoc, 0.15f, 0.20f, 0.35f, 0.80f);
                glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
                glBufferData(GL_ARRAY_BUFFER, sizeof(sv), sv, GL_DYNAMIC_DRAW);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            glUniform4f(colLoc, 0.55f, 0.80f, 1.00f, 1.0f);
            hudText(hudVBO, hudIBO, px + PAD + 2, cy, SCALE, sec.title);
            cy += SHEIGHT;

            // entradas
            for (const auto& e : sec.entries)
            {
                if (e.key[0] != '\0')
                {
                    // tecla amarela
                    glUniform4f(colLoc, 1.0f, 0.88f, 0.30f, 1.0f);
                    hudText(hudVBO, hudIBO, px + PAD + 4, cy, SCALE, e.key);
                }
                // descricao em branco suave
                glUniform4f(colLoc, 0.85f, 0.88f, 0.92f, 1.0f);
                hudText(hudVBO, hudIBO, px + PAD + COL_KEY, cy, SCALE, e.desc);
                cy += LHEIGHT;
            }
        }
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}


// ─── Carregamento de OBJ ─────────────────────────────────────────────────────

int loadSimpleOBJ(const string& filePath, int& nVertices, string& texturePath, Material& mat, GLuint& outVBO)
{
    vector<glm::vec3> positions;
    vector<glm::vec2> texCoords;
    vector<glm::vec3> normals;
    vector<GLfloat>   vBuffer;

    texturePath = "";
    string mtlFile;
    string directory = filePath.substr(0, filePath.find_last_of("/\\") + 1);
    mat = Material();

    ifstream file(filePath);
    if (!file.is_open()) { cerr << "Erro ao abrir: " << filePath << "\n"; return -1; }

    string line;
    while (getline(file, line))
    {
        istringstream ss(line);
        string word;
        ss >> word;

        if      (word == "mtllib") { ss >> mtlFile; }
        else if (word == "v")  { glm::vec3 v; ss >> v.x >> v.y >> v.z; positions.push_back(v); }
        else if (word == "vt") { glm::vec2 vt; ss >> vt.s >> vt.t; texCoords.push_back(vt); }
        else if (word == "vn") { glm::vec3 vn; ss >> vn.x >> vn.y >> vn.z; normals.push_back(vn); }
        else if (word == "f")
        {
            struct FaceIdx { int v, t, n; };
            vector<FaceIdx> face;

            auto resolveIdx = [](const string& s, int listSize) -> int
            {
                if (s.empty()) return -1;
                int v;
                try { v = stoi(s); } catch (...) { return -1; }
                if (v > 0)      return v - 1;
                else if (v < 0) return listSize + v;
                else            return -1;
            };

            while (ss >> word)
            {
                FaceIdx fi { -1, -1, -1 };
                istringstream si(word);
                string idx;
                if (getline(si, idx, '/')) fi.v = resolveIdx(idx, (int)positions.size());
                if (getline(si, idx, '/')) fi.t = resolveIdx(idx, (int)texCoords.size());
                if (getline(si, idx))      fi.n = resolveIdx(idx, (int)normals.size());
                face.push_back(fi);
            }

            auto pushVert = [&](const FaceIdx& fi)
            {
                if (fi.v >= 0 && fi.v < (int)positions.size())
                {
                    vBuffer.push_back(positions[fi.v].x);
                    vBuffer.push_back(positions[fi.v].y);
                    vBuffer.push_back(positions[fi.v].z);
                }
                else { vBuffer.push_back(0.0f); vBuffer.push_back(0.0f); vBuffer.push_back(0.0f); }

                if (fi.t >= 0 && fi.t < (int)texCoords.size())
                    { vBuffer.push_back(texCoords[fi.t].s); vBuffer.push_back(texCoords[fi.t].t); }
                else
                    { vBuffer.push_back(0.0f); vBuffer.push_back(0.0f); }

                if (fi.n >= 0 && fi.n < (int)normals.size())
                    { vBuffer.push_back(normals[fi.n].x); vBuffer.push_back(normals[fi.n].y); vBuffer.push_back(normals[fi.n].z); }
                else
                    { vBuffer.push_back(0.0f); vBuffer.push_back(1.0f); vBuffer.push_back(0.0f); }
            };

            for (size_t k = 1; k + 1 < face.size(); ++k)
            {
                pushVert(face[0]);
                pushVert(face[k]);
                pushVert(face[k + 1]);
            }
        }
    }
    file.close();

    if (!mtlFile.empty())
    {
        ifstream mtlIn(directory + mtlFile);
        if (mtlIn.is_open())
        {
            string mtlLine;
            while (getline(mtlIn, mtlLine))
            {
                istringstream mtlss(mtlLine);
                string mtlWord; mtlss >> mtlWord;
                if      (mtlWord == "map_Kd") { string tf; mtlss >> tf; texturePath = directory + tf; }
                else if (mtlWord == "Ka") mtlss >> mat.Ka.r >> mat.Ka.g >> mat.Ka.b;
                else if (mtlWord == "Kd") mtlss >> mat.Kd.r >> mat.Kd.g >> mat.Kd.b;
                else if (mtlWord == "Ks") mtlss >> mat.Ks.r >> mat.Ks.g >> mat.Ks.b;
                else if (mtlWord == "Ns") mtlss >> mat.Ns;
            }
            mtlIn.close();
        }
    }

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = (int)(vBuffer.size() / 8);
    cout << "Carregado: " << filePath << "  (" << nVertices << " vertices)\n";
    outVBO = VBO;
    return (int)VAO;
}

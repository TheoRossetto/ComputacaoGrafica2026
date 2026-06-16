/*
 M6 - Trajetórias para Objetos 3D (continuação de M5)

 Cada objeto da cena possui uma lista de pontos de controle (waypoints).
 O objeto percorre esses pontos ciclicamente com translação linear.

 Controles de câmera:
   WASD       - Mover câmera (frente/esq/atrás/dir)
   Mouse      - Rotacionar câmera (yaw/pitch)
   Scroll     - Zoom (FOV)

 Seleção e transformação:
   TAB        - Selecionar próximo objeto
   T          - Modo Translação manual (Setas / I·K)
   R          - Modo Rotação  (X/Y/Z para eixo)
   F          - Modo Escala   (]/[ escala uniforme)

 Trajetória:
   P          - Adicionar posição atual do objeto como waypoint
   C          - Ligar/desligar seguir trajetória (cyclic follow)
   BACKSPACE  - Remover último waypoint do objeto selecionado
   L          - Salvar waypoints em arquivo  (trajectories.txt)
   O          - Carregar waypoints de arquivo (trajectories.txt)

 ESC - Sair
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


// ─── Constantes ──────────────────────────────────────────────────────────────

const GLuint WIDTH  = 1000;
const GLuint HEIGHT = 800;

const string MODELS_DIR    = "../assets/Modelos3D/";
const string TRAJ_FILENAME = "trajectories.txt";   // salvo no diretório de trabalho

const float TRANSLATE_SPEED  = 2.5f;
const float SCALE_SPEED      = 1.0f;
const float ROT_SPEED        = 1.5f;
const float SCALE_MIN        = 0.05f;
const float TRAJ_SPEED       = 2.0f;   // unidades/segundo ao percorrer trajetória


// ─── Shaders ─────────────────────────────────────────────────────────────────

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

const GLchar* fragmentShaderSource = R"glsl(
#version 450
in vec2 texCoord;
in vec3 vNormal;
in vec3 fragPos;

uniform sampler2D texBuff;
uniform int       useTexture;
uniform vec3      objectColor;

uniform vec3  lightPos;
uniform vec3  camPos;

uniform vec3  Ka;
uniform vec3  Kd;
uniform vec3  Ks;
uniform float Ns;

out vec4 color;

void main()
{
    vec3 lightColor = vec3(1.0);
    vec3 baseColor  = (useTexture == 1) ? vec3(texture(texBuff, texCoord)) : objectColor;

    vec3 ambient = Ka * lightColor;

    vec3  N    = normalize(vNormal);
    vec3  L    = normalize(lightPos - fragPos);
    float diff = max(dot(N, L), 0.0);
    vec3  diffuse = Kd * diff * lightColor;

    vec3  R    = normalize(reflect(-L, N));
    vec3  V    = normalize(camPos - fragPos);
    float spec = pow(max(dot(R, V), 0.0), Ns);
    vec3  specular = Ks * spec * lightColor;

    vec3 result = (ambient + diffuse) * baseColor + specular;
    color = vec4(result, 1.0);
}
)glsl";

// Shader minimalista para desenhar waypoints (linhas e pontos coloridos)
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

// Estado de trajetória de um objeto
struct Trajectory
{
    vector<glm::vec3> waypoints;  // pontos de controle no espaço 3D
    bool   active     = false;    // se está seguindo a trajetória
    int    currentSeg = 0;        // índice do segmento atual  (de waypoints[i] para [i+1])
    float  t          = 0.0f;     // parâmetro [0,1] dentro do segmento atual

    // Avança a trajetória por dt segundos e retorna a posição interpolada.
    // Retorna glm::vec3(0) se não houver waypoints suficientes.
    glm::vec3 advance(float dt)
    {
        if (waypoints.size() < 2) return glm::vec3(0.0f);

        int n = (int)waypoints.size();

        glm::vec3 from = waypoints[currentSeg];
        glm::vec3 to   = waypoints[(currentSeg + 1) % n];

        float segLen = glm::length(to - from);

        // Se o segmento atual é degenerado (waypoints coincidentes), salta sem
        // consumir t — evita inflar t com divisão por valor minúsculo e evita
        // travas com vários waypoints duplicados consecutivos.
        int safety = n;  // limita a no máximo n saltos por chamada
        while (segLen <= 1e-5f && safety-- > 0)
        {
            currentSeg = (currentSeg + 1) % n;
            t = 0.0f;
            from = waypoints[currentSeg];
            to   = waypoints[(currentSeg + 1) % n];
            segLen = glm::length(to - from);
        }
        // Todos os segmentos são degenerados — fica parado no waypoint atual.
        if (segLen <= 1e-5f) return waypoints[currentSeg];

        float segTime = segLen / TRAJ_SPEED;
        t += dt / segTime;

        while (t >= 1.0f)
        {
            t -= 1.0f;
            currentSeg = (currentSeg + 1) % n;
            from = waypoints[currentSeg];
            to   = waypoints[(currentSeg + 1) % n];
            segLen = glm::length(to - from);

            // Pode cair em um segmento degenerado: pula sem consumir t.
            int safety2 = n;
            while (segLen <= 1e-5f && safety2-- > 0)
            {
                currentSeg = (currentSeg + 1) % n;
                from = waypoints[currentSeg];
                to   = waypoints[(currentSeg + 1) % n];
                segLen = glm::length(to - from);
            }
            if (segLen <= 1e-5f) return waypoints[currentSeg];

            segTime = segLen / TRAJ_SPEED;
        }

        return glm::mix(from, to, t);
    }
};

struct OBJModel
{
    GLuint    VAO       = 0;
    GLuint    texID     = 0;
    int       nVertices = 0;
    glm::vec3 position  = glm::vec3(0.0f);
    glm::vec3 scale     = glm::vec3(1.0f);
    float     rotAngle  = 0.0f;
    bool      rotX = false, rotY = false, rotZ = false;
    glm::vec3 color     = glm::vec3(1.0f);
    string    name;
    Material  mat;

    Trajectory traj;  // trajetória deste objeto
};


// ─── Estado global ────────────────────────────────────────────────────────────

Camera* gCamera = nullptr;

enum TransformMode { MODE_TRANSLATE, MODE_ROTATE, MODE_SCALE };
TransformMode currentMode = MODE_TRANSLATE;

vector<OBJModel> objects;
int              activeObj = 0;
bool             keys[1024] = {};


// ─── Declarações ─────────────────────────────────────────────────────────────

void   key_callback   (GLFWwindow*, int, int, int, int);
void   mouse_callback (GLFWwindow*, double, double);
void   scroll_callback(GLFWwindow*, double, double);
GLuint setupShader();
GLuint setupLineShader();
int    loadSimpleOBJ(const string&, int&, string&, Material&);
GLuint loadTexture(const string&);
void   updateWindowTitle(GLFWwindow*);
void   saveTrajectories();
void   loadTrajectories();


// ─── main ─────────────────────────────────────────────────────────────────────

int main()
{
    if (!glfwInit()) { cerr << "Failed to initialize GLFW\n"; return -1; }

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT,
        "M6 - Trajetorias", nullptr, nullptr);
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
    cout << "\n=== M6 - Trajetorias ===\n"
         << "  WASD / Mouse    - Mover/rotacionar camera\n"
         << "  Scroll          - Zoom (FOV)\n"
         << "  TAB             - Selecionar proximo objeto\n"
         << "  T/R/F           - Modo Translacao/Rotacao/Escala\n"
         << "  P               - Adicionar waypoint na posicao atual do objeto\n"
         << "  C               - Ligar/desligar trajetoria ciclica\n"
         << "  BACKSPACE       - Remover ultimo waypoint do objeto\n"
         << "  L               - Salvar waypoints em '" << TRAJ_FILENAME << "'\n"
         << "  O               - Carregar waypoints de '" << TRAJ_FILENAME << "'\n"
         << "  ESC             - Sair\n\n";

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    GLuint shader = setupShader();
    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "texBuff"), 0);
    glActiveTexture(GL_TEXTURE0);

    // Programa e VAO/VBO dinâmico para desenhar waypoints (linhas + pontos)
    GLuint lineShader = setupLineShader();
    GLint  lineViewLoc  = glGetUniformLocation(lineShader, "view");
    GLint  lineProjLoc  = glGetUniformLocation(lineShader, "projection");
    GLint  lineColorLoc = glGetUniformLocation(lineShader, "lineColor");

    GLuint lineVAO = 0, lineVBO = 0;
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    // alocação inicial vazia; será atualizada com glBufferData a cada frame
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_PROGRAM_POINT_SIZE);  // permite gl_PointSize no vertex shader

    Camera camera(glm::vec3(0.0f, 2.0f, 7.0f));
    gCamera = &camera;

    glm::vec3 lightPos(0.0f, 5.0f, 5.0f);
    glUniform3fv(glGetUniformLocation(shader, "lightPos"), 1, glm::value_ptr(lightPos));

    // Lambda auxiliar para carregar um modelo
    auto addModel = [&](const string& file, const string& modelName,
                        glm::vec3 color, glm::vec3 pos)
    {
        OBJModel m;
        m.name     = modelName;
        m.color    = color;
        m.position = pos;

        string texPath;
        int vaoID = loadSimpleOBJ(MODELS_DIR + file, m.nVertices, texPath, m.mat);
        if (vaoID < 0) return;
        m.VAO = (GLuint)vaoID;

        if (!texPath.empty())
        {
            m.texID = loadTexture(texPath);
            if (m.texID)  cout << "Textura carregada: " << texPath << "\n";
            else          cerr << "Falha ao carregar textura: " << texPath << "\n";
        }
        else
        {
            cout << "Sem textura para: " << modelName << " (usando cor solida)\n";
        }

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

    // Tenta carregar trajetórias salvas anteriormente
    loadTrajectories();

    // Uniform locations
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

        // ── Câmera ────────────────────────────────────────────────────────
        camera.processKeyboard(window, dt);

        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 proj = camera.getProjectionMatrix(aspect);

        glUniformMatrix4fv(viewLoc,  1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc,  1, GL_FALSE, glm::value_ptr(proj));
        glUniform3fv(camPosLoc, 1, glm::value_ptr(camera.position));

        // ── Trajetórias: avança todos os objetos que têm trajetória ativa ─
        for (auto& obj : objects)
        {
            if (obj.traj.active && obj.traj.waypoints.size() >= 2)
                obj.position = obj.traj.advance(dt);
        }

        // ── Transformação manual (somente quando trajetória está desligada) ─
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
                if (keys[GLFW_KEY_RIGHT_BRACKET])                           delta =  SCALE_SPEED * dt;
                if (keys[GLFW_KEY_LEFT_BRACKET] || keys[GLFW_KEY_MINUS])   delta = -SCALE_SPEED * dt;
                if (delta != 0.0f)
                    sel.scale = glm::max(sel.scale + glm::vec3(delta), glm::vec3(SCALE_MIN));
            }
        }

        // Rotação automática contínua para qualquer objeto com eixo ativo
        for (auto& o : objects)
            if (o.rotX || o.rotY || o.rotZ)
                o.rotAngle += ROT_SPEED * dt;

        // ── Renderização ─────────────────────────────────────────────────
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

            if (o.texID != 0)
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

            // Contorno do objeto selecionado
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

        // ── Desenho dos waypoints (linhas + pontos) ──────────────────────
        // Para cada objeto com waypoints, traça o polígono cíclico e marca
        // os pontos. Objeto selecionado em amarelo, demais em cinza claro.
        glUseProgram(lineShader);
        glUniformMatrix4fv(lineViewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(lineProjLoc, 1, GL_FALSE, glm::value_ptr(proj));
        glBindVertexArray(lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);

        for (size_t i = 0; i < objects.size(); ++i)
        {
            const auto& wps = objects[i].traj.waypoints;
            if (wps.empty()) continue;

            bool      isSel = ((int)i == activeObj);
            glm::vec3 col   = isSel ? glm::vec3(1.0f, 0.85f, 0.1f)   // amarelo
                                    : glm::vec3(0.6f, 0.6f, 0.7f);  // cinza azulado
            glUniform3fv(lineColorLoc, 1, glm::value_ptr(col));

            glBufferData(GL_ARRAY_BUFFER,
                         wps.size() * sizeof(glm::vec3),
                         wps.data(), GL_DYNAMIC_DRAW);

            // Linhas conectando os pontos (LINE_LOOP fecha o ciclo)
            if (wps.size() >= 2)
                glDrawArrays(GL_LINE_LOOP, 0, (GLsizei)wps.size());

            // Pontos por cima das linhas
            glDrawArrays(GL_POINTS, 0, (GLsizei)wps.size());
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(shader);  // restaura o shader principal

        glfwSwapBuffers(window);
    }

    // Limpeza dos recursos de linha
    glDeleteBuffers(1, &lineVBO);
    glDeleteVertexArrays(1, &lineVAO);
    glDeleteProgram(lineShader);

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

    // ── Seleção ──────────────────────────────────────────────────────────
    if (key == GLFW_KEY_TAB)
    {
        activeObj = (activeObj + 1) % (int)objects.size();
        updateWindowTitle(window);
        return;
    }

    // ── Modos de transformação ───────────────────────────────────────────
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

    if (currentMode == MODE_ROTATE)
    {
        OBJModel& obj = objects[activeObj];
        if (key == GLFW_KEY_X) { obj.rotX = !obj.rotX; if (obj.rotX) { obj.rotY = obj.rotZ = false; } }
        if (key == GLFW_KEY_Y) { obj.rotY = !obj.rotY; if (obj.rotY) { obj.rotX = obj.rotZ = false; } }
        if (key == GLFW_KEY_Z) { obj.rotZ = !obj.rotZ; if (obj.rotZ) { obj.rotX = obj.rotY = false; } }
    }

    // ── Trajetória ───────────────────────────────────────────────────────

    // P – adiciona waypoint na posição atual
    if (key == GLFW_KEY_P)
    {
        OBJModel& obj = objects[activeObj];
        obj.traj.waypoints.push_back(obj.position);
        int n = (int)obj.traj.waypoints.size();
        cout << "[Trajetoria] Waypoint " << n << " adicionado para '"
             << obj.name << "': ("
             << obj.position.x << ", "
             << obj.position.y << ", "
             << obj.position.z << ")\n";
        updateWindowTitle(window);
        return;
    }

    // C – liga/desliga seguir trajetória
    if (key == GLFW_KEY_C)
    {
        OBJModel& obj = objects[activeObj];
        if (obj.traj.waypoints.size() < 2)
        {
            cout << "[Trajetoria] Adicione pelo menos 2 waypoints antes de ligar a trajetoria.\n";
            return;
        }
        obj.traj.active = !obj.traj.active;

        // Ao ligar, reinicia no primeiro waypoint
        if (obj.traj.active)
        {
            obj.traj.currentSeg = 0;
            obj.traj.t          = 0.0f;
            obj.position        = obj.traj.waypoints[0];
        }
        cout << "[Trajetoria] " << obj.name
             << " – trajetoria " << (obj.traj.active ? "LIGADA" : "DESLIGADA")
             << " (" << obj.traj.waypoints.size() << " waypoints)\n";
        updateWindowTitle(window);
        return;
    }

    // BACKSPACE – remove último waypoint
    if (key == GLFW_KEY_BACKSPACE)
    {
        OBJModel& obj = objects[activeObj];
        if (!obj.traj.waypoints.empty())
        {
            obj.traj.waypoints.pop_back();
            // Trajetória precisa de pelo menos 2 waypoints — desliga caso contrário
            if (obj.traj.waypoints.size() < 2)
            {
                if (obj.traj.active)
                    cout << "[Trajetoria] " << obj.name
                         << " – trajetoria DESLIGADA (waypoints insuficientes)\n";
                obj.traj.active     = false;
                obj.traj.currentSeg = 0;
                obj.traj.t          = 0.0f;
            }
            else
            {
                // Garante que currentSeg seja válido
                obj.traj.currentSeg = obj.traj.currentSeg % (int)obj.traj.waypoints.size();
            }
            cout << "[Trajetoria] Ultimo waypoint removido de '"
                 << obj.name << "'. Restam: " << obj.traj.waypoints.size() << "\n";
        }
        else
        {
            cout << "[Trajetoria] Nenhum waypoint para remover.\n";
        }
        updateWindowTitle(window);
        return;
    }

    // L – salvar waypoints
    if (key == GLFW_KEY_L)
    {
        saveTrajectories();
        return;
    }

    // O – carregar waypoints
    if (key == GLFW_KEY_O)
    {
        loadTrajectories();
        return;
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (gCamera) gCamera->processMouse(xpos, ypos);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (gCamera) gCamera->processScroll(yoffset);
}


// ─── Trajetórias: salvar / carregar ──────────────────────────────────────────
/*
 Formato do arquivo trajectories.txt:
   object <nome>
   waypoint <x> <y> <z>
   waypoint <x> <y> <z>
   ...
   object <nome>
   ...
*/

void saveTrajectories()
{
    ofstream f(TRAJ_FILENAME);
    if (!f.is_open()) { cerr << "Erro ao salvar " << TRAJ_FILENAME << "\n"; return; }

    // Precisão alta para round-trip exato de save/load
    f << std::setprecision(9);

    int saved = 0;
    for (const auto& obj : objects)
    {
        if (obj.traj.waypoints.empty()) continue;  // não polui o arquivo
        f << "object " << obj.name << "\n";
        for (const auto& wp : obj.traj.waypoints)
            f << "waypoint " << wp.x << " " << wp.y << " " << wp.z << "\n";
        ++saved;
    }
    f.close();
    cout << "[Trajetoria] Waypoints salvos em '" << TRAJ_FILENAME
         << "' (" << saved << " objeto(s))\n";
}

void loadTrajectories()
{
    ifstream f(TRAJ_FILENAME);
    if (!f.is_open())
    {
        cout << "[Trajetoria] Arquivo '" << TRAJ_FILENAME << "' nao encontrado.\n";
        return;
    }

    cout << "[Trajetoria] Carregando '" << TRAJ_FILENAME
         << "' (waypoints atuais serao substituidos)...\n";

    // Limpa waypoints existentes e desativa qualquer trajetória em execução
    for (auto& obj : objects)
    {
        obj.traj.waypoints.clear();
        obj.traj.active     = false;
        obj.traj.currentSeg = 0;
        obj.traj.t          = 0.0f;
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
                cerr << "[Trajetoria] Objeto '" << name << "' nao encontrado na cena.\n";
        }
        else if (word == "waypoint" && current)
        {
            glm::vec3 wp;
            ss >> wp.x >> wp.y >> wp.z;
            current->traj.waypoints.push_back(wp);
        }
    }
    f.close();
    cout << "[Trajetoria] Waypoints carregados de '" << TRAJ_FILENAME << "'\n";
    for (const auto& obj : objects)
        cout << "  " << obj.name << ": " << obj.traj.waypoints.size() << " waypoints\n";
}


// ─── Atualização do título da janela ─────────────────────────────────────────

void updateWindowTitle(GLFWwindow* window)
{
    string modeStr;
    switch (currentMode)
    {
        case MODE_TRANSLATE: modeStr = "Translacao (Setas/I/K)"; break;
        case MODE_ROTATE:    modeStr = "Rotacao (X/Y/Z)";        break;
        case MODE_SCALE:     modeStr = "Escala (]/[)";           break;
    }

    const OBJModel& obj = objects[activeObj];
    string trajInfo;
    if (obj.traj.active)
        trajInfo = " [TRAJ:ON wp=" + to_string(obj.traj.waypoints.size()) + "]";
    else if (!obj.traj.waypoints.empty())
        trajInfo = " [wp=" + to_string(obj.traj.waypoints.size()) + "]";

    string title = "M6 | " + obj.name
                 + "  [" + to_string(activeObj + 1) + "/" + to_string(objects.size()) + "]"
                 + "  Modo: " + modeStr
                 + trajInfo
                 + "  | P=add C=toggle L=save O=load";
    glfwSetWindowTitle(window, title.c_str());
}


// ─── Setup do shader ─────────────────────────────────────────────────────────

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


// ─── Carregamento de textura ─────────────────────────────────────────────────

GLuint loadTexture(const string& filePath)
{
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filePath.c_str(), &w, &h, &ch, 0);

    if (data)
    {
        GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
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


// ─── Carregamento de OBJ ─────────────────────────────────────────────────────

int loadSimpleOBJ(const string& filePath, int& nVertices, string& texturePath, Material& mat)
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
            // Coleta todos os vértices da face primeiro (pode ser triângulo,
            // quad ou n-gon) e depois faz fan-triangulation: (0,1,2)(0,2,3)...
            // Também trata índices negativos do OBJ (relativos ao fim).
            struct FaceIdx { int v, t, n; };
            vector<FaceIdx> face;

            auto resolveIdx = [](const string& s, int listSize) -> int
            {
                if (s.empty()) return -1;
                int v;
                try { v = stoi(s); } catch (...) { return -1; }
                if (v > 0)        return v - 1;        // 1-based -> 0-based
                else if (v < 0)   return listSize + v; // -1 -> último
                else              return -1;           // índice 0 inválido em OBJ
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

            // Empurra um vértice (com pos/tex/normal) ao vBuffer
            auto pushVert = [&](const FaceIdx& fi)
            {
                if (fi.v >= 0 && fi.v < (int)positions.size())
                {
                    vBuffer.push_back(positions[fi.v].x);
                    vBuffer.push_back(positions[fi.v].y);
                    vBuffer.push_back(positions[fi.v].z);
                }
                else
                {
                    vBuffer.push_back(0.0f); vBuffer.push_back(0.0f); vBuffer.push_back(0.0f);
                }

                if (fi.t >= 0 && fi.t < (int)texCoords.size())
                    { vBuffer.push_back(texCoords[fi.t].s); vBuffer.push_back(texCoords[fi.t].t); }
                else
                    { vBuffer.push_back(0.0f); vBuffer.push_back(0.0f); }

                if (fi.n >= 0 && fi.n < (int)normals.size())
                    { vBuffer.push_back(normals[fi.n].x); vBuffer.push_back(normals[fi.n].y); vBuffer.push_back(normals[fi.n].z); }
                else
                    { vBuffer.push_back(0.0f); vBuffer.push_back(1.0f); vBuffer.push_back(0.0f); }
            };

            // Fan triangulation: (0,1,2), (0,2,3), (0,3,4) ...
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
    return (int)VAO;
}

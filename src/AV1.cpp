/* AV1 – Computação Gráfica
 * Selecionando e aplicando transformações em objetos 3D
 *
 * Controles:
 *   TAB        : selecionar próximo objeto (cicla pela lista)
 *   R          : modo Rotação
 *   T          : modo Translação
 *   S          : modo Escala
 *
 *   Modo Rotação (R):
 *     X/Y/Z    : ativar/desativar rotação contínua no eixo correspondente
 *
 *   Modo Translação (T):
 *     W / ↑    : mover -Z (frente)
 *     S / ↓    : mover +Z (trás)
 *     A / ←    : mover -X (esquerda)
 *     D / →    : mover +X (direita)
 *     I        : mover +Y (cima)
 *     K        : mover -Y (baixo)
 *
 *   Modo Escala (S):
 *     ]        : aumentar escala (uniforme ou no eixo selecionado)
 *     [        : diminuir escala (uniforme ou no eixo selecionado)
 *     X/Y/Z    : selecionar eixo (pressione novamente para voltar ao modo uniforme)
 *
 *   ESC        : sair
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ─── Constants ────────────────────────────────────────────────────────────────

const GLuint WIDTH  = 1000;
const GLuint HEIGHT = 800;

const string MODELS_DIR = "../assets/Modelos3D/";

const float TRANSLATE_SPEED = 2.5f; // units / second
const float SCALE_SPEED     = 1.0f; // scale / second
const float ROT_SPEED       = 1.5f; // radians / second
const float SCALE_MIN       = 0.05f;

// ─── Transform mode ───────────────────────────────────────────────────────────

enum TransformMode { MODE_TRANSLATE, MODE_ROTATE, MODE_SCALE };

TransformMode currentMode = MODE_TRANSLATE;
int           scaleAxis   = 0; // 0 = uniform, 1 = X, 2 = Y, 3 = Z

// ─── OBJ model ────────────────────────────────────────────────────────────────

struct OBJModel
{
    GLuint    VAO        = 0;
    int       nVertices  = 0;
    glm::vec3 position   = glm::vec3(0.0f);
    glm::vec3 scale      = glm::vec3(1.0f);
    float     rotAngle   = 0.0f;
    bool      rotX       = false;
    bool      rotY       = false;
    bool      rotZ       = false;
    glm::vec3 color;
    string    name;
};

vector<OBJModel> objects;
int              activeObj = 0;

bool keys[1024] = {};

// ─── Shaders ──────────────────────────────────────────────────────────────────

const GLchar* vertexShaderSource = R"glsl(
#version 450
layout(location = 0) in vec3 position;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 objectColor;
out vec3 fragColor;
void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);
    fragColor   = objectColor;
}
)glsl";

const GLchar* fragmentShaderSource = R"glsl(
#version 450
in  vec3 fragColor;
out vec4 color;
void main()
{
    color = vec4(fragColor, 1.0);
}
)glsl";

// ─── Forward declarations ─────────────────────────────────────────────────────

void   key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
GLuint setupShader();
int    loadSimpleOBJ(const string& filePath, int& nVertices);
void   updateWindowTitle(GLFWwindow* window);

// ─── Main ─────────────────────────────────────────────────────────────────────

int main()
{
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "AV1", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
    cout << "OpenGL:   " << glGetString(GL_VERSION)  << "\n";
    cout << "\nControles:\n"
         << "  TAB        - Selecionar proximo objeto\n"
         << "  R          - Modo Rotacao  (X/Y/Z para eixo)\n"
         << "  T          - Modo Translacao (WASD / setas / I·K)\n"
         << "  S          - Modo Escala  ([/] escala; X/Y/Z seleciona eixo)\n"
         << "  ESC        - Sair\n\n";

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    GLuint shader = setupShader();

    // ── Load models ──────────────────────────────────────────────────────────
    auto addModel = [&](const string& file, const string& name,
                        glm::vec3 color, glm::vec3 pos)
    {
        OBJModel m;
        m.name     = name;
        m.color    = color;
        m.position = pos;
        int vaoID  = loadSimpleOBJ(MODELS_DIR + file, m.nVertices);
        if (vaoID >= 0)
        {
            m.VAO = (GLuint)vaoID;
            objects.push_back(m);
        }
    };

    addModel("Suzanne.obj",        "Suzanne",        glm::vec3(1.0f, 0.35f, 0.35f), glm::vec3(-2.5f, 0.0f, 0.0f));
    addModel("SuzanneSubdiv1.obj", "SuzanneSubdiv1", glm::vec3(0.35f, 0.9f, 0.9f),  glm::vec3( 0.0f, 0.0f, 0.0f));
    addModel("Cube.obj",           "Cube",            glm::vec3(0.4f, 1.0f, 0.45f),  glm::vec3( 2.5f, 0.0f, 0.0f));

    if (objects.empty())
    {
        cerr << "Nenhum modelo carregado. Verifique os caminhos dos arquivos .OBJ.\n";
        glfwTerminate();
        return -1;
    }

    // ── Matrices ──────────────────────────────────────────────────────────────
    glUseProgram(shader);

    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 2.0f, 7.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 proj = glm::perspective(
        glm::radians(45.0f),
        (float)WIDTH / (float)HEIGHT,
        0.1f, 100.0f
    );

    GLint modelLoc = glGetUniformLocation(shader, "model");
    GLint colorLoc = glGetUniformLocation(shader, "objectColor");

    glUniformMatrix4fv(glGetUniformLocation(shader, "view"),       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(proj));

    glEnable(GL_DEPTH_TEST);

    updateWindowTitle(window);

    float prevTime = (float)glfwGetTime();

    // ── Render loop ───────────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(window))
    {
        float currTime = (float)glfwGetTime();
        float dt       = currTime - prevTime;
        prevTime       = currTime;

        glfwPollEvents();

        OBJModel& obj = objects[activeObj];

        // Continuous translation
        if (currentMode == MODE_TRANSLATE)
        {
            float step = TRANSLATE_SPEED * dt;
            if (keys[GLFW_KEY_W] || keys[GLFW_KEY_UP])    obj.position.z -= step;
            if (keys[GLFW_KEY_S] || keys[GLFW_KEY_DOWN])  obj.position.z += step;
            if (keys[GLFW_KEY_A] || keys[GLFW_KEY_LEFT])  obj.position.x -= step;
            if (keys[GLFW_KEY_D] || keys[GLFW_KEY_RIGHT]) obj.position.x += step;
            if (keys[GLFW_KEY_I])                          obj.position.y += step;
            if (keys[GLFW_KEY_K])                          obj.position.y -= step;
        }

        // Continuous scale
        if (currentMode == MODE_SCALE)
        {
            float delta = 0.0f;
            if (keys[GLFW_KEY_RIGHT_BRACKET]) delta =  SCALE_SPEED * dt;
            if (keys[GLFW_KEY_LEFT_BRACKET])  delta = -SCALE_SPEED * dt;
            if (delta != 0.0f)
            {
                switch (scaleAxis)
                {
                    case 0:
                        obj.scale = glm::max(obj.scale + glm::vec3(delta), glm::vec3(SCALE_MIN));
                        break;
                    case 1:
                        obj.scale.x = max(SCALE_MIN, obj.scale.x + delta);
                        break;
                    case 2:
                        obj.scale.y = max(SCALE_MIN, obj.scale.y + delta);
                        break;
                    case 3:
                        obj.scale.z = max(SCALE_MIN, obj.scale.z + delta);
                        break;
                }
            }
        }

        // Advance rotation angle for rotating objects
        for (auto& o : objects)
            if (o.rotX || o.rotY || o.rotZ)
                o.rotAngle += ROT_SPEED * dt;

        // ── Draw ──────────────────────────────────────────────────────────────
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

            glBindVertexArray(o.VAO);

            // Pass 1 – solid fill (pushed back slightly to avoid z-fighting with wireframe)
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.0f, 1.0f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glUniform3fv(colorLoc, 1, glm::value_ptr(o.color));
            glDrawArrays(GL_TRIANGLES, 0, o.nVertices);
            glDisable(GL_POLYGON_OFFSET_FILL);

            // Pass 2 – wireframe overlay on the selected object (DESAFIO)
            if (selected)
            {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glm::vec3 wf = glm::vec3(1.0f); // white wireframe
                glUniform3fv(colorLoc, 1, glm::value_ptr(wf));
                glDrawArrays(GL_TRIANGLES, 0, o.nVertices);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }

            glBindVertexArray(0);
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

// ─── Callbacks ────────────────────────────────────────────────────────────────

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
        return;
    }

    // Track key state for continuous input
    if (key >= 0 && key < 1024)
    {
        if      (action == GLFW_PRESS)   keys[key] = true;
        else if (action == GLFW_RELEASE) keys[key] = false;
    }

    // One-shot actions on press only
    if (action != GLFW_PRESS) return;

    // Cycle selected object
    if (key == GLFW_KEY_TAB)
    {
        activeObj = (activeObj + 1) % (int)objects.size();
        scaleAxis = 0; // reset scale-axis selection on object change
        updateWindowTitle(window);
        return;
    }

    // Mode switches
    if (key == GLFW_KEY_T) { currentMode = MODE_TRANSLATE; scaleAxis = 0; updateWindowTitle(window); return; }
    if (key == GLFW_KEY_R) { currentMode = MODE_ROTATE;    updateWindowTitle(window); return; }
    if (key == GLFW_KEY_S) { currentMode = MODE_SCALE;     scaleAxis = 0; updateWindowTitle(window); return; }

    OBJModel& obj = objects[activeObj];

    // Rotation axis toggle (ROTATE mode)
    if (currentMode == MODE_ROTATE)
    {
        if (key == GLFW_KEY_X) { obj.rotX = !obj.rotX; obj.rotY = obj.rotZ = false; }
        if (key == GLFW_KEY_Y) { obj.rotY = !obj.rotY; obj.rotX = obj.rotZ = false; }
        if (key == GLFW_KEY_Z) { obj.rotZ = !obj.rotZ; obj.rotX = obj.rotY = false; }
    }

    // Scale axis selection (SCALE mode) — pressing same key again returns to uniform
    if (currentMode == MODE_SCALE)
    {
        if (key == GLFW_KEY_X) { scaleAxis = (scaleAxis == 1) ? 0 : 1; updateWindowTitle(window); }
        if (key == GLFW_KEY_Y) { scaleAxis = (scaleAxis == 2) ? 0 : 2; updateWindowTitle(window); }
        if (key == GLFW_KEY_Z) { scaleAxis = (scaleAxis == 3) ? 0 : 3; updateWindowTitle(window); }
    }
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

void updateWindowTitle(GLFWwindow* window)
{
    static const char* axisNames[] = { "Uniforme", "X", "Y", "Z" };

    string modeStr;
    switch (currentMode)
    {
        case MODE_TRANSLATE: modeStr = "Translacao";                                    break;
        case MODE_ROTATE:    modeStr = "Rotacao";                                       break;
        case MODE_SCALE:     modeStr = string("Escala [") + axisNames[scaleAxis] + "]"; break;
    }

    string title = "AV1 | Objeto: " + objects[activeObj].name
                 + "  [" + to_string(activeObj + 1) + "/" + to_string(objects.size()) + "]"
                 + "  | Modo: " + modeStr;
    glfwSetWindowTitle(window, title.c_str());
}

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

int loadSimpleOBJ(const string& filePath, int& nVertices)
{
    vector<glm::vec3> positions;
    vector<glm::vec2> texCoords;
    vector<glm::vec3> normals;
    vector<GLfloat>   vBuffer;

    ifstream file(filePath);
    if (!file.is_open())
    {
        cerr << "Erro ao abrir: " << filePath << "\n";
        return -1;
    }

    string line;
    while (getline(file, line))
    {
        istringstream ss(line);
        string word;
        ss >> word;

        if (word == "v")
        {
            glm::vec3 v;
            ss >> v.x >> v.y >> v.z;
            positions.push_back(v);
        }
        else if (word == "vt")
        {
            glm::vec2 vt;
            ss >> vt.s >> vt.t;
            texCoords.push_back(vt);
        }
        else if (word == "vn")
        {
            glm::vec3 vn;
            ss >> vn.x >> vn.y >> vn.z;
            normals.push_back(vn);
        }
        else if (word == "f")
        {
            while (ss >> word)
            {
                istringstream si(word);
                string idx;
                int vi = 0;
                if (getline(si, idx, '/')) vi = !idx.empty() ? stoi(idx) - 1 : 0;
                // ti and ni are parsed but not stored (uniform color is used instead)
                vBuffer.push_back(positions[vi].x);
                vBuffer.push_back(positions[vi].y);
                vBuffer.push_back(positions[vi].z);
            }
        }
    }

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    // location 0: position (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = (int)(vBuffer.size() / 3);
    cout << "Carregado: " << filePath << "  (" << nVertices << " vertices)\n";
    return (int)VAO;
}

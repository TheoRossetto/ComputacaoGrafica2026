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
 *     ↓        : mover +Z (trás)   [S é reservado para o modo Escala]
 *     A / ←    : mover -X (esquerda)
 *     D / →    : mover +X (direita)
 *     I        : mover +Y (cima)
 *     K        : mover -Y (baixo)
 *
 *   Modo Escala (S):
 *     ]        : aumentar escala (uniforme ou no eixo selecionado)
 *     [ / -    : diminuir escala (uniforme ou no eixo selecionado)
 *     X/Y/Z    : selecionar eixo (pressione novamente para voltar ao modo uniforme)
 *
 *   ESC: sair
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


const GLuint WIDTH  = 1000;
const GLuint HEIGHT = 800;

const string MODELS_DIR = "../assets/Modelos3D/";

const float TRANSLATE_SPEED = 2.5f;
const float SCALE_SPEED     = 1.0f;
const float ROT_SPEED       = 1.5f;
const float SCALE_MIN       = 0.05f;


enum TransformMode { MODE_TRANSLATE, MODE_ROTATE, MODE_SCALE };

TransformMode currentMode = MODE_TRANSLATE;
int           scaleAxis   = 0;


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


void   key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
GLuint setupShader();
int    loadSimpleOBJ(const string& filePath, int& nVertices);
void   updateWindowTitle(GLFWwindow* window);


int main()
{
    if (!glfwInit())
    {
        cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "AV1", nullptr, nullptr);
    if (!window)
    {
        cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync
    glfwSetKeyCallback(window, key_callback);
    glfwSetWindowFocusCallback(window, [](GLFWwindow*, int focused) {
        if (!focused) std::fill(std::begin(keys), std::end(keys), false);
    });

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
         << "  T          - Modo Translacao (W/A/D/setas / I·K; DOWN=tras)\n"
         << "  S          - Modo Escala  (]/[ ou - escala; X/Y/Z seleciona eixo)\n"
         << "  ESC        - Sair\n\n";

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    GLuint shader = setupShader();

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

    while (!glfwWindowShouldClose(window))
    {
        float currTime = (float)glfwGetTime();
        float dt       = currTime - prevTime;
        prevTime       = currTime;

        glfwPollEvents();

        OBJModel& obj = objects[activeObj];

        if (currentMode == MODE_TRANSLATE)
        {
            float step = TRANSLATE_SPEED * dt;
            if (keys[GLFW_KEY_W] || keys[GLFW_KEY_UP])    obj.position.z -= step;
            if (keys[GLFW_KEY_DOWN])                       obj.position.z += step;
            if (keys[GLFW_KEY_A] || keys[GLFW_KEY_LEFT])  obj.position.x -= step;
            if (keys[GLFW_KEY_D] || keys[GLFW_KEY_RIGHT]) obj.position.x += step;
            if (keys[GLFW_KEY_I])                          obj.position.y += step;
            if (keys[GLFW_KEY_K])                          obj.position.y -= step;
        }

        if (currentMode == MODE_SCALE)
        {
            float delta = 0.0f;
            if (keys[GLFW_KEY_RIGHT_BRACKET])                          delta =  SCALE_SPEED * dt;
            if (keys[GLFW_KEY_LEFT_BRACKET] || keys[GLFW_KEY_MINUS]) delta = -SCALE_SPEED * dt;
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

        for (auto& o : objects)
            if (o.rotX || o.rotY || o.rotZ)
                o.rotAngle += ROT_SPEED * dt;

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

            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.0f, 1.0f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glUniform3fv(colorLoc, 1, glm::value_ptr(o.color));
            glDrawArrays(GL_TRIANGLES, 0, o.nVertices);
            glDisable(GL_POLYGON_OFFSET_FILL);

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

    if (key == GLFW_KEY_TAB)
    {
        activeObj = (activeObj + 1) % (int)objects.size();
        scaleAxis = 0;
        updateWindowTitle(window);
        return;
    }

    if (key == GLFW_KEY_T)
    {
        if (currentMode == MODE_ROTATE) { objects[activeObj].rotX = objects[activeObj].rotY = objects[activeObj].rotZ = false; }
        currentMode = MODE_TRANSLATE; scaleAxis = 0; updateWindowTitle(window); return;
    }
    if (key == GLFW_KEY_R) { currentMode = MODE_ROTATE; updateWindowTitle(window); return; }
    if (key == GLFW_KEY_S)
    {
        if (currentMode == MODE_ROTATE) { objects[activeObj].rotX = objects[activeObj].rotY = objects[activeObj].rotZ = false; }
        currentMode = MODE_SCALE; scaleAxis = 0; updateWindowTitle(window); return;
    }

    OBJModel& obj = objects[activeObj];

    if (currentMode == MODE_ROTATE)
    {
        if (key == GLFW_KEY_X) { obj.rotX = !obj.rotX; if (obj.rotX) { obj.rotY = obj.rotZ = false; } }
        if (key == GLFW_KEY_Y) { obj.rotY = !obj.rotY; if (obj.rotY) { obj.rotX = obj.rotZ = false; } }
        if (key == GLFW_KEY_Z) { obj.rotZ = !obj.rotZ; if (obj.rotZ) { obj.rotX = obj.rotY = false; } }
    }

    if (currentMode == MODE_SCALE)
    {
        if (key == GLFW_KEY_X) { scaleAxis = (scaleAxis == 1) ? 0 : 1; updateWindowTitle(window); }
        if (key == GLFW_KEY_Y) { scaleAxis = (scaleAxis == 2) ? 0 : 2; updateWindowTitle(window); }
        if (key == GLFW_KEY_Z) { scaleAxis = (scaleAxis == 3) ? 0 : 3; updateWindowTitle(window); }
    }
}


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

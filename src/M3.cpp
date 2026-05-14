/* M3 – Visualizador 3D com Texturas
 *
 * Adaptado por Rossana Baptista Queiroz
 * para as disciplinas de Processamento Gráfico/Computação Gráfica - Unisinos
 * Baseado em AV1.cpp — adiciona suporte a texturas via .MTL e stb_image
 *
 * O programa carrega arquivos .OBJ, lê o .MTL referenciado para obter o nome
 * da textura (map_Kd) e renderiza os objetos com a textura aplicada.
 * Objetos sem textura são renderizados com cor sólida.
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
 *     ↓        : mover +Z (trás)
 *     A / ←    : mover -X (esquerda)
 *     D / →    : mover +X (direita)
 *     I        : mover +Y (cima)
 *     K        : mover -Y (baixo)
 *
 *   Modo Escala (S):
 *     ]        : aumentar escala uniforme
 *     [ / -    : diminuir escala uniforme
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

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


// ---------------------------------------------------------------------------
// Constantes globais
// ---------------------------------------------------------------------------
const GLuint WIDTH  = 1000;
const GLuint HEIGHT = 800;

const string MODELS_DIR = "../assets/Modelos3D/";

const float TRANSLATE_SPEED = 2.5f;
const float SCALE_SPEED     = 1.0f;
const float ROT_SPEED       = 1.5f;
const float SCALE_MIN       = 0.05f;


// ---------------------------------------------------------------------------
// Shaders
//
// O vertex shader passa a coordenada de textura ao fragment shader.
// O fragment shader usa o uniform 'useTexture':
//   useTexture == 1 → amostra a textura
//   useTexture == 0 → usa a cor sólida 'objectColor'
// ---------------------------------------------------------------------------
const GLchar* vertexShaderSource = R"glsl(
#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texc;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 texCoord;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);
    texCoord = texc;
}
)glsl";

const GLchar* fragmentShaderSource = R"glsl(
#version 450
in vec2 texCoord;

uniform sampler2D texBuff;
uniform int       useTexture;   // 1 = usar textura, 0 = usar cor solida
uniform vec3      objectColor;

out vec4 color;

void main()
{
    if (useTexture == 1)
        color = texture(texBuff, texCoord);
    else
        color = vec4(objectColor, 1.0);
}
)glsl";


// ---------------------------------------------------------------------------
// Estruturas de dados
// ---------------------------------------------------------------------------
enum TransformMode { MODE_TRANSLATE, MODE_ROTATE, MODE_SCALE };

TransformMode currentMode = MODE_TRANSLATE;

struct OBJModel
{
    GLuint    VAO        = 0;
    GLuint    texID      = 0;      // 0 = sem textura
    int       nVertices  = 0;
    glm::vec3 position   = glm::vec3(0.0f);
    glm::vec3 scale      = glm::vec3(1.0f);
    float     rotAngle   = 0.0f;
    bool      rotX       = false;
    bool      rotY       = false;
    bool      rotZ       = false;
    glm::vec3 color      = glm::vec3(1.0f);
    string    name;
};

vector<OBJModel> objects;
int              activeObj = 0;

bool keys[1024] = {};


// ---------------------------------------------------------------------------
// Protótipos
// ---------------------------------------------------------------------------
void   key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
GLuint setupShader();
int    loadSimpleOBJ(const string& filePath, int& nVertices, string& texturePath);
GLuint loadTexture(const string& filePath);
void   updateWindowTitle(GLFWwindow* window);


// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    if (!glfwInit())
    {
        cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "M3 - Objetos Texturizados", nullptr, nullptr);
    if (!window)
    {
        cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, key_callback);
    glfwSetWindowFocusCallback(window, [](GLFWwindow*, int focused) {
        if (!focused) fill(begin(keys), end(keys), false);
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
         << "  S          - Modo Escala  (]/[ escala uniforme)\n"
         << "  ESC        - Sair\n\n";

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    GLuint shader = setupShader();
    glUseProgram(shader);

    // Slot de textura 0
    glUniform1i(glGetUniformLocation(shader, "texBuff"), 0);
    glActiveTexture(GL_TEXTURE0);

    // Carrega modelos com suas texturas
    auto addModel = [&](const string& file, const string& name,
                        glm::vec3 color, glm::vec3 pos)
    {
        OBJModel m;
        m.name     = name;
        m.color    = color;
        m.position = pos;

        string texPath;
        int vaoID = loadSimpleOBJ(MODELS_DIR + file, m.nVertices, texPath);
        if (vaoID < 0) return;

        m.VAO = (GLuint)vaoID;

        if (!texPath.empty())
        {
            m.texID = loadTexture(texPath);
            if (m.texID != 0)
                cout << "Textura carregada: " << texPath << "\n";
            else
                cerr << "Falha ao carregar textura: " << texPath << "\n";
        }
        else
        {
            cout << "Sem textura para: " << name << " (usando cor solida)\n";
        }

        objects.push_back(m);
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

    GLint modelLoc      = glGetUniformLocation(shader, "model");
    GLint colorLoc      = glGetUniformLocation(shader, "objectColor");
    GLint useTextureLoc = glGetUniformLocation(shader, "useTexture");

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
            if (keys[GLFW_KEY_RIGHT_BRACKET])                           delta =  SCALE_SPEED * dt;
            if (keys[GLFW_KEY_LEFT_BRACKET] || keys[GLFW_KEY_MINUS])   delta = -SCALE_SPEED * dt;
            if (delta != 0.0f)
                obj.scale = glm::max(obj.scale + glm::vec3(delta), glm::vec3(SCALE_MIN));
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

            // --- Desenho preenchido ---
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.0f, 1.0f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            if (o.texID != 0)
            {
                // Objeto com textura
                glUniform1i(useTextureLoc, 1);
                glBindTexture(GL_TEXTURE_2D, o.texID);
            }
            else
            {
                // Objeto sem textura → cor sólida
                glUniform1i(useTextureLoc, 0);
                glUniform3fv(colorLoc, 1, glm::value_ptr(o.color));
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            glDrawArrays(GL_TRIANGLES, 0, o.nVertices);
            glDisable(GL_POLYGON_OFFSET_FILL);

            // --- Wireframe para objeto selecionado ---
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

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}


// ---------------------------------------------------------------------------
// Callbacks e utilitários
// ---------------------------------------------------------------------------
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
        updateWindowTitle(window);
        return;
    }

    if (key == GLFW_KEY_T)
    {
        if (currentMode == MODE_ROTATE)
            objects[activeObj].rotX = objects[activeObj].rotY = objects[activeObj].rotZ = false;
        currentMode = MODE_TRANSLATE;
        updateWindowTitle(window);
        return;
    }
    if (key == GLFW_KEY_R) { currentMode = MODE_ROTATE;    updateWindowTitle(window); return; }
    if (key == GLFW_KEY_S)
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
}

void updateWindowTitle(GLFWwindow* window)
{
    string modeStr;
    switch (currentMode)
    {
        case MODE_TRANSLATE: modeStr = "Translacao"; break;
        case MODE_ROTATE:    modeStr = "Rotacao";    break;
        case MODE_SCALE:     modeStr = "Escala";     break;
    }
    string title = "M3 | Objeto: " + objects[activeObj].name
                 + "  [" + to_string(activeObj + 1) + "/" + to_string(objects.size()) + "]"
                 + "  | Modo: " + modeStr;
    glfwSetWindowTitle(window, title.c_str());
}


// ---------------------------------------------------------------------------
// Compilação de shaders
// ---------------------------------------------------------------------------
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


// ---------------------------------------------------------------------------
// Carregamento de textura com stb_image
// ---------------------------------------------------------------------------
GLuint loadTexture(const string& filePath)
{
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &nrChannels, 0);

    if (data)
    {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
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


// ---------------------------------------------------------------------------
// Carregamento do arquivo .OBJ
// Layout do vBuffer: x, y, z, s, t  (5 floats por vértice)
// Lê o .MTL referenciado para obter o caminho da textura (map_Kd)
// ---------------------------------------------------------------------------
int loadSimpleOBJ(const string& filePath, int& nVertices, string& texturePath)
{
    vector<glm::vec3> positions;
    vector<glm::vec2> texCoords;
    vector<glm::vec3> normals;
    vector<GLfloat>   vBuffer;

    texturePath = "";
    string mtlFile;
    string directory = filePath.substr(0, filePath.find_last_of("/\\") + 1);

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

        if (word == "mtllib")
        {
            ss >> mtlFile;
        }
        else if (word == "v")
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
                int vi = 0, ti = 0, ni = 0;
                istringstream si(word);
                string idx;

                if (getline(si, idx, '/')) vi = !idx.empty() ? stoi(idx) - 1 : 0;
                if (getline(si, idx, '/')) ti = !idx.empty() ? stoi(idx) - 1 : 0;
                if (getline(si, idx))      ni = !idx.empty() ? stoi(idx) - 1 : 0;

                // Posição
                vBuffer.push_back(positions[vi].x);
                vBuffer.push_back(positions[vi].y);
                vBuffer.push_back(positions[vi].z);

                // Coordenada de textura
                if (!texCoords.empty() && ti >= 0 && ti < (int)texCoords.size())
                {
                    vBuffer.push_back(texCoords[ti].s);
                    vBuffer.push_back(texCoords[ti].t);
                }
                else
                {
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(0.0f);
                }
            }
        }
    }
    file.close();

    // Lê .MTL para obter o nome da textura (map_Kd)
    if (!mtlFile.empty())
    {
        ifstream mtlIn(directory + mtlFile);
        if (mtlIn.is_open())
        {
            string mtlLine;
            while (getline(mtlIn, mtlLine))
            {
                istringstream mtlss(mtlLine);
                string mtlWord;
                mtlss >> mtlWord;
                if (mtlWord == "map_Kd")
                {
                    string texFile;
                    mtlss >> texFile;
                    texturePath = directory + texFile;
                    break;
                }
            }
            mtlIn.close();
        }
    }

    // Cria VBO e VAO
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Atributo 0: posição (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Atributo 1: coordenada de textura (s, t)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = (int)(vBuffer.size() / 5);
    cout << "Carregado: " << filePath << "  (" << nVertices << " vertices)\n";
    return (int)VAO;
}

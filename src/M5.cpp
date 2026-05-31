/*
 M5 - Câmera Sintética em Primeira Pessoa
 Controles:
   WASD - Mover câmera (frente/esq/atrás/dir)
   Mouse - Rotacionar câmera (yaw/pitch)
   Scroll - Zoom (FOV)
   TAB - Selecionar próximo objeto
   R - Modo Rotação  (X/Y/Z para eixo)
   T - Modo Translação (Setas / I·K)
   F - Modo Escala  (]/[ escala uniforme)
   ESC - Sair
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


const GLuint WIDTH  = 1000;
const GLuint HEIGHT = 800;

const string MODELS_DIR = "../assets/Modelos3D/";

const float TRANSLATE_SPEED = 2.5f;
const float SCALE_SPEED     = 1.0f;
const float ROT_SPEED       = 1.5f;
const float SCALE_MIN       = 0.05f;


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


// Classe Camera

class Camera
{
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;

    float yaw;
    float pitch;
    float fov;
    float speed;
    float sensitivity;

    bool  firstMouse;
    float lastX;
    float lastY;

    Camera(glm::vec3 startPos,
           float startYaw   = -90.0f,
           float startPitch =   0.0f)
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

    // WASD – movimento no plano XZ (sem subir/descer)
    void processKeyboard(GLFWwindow* window, float dt)
    {
        float step  = speed * dt;
        glm::vec3 right = glm::normalize(glm::cross(front, up));

        // Projeta front no plano horizontal para não voar
        glm::vec3 flatFront = glm::normalize(glm::vec3(front.x, 0.0f, front.z));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) position += step * flatFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) position -= step * flatFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) position -= step * right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) position += step * right;
    }

    // Movimento do mouse → yaw e pitch → recalcula vetores
    void processMouse(double xpos, double ypos)
    {
        if (firstMouse)
        {
            lastX = (float)xpos;
            lastY = (float)ypos;
            firstMouse = false;
        }

        float xoffset = (float)xpos - lastX;
        float yoffset = lastY - (float)ypos;   // y invertido: tela cresce pra baixo
        lastX = (float)xpos;
        lastY = (float)ypos;

        xoffset *= sensitivity;
        yoffset *= sensitivity;

        yaw   += xoffset;
        pitch += yoffset;

        if (pitch >  89.0f) pitch =  89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        updateVectors();
    }

    // Scroll do mouse → zoom com FOV
    void processScroll(double yoffset)
    {
        fov -= (float)yoffset;
        if (fov <  1.0f) fov =  1.0f;
        if (fov > 45.0f) fov = 45.0f;
    }

private:
    // Recalcula front e up a partir de yaw/pitch
    void updateVectors()
    {
        glm::vec3 newFront;
        newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        newFront.y = sin(glm::radians(pitch));
        newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(newFront);

        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
        up = glm::normalize(glm::cross(right, front));
    }
};

// Estado global

Camera* gCamera = nullptr;   // ponteiro global usado pelos callbacks

enum TransformMode { MODE_TRANSLATE, MODE_ROTATE, MODE_SCALE };
TransformMode currentMode = MODE_TRANSLATE;

struct Material
{
    glm::vec3 Ka = glm::vec3(0.2f);
    glm::vec3 Kd = glm::vec3(0.8f);
    glm::vec3 Ks = glm::vec3(0.5f);
    float     Ns = 32.0f;
};

struct OBJModel
{
    GLuint    VAO       = 0;
    GLuint    texID     = 0;
    int       nVertices = 0;
    glm::vec3 position  = glm::vec3(0.0f);
    glm::vec3 scale     = glm::vec3(1.0f);
    float     rotAngle  = 0.0f;
    bool      rotX      = false;
    bool      rotY      = false;
    bool      rotZ      = false;
    glm::vec3 color     = glm::vec3(1.0f);
    string    name;
    Material  mat;
};

vector<OBJModel> objects;
int              activeObj = 0;
bool             keys[1024] = {};


// Declarações

void   key_callback   (GLFWwindow*, int, int, int, int);
void   mouse_callback (GLFWwindow*, double, double);
void   scroll_callback(GLFWwindow*, double, double);
GLuint setupShader();
int    loadSimpleOBJ(const string&, int&, string&, Material&);
GLuint loadTexture(const string&);
void   updateWindowTitle(GLFWwindow*);


// main

int main()
{
    if (!glfwInit()) { cerr << "Failed to initialize GLFW\n"; return -1; }

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT,
        "M5 - Camera em Primeira Pessoa", nullptr, nullptr);
    if (!window) { cerr << "Failed to create GLFW window\n"; glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Travar e ocultar cursor para câmera FPS
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
    cout << "\nControles:\n"
         << "  WASD       - Mover camera\n"
         << "  Mouse      - Rotacionar camera\n"
         << "  Scroll     - Zoom (FOV)\n"
         << "  TAB        - Selecionar proximo objeto\n"
         << "  R          - Modo Rotacao  (X/Y/Z para eixo)\n"
         << "  T          - Modo Translacao (Setas / I·K)\n"
         << "  F          - Modo Escala  (]/[ escala uniforme)\n"
         << "  ESC        - Sair\n\n";

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    GLuint shader = setupShader();
    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "texBuff"), 0);
    glActiveTexture(GL_TEXTURE0);

    // Câmera começa em (0, 2, 7) olhando para a origem
    Camera camera(glm::vec3(0.0f, 2.0f, 7.0f));
    gCamera = &camera;

    glm::vec3 lightPos(0.0f, 5.0f, 5.0f);
    glUniform3fv(glGetUniformLocation(shader, "lightPos"), 1, glm::value_ptr(lightPos));

    auto addModel = [&](const string& file, const string& name,
                        glm::vec3 color, glm::vec3 pos)
    {
        OBJModel m;
        m.name     = name;
        m.color    = color;
        m.position = pos;

        string texPath;
        int vaoID = loadSimpleOBJ(MODELS_DIR + file, m.nVertices, texPath, m.mat);
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
    addModel("Cube.obj",           "Cube",           glm::vec3(0.4f, 1.0f, 0.45f),  glm::vec3( 2.5f, 0.0f, 0.0f));

    if (objects.empty())
    {
        cerr << "Nenhum modelo carregado. Verifique os caminhos dos arquivos .OBJ.\n";
        glfwTerminate();
        return -1;
    }

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

    // Projection só muda se o FOV muda (scroll) – atualizada no loop
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

        // Atualizar câmera
        camera.processKeyboard(window, dt);

        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 proj = camera.getProjectionMatrix(aspect);

        glUniformMatrix4fv(viewLoc,  1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc,  1, GL_FALSE, glm::value_ptr(proj));
        glUniform3fv(camPosLoc, 1, glm::value_ptr(camera.position));

        // Transformações do objeto selecionado (setas / I-K)
        OBJModel& obj = objects[activeObj];

        if (currentMode == MODE_TRANSLATE)
        {
            float step = TRANSLATE_SPEED * dt;
            if (keys[GLFW_KEY_UP])    obj.position.z -= step;
            if (keys[GLFW_KEY_DOWN])  obj.position.z += step;
            if (keys[GLFW_KEY_LEFT])  obj.position.x -= step;
            if (keys[GLFW_KEY_RIGHT]) obj.position.x += step;
            if (keys[GLFW_KEY_I])     obj.position.y += step;
            if (keys[GLFW_KEY_K])     obj.position.y -= step;
        }

        if (currentMode == MODE_SCALE)
        {
            float delta = 0.0f;
            if (keys[GLFW_KEY_RIGHT_BRACKET])                         delta =  SCALE_SPEED * dt;
            if (keys[GLFW_KEY_LEFT_BRACKET] || keys[GLFW_KEY_MINUS]) delta = -SCALE_SPEED * dt;
            if (delta != 0.0f)
                obj.scale = glm::max(obj.scale + glm::vec3(delta), glm::vec3(SCALE_MIN));
        }

        for (auto& o : objects)
            if (o.rotX || o.rotY || o.rotZ)
                o.rotAngle += ROT_SPEED * dt;

        // Renderização
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


// Callbacks

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
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (gCamera) gCamera->processMouse(xpos, ypos);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (gCamera) gCamera->processScroll(yoffset);
}

void updateWindowTitle(GLFWwindow* window)
{
    string modeStr;
    switch (currentMode)
    {
        case MODE_TRANSLATE: modeStr = "Translacao (Setas/I/K)"; break;
        case MODE_ROTATE:    modeStr = "Rotacao (X/Y/Z)";        break;
        case MODE_SCALE:     modeStr = "Escala (]/[)";           break;
    }
    string title = "M5 | Objeto: " + objects[activeObj].name
                 + "  [" + to_string(activeObj + 1) + "/" + to_string(objects.size()) + "]"
                 + "  | Modo: " + modeStr;
    glfwSetWindowTitle(window, title.c_str());
}


// Setup do shader

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


// Carregamento de textura

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


// Carregamento de OBJ

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
            glm::vec3 v; ss >> v.x >> v.y >> v.z;
            positions.push_back(v);
        }
        else if (word == "vt")
        {
            glm::vec2 vt; ss >> vt.s >> vt.t;
            texCoords.push_back(vt);
        }
        else if (word == "vn")
        {
            glm::vec3 vn; ss >> vn.x >> vn.y >> vn.z;
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

                vBuffer.push_back(positions[vi].x);
                vBuffer.push_back(positions[vi].y);
                vBuffer.push_back(positions[vi].z);

                if (!texCoords.empty() && ti >= 0 && ti < (int)texCoords.size())
                {
                    vBuffer.push_back(texCoords[ti].s);
                    vBuffer.push_back(texCoords[ti].t);
                }
                else { vBuffer.push_back(0.0f); vBuffer.push_back(0.0f); }

                if (!normals.empty() && ni >= 0 && ni < (int)normals.size())
                {
                    vBuffer.push_back(normals[ni].x);
                    vBuffer.push_back(normals[ni].y);
                    vBuffer.push_back(normals[ni].z);
                }
                else { vBuffer.push_back(0.0f); vBuffer.push_back(1.0f); vBuffer.push_back(0.0f); }
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
                string mtlWord;
                mtlss >> mtlWord;

                if      (mtlWord == "map_Kd") { string texFile; mtlss >> texFile; texturePath = directory + texFile; }
                else if (mtlWord == "Ka") mtlss >> mat.Ka.r >> mat.Ka.g >> mat.Ka.b;
                else if (mtlWord == "Kd") mtlss >> mat.Kd.r >> mat.Kd.g >> mat.Kd.b;
                else if (mtlWord == "Ks") mtlss >> mat.Ks.r >> mat.Ks.g >> mat.Ks.b;
                else if (mtlWord == "Ns") mtlss >> mat.Ns;
            }
            mtlIn.close();
        }
        else
        {
            cerr << "Aviso: nao foi possivel abrir " << directory + mtlFile << "\n";
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

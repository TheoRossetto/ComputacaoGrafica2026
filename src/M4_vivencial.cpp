/*
Controles:
TAB: selecionar próximo objeto
R: modo Rotação  (X/Y/Z para eixo)
T: modo Translação (W/A/D/setas / I·K)
S: modo Escala  (]/[ aumentar / diminuir)
1: ligar/desligar luz principal
2: ligar/desligar luz de preenchimento
3: ligar/desligar luz de fundo
ESC: sair
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

const glm::vec3 LIGHT_OFFSETS[3] = {
    glm::vec3(-1.5f,  2.0f,  2.5f),
    glm::vec3( 2.5f,  0.5f,  2.0f),
    glm::vec3( 0.0f,  1.5f, -3.0f)
};

const glm::vec3 LIGHT_COLORS[3] = {
    glm::vec3(1.0f,  1.0f,  0.95f),
    glm::vec3(0.5f,  0.5f,  0.55f),
    glm::vec3(0.7f,  0.7f,  0.75f)
};


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
uniform vec3      camPos;

uniform vec3  Ka;
uniform vec3  Kd;
uniform vec3  Ks;
uniform float Ns;

struct PointLight {
    vec3 position;
    vec3 color;     // cor × intensidade
    int  on;        // 1 = ligada, 0 = desligada
};
uniform PointLight lights[3];

out vec4 color;

void main()
{
    vec3 baseColor = (useTexture == 1) ? vec3(texture(texBuff, texCoord)) : objectColor;
    vec3 N = normalize(vNormal);
    vec3 V = normalize(camPos - fragPos);

    vec3 result = Ka * baseColor;

    for (int i = 0; i < 3; i++)
    {
        if (lights[i].on == 0) continue;

        vec3  toLight = lights[i].position - fragPos;
        float dist    = length(toLight);
        vec3  L       = normalize(toLight);

        float atten = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

        float diff    = max(dot(N, L), 0.0);
        vec3  diffuse = Kd * diff * lights[i].color * atten;

        vec3  R    = normalize(reflect(-L, N));
        float spec = pow(max(dot(R, V), 0.0), Ns);
        vec3  specular = Ks * spec * lights[i].color * atten;

        result += diffuse * baseColor + specular;
    }

    color = vec4(result, 1.0);
}
)glsl";



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

struct PointLight
{
    glm::vec3 color;
    glm::vec3 position;
    bool      on = true;
};

vector<OBJModel> objects;
int              activeObj = 0;
bool             keys[1024] = {};

PointLight lights[3];



void   key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
GLuint setupShader();
int    loadSimpleOBJ(const string& filePath, int& nVertices, string& texturePath, Material& mat);
GLuint loadTexture(const string& filePath);
void   updateWindowTitle(GLFWwindow* window);



int main()
{
    if (!glfwInit()) { cerr << "Failed to initialize GLFW\n"; return -1; }

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "M4_vivencial - Iluminacao 3 Pontos", nullptr, nullptr);
    if (!window) { cerr << "Failed to create GLFW window\n"; glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, key_callback);
    glfwSetWindowFocusCallback(window, [](GLFWwindow*, int focused) {
        if (!focused) fill(begin(keys), end(keys), false);
    });

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    { cerr << "Failed to initialize GLAD\n"; return -1; }

    cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
    cout << "OpenGL:   " << glGetString(GL_VERSION)  << "\n";
    cout << "\nControles:\n"
         << "  TAB        - Selecionar proximo objeto\n"
         << "  R          - Modo Rotacao  (X/Y/Z para eixo)\n"
         << "  T          - Modo Translacao (W/A/D/setas / I·K)\n"
         << "  S          - Modo Escala  (]/[ escala uniforme)\n"
         << "  1          - Ligar/desligar luz principal (key light)\n"
         << "  2          - Ligar/desligar luz de preenchimento (fill light)\n"
         << "  3          - Ligar/desligar luz de fundo (back light)\n"
         << "  ESC        - Sair\n\n";

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    GLuint shader = setupShader();
    glUseProgram(shader);

    glUniform1i(glGetUniformLocation(shader, "texBuff"), 0);
    glActiveTexture(GL_TEXTURE0);

    for (int i = 0; i < 3; i++)
    {
        lights[i].color = LIGHT_COLORS[i];
        lights[i].on    = true;
    }

    glm::vec3 camPos(0.0f, 2.0f, 7.0f);
    glUniform3fv(glGetUniformLocation(shader, "camPos"), 1, glm::value_ptr(camPos));

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

    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(
        glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

    GLint modelLoc      = glGetUniformLocation(shader, "model");
    GLint colorLoc      = glGetUniformLocation(shader, "objectColor");
    GLint useTextureLoc = glGetUniformLocation(shader, "useTexture");
    GLint KaLoc         = glGetUniformLocation(shader, "Ka");
    GLint KdLoc         = glGetUniformLocation(shader, "Kd");
    GLint KsLoc         = glGetUniformLocation(shader, "Ks");
    GLint NsLoc         = glGetUniformLocation(shader, "Ns");

    GLint lightPosLoc[3], lightColorLoc[3], lightOnLoc[3];
    for (int i = 0; i < 3; i++)
    {
        string prefix = "lights[" + to_string(i) + "].";
        lightPosLoc[i]   = glGetUniformLocation(shader, (prefix + "position").c_str());
        lightColorLoc[i] = glGetUniformLocation(shader, (prefix + "color").c_str());
        lightOnLoc[i]    = glGetUniformLocation(shader, (prefix + "on").c_str());
    }

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

        {
            const OBJModel& main = objects[0];
            float s = glm::length(main.scale);  // fator de escala uniforme
            for (int i = 0; i < 3; i++)
                lights[i].position = main.position + LIGHT_OFFSETS[i] * s;
        }

        for (int i = 0; i < 3; i++)
        {
            glUniform3fv(lightPosLoc[i],   1, glm::value_ptr(lights[i].position));
            glUniform3fv(lightColorLoc[i], 1, glm::value_ptr(lights[i].color));
            glUniform1i (lightOnLoc[i],    lights[i].on ? 1 : 0);
        }

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
            glUniform1f(NsLoc,  o.mat.Ns);

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

    if (key == GLFW_KEY_1) { lights[0].on = !lights[0].on; updateWindowTitle(window); return; }
    if (key == GLFW_KEY_2) { lights[1].on = !lights[1].on; updateWindowTitle(window); return; }
    if (key == GLFW_KEY_3) { lights[2].on = !lights[2].on; updateWindowTitle(window); return; }

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
        OBJModel& o = objects[activeObj];
        if (key == GLFW_KEY_X) { o.rotX = !o.rotX; if (o.rotX) { o.rotY = o.rotZ = false; } }
        if (key == GLFW_KEY_Y) { o.rotY = !o.rotY; if (o.rotY) { o.rotX = o.rotZ = false; } }
        if (key == GLFW_KEY_Z) { o.rotZ = !o.rotZ; if (o.rotZ) { o.rotX = o.rotY = false; } }
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

    const char* names[3] = { "Key", "Fill", "Back" };
    string luzes = "  Luzes: ";
    for (int i = 0; i < 3; i++)
        luzes += string(names[i]) + (lights[i].on ? ":ON" : ":off") + (i < 2 ? " " : "");

    string title = "M4_vivencial | Obj: " + objects[activeObj].name
                 + "  [" + to_string(activeObj + 1) + "/" + to_string(objects.size()) + "]"
                 + "  Modo: " + modeStr
                 + luzes;
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

                vBuffer.push_back(positions[vi].x);
                vBuffer.push_back(positions[vi].y);
                vBuffer.push_back(positions[vi].z);

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

                if (!normals.empty() && ni >= 0 && ni < (int)normals.size())
                {
                    vBuffer.push_back(normals[ni].x);
                    vBuffer.push_back(normals[ni].y);
                    vBuffer.push_back(normals[ni].z);
                }
                else
                {
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(1.0f);
                    vBuffer.push_back(0.0f);
                }
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

                if (mtlWord == "map_Kd")
                {
                    string texFile;
                    mtlss >> texFile;
                    texturePath = directory + texFile;
                }
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

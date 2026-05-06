/* Hello Triangle - código adaptado de https://learnopengl.com/#!Getting-started/Hello-Triangle
 *
 * Adaptado por Rossana Baptista Queiroz
 * para as disciplinas de Processamento Gráfico/Computação Gráfica - Unisinos
 * Versão inicial: 7/4/2017
 * Última atualização em 07/03/2025
 *
 * M2: Cubo com múltiplas instâncias, translação, escala e rotação via teclado
 * Controles:
 *   1 / 2 / 3  : selecionar cubo ativo
 *   X / Y / Z  : ativar rotação no eixo respectivo (toggle)
 *   W / S      : mover no eixo Z (frente/trás)
 *   A / D      : mover no eixo X (esquerda/direita)
 *   I / J      : mover no eixo Y (cima/baixo)
 *   [  /  ]    : diminuir / aumentar escala uniforme
 *   ESC        : sair
 */

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <assert.h>

using namespace std;

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

//GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
int setupShader();
int setupGeometry();

const GLuint WIDTH = 1000, HEIGHT = 1000;

const GLchar* vertexShaderSource =
    "#version 450\n"
    "layout (location = 0) in vec3 position;\n"
    "layout (location = 1) in vec3 color;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "out vec4 finalColor;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = projection * view * model * vec4(position, 1.0);\n"
    "    finalColor = vec4(color, 1.0);\n"
    "}\0";

const GLchar* fragmentShaderSource =
    "#version 450\n"
    "in vec4 finalColor;\n"
    "out vec4 color;\n"
    "void main()\n"
    "{\n"
    "    color = finalColor;\n"
    "}\n\0";

struct CubeInstance {
    glm::vec3 position;
    float scale;
    bool rotateX, rotateY, rotateZ;

    CubeInstance(glm::vec3 pos)
        : position(pos), scale(0.8f),
          rotateX(false), rotateY(false), rotateZ(false) {}
};

vector<CubeInstance> cubes;
int activeCube = 0;

bool keys[1024] = {false};

const float TRANSLATE_SPEED = 0.03f;
const float SCALE_STEP      = 0.02f;
const float SCALE_MIN       = 0.1f;

int main()
{
    glfwInit();

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "M2 - Cubo 3D | Cubo ativo: 1", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }

    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version  = glGetString(GL_VERSION);
    cout << "Renderer: " << renderer << endl;
    cout << "OpenGL version supported " << version << endl;
    cout << "\nControles:" << endl;
    cout << "  1/2/3  - Selecionar cubo ativo" << endl;
    cout << "  X/Y/Z  - Rotacionar no eixo (toggle)" << endl;
    cout << "  W/S    - Mover no eixo Z" << endl;
    cout << "  A/D    - Mover no eixo X" << endl;
    cout << "  I/J    - Mover no eixo Y" << endl;
    cout << "  [/]    - Diminuir/Aumentar escala" << endl;
    cout << "  ESC    - Sair" << endl;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    GLuint shaderID = setupShader();
    GLuint VAO      = setupGeometry();

    // Três instâncias de cubo com posições iniciais distintas
    cubes.push_back(CubeInstance(glm::vec3(-1.8f,  0.0f, 0.0f)));
    cubes.push_back(CubeInstance(glm::vec3( 0.0f,  0.0f, 0.0f)));
    cubes.push_back(CubeInstance(glm::vec3( 1.8f,  0.0f, 0.0f)));

    glUseProgram(shaderID);

    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 1.5f, 6.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    glm::mat4 projection = glm::perspective(
        glm::radians(60.0f),
        (float)WIDTH / (float)HEIGHT,
        0.1f,
        100.0f
    );

    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint viewLoc  = glGetUniformLocation(shaderID, "view");
    GLint projLoc  = glGetUniformLocation(shaderID, "projection");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Controle contínuo do cubo ativo
        CubeInstance& cube = cubes[activeCube];
        if (keys[GLFW_KEY_W]) cube.position.z -= TRANSLATE_SPEED;
        if (keys[GLFW_KEY_S]) cube.position.z += TRANSLATE_SPEED;
        if (keys[GLFW_KEY_A]) cube.position.x -= TRANSLATE_SPEED;
        if (keys[GLFW_KEY_D]) cube.position.x += TRANSLATE_SPEED;
        if (keys[GLFW_KEY_I]) cube.position.y += TRANSLATE_SPEED;
        if (keys[GLFW_KEY_J]) cube.position.y -= TRANSLATE_SPEED;
        if (keys[GLFW_KEY_LEFT_BRACKET])  cube.scale = max(SCALE_MIN, cube.scale - SCALE_STEP);
        if (keys[GLFW_KEY_RIGHT_BRACKET]) cube.scale += SCALE_STEP;

        glClearColor(0.12f, 0.12f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float angle = (GLfloat)glfwGetTime();

        glBindVertexArray(VAO);

        for (size_t i = 0; i < cubes.size(); i++)
        {
            const CubeInstance& c = cubes[i];

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, c.position);
            model = glm::scale(model, glm::vec3(c.scale));

            if      (c.rotateX) model = glm::rotate(model, angle, glm::vec3(1.0f, 0.0f, 0.0f));
            else if (c.rotateY) model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
            else if (c.rotateZ) model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    // Rastreia estado de teclas para movimento contínuo
    if (key >= 0 && key < 1024)
    {
        if (action == GLFW_PRESS)   keys[key] = true;
        else if (action == GLFW_RELEASE) keys[key] = false;
    }

    // Selecionar cubo ativo com 1/2/3
    if (action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_1) { activeCube = 0; glfwSetWindowTitle(window, "M2 - Cubo 3D | Cubo ativo: 1"); }
        if (key == GLFW_KEY_2) { activeCube = 1; glfwSetWindowTitle(window, "M2 - Cubo 3D | Cubo ativo: 2"); }
        if (key == GLFW_KEY_3) { activeCube = 2; glfwSetWindowTitle(window, "M2 - Cubo 3D | Cubo ativo: 3"); }
    }

    // Rotação do cubo ativo (toggle; ativar um desativa os outros)
    if (key == GLFW_KEY_X && action == GLFW_PRESS)
    {
        cubes[activeCube].rotateX = !cubes[activeCube].rotateX;
        cubes[activeCube].rotateY = false;
        cubes[activeCube].rotateZ = false;
    }
    if (key == GLFW_KEY_Y && action == GLFW_PRESS)
    {
        cubes[activeCube].rotateX = false;
        cubes[activeCube].rotateY = !cubes[activeCube].rotateY;
        cubes[activeCube].rotateZ = false;
    }
    if (key == GLFW_KEY_Z && action == GLFW_PRESS)
    {
        cubes[activeCube].rotateX = false;
        cubes[activeCube].rotateY = false;
        cubes[activeCube].rotateZ = !cubes[activeCube].rotateZ;
    }
}

int setupShader()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

int setupGeometry()
{
    // Cubo: 6 faces × 2 triângulos × 3 vértices = 36 vértices
    // Cada face tem uma cor distinta  (x, y, z, r, g, b)
    GLfloat vertices[] = {
        // Face inferior (Y = -0.5) - Amarelo
        -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,

        // Face superior (Y = +0.5) - Ciano
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,

        // Face frontal (Z = +0.5) - Vermelho
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,

        // Face traseira (Z = -0.5) - Verde
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,

        // Face esquerda (X = -0.5) - Azul
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,

        // Face direita (X = +0.5) - Magenta
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
    };

    GLuint VBO, VAO;

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Atributo posição (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Atributo cor (r, g, b)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return VAO;
}

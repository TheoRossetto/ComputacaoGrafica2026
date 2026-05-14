/*
 *  Codificado por Rossana Baptista Queiroz
 *  para as disciplinas de Processamento Gráfico/Computação Gráfica - Unisinos
 *  Versão inicial: 07/04/2017
 *  Última atualização: 14/05/2025
 *
 *  Este arquivo contém a função `loadSimpleOBJ`, responsável por carregar arquivos
 *  no formato Wavefront .OBJ e armazenar seus vértices em um VAO para renderização
 *  com OpenGL.
 *
 *  Forma de uso (carregamento de um .obj)
 *  -----------------
 *  ...
 *  int nVertices;
 *  string texturePath;
 *  GLuint objVAO = loadSimpleOBJ("../Modelos3D/Cube.obj", nVertices, texturePath);
 *  ...
 *
 *  Chamada de desenho (Polígono Preenchido - GL_TRIANGLES), no loop do programa:
 *  ----------------------------------------------------------
 *  ...
 *  glBindVertexArray(objVAO);
 *  glDrawArrays(GL_TRIANGLES, 0, nVertices);
 *
 */

 // Cabeçalhos necessários (para esta função), acrescentar ao seu código
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>


using namespace std;

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

//GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Mesh
{
    GLuint VAO;
    string texturePath;
};

// filePATH  : caminho do arquivo .OBJ
// nVertices : número de vértices processados (saída por referência)
// texturePath: caminho da textura lida do .MTL (saída por referência; vazia se não houver)
int loadSimpleOBJ(string filePATH, int &nVertices, string &texturePath)
{
    vector<glm::vec3> vertices;
    vector<glm::vec2> texCoords;
    vector<glm::vec3> normals;
    vector<GLfloat> vBuffer;

    texturePath = "";
    string mtlFile;

    // Diretório base do arquivo OBJ (para resolver caminhos relativos do MTL)
    string directory = filePATH.substr(0, filePATH.find_last_of("/\\") + 1);

    ifstream arqEntrada(filePATH.c_str());
    if (!arqEntrada.is_open())
    {
        cerr << "Erro ao tentar ler o arquivo " << filePATH << endl;
        return -1;
    }

    string line;
    while (getline(arqEntrada, line))
    {
        istringstream ssline(line);
        string word;
        ssline >> word;

        if (word == "mtllib")
        {
            // Nome do arquivo .MTL referenciado pelo .OBJ
            ssline >> mtlFile;
        }
        else if (word == "v")
        {
            glm::vec3 vertice;
            ssline >> vertice.x >> vertice.y >> vertice.z;
            vertices.push_back(vertice);
        }
        else if (word == "vt")
        {
            glm::vec2 vt;
            ssline >> vt.s >> vt.t;
            texCoords.push_back(vt);
        }
        else if (word == "vn")
        {
            glm::vec3 normal;
            ssline >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        }
        else if (word == "f")
        {
            while (ssline >> word)
            {
                int vi = 0, ti = 0, ni = 0;
                istringstream ss(word);
                string index;

                if (getline(ss, index, '/')) vi = !index.empty() ? stoi(index) - 1 : 0;
                if (getline(ss, index, '/')) ti = !index.empty() ? stoi(index) - 1 : 0;
                if (getline(ss, index))      ni = !index.empty() ? stoi(index) - 1 : 0;

                // Posição do vértice (x, y, z)
                vBuffer.push_back(vertices[vi].x);
                vBuffer.push_back(vertices[vi].y);
                vBuffer.push_back(vertices[vi].z);

                // Coordenadas de textura (s, t)
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

    arqEntrada.close();

    // Leitura do arquivo .MTL para obter o nome da textura (map_Kd)
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
        else
        {
            cerr << "Aviso: nao foi possivel abrir " << directory + mtlFile << endl;
        }
    }

    cout << "Gerando o buffer de geometria..." << endl;
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Atributo 0: posição (x, y, z) — 3 floats, offset 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Atributo 1: coordenada de textura (s, t) — 2 floats, offset 3 floats
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = vBuffer.size() / 5;  // x, y, z, s, t (5 valores por vértice)

    return VAO;
}

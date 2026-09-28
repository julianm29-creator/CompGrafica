#include <iostream>
#include <vector>
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

const int DIV = 100;       
const int VENTANA = 800;

const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"layout (location = 1) in vec3 aColor;\n"
"out vec3 vColor;\n"
"void main()\n"
"{\n"
"    gl_Position = vec4(aPos, 1.0);\n"
"    vColor = aColor;\n"
"}\0";

const char* fragmentShaderSource = "#version 330 core\n"
"in vec3 vColor;\n"
"out vec4 FragColor;\n"
"void main()\n"
"{\n"
"    FragColor = vec4(vColor, 1.0);\n"
"}\n\0";


std::vector<float> lienzo(DIV* DIV * 3, 1.0f);   
std::vector<float> pantalla(DIV* DIV * 3, 1.0f); 

bool dibujando = false;      // true mientras el boton esta apretado
int  inicioX = 0, inicioY = 0;
int  actualX = 0, actualY = 0;
bool necesitaActualizar = true;
float colorLapiz[3] = { 0.9f, 0.2f, 0.2f };  // r
ojo


void pintarCelda(std::vector<float>& buf, int col, int fila, const float c[3])
{
    if (col < 0 || col >= DIV || fila < 0 || fila >= DIV) return;
    int i = (fila * DIV + col) * 3;
    buf[i] = c[0]; buf[i + 1] = c[1]; buf[i + 2] = c[2];
}

//BRESENHAM
void bresenham(std::vector<float>& buf, int x0, int y0, int x1, int y1, const float c[3])
{
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true)
    {
        pintarCelda(buf, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// pixeles del mouse a coordenadas de celda
void pixelACelda(GLFWwindow* w, double mx, double my, int& col, int& fila)
{
    int ancho, alto;
    glfwGetWindowSize(w, &ancho, &alto);
    col = (int)(mx / (ancho / (double)DIV));
    fila = DIV - 1 - (int)(my / (alto / (double)DIV));  
    if (col < 0) col = 0; if (col >= DIV) col = DIV - 1;
    if (fila < 0) fila = 0; if (fila >= DIV) fila = DIV - 1;
}

// 
void mouseBotonCallback(GLFWwindow* window, int boton, int accion, int mods)
{
    if (boton != GLFW_MOUSE_BUTTON_LEFT) return;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    if (accion == GLFW_PRESS)
    {
        pixelACelda(window, mx, my, inicioX, inicioY);
        actualX = inicioX;
        actualY = inicioY;
        dibujando = true;
        necesitaActualizar = true;
    }
    else if (accion == GLFW_RELEASE && dibujando)
    {
        pixelACelda(window, mx, my, actualX, actualY);
        // Se confirma la linea
        bresenham(lienzo, inicioX, inicioY, actualX, actualY, colorLapiz);
        dibujando = false;
        necesitaActualizar = true;
    }
}

void mouseMovimientoCallback(GLFWwindow* window, double mx, double my)
{
    if (!dibujando) return;
    int c, f;
    pixelACelda(window, mx, my, c, f);
    if (c != actualX || f != actualY)  
    {
        actualX = c;
        actualY = f;
        necesitaActualizar = true;
    }
}

void teclaCallback(GLFWwindow* window, int key, int sc, int accion, int mods)
{
    if (accion != GLFW_PRESS) return;
    if (key == GLFW_KEY_C)  // limpiar
    {
        std::fill(lienzo.begin(), lienzo.end(), 1.0f);
        necesitaActualizar = true;
    }
    if (key == GLFW_KEY_1) { colorLapiz[0] = 0.9f; colorLapiz[1] = 0.2f; colorLapiz[2] = 0.2f; } 
    if (key == GLFW_KEY_2) { colorLapiz[0] = 0.2f; colorLapiz[1] = 0.5f; colorLapiz[2] = 0.9f; } 
    if (key == GLFW_KEY_3) { colorLapiz[0] = 0.2f; colorLapiz[1] = 0.7f; colorLapiz[2] = 0.3f; } 
    if (key == GLFW_KEY_4) { colorLapiz[0] = 0.1f; colorLapiz[1] = 0.1f; colorLapiz[2] = 0.1f; } 
}

std::vector<float> generarCuadrados()
{
    std::vector<float> v;
    float paso = 2.0f / DIV;
    for (int fila = 0; fila < DIV; fila++)
        for (int col = 0; col < DIV; col++)
        {
            float x0 = -1.0f + col * paso, y0 = -1.0f + fila * paso;
            float x1 = x0 + paso, y1 = y0 + paso;
            v.insert(v.end(), { x0,y0,0.0f,  x1,y0,0.0f,  x1,y1,0.0f });
            v.insert(v.end(), { x0,y0,0.0f,  x1,y1,0.0f,  x0,y1,0.0f });
        }
    return v;
}

std::vector<float> coloresPorVertice(const std::vector<float>& buf)
{
    std::vector<float> v;
    v.reserve(DIV * DIV * 6 * 3);
    for (int c = 0; c < DIV * DIV; c++)
        for (int k = 0; k < 6; k++)
            v.insert(v.end(), { buf[c * 3], buf[c * 3 + 1], buf[c * 3 + 2] });
    return v;
}

std::vector<float> generarLineas()
{
    std::vector<float> v;
    float paso = 2.0f / DIV;
    for (int i = 0; i <= DIV; i++)
    {
        float p = -1.0f + i * paso;
        v.insert(v.end(), { p,-1.0f,0.0f,  p,1.0f,0.0f });
        v.insert(v.end(), { -1.0f,p,0.0f,  1.0f,p,0.0f });
    }
    return v;
}

// MAIN 
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(VENTANA, VENTANA, "Bresenham con mouse - C: limpiar | 1-4: color", NULL, NULL);
    if (!window) { std::cout << "Failed to create GLFW window\n"; glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    gladLoadGL();
    glViewport(0, 0, VENTANA, VENTANA);

    
    glfwSetMouseButtonCallback(window, mouseBotonCallback);
    glfwSetCursorPosCallback(window, mouseMovimientoCallback);
    glfwSetKeyCallback(window, teclaCallback);

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, NULL); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, NULL); glCompileShader(fs);
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    glDeleteShader(vs); glDeleteShader(fs);

    std::vector<float> pos = generarCuadrados();
    std::vector<float> col = coloresPorVertice(pantalla);

    GLuint VAO, VBOpos, VBOcol;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBOpos);
    glGenBuffers(1, &VBOcol);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBOpos);
    glBufferData(GL_ARRAY_BUFFER, pos.size() * sizeof(float), pos.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, VBOcol);
    glBufferData(GL_ARRAY_BUFFER, col.size() * sizeof(float), col.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);

    std::vector<float> lin = generarLineas();
    GLuint VAOl, VBOl;
    glGenVertexArrays(1, &VAOl);
    glGenBuffers(1, &VBOl);
    glBindVertexArray(VAOl);
    glBindBuffer(GL_ARRAY_BUFFER, VBOl);
    glBufferData(GL_ARRAY_BUFFER, lin.size() * sizeof(float), lin.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttrib3f(1, 0.6f, 0.75f, 0.95f);
    glBindVertexArray(0);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (necesitaActualizar)
        {
            pantalla = lienzo;   
            if (dibujando)      
                bresenham(pantalla, inicioX, inicioY, actualX, actualY, colorLapiz);

            col = coloresPorVertice(pantalla);
            glBindBuffer(GL_ARRAY_BUFFER, VBOcol);
            glBufferSubData(GL_ARRAY_BUFFER, 0, col.size() * sizeof(float), col.data());
            necesitaActualizar = false;
        }

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, pos.size() / 3);
        glBindVertexArray(VAOl);
        glDrawArrays(GL_LINES, 0, lin.size() / 3);

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteVertexArrays(1, &VAOl);
    glDeleteBuffers(1, &VBOpos);
    glDeleteBuffers(1, &VBOcol);
    glDeleteBuffers(1, &VBOl);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}
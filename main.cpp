#include <iostream>
#include <vector>
#include <utility>
#include <cmath>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// TAMANOS
const int DIV = 150;              // el lienzo es de 150x150 celdas
const int CELDA = 24;              // tamano de cada celda en pixeles
const int VENTANA = DIV * CELDA;     // el lienzo mide 720 x 720 px
const int PANEL = 280;             // ancho del panel de ImGui (a la izquierda del lienzo)

const float PI = 3.14159265f;

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

// COLORES
const int N_COLORES = 8;
const float PALETA[N_COLORES][3] = {
    { 0.90f, 0.20f, 0.20f },   // 1 rojo
    { 0.95f, 0.60f, 0.15f },   // 2 naranja
    { 0.98f, 0.85f, 0.20f },   // 3 amarillo
    { 0.20f, 0.70f, 0.30f },   // 4 verde
    { 0.30f, 0.70f, 0.95f },   // 5 celeste
    { 0.20f, 0.35f, 0.85f },   // 6 azul
    { 0.60f, 0.30f, 0.75f },   // 7 morado
    { 0.05f, 0.05f, 0.05f }    // 8 negro
};
const float GRIS_CAJA[3] = { 0.55f, 0.55f, 0.55f };   // caja que rodea a la figura seleccionada

// HERRAMIENTAS
enum Modo { LINEA, CIRCULO, TRIANGULO, CUADRADO, LIBRE, RELLENO, SELECCIONAR };
const int N_MODOS = 7;
const char* NOMBRES_MODO[N_MODOS] = {
    "Linea", "Circulo", "Triangulo", "Cuadrado", "Forma libre", "Relleno", "Seleccionar"
};
const char* AYUDA_MODO[N_MODOS] = {
    "Clic y arrastra: se dibuja una recta.",
    "El clic es el centro y al arrastrar defines el radio.",
    "El clic es una esquina de la base y al arrastrar defines la punta.",
    "El clic es una esquina y al arrastrar defines el tamano.",
    "Manten apretado el clic y dibuja a mano alzada.",
    "Haz clic dentro de un triangulo, cuadrado o circulo para rellenarlo.",
    "Haz clic en una figura para elegirla. Arrastrala para moverla o usa los controles de Transformar."
};

// PUNTOS Y MATRICES (TRANSFORMACIONES)
struct Punto { float x, y; };

Punto punto(float x, float y)
{
    Punto p;
    p.x = x;
    p.y = y;
    return p;
}

// Matriz de 3x3 para coordenadas homogeneas: [ a b tx ]
//                                            [ c d ty ]
//                                            [ 0 0 1  ]
struct Mat3 { float m[3][3]; };

Mat3 matIdentidad()
{
    Mat3 r;

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            r.m[i][j] = (i == j) ? 1.0f : 0.0f;
    return r;
}

Mat3 matTraslacion(float tx, float ty)
{
    Mat3 r = matIdentidad();
    r.m[0][2] = tx;
    r.m[1][2] = ty;
    return r;
}

Mat3 matEscala(float s)
{
    Mat3 r = matIdentidad();
    r.m[0][0] = s;
    r.m[1][1] = s;
    return r;
}

Mat3 matRotacion(float grados)
{
    float a = grados * PI / 180.0f;
    float c = std::cos(a);
    float s = std::sin(a);

    Mat3 r = matIdentidad();
    r.m[0][0] = c;  r.m[0][1] = -s;
    r.m[1][0] = s;  r.m[1][1] = c;
    return r;
}

// Producto de matrices: a * b (primero se aplica b y despues a)
Mat3 matMul(const Mat3& a, const Mat3& b)
{
    Mat3 r;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
        {
            r.m[i][j] = 0.0f;
            for (int k = 0; k < 3; k++)
                r.m[i][j] += a.m[i][k] * b.m[k][j];
        }
    return r;
}

// Multiplica la matriz por el punto (x, y, 1)
Punto matAplicar(const Mat3& m, Punto p)
{
    return punto(m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2],
        m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2]);
}

int redondear(float v) { return (int)std::lround(v); }

// FIGURAS 
// Una figura guarda sus puntos ORIGINALES y sus transformaciones por separado.
// Los pixeles se calculan cada vez que hace falta dibujarla.
struct Figura
{
    Modo tipo = LINEA;
    std::vector<Punto> pts;                       // LINEA: 2 pts | CIRCULO: el centro | TRIANGULO: 3 | CUADRADO: 4 | LIBRE: todos los del trazo
    float radio = 0.0f;                           // solo para el circulo
    float color[3] = { 0.0f, 0.0f, 0.0f };        // color del contorno
    bool  relleno = false;
    float colorRelleno[3] = { 1.0f, 1.0f, 1.0f };

    // transformaciones (se aplican alrededor del centro de la figura)
    float tx = 0.0f, ty = 0.0f;                   // traslacion en celdas
    float escala = 1.0f;
    float angulo = 0.0f;                          // rotacion en grados
};

// ESTADO 
std::vector<float> lienzo(DIV* DIV * 3, 1.0f);    // lo ya dibujado (se arma a partir de las figuras)
std::vector<float> pantalla(DIV* DIV * 3, 1.0f);  // lienzo + caja de seleccion + vista previa

std::vector<Figura> figuras;     // todas las figuras que hay en la hoja
int    seleccionada = -1;        // indice de la figura seleccionada (-1 = ninguna)
Figura enCreacion;               // la figura que se esta dibujando ahora mismo (vista previa)

bool dibujando = false;          // true mientras el boton esta apretado durante un gesto valido
int  inicioX = 0, inicioY = 0;
int  actualX = 0, actualY = 0;
bool necesitaActualizar = true;

Modo  modo = LINEA;                                  // herramienta activa
float colorLapiz[3] = { 0.90f, 0.20f, 0.20f };       // color activo (lo cambia ImGui)

//  COLORES: AYUDAS 
bool colorIgual(const float a[3], const float b[3])
{
    return std::fabs(a[0] - b[0]) < 0.001f &&
        std::fabs(a[1] - b[1]) < 0.001f &&
        std::fabs(a[2] - b[2]) < 0.001f;
}

void copiarColor(float destino[3], const float origen[3])
{
    destino[0] = origen[0];
    destino[1] = origen[1];
    destino[2] = origen[2];
}

// CELDAS 
// Escribe el color de una celda, ignora lo que se salga de la hoja
void pintarCelda(std::vector<float>& buf, int col, int fila, const float c[3])
{
    if (col < 0 || col >= DIV || fila < 0 || fila >= DIV) return;
    int i = (fila * DIV + col) * 3;
    buf[i] = c[0]; buf[i + 1] = c[1]; buf[i + 2] = c[2];
}

// ALGORITMO DE BRESENHAM (LINEA) 
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

// Pinta los 8 puntos simetricos de (x, y) alrededor del centro (cx, cy)
void pintar8(std::vector<float>& buf, int cx, int cy, int x, int y, const float c[3])
{
    pintarCelda(buf, cx + x, cy + y, c);
    pintarCelda(buf, cx - x, cy + y, c);
    pintarCelda(buf, cx + x, cy - y, c);
    pintarCelda(buf, cx - x, cy - y, c);
    pintarCelda(buf, cx + y, cy + x, c);
    pintarCelda(buf, cx - y, cy + x, c);
    pintarCelda(buf, cx + y, cy - x, c);
    pintarCelda(buf, cx - y, cy - x, c);
}

// CIRCULO: ALGORITMO DE BRESENHAM
void circulo(std::vector<float>& buf, int cx, int cy, int r, const float c[3])
{
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;      // parametro de decision inicial

    while (y >= x)
    {
        pintar8(buf, cx, cy, x, y, c);

        if (d < 0)
            d += 4 * x + 6;              // y se queda
        else
        {
            d += 4 * (x - y) + 10;       // y baja una celda
            y--;
        }
        x++;
    }
}

// RECTANGULO: 4 LINEAS DE BRESENHAM (se usa para la caja de seleccion)
void rectangulo(std::vector<float>& buf, int x0, int y0, int x1, int y1, const float c[3])
{
    bresenham(buf, x0, y0, x1, y0, c);
    bresenham(buf, x1, y0, x1, y1, c);
    bresenham(buf, x1, y1, x0, y1, c);
    bresenham(buf, x0, y1, x0, y0, c);
}

// RELLENO (FLOOD FILL)
bool mismoColor(const std::vector<float>& buf, int col, int fila, const float c[3])
{
    return colorIgual(&buf[(fila * DIV + col) * 3], c);
}

// Pinta la celda (x, y) y todas las vecinas que tengan su mismo color
// (vecinas = arriba, abajo, izquierda y derecha; las diagonales no cuentan)
void rellenar(std::vector<float>& buf, int x, int y, const float nuevo[3])
{
    if (x < 0 || x >= DIV || y < 0 || y >= DIV) return;

    int i0 = (y * DIV + x) * 3;
    float objetivo[3] = { buf[i0], buf[i0 + 1], buf[i0 + 2] };   // el color que voy a reemplazar

    if (colorIgual(objetivo, nuevo)) return;    // ya tiene ese color: no hay nada que hacer

    std::vector<std::pair<int, int>> pila;      // celdas pendientes por revisar
    pila.push_back(std::make_pair(x, y));

    while (!pila.empty())
    {
        std::pair<int, int> p = pila.back();
        pila.pop_back();
        int cx = p.first;
        int cy = p.second;

        if (cx < 0 || cx >= DIV || cy < 0 || cy >= DIV) continue;   // fuera de la hoja
        if (!mismoColor(buf, cx, cy, objetivo)) continue;           // es un borde u otro color

        pintarCelda(buf, cx, cy, nuevo);

        pila.push_back(std::make_pair(cx + 1, cy));
        pila.push_back(std::make_pair(cx - 1, cy));
        pila.push_back(std::make_pair(cx, cy + 1));
        pila.push_back(std::make_pair(cx, cy - 1));
    }
}

// FIGURAS: TRANSFORMACION Y DIBUJO 
bool esCerrada(Modo tipo)
{
    return tipo == CIRCULO || tipo == TRIANGULO || tipo == CUADRADO;
}

// Centro de la figura: es el punto alrededor del que se rota y se escala
Punto centroDe(const Figura& f)
{
    float sx = 0.0f, sy = 0.0f;
    for (size_t i = 0; i < f.pts.size(); i++)
    {
        sx += f.pts[i].x;
        sy += f.pts[i].y;
    }
    return punto(sx / f.pts.size(), sy / f.pts.size());
}

// Matriz de transformacion de una figura:
//   M = T(centro + traslacion) * R(angulo) * S(escala) * T(-centro)
// Se lee de derecha a izquierda: llevo el centro al origen, escalo, roto,
// y por ultimo devuelvo la figura a su sitio sumando la traslacion.
Mat3 matrizDe(const Figura& f)
{
    Punto c = centroDe(f);

    Mat3 m = matTraslacion(-c.x, -c.y);
    m = matMul(matEscala(f.escala), m);
    m = matMul(matRotacion(f.angulo), m);
    m = matMul(matTraslacion(c.x + f.tx, c.y + f.ty), m);
    return m;
}

// Los puntos de la figura despues de aplicarles la transformacion
std::vector<Punto> puntosTransformados(const Figura& f)
{
    Mat3 m = matrizDe(f);
    std::vector<Punto> r;
    for (size_t i = 0; i < f.pts.size(); i++)
        r.push_back(matAplicar(m, f.pts[i]));
    return r;
}

// Dibuja solo el contorno de la figura (ya transformada) con Bresenham
void dibujarContorno(std::vector<float>& buf, const Figura& f, const float color[3])
{
    std::vector<Punto> p = puntosTransformados(f);
    size_t n = p.size();

    switch (f.tipo)
    {
    case LINEA:
        bresenham(buf, redondear(p[0].x), redondear(p[0].y),
            redondear(p[1].x), redondear(p[1].y), color);
        break;

    case CIRCULO:
        // el circulo solo se escala de forma uniforme, asi que sigue siendo un circulo
        circulo(buf, redondear(p[0].x), redondear(p[0].y), redondear(f.radio * f.escala), color);
        break;

    case TRIANGULO:
    case CUADRADO:
        // poligono cerrado: une cada vertice con el siguiente y el ultimo con el primero
        for (size_t i = 0; i < n; i++)
        {
            size_t j = (i + 1) % n;
            bresenham(buf, redondear(p[i].x), redondear(p[i].y),
                redondear(p[j].x), redondear(p[j].y), color);
        }
        break;

    case LIBRE:
        if (n == 1)
            pintarCelda(buf, redondear(p[0].x), redondear(p[0].y), color);
        for (size_t i = 1; i < n; i++)
            bresenham(buf, redondear(p[i - 1].x), redondear(p[i - 1].y),
                redondear(p[i].x), redondear(p[i].y), color);
        break;

    default:
        break;
    }
}

// Dibuja la figura completa (contorno y relleno) sobre buf.
// Primero la dibujo sola en un buffer temporal donde -1 significa "celda vacia":
// asi el relleno nunca se sale de la figura ni invade a las demas.
void rasterizarFigura(std::vector<float>& buf, const Figura& f)
{
    std::vector<float> temp(DIV * DIV * 3, -1.0f);
    dibujarContorno(temp, f, f.color);

    if (f.relleno && esCerrada(f.tipo))
    {
        Punto c = centroDe(f);
        int sx = redondear(c.x + f.tx);
        int sy = redondear(c.y + f.ty);

        // solo relleno si la semilla cayo en una celda vacia (dentro de la figura)
        if (sx >= 0 && sx < DIV && sy >= 0 && sy < DIV && temp[(sy * DIV + sx) * 3] < -0.5f)
            rellenar(temp, sx, sy, f.colorRelleno);
    }

    // paso al buffer solo las celdas que la figura pinto
    for (int i = 0; i < DIV * DIV; i++)
        if (temp[i * 3] > -0.5f)
        {
            buf[i * 3] = temp[i * 3];
            buf[i * 3 + 1] = temp[i * 3 + 1];
            buf[i * 3 + 2] = temp[i * 3 + 2];
        }
}

// DETECTAR SI UN CLIC CAE EN UNA FIGURA 
bool celdaPintada(const std::vector<float>& buf, int x, int y)
{
    if (x < 0 || x >= DIV || y < 0 || y >= DIV) return false;
    return buf[(y * DIV + x) * 3] > -0.5f;
}

// Prueba de "punto dentro de un poligono" (se cuentan cuantas veces un rayo cruza los lados)
bool dentroPoligono(const std::vector<Punto>& p, float x, float y)
{
    bool dentro = false;
    size_t n = p.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++)
    {
        if (((p[i].y > y) != (p[j].y > y)) &&
            (x < (p[j].x - p[i].x) * (y - p[i].y) / (p[j].y - p[i].y) + p[i].x))
            dentro = !dentro;
    }
    return dentro;
}

bool dentroDeFigura(const Figura& f, float x, float y)
{
    std::vector<Punto> p = puntosTransformados(f);

    if (f.tipo == CIRCULO)
    {
        float dx = x - p[0].x;
        float dy = y - p[0].y;
        float r = f.radio * f.escala;
        return dx * dx + dy * dy <= r * r;
    }
    return dentroPoligono(p, x, y);
}

bool tocaFigura(const Figura& f, int x, int y)
{
    std::vector<float> temp(DIV * DIV * 3, -1.0f);
    dibujarContorno(temp, f, f.color);

    if (esCerrada(f.tipo))
    {
        if (celdaPintada(temp, x, y)) return true;     // clic sobre el borde
        return dentroDeFigura(f, (float)x, (float)y);  // clic por dentro
    }

    // lineas y trazos libres son muy delgados: acepto tambien las celdas vecinas
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (celdaPintada(temp, x + dx, y + dy)) return true;
    return false;
}

// Busca la figura que esta en esa celda (empezando por la que se dibujo ultima, que queda encima)
int figuraEnCelda(int x, int y, bool soloCerradas)
{
    for (int i = (int)figuras.size() - 1; i >= 0; i--)
    {
        if (soloCerradas && !esCerrada(figuras[i].tipo)) continue;
        if (tocaFigura(figuras[i], x, y)) return i;
    }
    return -1;
}

// CREAR UNA FIGURA A PARTIR DEL ARRASTRE DEL MOUSE 
Figura crearFigura(Modo tipo, int x0, int y0, int x1, int y1)
{
    Figura f;
    f.tipo = tipo;
    copiarColor(f.color, colorLapiz);

    switch (tipo)
    {
    case LINEA:
        f.pts.push_back(punto((float)x0, (float)y0));
        f.pts.push_back(punto((float)x1, (float)y1));
        break;

    case CIRCULO:
    {
        // centro = donde hice clic, radio = distancia hasta el mouse
        int dx = x1 - x0;
        int dy = y1 - y0;
        f.pts.push_back(punto((float)x0, (float)y0));
        f.radio = std::sqrt((float)(dx * dx + dy * dy));
        break;
    }

    case TRIANGULO:
    {
        // la base va de (x0, y0) a (x1, y0) y la punta queda en la mitad, a la altura y1
        float ax = (x0 + x1) / 2.0f;
        f.pts.push_back(punto((float)x0, (float)y0));
        f.pts.push_back(punto((float)x1, (float)y0));
        f.pts.push_back(punto(ax, (float)y1));
        break;
    }

    case CUADRADO:
    {
        // lados iguales: el lado es lo que mas se haya arrastrado (en X o en Y)
        int lado = std::max(abs(x1 - x0), abs(y1 - y0));
        int sx = (x1 >= x0) ? 1 : -1;
        int sy = (y1 >= y0) ? 1 : -1;
        f.pts.push_back(punto((float)x0, (float)y0));
        f.pts.push_back(punto((float)(x0 + sx * lado), (float)y0));
        f.pts.push_back(punto((float)(x0 + sx * lado), (float)(y0 + sy * lado)));
        f.pts.push_back(punto((float)x0, (float)(y0 + sy * lado)));
        break;
    }

    default:   // LIBRE: empieza con un solo punto y se le van agregando mas
        f.pts.push_back(punto((float)x0, (float)y0));
        break;
    }
    return f;
}

// Arma el lienzo desde cero dibujando todas las figuras en el orden en que se crearon
void reconstruirLienzo()
{
    std::fill(lienzo.begin(), lienzo.end(), 1.0f);
    for (size_t i = 0; i < figuras.size(); i++)
        rasterizarFigura(lienzo, figuras[i]);
}

// Caja gris alrededor de la figura seleccionada
void dibujarCaja(std::vector<float>& buf, const Figura& f)
{
    std::vector<Punto> p = puntosTransformados(f);
    float minX = p[0].x, maxX = p[0].x;
    float minY = p[0].y, maxY = p[0].y;

    for (size_t i = 1; i < p.size(); i++)
    {
        minX = std::min(minX, p[i].x);  maxX = std::max(maxX, p[i].x);
        minY = std::min(minY, p[i].y);  maxY = std::max(maxY, p[i].y);
    }

    if (f.tipo == CIRCULO)
    {
        float r = f.radio * f.escala;
        minX -= r;  maxX += r;
        minY -= r;  maxY += r;
    }

    rectangulo(buf, redondear(minX) - 1, redondear(minY) - 1,
        redondear(maxX) + 1, redondear(maxY) + 1, GRIS_CAJA);
}

void eliminarSeleccionada()
{
    if (seleccionada < 0 || seleccionada >= (int)figuras.size()) return;
    figuras.erase(figuras.begin() + seleccionada);
    seleccionada = -1;
    necesitaActualizar = true;
}

// PANEL DE DEAR IMGUI 
void dibujarPanel()
{
    // el panel ocupa toda la parte izquierda de la ventana y no se mueve
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2((float)PANEL, (float)VENTANA));
    ImGui::Begin("Herramientas", NULL,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    // --- Elegir la forma ---
    ImGui::Text("Forma");
    for (int i = 0; i < N_MODOS; i++)
    {
        if (ImGui::RadioButton(NOMBRES_MODO[i], modo == i))
            modo = (Modo)i;
        if (i % 2 == 0 && i < N_MODOS - 1) ImGui::SameLine(140.0f);   // 2 botones por fila
    }

    ImGui::Spacing();
    ImGui::TextWrapped("%s", AYUDA_MODO[modo]);
    ImGui::Separator();

    // --- Elegir el color ---
    ImGui::Text("Color");
    for (int i = 0; i < N_COLORES; i++)
    {
        ImGui::PushID(i);

        ImVec4 color4(PALETA[i][0], PALETA[i][1], PALETA[i][2], 1.0f);
        if (ImGui::ColorButton("##paleta", color4, 0, ImVec2(40.0f, 40.0f)))
            copiarColor(colorLapiz, PALETA[i]);

        // borde negro al color que esta seleccionado
        if (colorIgual(colorLapiz, PALETA[i]))
            ImGui::GetWindowDrawList()->AddRect(
                ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                IM_COL32(0, 0, 0, 255), 0.0f, 0, 3.0f);

        ImGui::PopID();
        if (i % 4 != 3) ImGui::SameLine();      // 4 colores por fila
    }

    ImGui::Spacing();
    ImGui::Text("Color personalizado");
    ImGui::ColorEdit3("##personalizado", colorLapiz);
    ImGui::Separator();

    // --- Transformaciones de la figura seleccionada ---
    ImGui::Text("Transformar");
    if (seleccionada >= 0 && seleccionada < (int)figuras.size())
    {
        Figura& f = figuras[seleccionada];
        bool cambio = false;

        cambio |= ImGui::SliderFloat("Mover X", &f.tx, -(float)DIV, (float)DIV, "%.1f");
        cambio |= ImGui::SliderFloat("Mover Y", &f.ty, -(float)DIV, (float)DIV, "%.1f");
        cambio |= ImGui::SliderFloat("Escala", &f.escala, 0.1f, 3.0f, "%.2f");
        cambio |= ImGui::SliderFloat("Rotar", &f.angulo, -180.0f, 180.0f, "%.0f grados");

        if (ImGui::Button("Restablecer"))
        {
            f.tx = 0.0f;  f.ty = 0.0f;
            f.escala = 1.0f;  f.angulo = 0.0f;
            cambio = true;
        }
        if (cambio) necesitaActualizar = true;

        ImGui::SameLine();
        if (ImGui::Button("Eliminar"))      // al final: despues de esto la referencia f ya no vale
            eliminarSeleccionada();
    }
    else
    {
        ImGui::TextDisabled("Elige una figura con 'Seleccionar'.");
    }
    ImGui::Separator();

    // --- Limpiar ---
    if (ImGui::Button("Limpiar lienzo", ImVec2(-1.0f, 0.0f)))
    {
        figuras.clear();
        seleccionada = -1;
        necesitaActualizar = true;
    }

    ImGui::End();
}

//  MOUSE -> CELDAS 
// Celda del lienzo (0..DIV-1). El lienzo es un cuadrado pegado al lado derecho de la ventana.
// La fila 0 queda abajo, como en OpenGL.
void pixelACelda(GLFWwindow* w, double mx, double my, int& col, int& fila)
{
    int ancho, alto;
    glfwGetWindowSize(w, &ancho, &alto);

    double lado = (double)alto;               // el lienzo mide alto x alto
    double inicio = (double)(ancho - alto);     // donde empieza (lo que ocupa el panel)
    double celda = lado / DIV;

    col = (int)((mx - inicio) / celda);
    fila = DIV - 1 - (int)(my / celda);
    if (col < 0)    col = 0;
    if (col >= DIV) col = DIV - 1;
    if (fila < 0)    fila = 0;
    if (fila >= DIV) fila = DIV - 1;
}

// CALLBACKS 
void mouseBotonCallback(GLFWwindow* window, int boton, int accion, int mods)
{
    if (boton != GLFW_MOUSE_BUTTON_LEFT) return;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    if (accion == GLFW_PRESS)
    {
        // si el clic cayo sobre el panel de ImGui, no se dibuja nada
        if (ImGui::GetIO().WantCaptureMouse) return;

        pixelACelda(window, mx, my, inicioX, inicioY);
        actualX = inicioX;
        actualY = inicioY;

        if (modo == RELLENO)            // el relleno actua con un solo clic, sobre la figura que se toco
        {
            int idx = figuraEnCelda(inicioX, inicioY, true);
            if (idx >= 0)
            {
                figuras[idx].relleno = true;
                copiarColor(figuras[idx].colorRelleno, colorLapiz);
            }
            necesitaActualizar = true;
            return;
        }

        if (modo == SELECCIONAR)        // elige la figura; si hay una, se puede arrastrar para moverla
        {
            seleccionada = figuraEnCelda(inicioX, inicioY, false);
            dibujando = (seleccionada >= 0);
            necesitaActualizar = true;
            return;
        }

        enCreacion = crearFigura(modo, inicioX, inicioY, actualX, actualY);
        dibujando = true;
        necesitaActualizar = true;
    }
    else if (accion == GLFW_RELEASE && dibujando)
    {
        dibujando = false;

        if (modo != SELECCIONAR)
        {
            pixelACelda(window, mx, my, actualX, actualY);

            if (modo == LIBRE)
                enCreacion.pts.push_back(punto((float)actualX, (float)actualY));
            else
                enCreacion = crearFigura(modo, inicioX, inicioY, actualX, actualY);

            figuras.push_back(enCreacion);      // ahora si queda guardada
        }
        necesitaActualizar = true;
    }
}

void mouseMovimientoCallback(GLFWwindow* window, double mx, double my)
{
    if (!dibujando) return;

    int c, f;
    pixelACelda(window, mx, my, c, f);
    if (c == actualX && f == actualY) return;   // solo si cambio de celda

    if (modo == SELECCIONAR)
    {
        // mover la figura: sumo a la traslacion cuantas celdas se movio el mouse
        figuras[seleccionada].tx += (float)(c - actualX);
        figuras[seleccionada].ty += (float)(f - actualY);
    }
    else if (modo == LIBRE)
    {
        // forma libre: guardo cada celda por la que pasa el mouse (Bresenham une los puntos al dibujar)
        enCreacion.pts.push_back(punto((float)c, (float)f));
    }
    else
    {
        enCreacion = crearFigura(modo, inicioX, inicioY, c, f);
    }

    actualX = c;
    actualY = f;
    necesitaActualizar = true;
}

// Atajos de teclado (siguen funcionando, ademas del panel)
void teclaCallback(GLFWwindow* window, int key, int sc, int accion, int mods)
{
    if (accion != GLFW_PRESS) return;
    if (ImGui::GetIO().WantCaptureKeyboard) return;   // por ejemplo, si estas escribiendo en un campo de ImGui
    if (dibujando) return;                            // no cambiar de herramienta a la mitad de un trazo

    if (key == GLFW_KEY_C)
    {
        figuras.clear();
        seleccionada = -1;
        necesitaActualizar = true;
    }
    if (key == GLFW_KEY_DELETE) eliminarSeleccionada();

    if (key == GLFW_KEY_L) modo = LINEA;
    if (key == GLFW_KEY_O) modo = CIRCULO;
    if (key == GLFW_KEY_T) modo = TRIANGULO;
    if (key == GLFW_KEY_Q) modo = CUADRADO;
    if (key == GLFW_KEY_F) modo = LIBRE;
    if (key == GLFW_KEY_B) modo = RELLENO;
    if (key == GLFW_KEY_S) modo = SELECCIONAR;

    // Teclas 1 a 8 = colores de la paleta
    if (key >= GLFW_KEY_1 && key < GLFW_KEY_1 + N_COLORES)
        copiarColor(colorLapiz, PALETA[key - GLFW_KEY_1]);
}

// GEOMETRIA 
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
        v.insert(v.end(), { p,-1.0f,0.0f,  p,1.0f,0.0f });     // vertical
        v.insert(v.end(), { -1.0f,p,0.0f,  1.0f,p,0.0f });     // horizontal
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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // tamano fijo para que el mouse siempre calce

    // la ventana es el panel (a la izquierda) + el lienzo cuadrado (a la derecha)
    GLFWwindow* window = glfwCreateWindow(PANEL + VENTANA, VENTANA, "Mini Paint", NULL, NULL);
    if (!window) { std::cout << "Failed to create GLFW window\n"; glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    gladLoadGL();

    // Primero registramos NUESTROS callbacks...
    glfwSetMouseButtonCallback(window, mouseBotonCallback);
    glfwSetCursorPosCallback(window, mouseMovimientoCallback);
    glfwSetKeyCallback(window, teclaCallback);

    // ...y despues iniciamos ImGui: asi ImGui recibe los eventos primero y luego nos los pasa a nosotros
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsLight();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

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

    // ---- VAO de los cuadraditos ----
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

    // ---- VAO de las lineas de la cuadricula ----
    std::vector<float> lin = generarLineas();
    GLuint VAOl, VBOl;
    glGenVertexArrays(1, &VAOl);
    glGenBuffers(1, &VBOl);
    glBindVertexArray(VAOl);
    glBindBuffer(GL_ARRAY_BUFFER, VBOl);
    glBufferData(GL_ARRAY_BUFFER, lin.size() * sizeof(float), lin.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttrib3f(1, 0.6f, 0.75f, 0.95f);   // celeste fijo para las lineas
    glBindVertexArray(0);

    // ---- Bucle principal ----
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // 1. Armamos el panel de ImGui de este frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        dibujarPanel();
        ImGui::Render();

        // 2. Si algo cambio, rehacemos el lienzo a partir de las figuras y actualizamos los colores
        if (necesitaActualizar)
        {
            reconstruirLienzo();       // todas las figuras, con sus transformaciones
            pantalla = lienzo;

            if (seleccionada >= 0 && seleccionada < (int)figuras.size())
                dibujarCaja(pantalla, figuras[seleccionada]);    // caja gris de la seleccionada

            if (dibujando && modo != SELECCIONAR)
                rasterizarFigura(pantalla, enCreacion);          // vista previa de lo que se esta dibujando

            col = coloresPorVertice(pantalla);
            glBindBuffer(GL_ARRAY_BUFFER, VBOcol);
            glBufferSubData(GL_ARRAY_BUFFER, 0, col.size() * sizeof(float), col.data());
            necesitaActualizar = false;
        }

        // 3. Fondo de toda la ventana
        int fbAncho, fbAlto;
        glfwGetFramebufferSize(window, &fbAncho, &fbAlto);
        glViewport(0, 0, fbAncho, fbAlto);
        glClearColor(0.94f, 0.94f, 0.94f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 4. El lienzo se dibuja solo en el cuadrado de la derecha
        glViewport(fbAncho - fbAlto, 0, fbAlto, fbAlto);
        glUseProgram(shaderProgram);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, pos.size() / 3);   // celdas pintadas

        glBindVertexArray(VAOl);
        glDrawArrays(GL_LINES, 0, lin.size() / 3);       // cuadricula encima

        // 5. Encima de todo, el panel de ImGui
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &VAO);
    glDeleteVertexArrays(1, &VAOl);
    glDeleteBuffers(1, &VBOpos);
    glDeleteBuffers(1, &VBOcol);
    glDeleteBuffers(1, &VBOl);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}
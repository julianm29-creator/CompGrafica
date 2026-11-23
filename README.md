# CompGrafica
// CompGrafica – Mini Paint

Nombre: Julian Mauricio Forero Castro
Código:6000983
Asignatura: Introduccion a la Computación Gráfica

Descripción del proyecto:
El proyecto consiste en el desarrollo de una aplicación gráfica interactiva llamada Mini Paint, utilizando C++ y OpenGL. La aplicación permite dibujar diferentes figuras geométricas sobre un lienzo dividido en una cuadrícula de 150 x 150 celdas.
El programa cuenta con herramientas para dibujar líneas, círculos, triángulos, cuadrados y formas libres. También permite cambiar los colores, aplicar rellenos, seleccionar figuras, moverlas, escalarlas, rotarlas y eliminarlas.
La interfaz gráfica se desarrolla mediante Dear ImGui, mientras que OpenGL se encarga de representar los elementos visuales en pantalla.


Arquitectura del proyecto

El programa está desarrollado principalmente en un único archivo `main.cpp`, organizado mediante funciones y estructuras que dividen las responsabilidades de la aplicación.

1. Inicialización y configuración gráfica
Se encarga de configurar la ventana y preparar el entorno gráfico utilizando GLFW, GLAD y OpenGL. También inicializa los shaders y los recursos necesarios para dibujar el lienzo.

2. Sistema de figuras geométricas
Utiliza la estructura `Figura` para almacenar el tipo de figura, sus puntos, colores, relleno y transformaciones. Las figuras se almacenan en un vector que permite administrarlas durante la ejecución.

3. Algoritmos de dibujo
Se implementan algoritmos de rasterización para representar las figuras sobre la cuadrícula:

- Bresenham para líneas: permite dibujar líneas mediante el cálculo de las celdas que deben pintarse.
- Bresenham para círculos: utiliza simetría para calcular los puntos que forman el contorno del círculo.
- Flood Fill: permite rellenar regiones cerradas utilizando una pila y revisando las celdas vecinas.

4. Sistema de transformaciones
Se utilizan matrices de transformación 3x3 con coordenadas homogéneas para realizar operaciones geométricas:

- Traslación.
- Escalamiento.
- Rotación.

Estas transformaciones se aplican a los puntos de las figuras tomando como referencia su centro.

5. Interfaz gráfica
Se utiliza Dear ImGui para construir un panel de herramientas que permite seleccionar las figuras, cambiar colores, elegir herramientas y modificar las transformaciones de los objetos.

6. Interacción con el usuario
Mediante los callbacks de GLFW se procesan los eventos del mouse y el teclado. Esto permite dibujar figuras, seleccionar objetos, arrastrarlos y utilizar atajos de teclado.

7 Renderizado
OpenGL se encarga de representar el lienzo mediante buffers de vértices y colores. El programa actualiza los colores de las celdas cuando se realizan modificaciones y dibuja la cuadrícula encima de las figuras.

Tecnologías y bibliotecas utilizadas

- C++: lenguaje principal de programación.
- OpenGL 3.3: representación gráfica.
- GLFW: creación de ventanas y gestión de eventos.
- GLAD: carga de funciones de OpenGL.
- Dear ImGui: desarrollo de la interfaz gráfica.

Herramientas disponibles

- Dibujo de líneas.
- Dibujo de círculos.
- Dibujo de triángulos.
- Dibujo de cuadrados.
- Dibujo a mano alzada.
- Relleno de figuras.
- Selección y eliminación de figuras.
- Traslación, escalamiento y rotación.
- Selección de colores mediante una paleta y un selector personalizado.

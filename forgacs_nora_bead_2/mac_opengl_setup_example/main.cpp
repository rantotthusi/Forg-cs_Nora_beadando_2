#include <array>
#include <fstream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <math.h>
#include <string>
#include <vector>

using namespace std;

int        window_width = 600;
int        window_height = 600;
char    window_title[] = "Bezier curve";

std::vector<glm::vec3> pointToDraw;
GLint dragged = -1;

std::vector<glm::vec3> myControlPoints = {
    glm::vec3(-0.5f, -0.5f, 0.0f),
    glm::vec3(-0.5f,  0.5f, 0.0f),
    glm::vec3(0.5f, -0.5f, 0.0f),
    glm::vec3(0.5f,  0.5f, 0.0f),
    //glm::vec3(-1.0f,  1.0f, 0.0f),

};

/* Vertex buffer objektum és vertex array objektum az adattároláshoz.*/
#define numVBOs 1
#define numVAOs 1
GLuint VBO[numVBOs];
GLuint VAO[numVAOs];

GLuint renderingProgram;

bool checkOpenGLError() {
    bool foundError = false;
    int glErr = glGetError();
    while (glErr != GL_NO_ERROR) {
        cout << "glError: " << glErr << endl;
        foundError = true;
        glErr = glGetError();
    }
    return foundError;
}

void printShaderLog(GLuint shader) {
    int len = 0;
    int chWrittn = 0;
    char* log;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len > 0) {
        log = (char*)malloc(len);
        glGetShaderInfoLog(shader, len, &chWrittn, log);
        cout << "Shader Info Log: " << log << endl;
        free(log);
    }
}

void printProgramLog(int prog) {
    int len = 0;
    int chWrittn = 0;
    char* log;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    if (len > 0) {
        log = (char*)malloc(len);
        glGetProgramInfoLog(prog, len, &chWrittn, log);
        cout << "Program Info Log: " << log << endl;
        free(log);
    }
}

string readShaderSource(const char* filePath) {
    string content;
    ifstream fileStream(filePath, ios::in);
    string line = "";

    while (!fileStream.eof()) {
        getline(fileStream, line);
        content.append(line + "\n");
    }
    fileStream.close();
    return content;
}

GLuint createShaderProgram()
{
    GLint vertCompiled;
    GLint fragCompiled;
    GLint linked;

    string vertShaderStr = readShaderSource("shaders/vertex_shader.glsl");
    string fragShaderStr = readShaderSource("shaders/fragment_shader.glsl");

    if (vertShaderStr.empty() || fragShaderStr.empty()) {
        cerr << "Shader source file could not be read." << endl;
        return 0;
    }

    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);

    if (vShader == 0 || fShader == 0) {
        cerr << "Error creating shader objects." << endl;
        return 0;
    }

    const char* vertShaderSrc = vertShaderStr.c_str();
    const char* fragShaderSrc = fragShaderStr.c_str();

    glShaderSource(vShader, 1, &vertShaderSrc, NULL);
    glShaderSource(fShader, 1, &fragShaderSrc, NULL);

    glCompileShader(vShader);
    checkOpenGLError();
    glGetShaderiv(vShader, GL_COMPILE_STATUS, &vertCompiled);
    if (vertCompiled != GL_TRUE) {
        cerr << "Vertex shader compilation failed." << endl;
        printShaderLog(vShader);
        glDeleteShader(vShader);
        glDeleteShader(fShader);
        return 0;
    }

    glCompileShader(fShader);
    checkOpenGLError();
    glGetShaderiv(fShader, GL_COMPILE_STATUS, &fragCompiled);
    if (fragCompiled != GL_TRUE) {
        cerr << "Fragment shader compilation failed." << endl;
        printShaderLog(fShader);
        glDeleteShader(vShader);
        glDeleteShader(fShader);
        return 0;
    }

    GLuint vfProgram = glCreateProgram();
    if (vfProgram == 0) {
        cerr << "Error creating shader program." << endl;
        return 0;
    }

    glAttachShader(vfProgram, vShader);
    glAttachShader(vfProgram, fShader);
    glLinkProgram(vfProgram);
    checkOpenGLError();
    glGetProgramiv(vfProgram, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        cerr << "Shader program linking failed." << endl;
        printProgramLog(vfProgram);
        glDeleteShader(vShader);
        glDeleteShader(fShader);
        glDeleteProgram(vfProgram);
        return 0;
    }

    glDeleteShader(vShader);
    glDeleteShader(fShader);

    return vfProgram;
}

GLfloat dist2(glm::vec3 P1, glm::vec3 P2) {
    GLfloat dx = P1.x - P2.x;
    GLfloat dy = P1.y - P2.y;

    return dx * dx + dy * dy;
}

GLint getActivePoint(vector<glm::vec3> p, GLfloat sensitivity, GLfloat x, GLfloat y) {
    GLfloat        s = sensitivity * sensitivity;
    GLint        size = p.size();
    GLfloat        xNorm = x / (window_width / 2) - 1.0f;
    GLfloat        yNorm = y / (window_height / 2) - 1.0f;
    glm::vec3    P = glm::vec3(xNorm, yNorm, 0.0f);

    for (GLint i = 0; i < size; i++)
        if (dist2(p[i], P) < s)
            return i;

    return -1;
}

/*
The definition of N choose R is to compute the two productsand divide one with the other,
(N * (N - 1) * (N - 2) * ... * (N - R + 1)) / (1 * 2 * 3 * ... * R)
However, the multiplications may become too large really quickand overflow existing data type.The implementation trick is to reorder the multiplicationand divisions as,
👎 / 1 * (N - 1) / 2 * (N - 2) / 3 * ... * (N - R + 1) / R
It's guaranteed that at each step the results is divisible (for n continuous numbers, one of them must be divisible by n, so is the product of these numbers).
For example, for N choose 3, at least one of the N, N - 1, N - 2 will be a multiple of 3, and for N choose 4, at least one of N, N - 1, N - 2, N - 3 will be a multiple of 4.
C++ code given below.
*/
int NCR(int n, int r) {
    /*
    binomial coefficient
    */
    if (r == 0) return 1;

    /*
     Extra computation saving for large R,
     using property:
     N choose R = N choose (N - R)
    */
    if (r > n / 2) return NCR(n, n - r);

    long res = 1;

    for (int k = 1; k <= r; ++k) {
        res *= n - k + 1;
        res /= k;
    }

    return res;
}

/*
It will be the Bernstein basis polynomial of degree n.
*/
GLfloat blending(GLint n, GLint i, GLfloat t) {
    return NCR(n, i) * pow(t, i) * pow(1.0f - t, n - i);
}

void drawBezierCurve(std::vector<glm::vec3> controlPoints) {
    /*
    https://en.wikipedia.org/wiki/B%C3%A9zier_curve#Explicit_definition
    */
    glm::vec3    nextPoint;
    GLfloat        t = 0.0f, B;
    GLfloat        increment = 1.0f / 100.0f; /* hány darab szakaszból rakjuk össze a görbénket? */

    while (t <= 1.0f) {
        nextPoint = glm::vec3(0.0f, 0.0f, 0.0f);
        for (int i = 0; i < controlPoints.size(); i++) {
            B = blending(controlPoints.size() - 1, i, t);
            nextPoint.x += B * controlPoints.at(i).x;
            nextPoint.y += B * controlPoints.at(i).y;
            nextPoint.z += B * controlPoints.at(i).z;
        }

        pointToDraw.push_back(nextPoint);
        t += increment;
    }
    for (int i = 0; i < controlPoints.size(); i++)
    {
        pointToDraw.push_back(controlPoints[i]);
    }
    
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
}

void cursorPosCallback(GLFWwindow* window, double xPos, double yPos) {
    if (dragged >= 0) {
        GLfloat    xNorm = xPos / (window_width / 2) - 1.0f;
        GLfloat    yNorm = (window_height - yPos) / (window_height / 2) - 1.0f;

        myControlPoints.at(dragged).x = xNorm;
        myControlPoints.at(dragged).y = yNorm;

        glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
        glBufferData(GL_ARRAY_BUFFER, pointToDraw.size() * sizeof(glm::vec3), pointToDraw.data(), GL_STATIC_DRAW);
        
        pointToDraw.clear();
        drawBezierCurve(myControlPoints);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

//void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
//    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
//        double    x, y;
//
//        glfwGetCursorPos(window, &x, &y);
//        dragged = getActivePoint(myControlPoints, 0.1f, x, window_height - y);
//    }
//
//    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
//        dragged = -1;
//
//}
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    double x, y;
    glfwGetCursorPos(window, &x, &y);

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        // Try to drag an existing point
        dragged = getActivePoint(myControlPoints, 0.1f, x, window_height - y);

        // If no point selected, add new point
        if (dragged == -1) {
            GLfloat xNorm = x / (window_width / 2) - 1.0f;
            GLfloat yNorm = (window_height - y) / (window_height / 2) - 1.0f;

            myControlPoints.push_back(glm::vec3(xNorm, yNorm, 0.0f));
        }

        // Update curve and buffer
        pointToDraw.clear();
        drawBezierCurve(myControlPoints);
        glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
        glBufferData(GL_ARRAY_BUFFER, pointToDraw.size() * sizeof(glm::vec3), pointToDraw.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        dragged = -1;
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        GLint indexToRemove = getActivePoint(myControlPoints, 0.1f, x, window_height - y);
        if (indexToRemove != -1) {
            myControlPoints.erase(myControlPoints.begin() + indexToRemove);

            // Recompute curve
            pointToDraw.clear();
            drawBezierCurve(myControlPoints);
            glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
            glBufferData(GL_ARRAY_BUFFER, pointToDraw.size() * sizeof(glm::vec3), pointToDraw.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
}


void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    window_width = width;
    window_height = height;

    glViewport(0, 0, width, height);
}

void init(GLFWwindow* window) {
    renderingProgram = createShaderProgram();

    drawBezierCurve(myControlPoints);

    /* Létrehozzuk a szükséges Vertex buffer és vertex array objektumot. */
    glGenBuffers(numVBOs, VBO);
    glGenVertexArrays(numVAOs, VAO);

    /* Típus meghatározása: a GL_ARRAY_BUFFER nevesített csatolóponthoz kapcsoljuk a buffert (ide kerülnek a vertex adatok). */
    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);

    /* Másoljuk az adatokat a pufferbe! Megadjuk az aktuálisan csatolt puffert,  azt hogy hány bájt adatot másolunk,
    a másolandó adatot, majd a feldolgozás módját is meghatározzuk: most az adat nem változik a feltöltés után */
    glBufferData(GL_ARRAY_BUFFER, pointToDraw.size() * sizeof(glm::vec3), pointToDraw.data(), GL_STATIC_DRAW);

    /* Csatoljuk a vertex array objektumunkat a konfiguráláshoz. */
    glBindVertexArray(VAO[0]);

    /* Ezen adatok szolgálják a 0 indexű vertex attribútumot (itt: pozíció).
    Elsőként megadjuk ezt az azonosítószámot.
    Utána az attribútum méretét (vec3, láttuk a shaderben).
    Harmadik az adat típusa.
    Negyedik az adat normalizálása, ez maradhat FALSE jelen példában.
    Az attribútum értékek hogyan következnek egymás után? Milyen lépésköz után találom a következő vertex adatait?
    Végül megadom azt, hogy honnan kezdődnek az értékek a pufferben. Most rögtön, a legelejétől veszem őket.*/
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    /* Engedélyezzük az imént definiált 0 indexű attribútumot. */
    glEnableVertexAttribArray(0);

    /* Leválasztjuk a vertex array objektumot és a puffert is.*/
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // aktiváljuk a shader-program objektumunkat.
    glUseProgram(renderingProgram);

    glClearColor(0.0, 0.0, 0.0, 1.0);
}

/** A jelenetünk utáni takarítás. */
void cleanUpScene() {
    /** Töröljük a vertex puffer és vertex array objektumokat. */
    glDeleteVertexArrays(numVAOs, VAO);
    glDeleteBuffers(numVBOs, VBO);

    /** Töröljük a shader programot. */
    glDeleteProgram(renderingProgram);
}

void display(GLFWwindow* window, double currentTime) {
    glClear(GL_COLOR_BUFFER_BIT); // fontos lehet minden egyes alkalommal törölni!

    /*Csatoljuk a vertex array objektumunkat. */
    glBindVertexArray(VAO[0]);
    glLineWidth(2.0f);
    glPointSize(7.0f);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glDrawArrays(GL_LINE_STRIP, 0, pointToDraw.size() - myControlPoints.size());
    //glDrawArrays(GL_LINE_STRIP, pointToDraw.size() - myControlPoints.size(), myControlPoints.size());
    glDrawArrays(GL_POINTS, pointToDraw.size() - myControlPoints.size(), myControlPoints.size());
    
    /* Leválasztjuk, nehogy bármilyen érték felülíródjon.*/
    glBindVertexArray(0);
}

int main(void) {
    /* Próbáljuk meg inicializálni a GLFW-t! */
    if (!glfwInit()) { exit(EXIT_FAILURE); }

    /* A kívánt OpenGL verzió (4.3) */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    /* Próbáljuk meg létrehozni az ablakunkat. */
    GLFWwindow* window = glfwCreateWindow(window_width, window_height, window_title, NULL, NULL);

    /* Válasszuk ki az ablakunk OpenGL kontextusát, hogy használhassuk. */
    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    /* Incializáljuk a GLEW-t, hogy elérhetővé váljanak az OpenGL függvények. */
    if (glewInit() != GLEW_OK) { exit(EXIT_FAILURE); }
    glfwSwapInterval(1);

    /* Az alkalmazáshoz kapcsolódó előkészítő lépések, pl. hozd létre a shader objektumokat. */
    init(window);

    while (!glfwWindowShouldClose(window)) {
        /* a kód, amellyel rajzolni tudunk a GLFWwindow ojektumunkba. */
        display(window, glfwGetTime());
        /* double buffered */
        glfwSwapBuffers(window);
        /* események kezelése az ablakunkkal kapcsolatban, pl. gombnyomás */
        glfwPollEvents();
    }

    /* töröljük a GLFW ablakot. */
    glfwDestroyWindow(window);
    /* Leállítjuk a GLFW-t */

    cleanUpScene();

    glfwTerminate();
    exit(EXIT_SUCCESS);
}

//#include <GL/glew.h>
//#include <GLFW/glfw3.h>
//
//#include "glm/glm.hpp"
//
//#include <array>
//#include <iostream>
//#include <string>
//#include <fstream>
//#include <vector>
//
//#define PI 3.14159265359
//
//using namespace std;
//
//#define numVAOs 1
//
//#include "shaders.h"
//
//GLuint VBO;
//GLuint VAO;
//
//std::vector<glm::vec3> verticesBuffer;
//GLfloat centerx = 0;
//GLfloat centery = 0.5;
//GLfloat radius = 0.14;
//GLdouble deltaTime = 0;
//GLboolean direction = false;
//void updateVertexData()
//{
//    verticesBuffer.clear();
//    verticesBuffer.push_back(glm::vec3(0.33, centery, 0));
//    verticesBuffer.push_back(glm::vec3(-0.33, centery, 0));
//
//    for (int i = 0; i < 100; i++)
//    {
//        verticesBuffer.push_back(glm::vec3(centerx + radius * cos(i * (2 * PI / 100)), centery + radius * sin(i * (2 * PI / 100)), 0));
//    }
//}
//
//void updateVBO() {
//    glBindBuffer(GL_ARRAY_BUFFER, VBO);
//    glBufferData(GL_ARRAY_BUFFER, verticesBuffer.size() * sizeof(glm::vec3), verticesBuffer.data(), GL_STATIC_DRAW);
//    glBindBuffer(GL_ARRAY_BUFFER, 0);
//}
//
//GLuint renderingProgram;
//GLuint vao[numVAOs];
//
//bool checkOpenGLError() {
//    bool foundError = false;
//    int glErr = glGetError();
//    while (glErr != GL_NO_ERROR) {
//        cout << "glError: " << glErr << endl;
//        foundError = true;
//        glErr = glGetError();
//    }
//    return foundError;
//}
//
//void printShaderLog(GLuint shader) {
//    int len = 0;
//    int chWrittn = 0;
//    char* log;
//    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
//    if (len > 0) {
//        log = (char*)malloc(len);
//        glGetShaderInfoLog(shader, len, &chWrittn, log);
//        cout << "Shader Info Log: " << log << endl;
//        free(log);
//    }
//}
//
//void printProgramLog(int prog) {
//    int len = 0;
//    int chWrittn = 0;
//    char* log;
//    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
//    if (len > 0) {
//        log = (char*)malloc(len);
//        glGetProgramInfoLog(prog, len, &chWrittn, log);
//        cout << "Program Info Log: " << log << endl;
//        free(log);
//    }
//}
//
//string readShaderSource(const char* filePath) {
//    string content;
//    ifstream fileStream(filePath, ios::in);
//    string line = "";
//
//    while (!fileStream.eof()) {
//        getline(fileStream, line);
//        content.append(line + "\n");
//    }
//    fileStream.close();
//    return content;
//}
//
//GLuint createShaderProgram() {
//    const char* vertShaderStr = "shaders/fragment_shader.glsl";
//    const char* fragShaderStr = "shaders/vertex_shader.glsl";
//
//    GLuint vfProgram = glCreateProgram();
//
//    if(!InitShader(vfProgram, vertShaderStr, fragShaderStr)) {
//        std::cerr << "Could not initialize shaders" << std::endl;
//        return 1;
//    }
//
//    return vfProgram;
//}
//
////Nyilak
//void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
//{
//    if (action == GLFW_PRESS)
//    {
//        switch (key) {
//            case GLFW_KEY_UP:
//                centery += 0.1;
//                break;
//            case GLFW_KEY_DOWN:
//                centery -= 0.1;
//                break;
//            case GLFW_KEY_RIGHT:
//                centerx += 0.1;
//                break;
//            case GLFW_KEY_LEFT:
//                centerx -= 0.1;
//                break;
//        }
//    }
//    updateVertexData();
//    updateVBO();
//}
//
//void cursorPosCallback(GLFWwindow* window, double xPos, double yPos)
//{
//
//}
//
//void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
//{
//
//}
//
//void init() {
//    renderingProgram = createShaderProgram();
//
//    /* Létrehozzuk a szükséges Vertex buffer és vertex array objektumot. */
//    glGenBuffers(1, &VBO);
//    glGenVertexArrays(1, &VAO);
//
//    /* Típus meghatározása: a GL_ARRAY_BUFFER nevesített csatolóponthoz kapcsoljuk a buffert (ide kerülnek a vertex adatok). */
//    glBindBuffer(GL_ARRAY_BUFFER, VBO);
//
//    /* Másoljuk az adatokat a pufferbe! Megadjuk az aktuálisan csatolt puffert,  azt hogy hány bájt adatot másolunk,
//     a másolandó adatot, majd a feldolgozás módját is meghatározzuk: most az adat nem változik a feltöltés után */
//    //glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
//    glBufferData(GL_ARRAY_BUFFER, verticesBuffer.size() * sizeof(glm::vec3), verticesBuffer.data(), GL_STATIC_DRAW);
//
//    /* A puffer kész, lecsatoljuk, már nem szeretnénk módosítani. */
//    glBindBuffer(GL_ARRAY_BUFFER, 0);
//
//    /* Csatoljuk a vertex array objektumunkat a konfiguráláshoz. */
//    glBindVertexArray(VAO);
//
//    /* Vertex buffer objektum újracsatolása. */
//    glBindBuffer(GL_ARRAY_BUFFER, VBO);
//
//    /* Ezen adatok szolgálják a 0 indexû vertex attribútumot (itt: pozíció).
//     Elsõként megadjuk ezt az azonosítószámot.
//     Utána az attribútum méretét (vec3, láttuk a shaderben).
//     Harmadik az adat típusa.
//     Negyedik az adat normalizálása, ez maradhat FALSE jelen példában.
//     Az attribútum értékek hogyan következnek egymás után? Milyen lépésköz után találom a következõ vertex adatait?
//     Végül megadom azt, hogy honnan kezdõdnek az értékek a pufferben. Most rögtön, a legelejétõl veszem õket.*/
//    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
//
//    /* Engedélyezzük az imént definiált 0 indexû attribútumot. */
//    glEnableVertexAttribArray(0);
//
//    /* Leválasztjuk a vertex array objektumot és a puufert is.*/
//    glBindVertexArray(0);
//    glBindBuffer(GL_ARRAY_BUFFER, 0);
//
//}
//
///** A jelenetünk utáni takarítás. */
//void cleanUpScene()
//{
//    /** Töröljük a vertex puffer és vertex array objektumokat. */
//    glDeleteVertexArrays(1, &VAO);
//    glDeleteBuffers(1, &VBO);
//
//    /** Töröljük a shader programot. */
//    glDeleteProgram(renderingProgram);
//}
//
//void display(GLFWwindow* window, double currentTime) {
//
//    glClearColor(0.96, 0.84, 0.26, 1.0);
//    glClear(GL_COLOR_BUFFER_BIT); // fontos lehet minden egyes alkalommal törölni!
//
//    // aktiváljuk a shader-program objektumunkat.
//    glUseProgram(renderingProgram);
//
//    /*Csatoljuk a vertex array objektumunkat. */
//    glBindVertexArray(VAO);
//    unsigned int isLine = glGetUniformLocation(renderingProgram, "isLine");
//    unsigned int center = glGetUniformLocation(renderingProgram, "center");
//
//    glUniform3fv(center, 1, new float[] { centerx * 300, centery * 300, 0 });
//
//    glPointSize(25.0);
//    glLineWidth(10.0);
//    int offset = 0;
//    glUniform1i(isLine, true);
//    glDrawArrays(GL_LINE_LOOP, offset, 2);
//    offset += 2;
//    glUniform1i(isLine, false);
//    glLineWidth(1.0);
//    glDrawArrays(GL_TRIANGLE_FAN, offset, 100);
//    offset += 100;
//
//    if (currentTime - 0.05f > deltaTime) {
//        deltaTime = currentTime;
//        if (centerx + radius > 1 || centerx - radius < -1) {
//            direction = !direction;
//        }
//        if (direction) {
//            centerx += 0.05f;
//        }
//        else
//        {
//            centerx -= 0.05f;
//        }
//        updateVertexData();
//        updateVBO();
//    }
//    /* Leválasztjuk, nehogy bármilyen érték felülíródjon.*/
//    glBindVertexArray(0);
//
//}
//
////error_callback()
//void error_callback(int error, const char* description)
//{
//    fputs(description, stderr);
//    std::cout << std::endl;
//}
//
//int main(void) {
//    if (!glfwInit()) { exit(EXIT_FAILURE); }
//
//    glfwSetErrorCallback(error_callback);
//    updateVertexData();
//
//    const GLint kWindowWidth = 600; //Szélesség: 600px
//    const GLint kWindowHeight = 600; //Magasság: 600px
//
//    std::cout << "GL setup example\n";
//
//    std::string s_cwd;
//    s_cwd = std::filesystem::current_path();
//
//    std::cout << "C++ Current working directory is: " << s_cwd << std::endl;
//
//    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
//    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
//    glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
//    glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
//    glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE);
//
//    GLFWwindow *window = glfwCreateWindow(kWindowWidth, kWindowHeight, "OpenGL Setup Example", nullptr, nullptr);
//
//    // may differ than expected due to Retina display
//    int screenWidth, screenHeight;
//    glfwGetFramebufferSize(window, &screenWidth, &screenHeight);
//
//
//
//    glfwMakeContextCurrent(window);
//
//    glewExperimental = true; // Needed for core profile
//    if (glewInit() != GLEW_OK) {
//        fprintf(stderr, "Failed to initialize GLEW\n");
//        getchar();
//        glfwTerminate();
//        return -1;
//    }
//
//    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
//    glfwSetKeyCallback(window, keyCallback);
//    glfwSetCursorPosCallback(window, cursorPosCallback);
//    glfwSetMouseButtonCallback(window, mouseButtonCallback);
//
//
//    /* Incializáljuk a GLEW-t, hogy elérhetõvé váljanak az OpenGL függvények. */
//    if (glewInit() != GLEW_OK) { exit(EXIT_FAILURE); }
//    glfwSwapInterval(1);
//
//    /* Az alkalmazáshoz kapcsolódó elõkészítõ lépések, pl. hozd létre a shader objektumokat. */
//    init();
//
//    while (!glfwWindowShouldClose(window)) {
//        /* a kód, amellyel rajzolni tudunk a GLFWwindow ojektumunkba. */
//        display(window, glfwGetTime());
//        /* double buffered */
//        glfwSwapBuffers(window);
//        /* események kezelése az ablakunkkal kapcsolatban, pl. gombnyomás */
//        glfwPollEvents();
//    }
//
//    glfwDestroyWindow(window);
//    cleanUpScene();
//
//    glfwTerminate();
//    exit(EXIT_SUCCESS);
//}

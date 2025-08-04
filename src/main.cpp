#include <iostream>
#include <vector>
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// stb_easy_font implementation (public domain)
#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

// Vertex shader source
const char* vertexSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec2 uOffset;
uniform vec2 uScale;
void main() {
    vec2 pos = aPos * uScale + uOffset;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)";

// Fragment shader source for text
const char* textFragmentSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uTextColor;
void main() {
    FragColor = vec4(uTextColor, 1.0); // Dynamic text color
}
)";

// Vertex shader source for text
const char* textVertexSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec2 uScreenSize;
void main() {
    // Convert screen coordinates to NDC
    vec2 ndc = (aPos / uScreenSize) * 2.0 - 1.0;
    ndc.y = -ndc.y; // Flip Y coordinate
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

// Fragment shader source
const char* fragmentSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

// Window dimensions
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Unit quad verts
float quadVerts[] = { -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f };
unsigned int quadIdx[] = { 0,1,2,  2,3,0 };

// Game state - inspired by test.cpp complexity
float dinoY = 0.0f;
float vel = 0.0f;
const float grav = -5.0f;  // Chrome-like gravity  
const float jump = 2.2f;   // Improved jump height like test.cpp
bool onGround = true;
bool gameOver = false;
int score = 0;
bool nightMode = false;    // Night mode feature
int nightModeThreshold = 10; // Switch to night mode after 100 points

struct Obstacle { float x, w; bool scored; };
std::vector<Obstacle> obstacles;
float timer = 0.0f;
float baseInterval = 1.5f;    // Dynamic base interval like test.cpp

// Function to reset game state
void resetGame() {
    dinoY = 0.0f;
    vel = 0.0f;
    onGround = true;
    gameOver = false;
    score = 0;
    nightMode = false;  // Reset night mode
    obstacles.clear();
    timer = 0.0f;
}

// Compile shader helper
unsigned int compileShader(unsigned int type, const char* src) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << std::endl;
    }
    return shader;
}

int main() {
    // Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Dino Runner", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr<<"GLAD fail"; return -1; }
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

    // Build shaders
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    unsigned int prog = glCreateProgram(); glAttachShader(prog, vs); glAttachShader(prog, fs); glLinkProgram(prog); glDeleteShader(vs); glDeleteShader(fs);

    // Build text shaders
    unsigned int textVs = compileShader(GL_VERTEX_SHADER, textVertexSrc);
    unsigned int textFs = compileShader(GL_FRAGMENT_SHADER, textFragmentSrc);
    unsigned int textProg = glCreateProgram(); glAttachShader(textProg, textVs); glAttachShader(textProg, textFs); glLinkProgram(textProg); glDeleteShader(textVs); glDeleteShader(textFs);

    // VAO/VBO/EBO
    unsigned int VAO, VBO, EBO; glGenVertexArrays(1,&VAO); glGenBuffers(1,&VBO); glGenBuffers(1,&EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO); glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO); glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIdx), quadIdx, GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0); glEnableVertexAttribArray(0);

    // Text VAO/VBO for dynamic text rendering
    unsigned int textVAO, textVBO;
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    float last = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        float now = glfwGetTime(), dt = now - last; last = now;
        // Input handling
        if (!gameOver) {
            // Jump when UP key is pressed and dinosaur is on ground
            if (glfwGetKey(window, GLFW_KEY_UP)==GLFW_PRESS && onGround) { 
                vel = jump; 
                onGround = false; 
            }
            // Alternative jump with SPACE key
            if (glfwGetKey(window, GLFW_KEY_SPACE)==GLFW_PRESS && onGround) { 
                vel = jump; 
                onGround = false; 
            }
        } else {
            // Game over - allow restart
            if (glfwGetKey(window, GLFW_KEY_R)==GLFW_PRESS) {
                resetGame();
            }
            if (glfwGetKey(window, GLFW_KEY_SPACE)==GLFW_PRESS) {
                resetGame();
            }
        }
        // Exit game
        if (glfwGetKey(window, GLFW_KEY_ESCAPE)==GLFW_PRESS) glfwSetWindowShouldClose(window,true);
        // Update - Enhanced complexity inspired by test.cpp
        if (!gameOver) {
            vel += grav*dt; dinoY += vel*dt;
            if (dinoY<=0) { dinoY=0; vel=0; onGround=true; }
            
            // Night mode toggle like Chrome (every 100 points)
            bool shouldBeNight = (score / nightModeThreshold) % 2 == 1;
            nightMode = shouldBeNight;
            
            // Dynamic difficulty system like test.cpp
            float currentInterval = baseInterval - std::min(score*0.02f, 1.0f); // Decreases with score
            
            timer+=dt; 
            if (timer>=currentInterval) { 
                timer=0; 
                obstacles.push_back({1.2f, 0.08f, false}); // Start farther right like test.cpp
            }
            
            // More aggressive speed increase like test.cpp
            float obstacleSpeed = 0.5f + score * 0.05f; // Faster acceleration
            
            for (size_t i=0;i<obstacles.size();) {
                obstacles[i].x -= obstacleSpeed*dt;
                float hw=obstacles[i].w*0.5f;
                if (!obstacles[i].scored && obstacles[i].x+hw< -0.5f) { score++; obstacles[i].scored=true; }
                // Precise collision detection for dinosaur with forward-pointing triangular head
                float dl=-0.52f, dr=-0.45f, db=-0.5f+dinoY, dtp=db+0.12f; // Tighter dinosaur bounds (reduced gap)
                float ol=obstacles[i].x-hw*0.8f, orr=obstacles[i].x+hw*0.8f, ob=-0.52f, ot=-0.47f; // Tighter obstacle bounds
                if (dr>ol && dl<orr && dtp>ob && db<ot) { 
                    gameOver = true; // Game over without console spam
                }
                if (obstacles[i].x+hw < -1.0f) obstacles.erase(obstacles.begin()+i); else ++i;
            }
        }
        // Render with dynamic day/night mode
        if (nightMode) {
            glClearColor(0.15f, 0.15f, 0.15f, 1.0f); // Dark gray background for night mode
        } else {
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // White background for day mode
        }
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog); glBindVertexArray(VAO);
        GLint oLoc=glGetUniformLocation(prog,"uOffset"), sLoc=glGetUniformLocation(prog,"uScale"), cLoc=glGetUniformLocation(prog,"uColor");
        
        // Ground color changes with night mode
        if (nightMode) {
            glUniform3f(cLoc, 0.6f, 0.6f, 0.6f); // Lighter gray ground for night mode
        } else {
            glUniform3f(cLoc, 0.9f, 0.9f, 0.9f); // Light gray ground for day mode
        }
        glUniform2f(sLoc,2.0f,0.05f); glUniform2f(oLoc,0.0f,-0.5f); glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
        
        // Dinosaur color changes with night mode
        if (nightMode) {
            glUniform3f(cLoc, 0.9f, 0.9f, 0.9f); // Light gray dinosaur for night mode (inverted)
        } else {
            glUniform3f(cLoc, 0.0f, 0.0f, 0.0f); // Black dinosaur for day mode
        }
        
        // Helper lambda for drawing blocks (inspired by test.cpp)
        auto drawBlock = [&](float ox, float oy, float sx, float sy) {
            glUniform2f(sLoc, sx, sy);
            glUniform2f(oLoc, ox, oy + dinoY);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        };
        
        // Dinosaur parts using test.cpp sequence and proportions (taller with better tail)
        drawBlock(-0.5f, -0.43f, 0.08f, 0.08f);     // Main body (taller)
        drawBlock(-0.54f, -0.46f, 0.04f, 0.05f);     // Body extension (taller)
        
        // Forward-pointing triangular head (point at front, base at body)
        drawBlock(-0.42f, -0.35f, 0.006f, 0.04f);    // Head tip (narrow point at front)
        drawBlock(-0.435f, -0.35f, 0.015f, 0.045f);  // Head middle section
        drawBlock(-0.455f, -0.35f, 0.025f, 0.05f);   // Head base (wide, connects to body)
        drawBlock(-0.435f, -0.345f, 0.006f, 0.006f); // Eye (positioned on head)
        
        // Enhanced leg animation like test.cpp (frame-based, not smooth)
        if (onGround) {
            float now = glfwGetTime();
            bool legForward = ((int)(now * 8.0f) % 2) == 0; // 8 FPS alternation like test.cpp
            
            if (legForward) {
                drawBlock(-0.49f, -0.50f, 0.015f, 0.04f);   // Front leg forward
                drawBlock(-0.51f, -0.505f, 0.015f, 0.035f); // Back leg back
            } else {
                drawBlock(-0.51f, -0.50f, 0.015f, 0.04f);   // Back leg forward  
                drawBlock(-0.49f, -0.505f, 0.015f, 0.035f); // Front leg back
            }
        } else {
            // Jumping pose - both legs together
            drawBlock(-0.49f, -0.50f, 0.015f, 0.04f);
            drawBlock(-0.51f, -0.50f, 0.015f, 0.04f);
        }
        
        // Simple tail
        drawBlock(-0.575f, -0.46f, 0.025f, 0.018f);  // Simple tail block
        drawBlock(-0.47f, -0.42f, 0.008f, 0.025f);   // Arms (positioned on side of body)
        
        // Draw grounded cactus obstacles with night mode colors
        for (auto &o:obstacles) { 
            // Obstacle color changes with night mode
            if (nightMode) {
                glUniform3f(cLoc, 0.9f, 0.9f, 0.9f); // Light gray obstacles for night mode
            } else {
                glUniform3f(cLoc, 0.0f, 0.0f, 0.0f); // Black obstacles for day mode
            }
            
            // Use same drawBlock helper for consistency
            auto drawObstacle = [&](float ox, float oy, float sx, float sy) {
                glUniform2f(sLoc, sx, sy);
                glUniform2f(oLoc, ox, oy);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            };
            
            // Main cactus body (grounded, matching test.cpp proportions)
            drawObstacle(o.x, -0.48f, o.w*0.6f, 0.10f);
            
            // Small cactus arm (simple, grounded)
            drawObstacle(o.x + o.w*0.22f, -0.44f, o.w*0.3f, 0.04f);
        }
        
        // Draw score on screen using stb_easy_font with night mode colors
        char buf[64]; 
        if (!gameOver) {
            snprintf(buf, 64, "Score: %d", score);
        } else {
            snprintf(buf, 64, "Game Over! Score: %d -Press R to restart", score);
        }
        
        // Text color changes with night mode
        unsigned char color[4];
        if (nightMode) {
            color[0] = 255; color[1] = 255; color[2] = 255; color[3] = 255; // White text for night mode
        } else {
            color[0] = 0; color[1] = 0; color[2] = 0; color[3] = 255; // Black text for day mode
        }
        
        // Generate text vertices with larger scale
        char textVerts[10000];
        float textScale = 3.0f; // Make text 3x larger
        float textX = 50.0f; // Move to top-left for better visibility
        float textY = 50.0f;
        
        // Create subtle bold effect by drawing text with minimal offsets
        float boldOffsets[][2] = {
            {0, 0}, {1, 0}, {0, 1}, {1, 1} // Only 4 copies for readable bold effect
        };
        
        glUseProgram(textProg);
        glUniform2f(glGetUniformLocation(textProg, "uScreenSize"), (float)SCR_WIDTH, (float)SCR_HEIGHT);
        
        // Set text color based on night mode
        if (nightMode) {
            glUniform3f(glGetUniformLocation(textProg, "uTextColor"), 1.0f, 1.0f, 1.0f); // White text for night mode
        } else {
            glUniform3f(glGetUniformLocation(textProg, "uTextColor"), 0.0f, 0.0f, 0.0f); // Black text for day mode
        }
        
        glBindVertexArray(textVAO);
        
        for (int bold = 0; bold < 4; bold++) {
            float offsetX = textX + boldOffsets[bold][0] * textScale;
            float offsetY = textY + boldOffsets[bold][1] * textScale;
            
            int numQuads = stb_easy_font_print(offsetX, offsetY, buf, color, textVerts, sizeof(textVerts));
            
            if (numQuads > 0) {
                // Scale the vertices for larger text
                float* verts = (float*)textVerts;
                for (int i = 0; i < numQuads * 4; i++) {
                    verts[i * 4 + 0] = (verts[i * 4 + 0] - offsetX) * textScale + offsetX; // Scale X
                    verts[i * 4 + 1] = (verts[i * 4 + 1] - offsetY) * textScale + offsetY; // Scale Y
                }
                
                glBindBuffer(GL_ARRAY_BUFFER, textVBO);
                glBufferData(GL_ARRAY_BUFFER, numQuads * 4 * 4 * sizeof(float), textVerts, GL_DYNAMIC_DRAW);
                
                // Draw text quads as triangles
                for (int i = 0; i < numQuads; i++) {
                    glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
                }
            }
        }
        glfwSwapBuffers(window); glfwPollEvents();
    }
    // Cleanup
    glDeleteProgram(prog); glDeleteProgram(textProg); 
    glDeleteVertexArrays(1,&VAO); glDeleteVertexArrays(1,&textVAO);
    glDeleteBuffers(1,&VBO); glDeleteBuffers(1,&EBO); glDeleteBuffers(1,&textVBO);
    glfwTerminate(); return 0;
}

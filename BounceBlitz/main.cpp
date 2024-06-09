#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "shader.h"
#include "cube.h"
#include "arcball.h"
#include <iostream>
#include <vector>
#include <map>
#include <cstdlib> // For rand function
#include <ctime>   // For time function

using namespace std;

// Settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

bool isJumping = true;  // Start in a jumping state for continuous bounce
float jumpVelocity = 5.0f;  // Initial velocity for bouncing
float gravity = -12.8f;
float ballY = 0.5f; // Adjusted to start above the platform
float deltaTime = 0.0f;
float lastFrame = 0.0f;

Arcball arcball(SCR_WIDTH, SCR_HEIGHT, 0.1f, true, true);

// Define the platform dimensions and position
glm::vec3 platformPosition(0.0f, -1.0f, 0.0f);
glm::vec3 platformSize(4.0f, 0.2f, 4.0f); // Width, Height, Depth

// Define the ball size (assuming it's a sphere with a uniform radius)
float ballRadius = 0.5f;

// Global variable for ball position
glm::vec3 ballPosition(0.0f, ballY, 0.0f);

// Camera parameters
float cameraDistance = 5.0f;
float cameraHeight = 4.0f;
float cameraAngle = 0.0f; // Angle around the ball
float mouseSensitivity = 0.1f; // Sensitivity of the mouse movement
float factor = 1.0f;

int points = 0; // Point counter

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow* window);

bool checkCollision(glm::vec3 ballPosition, float ballRadius, glm::vec3 platformPosition, glm::vec3 platformSize);

double lastX = SCR_WIDTH / 2.0f; // Last X position of the mouse
bool firstMouse = true; // Whether the mouse is first moved

int bounceCount = 0; // Counter to track number of bounces

// Structure to hold character information
struct Character {
    GLuint TextureID;   // ID handle of the glyph texture
    glm::ivec2 Size;    // Size of glyph
    glm::ivec2 Bearing; // Offset from baseline to left/top of glyph
    GLuint Advance;     // Offset to advance to next glyph
};

std::map<GLchar, Character> Characters;
GLuint VAO, VBO;

// Function to initialize FreeType and load font
void initFreeType(Shader &textShader) {
    // Initialize FreeType
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return;
    }

    // Load font
    FT_Face face;
    if (FT_New_Face(ft, "path/to/your/font.ttf", 0, &face)) {
        std::cerr << "ERROR::FREETYPE: Failed to load font" << std::endl;
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, 48);

    // Load first 128 characters of the ASCII set
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Disable byte-alignment restriction
    for (GLubyte c = 0; c < 128; c++) {
        // Load character glyph
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cerr << "ERROR::FREETYPE: Failed to load Glyph" << std::endl;
            continue;
        }
        // Generate texture
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );
        // Set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // Store character for later use
        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            face->glyph->advance.x
        };
        Characters.insert(std::pair<GLchar, Character>(c, character));
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    // Destroy FreeType once we're done
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Configure VAO/VBO for texture quads
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// Function to render text
void renderText(Shader &shader, std::string text, GLfloat x, GLfloat y, GLfloat scale, glm::vec3 color) {
    // Activate corresponding render state
    shader.use();
    glUniform3f(glGetUniformLocation(shader.ID, "textColor"), color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);

    // Iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++) {
        Character ch = Characters[*c];

        GLfloat xpos = x + ch.Bearing.x * scale;
        GLfloat ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        GLfloat w = ch.Size.x * scale;
        GLfloat h = ch.Size.y * scale;
        // Update VBO for each character
        GLfloat vertices[6][4] = {
            { xpos,     ypos + h,   0.0, 0.0 },
            { xpos,     ypos,       0.0, 1.0 },
            { xpos + w, ypos,       1.0, 1.0 },

            { xpos,     ypos + h,   0.0, 0.0 },
            { xpos + w, ypos,       1.0, 1.0 },
            { xpos + w, ypos + h,   1.0, 0.0 }
        };
        // Render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        // Update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // Render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

int main()
{
    srand(time(0));  // Seed the random number generator

    // Initialize and configure GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "BounceBlitz", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // Build and compile shader program
    Shader ourShader("3.3.shader.vs", "3.3.shader.fs");
    Shader textShader("text.vs", "text.fs");

    // Initialize FreeType
    initFreeType(textShader);

    // Define sphere vertices
    const int LATITUDE_COUNT = 18;
    const int LONGITUDE_COUNT = 36;
    std::vector<GLfloat> sphereVertices;
    std::vector<GLuint> sphereIndices;

    for (int i = 0; i <= LATITUDE_COUNT; ++i) {
        float theta = i * glm::pi<float>() / LATITUDE_COUNT;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);

        for (int j = 0; j <= LONGITUDE_COUNT; ++j) {
            float phi = j * 2 * glm::pi<float>() / LONGITUDE_COUNT;
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);

            // Adjust the size to be half
            float x = 0.5f * cosPhi * sinTheta; // Adjusted size to half
            float y = 0.5f * cosTheta;          // Adjusted size to half
            float z = 0.5f * sinPhi * sinTheta; // Adjusted size to half
            sphereVertices.push_back(x);
            sphereVertices.push_back(y);
            sphereVertices.push_back(z);
        }
    }

    for (int i = 0; i < LATITUDE_COUNT; ++i) {
        for (int j = 0; j < LONGITUDE_COUNT; ++j) {
            int first = (i * (LONGITUDE_COUNT + 1)) + j;
            int second = first + LONGITUDE_COUNT + 1;

            sphereIndices.push_back(first);
            sphereIndices.push_back(second);
            sphereIndices.push_back(first + 1);

            sphereIndices.push_back(second);
            sphereIndices.push_back(second + 1);
            sphereIndices.push_back(first + 1);
        }
    }

    // Create VAO and VBO for sphere
    GLuint sphereVAO, sphereVBO, sphereEBO;
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &sphereVBO);
    glGenBuffers(1, &sphereEBO);

    glBindVertexArray(sphereVAO);

    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(GLfloat), sphereVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereIndices.size() * sizeof(GLuint), sphereIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Create platform
    Cube platform(platformPosition.x, platformPosition.y, platformPosition.z, platformSize.x, platformSize.y, platformSize.z);

    // Render loop
    while (!glfwWindowShouldClose(window))
    {
        // Time logic
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Input
        processInput(window);

        // Apply gravity and bounce logic
        ballPosition.y += jumpVelocity * deltaTime;
        jumpVelocity += gravity * deltaTime;

        // Check for collision with platform and bounce
        if (ballPosition.y + ballRadius <= platformPosition.y - platformSize.y) {
            if (checkCollision(ballPosition, ballRadius, platformPosition, platformSize)) {
                ballPosition.y = platformPosition.y - platformSize.y - ballRadius; // Adjust ball position to be on top of the platform
                jumpVelocity = -jumpVelocity; // Reverse velocity to simulate bounce
                bounceCount++; // Increment bounce count

                // Check if platform needs to be relocated
                if (bounceCount >= 2) {
                    factor += 0.1;
                    // Relocate the platform to a new position
                    int xDirection = (rand() % 2) * -5 * factor;
                    int zDirection = (rand() % 2) * -5 * factor;

                    // Ensure xDirection and zDirection are not both zero
                    while (xDirection == 0 && zDirection == 0) {
                        xDirection = (rand() % 2) * -5 * factor;
                        zDirection = (rand() % 2) * -5 * factor;
                    }

                    platformPosition += glm::vec3(xDirection, 0, zDirection);
                    bounceCount = 0; // Reset bounce count
                    points++; // Increment points
                }
            }
        }

        // Check if ball falls below -30 on y-axis
        if (ballPosition.y < -30.0f) {
            std::cout << "You died" << std::endl;
            break;
        }

        // Render
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Activate shader
        ourShader.use();

        // Projection matrix
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        ourShader.setMat4("projection", projection);

        // View matrix with arcball, updated to follow the ball
        float camX = ballPosition.x + cameraDistance * cos(glm::radians(cameraAngle));
        float camZ = ballPosition.z + cameraDistance * sin(glm::radians(cameraAngle));
        glm::vec3 cameraPos = glm::vec3(camX, ballPosition.y + cameraHeight, camZ);
        glm::mat4 view = glm::lookAt(cameraPos, ballPosition, glm::vec3(0.0f, 1.0f, 0.0f));
        ourShader.setMat4("view", view);

        // Render platform
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, platformPosition);
        ourShader.setMat4("model", model);
        ourShader.setVec3("objectColor", glm::vec3(0.0f, 0.0f, 1.0f)); // Set platform color to blue
        platform.draw(&ourShader);

        // Render ball
        model = glm::mat4(1.0f);
        model = glm::translate(model, ballPosition);
        ourShader.setMat4("model", model);
        ourShader.setVec3("objectColor", glm::vec3(1.0f, 0.0f, 0.0f)); // Set ball color to red
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);

        // Render point counter
        renderText(textShader, "Points: " + std::to_string(points), 25.0f, 25.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f)); // Adjust position and color as needed

        // Swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Terminate GLFW
    glfwTerminate();
    return 0;
}

// Process input
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float ballSpeed = 2.5f * deltaTime; // Speed of ball movement

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        ballPosition.x -= ballSpeed; // Move forward
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        ballPosition.x += ballSpeed; // Move backward
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        ballPosition.z += ballSpeed; // Move left
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        ballPosition.z -= ballSpeed; // Move right
}

// Collision detection function
bool checkCollision(glm::vec3 ballPosition, float ballRadius, glm::vec3 platformPosition, glm::vec3 platformSize) {
    float ballBottom = ballPosition.y - ballRadius;
    float platformTop = platformPosition.y + platformSize.y / 2.0f;

    bool collisionX = ballPosition.x + ballRadius / 2.0f >= platformPosition.x - platformSize.x / 2.0f &&
        ballPosition.x - ballRadius / 2.0f <= platformPosition.x + platformSize.x / 2.0f;
    bool collisionZ = ballPosition.z + ballRadius / 2.0f >= platformPosition.z - platformSize.z / 2.0f &&
        ballPosition.z - ballRadius / 2.0f <= platformPosition.z + platformSize.z / 2.0f;

    return collisionX && collisionZ;
}

// GLFW: when the window size changed (by OS or user resize) this callback function executes
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// Mouse button callback
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        arcball.mouseButtonCallback(window, button, action, mods);
    }
}

// Cursor position callback
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse)
    {
        lastX = xpos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    lastX = xpos;

    xoffset *= mouseSensitivity;

    cameraAngle -= xoffset;
}

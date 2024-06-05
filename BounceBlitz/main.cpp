#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "shader.h"
#include "cube.h"
#include "arcball.h"
#include <iostream>
#include <vector>

using namespace std;

// Settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

bool isJumping = false;
float jumpVelocity = 0.0f;
float gravity = -9.8f;
float ballY = 0.5f; // Adjusted to start above the platform
float deltaTime = 0.0f;
float lastFrame = 0.0f;

Arcball arcball(SCR_WIDTH, SCR_HEIGHT, 0.1f, true, true);

// Define the platform dimensions and position
glm::vec3 platformPosition(0.0f, -1.0f, 0.0f);
glm::vec3 platformSize(2.0f, 0.2f, 2.0f); // Width, Height, Depth

// Define the ball size (assuming it's a sphere with a uniform radius)
float ballRadius = 0.5f;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow* window);

bool checkCollision(glm::vec3 ballPosition, float ballRadius, glm::vec3 platformPosition, glm::vec3 platformSize);

int main()
{
    // Initialize and configure GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D Bounce Ball Game", NULL, NULL);
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
    Cube platform(0.0f, -1.0f, 0.0f, 2.0f, 0.2f, 2.0f);

    // Render loop
    while (!glfwWindowShouldClose(window))
    {
        // Time logic
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Input
        processInput(window);

        // Apply gravity
        glm::vec3 ballPosition(0.0f, ballY, 0.0f);
        if (!checkCollision(ballPosition, ballRadius, platformPosition, platformSize) || isJumping) {
            if (checkCollision(ballPosition, ballRadius, platformPosition, platformSize)) {
                isJumping = false;
            }
            ballY += jumpVelocity * deltaTime;
            jumpVelocity += gravity * deltaTime;
        }
        else {
            //ballY = platformPosition.y + platformSize.y + ballRadius;
            isJumping = false;
            jumpVelocity = 0.0f;
        }

        // Render
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Activate shader
        ourShader.use();

        // Projection matrix
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        ourShader.setMat4("projection", projection);

        // View matrix with arcball
        glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 5.0f); // Distance of 5 units from the origin
        glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f)) * arcball.createRotationMatrix();
        ourShader.setMat4("view", view);

        // Render platform
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, platformPosition);
        ourShader.setMat4("model", model);
        ourShader.setVec3("objectColor", glm::vec3(0.0f, 0.0f, 1.0f)); // Set platform color to blue
        platform.draw(&ourShader);

        // Render ball
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, ballY, 0.0f));
        ourShader.setMat4("model", model);
        ourShader.setVec3("objectColor", glm::vec3(1.0f, 0.0f, 0.0f)); // Set ball color to red
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);

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

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !isJumping) {
        isJumping = true;
        jumpVelocity = 5.0f;
    }
}

// Collision detection function
bool checkCollision(glm::vec3 ballPosition, float ballRadius, glm::vec3 platformPosition, glm::vec3 platformSize) {
    float ballBottom = ballPosition.y;
    float platformTop = platformPosition.y - 2.0f * platformSize.y;
    bool temp = ballBottom <= platformTop && ballPosition.x >= platformPosition.x - platformSize.x / 2.0f &&
        ballPosition.x <= platformPosition.x + platformSize.x / 2.0f &&
        ballPosition.z >= platformPosition.z - platformSize.z / 2.0f &&
        ballPosition.z <= platformPosition.z + platformSize.z / 2.0;
    //cout << temp;
    return temp;
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
    arcball.cursorCallback(window, xpos, ypos);
}

#include "ViewerApplication.hpp"

#include <stb_image_write.h>
#include <tiny_gltf.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <iostream>
#include <numeric>
#include <random>

#include "utils/cameras.hpp"
#include "utils/images.hpp"

template <typename T>
T random_gen(T range_from, T range_to) {
    std::random_device rand_dev;
    std::mt19937 generator(rand_dev());
    std::uniform_real_distribution<T> distr(range_from, range_to);
    return distr(generator);
}

void keyCallback(GLFWwindow *window, int key, int scancode, int action,
                 int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE) {
        glfwSetWindowShouldClose(window, 1);
    }
}

int ViewerApplication::run() {
    // Loader shaders
    const auto glslProgram =
        compileProgram({m_ShadersRootPath / m_vertexShader,
                        m_ShadersRootPath / m_fragmentShader});

    const auto modelViewProjMatrixLocation =
        glGetUniformLocation(glslProgram.glId(), "uModelViewProjMatrix");
    const auto modelViewMatrixLocation =
        glGetUniformLocation(glslProgram.glId(), "uModelViewMatrix");
    const auto normalMatrixLocation =
        glGetUniformLocation(glslProgram.glId(), "uNormalMatrix");

    const auto uLightDirectionLocation =
        glGetUniformLocation(glslProgram.glId(), "uLightDirection");
    const auto uLightIntensityLocation =
        glGetUniformLocation(glslProgram.glId(), "uLightIntensity");
    glm::vec3 lightDirection(1);
    glm::vec3 lightIntensity(1);
    bool lightFromCamera = false;

    // Build projection matrix
    auto maxDistance = 500.f;  // TODO use scene bounds instead to compute this

    tinygltf::Model model;
    if (!loadGltfFile(model)) {
        return -1;
    }

    glm::vec3 bboxMin, bboxMax;
    computeSceneBounds(model, bboxMin, bboxMax);
    const auto diag = bboxMax - bboxMin;
    maxDistance = glm::length(diag);  // TODO use scene bounds instead to compute this

    maxDistance = maxDistance > 0.f ? maxDistance : 100.f;
    const auto projMatrix =
        glm::perspective(70.f, float(m_nWindowWidth) / m_nWindowHeight,
                         0.001f * maxDistance, 1.5f * maxDistance);

    // TODO Implement a new CameraController model and use it instead. Propose the choice from the GUI
    // FirstPersonCameraController cameraController{m_GLFWHandle.window(), 0.1f * maxDistance};

    // TrackballCameraController cameraController{m_GLFWHandle.window(), 0.1f * maxDistance};
    std::unique_ptr<CameraController> cameraController = std::make_unique<TrackballCameraController>(m_GLFWHandle.window(), 0.1f * maxDistance);
    ;
    std::cout << std::endl
              << std::endl
              << "!!!! I'm using LEFT_ALT instead of LEFT_CONTROL for trackball due to laptop constraint" << std::endl;

    // Replace the default camera with a camera such that center is the center of the bounding box, eye is computed as center + diagonal vector, and up is (0, 1, 0). (ideally the up vector should be specified with the file, on the command line for example, because some 3d modelers use the convention up = (0, 0, 1)).

    if (m_hasUserCamera) {
        cameraController->setCamera(m_userCamera);
    } else {
        // TODO Use scene bounds to compute a better default camera
        // cameraController.setCamera(Camera{glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)});

        //  Camera(glm::vec3 e, glm::vec3 c, glm::vec3 u) : m_eye(e), m_center(c), m_up(u)

        const auto up = glm::vec3(0, 1, 0);
        const auto center = ((bboxMax + bboxMin) * 0.5f);
        const auto eye = diag.z > 0 ? center + diag : center + 2.f * glm::cross(diag, up);
        cameraController->setCamera(Camera{eye, center, up});
    }

    // ======= Textures =============
    const auto textureObjects = createTextureObjects(model);

    // White textures
    GLuint whiteTexture = 0;
    float white[] = {1, 1, 1, 1};
    glGenTextures(1, &whiteTexture);
    glBindTexture(GL_TEXTURE_2D, whiteTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_FLOAT, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    // ======= END Textures =============

    // TODO Creation of Buffer Objects
    const auto bufferObjects = ViewerApplication::createBufferObjects(model);

    // TODO Creation of Vertex Array Objects
    std::vector<VaoRange> meshToVertexArrays;
    const auto VertexArrayObjects = ViewerApplication::createVertexArrayObjects(model, bufferObjects, meshToVertexArrays);

    // Setup OpenGL state for rendering
    glEnable(GL_DEPTH_TEST);
    glslProgram.use();

    // Lambda function to draw the scene
    const auto drawScene = [&](const Camera &camera) {
        glViewport(0, 0, m_nWindowWidth, m_nWindowHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const auto viewMatrix = camera.getViewMatrix();

        if (uLightDirectionLocation >= 0) {
            // Pass value if location not null
            const auto lightDirectionInViewSpace =
                glm::normalize(glm::vec3(viewMatrix * glm::vec4(lightDirection, 0.)));                                                       // Transform to viexMatrix then normalize it
            if (lightFromCamera) {
                glUniform3f(uLightDirectionLocation, 0, 0, 1);
            } else {
                glUniform3f(uLightDirectionLocation, lightDirectionInViewSpace[0], lightDirectionInViewSpace[1], lightDirectionInViewSpace[2]);  // Pass each value to fragment shader like that
            }
        }

        if (uLightIntensityLocation >= 0) {
            glUniform3f(uLightIntensityLocation, lightIntensity[0], lightIntensity[1], lightIntensity[2]);
        }

        // The recursive function that should draw a node
        // We use a std::function because a simple lambda cannot be recursive
        const std::function<void(int, const glm::mat4 &)> drawNode =
            [&](int nodeIdx, const glm::mat4 &parentMatrix) {
                // TODO The drawNode function
                auto node = model.nodes[nodeIdx];
                glm::mat4 modelMatrix = getLocalToWorldMatrix(node, parentMatrix);
                if (node.mesh >= 0) {
                    // Compute modelViewMatrix, modelViewProjectionMatrix, normalMatrix and send all of these to the shaders with glUniformMatrix4fv.
                    const auto modelViewMatrix = viewMatrix * modelMatrix;
                    const auto modelViewProjectionMatrix = projMatrix * modelViewMatrix;
                    const auto normalMatrix = transpose(inverse(modelViewMatrix));

                    glUniformMatrix4fv(modelViewProjMatrixLocation, 1, GL_FALSE, glm::value_ptr(modelViewProjectionMatrix));
                    glUniformMatrix4fv(modelViewMatrixLocation, 1, GL_FALSE, glm::value_ptr(modelViewMatrix));
                    glUniformMatrix4fv(normalMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalMatrix));

                    // Get the mesh and the vertex array objects range of the current mesh.

                    const auto &current_mesh = model.meshes[node.mesh];
                    const auto &vaoRange = meshToVertexArrays[node.mesh];

                    for (size_t i = 0; i < current_mesh.primitives.size(); i++) {
                        const auto &primitive = current_mesh.primitives[i];
                        const auto vao = VertexArrayObjects[vaoRange.begin + i];

                        glBindVertexArray(vao);

                        if (primitive.indices >= 0) {
                            const auto &accessor = model.accessors[primitive.indices];

                            const auto &bufferView = model.bufferViews[accessor.bufferView];
                            const auto byteOffset =
                                accessor.byteOffset + bufferView.byteOffset;

                            // glDrawElements(mode, count, type, indices)
                            glDrawElements(primitive.mode, GLsizei(accessor.count), accessor.componentType, (const GLvoid *)byteOffset);

                        } else {
                            const auto accessorIdx = (*begin(primitive.attributes)).second;
                            const auto &accessor = model.accessors[accessorIdx];

                            glDrawArrays(primitive.mode, 0, GLsizei(accessor.count));
                        }
                    }
                }
                // Draw the children recursively
                for (const auto child : node.children) {
                    drawNode(child, modelMatrix);
                }
            };

        // Draw the scene referenced by gltf file
        if (model.defaultScene >= 0) {
            // TODO Draw all nodes
            for (const auto nodeIdx : model.scenes[model.defaultScene].nodes) {
                drawNode(nodeIdx, glm::mat4(1));
            }
        }
    };

    // --output functionnality

    // Please change argument in .vscode/bash-init.sh, view_sponza
    std::cout << m_OutputPath.string() << std::endl;
    if (!m_OutputPath.empty()) {
        // TODO
        std::vector<unsigned char> pixels(m_nWindowHeight * m_nWindowWidth * 3);

        renderToImage(m_nWindowWidth, m_nWindowHeight, 3, pixels.data(), [&]() {
            drawScene(cameraController->getCamera());
        });

        //: flip the image vertically, because OpenGL does not use the same convention for that than png files, and write the png file with stb_image_write library which is included in the third-parties.
        flipImageYAxis(m_nWindowWidth, m_nWindowHeight, 3, pixels.data());

        // output the png
        const auto strPath = m_OutputPath.string();
        stbi_write_png(
            strPath.c_str(), m_nWindowWidth, m_nWindowHeight, 3, pixels.data(), 0);

        return 0;
    }

    // RENDER LOOP
    // Loop until the user closes the window
    for (auto iterationCount = 0u; !m_GLFWHandle.shouldClose();
         ++iterationCount) {
        const auto seconds = glfwGetTime();

        const auto camera = cameraController->getCamera();
        drawScene(camera);

        // GUI code:
        imguiNewFrame();

        {
            ImGui::Begin("GUI");
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                        1000.0f / ImGui::GetIO().Framerate,
                        ImGui::GetIO().Framerate);
            static int e = 1;
            if (
                ImGui::RadioButton("First Person", &e, 0) ||
                ImGui::RadioButton("Trackball", &e, 1)) {
                const auto currentCamera = cameraController->getCamera();
                if (e == 1) {
                    cameraController = std::make_unique<TrackballCameraController>(m_GLFWHandle.window(), 0.1f * maxDistance);
                } else {
                    cameraController = std::make_unique<FirstPersonCameraController>(m_GLFWHandle.window(), 0.1f * maxDistance);
                }
                cameraController->setCamera(currentCamera);
            }

            if (ImGui::CollapsingHeader("Light",
                                        ImGuiTreeNodeFlags_DefaultOpen)) {
                static float phi = 0;
                static float theta = 0;
                static float intensity[3] = {1.f, 1.f, 1.f};

                static bool autoIncrement = true;
                static bool pressed = true;
                // static else it reset every loop

                if (autoIncrement) {
                    // increment phi and theta value each render loop
                    theta = (theta + 0.001);
                    if (theta >= 6.28) {
                        theta = 0;
                    }
                    phi = (phi + 0.001);
                    if (phi >= 3.14) {
                        phi = 0;
                    }
                }

                lightDirection = glm::vec3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));

                if (ImGui::SliderAngle("Phi", &phi, 0, 180) ||
                    ImGui::SliderAngle("Theta", &theta, 0, 360)) {
                    lightDirection = glm::vec3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));
                }
                if (ImGui::ColorEdit3("lightIntensity", intensity)) {
                    lightIntensity.x = intensity[0];
                    lightIntensity.y = intensity[1];
                    lightIntensity.z = intensity[2];
                }
                if (ImGui::Checkbox("Lighting from camera", &lightFromCamera)) {
                    glUniform3f(uLightDirectionLocation, 0, 0, 1);
                }
                if (ImGui::Checkbox("Auto-increment phi and theta", &pressed)) {
                    if (autoIncrement) {
                        autoIncrement = false;
                    } else {
                        autoIncrement = true;
                    }
                }
            }

            if (ImGui::CollapsingHeader("Camera",
                                        ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("eye: %.3f %.3f %.3f", camera.eye().x,
                            camera.eye().y, camera.eye().z);
                ImGui::Text("center: %.3f %.3f %.3f", camera.center().x,
                            camera.center().y, camera.center().z);
                ImGui::Text("up: %.3f %.3f %.3f", camera.up().x, camera.up().y,
                            camera.up().z);

                ImGui::Text("front: %.3f %.3f %.3f", camera.front().x,
                            camera.front().y, camera.front().z);
                ImGui::Text("left: %.3f %.3f %.3f", camera.left().x,
                            camera.left().y, camera.left().z);

                if (ImGui::Button("CLI camera args to clipboard")) {
                    std::stringstream ss;
                    ss << "--lookat " << camera.eye().x << "," << camera.eye().y
                       << "," << camera.eye().z << "," << camera.center().x
                       << "," << camera.center().y << "," << camera.center().z
                       << "," << camera.up().x << "," << camera.up().y << ","
                       << camera.up().z;
                    const auto str = ss.str();
                    glfwSetClipboardString(m_GLFWHandle.window(), str.c_str());
                }
            }
            ImGui::End();
        }

        imguiRenderFrame();

        glfwPollEvents();  // Poll for and process events

        auto ellapsedTime = glfwGetTime() - seconds;
        auto guiHasFocus = ImGui::GetIO().WantCaptureMouse ||
                           ImGui::GetIO().WantCaptureKeyboard;
        if (!guiHasFocus) {
            cameraController->update(float(ellapsedTime));
        }

        m_GLFWHandle.swapBuffers();  // Swap front and back buffers
    }

    // TODO clean up allocated GL data

    return 0;
}

std::vector<GLuint> ViewerApplication::createTextureObjects(const tinygltf::Model &model) const {
    // Here we assume a texture object has been created and bound to GL_TEXTURE_2D

    std::vector<GLuint> textureObjects(model.textures.size(), 0);

    // Default Sampler
    tinygltf::Sampler defaultSampler;
    defaultSampler.minFilter = GL_LINEAR;
    defaultSampler.magFilter = GL_LINEAR;
    defaultSampler.wrapS = GL_REPEAT;
    defaultSampler.wrapT = GL_REPEAT;
    defaultSampler.wrapR = GL_REPEAT;

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(GLsizei(model.textures.size()), textureObjects.data());

    for (int i = 0; i < model.textures.size(); ++i) {
        const auto &texture = model.textures[i];           // get i-th texture
        assert(texture.source >= 0);                       // ensure a source image is present
        const auto &image = model.images[texture.source];  // get the image

        const auto &sampler =
            texture.sampler >= 0 ? model.samplers[texture.sampler] : defaultSampler;
        glBindTexture(GL_TEXTURE_2D, textureObjects[i]);
        // fill the texture object with the data from the image
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, image.pixel_type, image.image.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        sampler.minFilter != -1 ? sampler.minFilter : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                        sampler.magFilter != -1 ? sampler.magFilter : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler.wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler.wrapT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, sampler.wrapR);

        // Some samplers use mipmapping for their minification filter. In that case, the specification tells us we need to have mipmaps computed for the texture. OpenGL can compute them for us:
        if (sampler.minFilter == GL_NEAREST_MIPMAP_NEAREST ||
            sampler.minFilter == GL_NEAREST_MIPMAP_LINEAR ||
            sampler.minFilter == GL_LINEAR_MIPMAP_NEAREST ||
            sampler.minFilter == GL_LINEAR_MIPMAP_LINEAR) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    return textureObjects;
}

ViewerApplication::ViewerApplication(const fs::path &appPath, uint32_t width,
                                     uint32_t height, const fs::path &gltfFile,
                                     const std::vector<float> &lookatArgs,
                                     const std::string &vertexShader,
                                     const std::string &fragmentShader,
                                     const fs::path &output)
    : m_nWindowWidth(width),
      m_nWindowHeight(height),
      m_AppPath{appPath},
      m_AppName{m_AppPath.stem().string()},
      m_ImGuiIniFilename{m_AppName + ".imgui.ini"},
      m_ShadersRootPath{m_AppPath.parent_path() / "shaders"},
      m_gltfFilePath{gltfFile},
      m_OutputPath{output} {
    if (!lookatArgs.empty()) {
        m_hasUserCamera = true;
        m_userCamera =
            Camera{glm::vec3(lookatArgs[0], lookatArgs[1], lookatArgs[2]),
                   glm::vec3(lookatArgs[3], lookatArgs[4], lookatArgs[5]),
                   glm::vec3(lookatArgs[6], lookatArgs[7], lookatArgs[8])};
    }

    if (!vertexShader.empty()) {
        m_vertexShader = vertexShader;
    }
    if (!fragmentShader.empty()) {
        m_fragmentShader = fragmentShader;
    }

    ImGui::GetIO().IniFilename =
        m_ImGuiIniFilename.c_str();  // At exit, ImGUI will store its windows
                                     // positions in this file

    glfwSetKeyCallback(m_GLFWHandle.window(), keyCallback);

    printGLVersion();
}

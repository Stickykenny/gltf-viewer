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

    const auto uBaseColorTexture =
        glGetUniformLocation(glslProgram.glId(), "uBaseColorTexture");
    const auto uBaseColorFactorLocation =
        glGetUniformLocation(glslProgram.glId(), "uBaseColorFactor");

    const auto uMetallicFactorLocation =
        glGetUniformLocation(glslProgram.glId(), "uMetallicFactor");
    const auto uRoughnessFactorLocation =
        glGetUniformLocation(glslProgram.glId(), "uRougnessFactor");
    const auto uMetallicRoughnessTextureLocation =
        glGetUniformLocation(glslProgram.glId(), "uMetallicRoughnessTexture");

    const auto uEmissiveFactorLocation =
        glGetUniformLocation(glslProgram.glId(), "uEmissiveFactor");
    const auto uEmissiveTextureLocation =
        glGetUniformLocation(glslProgram.glId(), "uEmissiveTexture");

    // Occlusion
    const auto uOcclusionTextureLocation  =
      glGetUniformLocation(glslProgram.glId(), "uOcclusionTexture");
  const auto uOcclusionStrengthLocation =
      glGetUniformLocation(glslProgram.glId(), "uOcclusionStrength");
  const auto uOcclusionOnOffLocation =
      glGetUniformLocation(glslProgram.glId(), "uOcclusionOnOff");


    const auto uLightDirectionLocation =
        glGetUniformLocation(glslProgram.glId(), "uLightDirection");
    const auto uLightIntensityLocation =
        glGetUniformLocation(glslProgram.glId(), "uLightIntensity");

    // Normal Mapping
    const auto uNormalTextureOnOffLocation =
        glGetUniformLocation(glslProgram.glId(), "uNormalTextureOnOff");
    const auto uNormalTBNOnOffLocation =
        glGetUniformLocation(glslProgram.glId(), "uNormalTBNOnOff");
    const auto uNormalTextureScaleLocation =
        glGetUniformLocation(glslProgram.glId(), "uNormalTextureScale");
    const auto uNormalTextureLocation =
        glGetUniformLocation(glslProgram.glId(), "uNormalTexture");

    const auto uViewNormalOnOffLocation =
        glGetUniformLocation(glslProgram.glId(), "uViewNormalOnOff");

    bool useOcclusion = true;
    bool useNormalMap = true;
    bool useTBN = true;
    bool viewNormal = false;
    glm::vec3 lightDirection(1);
    glm::vec3 lightIntensity({1.,1.,1.});
    static float lightIntensityFactor = 5.;
    bool lightFromCamera = false;

    // Build projection matrix
    auto maxDistance = 500.f;  // Default value, replaced with scene bounds

    tinygltf::Model model;
    if (!loadGltfFile(model)) {
        return -1;
    }

    glm::vec3 bboxMin, bboxMax;
    computeSceneBounds(model, bboxMin, bboxMax);
    const auto diag = bboxMax - bboxMin;
    maxDistance = glm::length(diag);

    maxDistance = maxDistance > 0.f ? maxDistance : 100.f;
    const auto projMatrix =
        glm::perspective(70.f, float(m_nWindowWidth) / m_nWindowHeight,
                         0.001f * maxDistance, 1.5f * maxDistance);

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

    const auto bufferObjects = ViewerApplication::createBufferObjects(model);
    std::vector<VaoRange> meshToVertexArrays;
    const auto VertexArrayObjects = ViewerApplication::createVertexArrayObjects(model, bufferObjects, meshToVertexArrays);

    // Setup OpenGL state for rendering
    glEnable(GL_DEPTH_TEST);
    glslProgram.use();

    // Lambda function to bind texture
    const auto bindMaterial = [&](const auto materialIndex) {
        if (materialIndex >= 0) {
            const auto &material = model.materials[materialIndex];
            const auto &pbrMetallicRoughness = material.pbrMetallicRoughness;
            if (uBaseColorFactorLocation >= 0) {
                glUniform4f(uBaseColorFactorLocation,
                            (float)pbrMetallicRoughness.baseColorFactor[0],
                            (float)pbrMetallicRoughness.baseColorFactor[1],
                            (float)pbrMetallicRoughness.baseColorFactor[2],
                            (float)pbrMetallicRoughness.baseColorFactor[3]);
            }
            if (uBaseColorTexture >= 0) {
                auto textureObject = whiteTexture;
                if (pbrMetallicRoughness.baseColorTexture.index >= 0) {
                    const auto &texture =
                        model.textures[pbrMetallicRoughness.baseColorTexture.index];
                    if (texture.source >= 0) {
                        textureObject = textureObjects[texture.source];
                    }
                }

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, textureObject);
                glUniform1i(uBaseColorTexture, 0);
            }
            if (uMetallicFactorLocation >= 0) {
                glUniform1f(
                    uMetallicFactorLocation, (float)pbrMetallicRoughness.metallicFactor);
            }
            if (uRoughnessFactorLocation >= 0) {
                glUniform1f(
                    uRoughnessFactorLocation, (float)pbrMetallicRoughness.roughnessFactor);
            }
            if (uMetallicRoughnessTextureLocation >= 0) {
                auto textureObject = whiteTexture;
                if (pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
                    const auto &texture =
                        model.textures[pbrMetallicRoughness.metallicRoughnessTexture.index];
                    if (texture.source >= 0) {
                        textureObject = textureObjects[texture.source];
                    }
                }

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, textureObject);
                glUniform1i(uMetallicRoughnessTextureLocation, 1);
            }

            if (uEmissiveFactorLocation >= 0) {
                glUniform3f(
                    uEmissiveFactorLocation, (float)material.emissiveFactor[0],
                    (float)material.emissiveFactor[1],
                    (float)material.emissiveFactor[2]);
            }
            if (uEmissiveTextureLocation >= 0) {
                auto textureObject = whiteTexture;
                if (material.emissiveTexture.index >= 0) {
                    const auto &texture =
                        model.textures[material.emissiveTexture.index];
                    if (texture.source >= 0) {
                        textureObject = textureObjects[texture.source];
                    }
                }

                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, textureObject);
                glUniform1i(uEmissiveTextureLocation, 2);
            }

            // Occlusion
            const auto &occlusionTexture = material.occlusionTexture;
            if (uOcclusionStrengthLocation >= 0) {
                glUniform1f(uOcclusionStrengthLocation, (float)material.occlusionTexture.strength);
            }
            if (uOcclusionTextureLocation >= 0) {
                auto textureObject = whiteTexture;
                if (occlusionTexture.index >= 0) {
                    const auto &texture = model.textures[occlusionTexture.index];
                    if (texture.source >= 0) {
                        textureObject = textureObjects[texture.source];
                    }
                }
                glActiveTexture(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_2D, textureObject);
                glUniform1i(uOcclusionTextureLocation, 3);
            }
            // Normal Mapping
            const auto &normalTexture = material.normalTexture;

            if (uNormalTextureLocation >= 0) {
                auto textureObject = whiteTexture;
                if (normalTexture.index >= 0) {
                    const auto &texture =
                        model.textures[normalTexture.index];
                    if (texture.source >= 0) {
                        textureObject = textureObjects[texture.source];
                    }
                }

                if (uNormalTextureScaleLocation >= 0) {
                    glUniform1f(uNormalTextureScaleLocation, (float)normalTexture.scale);
                }
                glActiveTexture(GL_TEXTURE4);
                glBindTexture(GL_TEXTURE_2D, textureObject);
                glUniform1i(uNormalTextureLocation, 4);
            }
        } else {
            // No texture found
            if (uBaseColorFactorLocation >= 0) {
                glUniform4f(uBaseColorFactorLocation, 1, 1, 1, 1);
            }
            if (uBaseColorTexture >= 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, whiteTexture);
                glUniform1i(uBaseColorTexture, 0);
            }
            if (uMetallicFactorLocation >= 0) {
                glUniform1f(uMetallicFactorLocation, 1.f);
            }
            if (uRoughnessFactorLocation >= 0) {
                glUniform1f(uRoughnessFactorLocation, 1.f);
            }
            if (uMetallicRoughnessTextureLocation >= 0) {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, 0);
                glUniform1i(uMetallicRoughnessTextureLocation, 1);
            }
            if (uEmissiveFactorLocation >= 0) {
                glUniform3f(uEmissiveFactorLocation, 0., 0., 0.);
            }
            if (uEmissiveTextureLocation >= 0) {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, 0);
                glUniform1i(uEmissiveTextureLocation, 2);
            }
            if (uOcclusionStrengthLocation >= 0) {
                glUniform1f(uOcclusionStrengthLocation, 0.);
            }
            if (uOcclusionTextureLocation >= 0) {
                glActiveTexture(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_2D, 0);
                glUniform1i(uOcclusionTextureLocation, 3);
            }
            if (uNormalTextureScaleLocation >= 0) {
                glUniform1f(uNormalTextureScaleLocation, 1);
            }
            if (uNormalTextureLocation >= 0) {
                glActiveTexture(GL_TEXTURE4);
                glBindTexture(GL_TEXTURE_2D, 0.5);
                glUniform1i(uNormalTextureLocation, 4);
            }
        }
    };

    // Lambda function to draw the scene
    const auto drawScene = [&](const Camera &camera) {
        glViewport(0, 0, m_nWindowWidth, m_nWindowHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const auto viewMatrix = camera.getViewMatrix();

        if (uLightDirectionLocation >= 0) {
            // Pass value if location not null
            const auto lightDirectionInViewSpace =
                glm::normalize(glm::vec3(viewMatrix * glm::vec4(lightDirection, 0.)));  // Transform to viexMatrix then normalize it
            if (lightFromCamera) {
                glUniform3f(uLightDirectionLocation, 0, 0, 1);
            } else {
                glUniform3f(uLightDirectionLocation, lightDirectionInViewSpace[0], lightDirectionInViewSpace[1], lightDirectionInViewSpace[2]);  // Pass each value to fragment shader like that
            }
        }

        if (uLightIntensityLocation >= 0) {
            glUniform3f(uLightIntensityLocation, lightIntensity[0] * lightIntensityFactor, lightIntensity[1] * lightIntensityFactor, lightIntensity[2] * lightIntensityFactor);
        }

        if (uOcclusionOnOffLocation >= 0) {
            glUniform1i(uOcclusionOnOffLocation, useOcclusion);
        }

        if (uNormalTextureOnOffLocation >= 0) {
            glUniform1i(uNormalTextureOnOffLocation, useNormalMap);
        }
        if (uNormalTBNOnOffLocation >= 0) {
            glUniform1i(uNormalTBNOnOffLocation, useTBN);
        }
        if (uViewNormalOnOffLocation >= 0) {
            glUniform1i(uViewNormalOnOffLocation, viewNormal);
        }
        // The recursive function that should draw a node
        // We use a std::function because a simple lambda cannot be recursive
        const std::function<void(int, const glm::mat4 &)> drawNode =
            [&](int nodeIdx, const glm::mat4 &parentMatrix) {
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

                        bindMaterial(primitive.material);
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

                            bindMaterial(primitive.material);
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
            for (const auto nodeIdx : model.scenes[model.defaultScene].nodes) {
                drawNode(nodeIdx, glm::mat4(1));
            }
        }
    };

    std::cout << m_OutputPath.string() << std::endl;
    if (!m_OutputPath.empty()) {
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
            ImGui::Columns(2, "Camera");
            if (
                ImGui::RadioButton("First Person", &e, 0) ||
                nextColumnWrapper() ||
                ImGui::RadioButton("Trackball", &e, 1)) {
                const auto currentCamera = cameraController->getCamera();
                if (e == 1) {
                    cameraController = std::make_unique<TrackballCameraController>(m_GLFWHandle.window(), 0.1f * maxDistance);
                } else {
                    cameraController = std::make_unique<FirstPersonCameraController>(m_GLFWHandle.window(), 0.1f * maxDistance);
                }
                cameraController->setCamera(currentCamera);
            }
            ImGui::Columns();

            if (ImGui::CollapsingHeader("Light",
                                        ImGuiTreeNodeFlags_DefaultOpen)) {
                static float phi = 0;
                static float theta = 0;
                static float angle_delta[2] = {random_gen(0.f, .002f), random_gen(0.f, .002f)};

                const int nb_channels = 3;
                static glm::vec3 lightColor(lightIntensity[0], lightIntensity[1], lightIntensity[2]);
                static float color_delta_min = 0.00025f, color_delta_max = .001f;
                static float color_delta[nb_channels] = {random_gen(color_delta_min, color_delta_max), random_gen(color_delta_min, color_delta_max), random_gen(color_delta_min, color_delta_max)};

                static bool autoIncrement = false;
                static bool pressed = autoIncrement;
                static bool autoIncrement_colors = false;
                static bool pressed_colors = autoIncrement_colors;

                // Normal buttons

                static bool pressed_normal = useNormalMap;
                static bool pressed_TBN = useTBN;

                // static variable else it reset every loop

                if (autoIncrement) {
                    // increment phi and theta value each render loop
                    theta = (theta + angle_delta[0]);
                    if (theta >= 6.28 || theta <= 0.f) {
                        angle_delta[0] = theta < 0 ? random_gen(color_delta_min, color_delta_max) : -random_gen(color_delta_min, color_delta_max);
                    }
                    phi = (phi + angle_delta[1]);
                    if (phi >= 3.14 || phi <= 0.f) {
                        angle_delta[1] = phi < 0 ? random_gen(color_delta_min, color_delta_max) : -random_gen(color_delta_min, color_delta_max);
                    }
                }

                if (autoIncrement_colors) {
                    // Make colors value change
                    for (int i = 0; i < nb_channels; ++i) {
                        lightColor[i] += color_delta[i];
                        if (lightColor[i] >= 1.f || lightColor[i] <= 0.f) {
                            color_delta[i] = lightColor[i] <= 0 ? random_gen(color_delta_min, color_delta_max) : -random_gen(color_delta_min, color_delta_max);
                        }
                    }
                    lightIntensity = lightColor;
                }

                lightDirection = glm::vec3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));

                if (ImGui::SliderAngle("Phi", &phi, 0, 180) ||
                    ImGui::SliderAngle("Theta", &theta, 0, 360)) {
                    lightDirection = glm::vec3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));
                }
                if (ImGui::ColorEdit3("LightColor", (float *)&lightColor) || 
                    ImGui::InputFloat("intensity", &lightIntensityFactor)) {
                    lightIntensity = lightColor;
                }

                ImGui::Columns(2, "");

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
                if (ImGui::Checkbox("Auto-evolve colors", &pressed_colors)) {
                    if (autoIncrement_colors) {
                        autoIncrement_colors = false;
                    } else {
                        autoIncrement_colors = true;
                    }
                }
                ImGui::NextColumn();
                ImGui::Checkbox("Apply Occlusion", &useOcclusion);
                if (ImGui::Checkbox("Apply Normal Map", &pressed_normal)) {
                    if (useNormalMap) {
                        useNormalMap = false;
                        useTBN = false;
                        pressed_TBN = false;
                    } else {
                        useNormalMap = true;
                    }
                };
                if (ImGui::Checkbox("Apply Normal w/ TBN", &pressed_TBN)) {
                    if (useNormalMap) {
                        if (useTBN) {
                            useTBN = false;
                        } else {
                            useTBN = true;
                        }
                    } else {
                        useNormalMap = true;
                        useTBN = true;
                        pressed_TBN = true;
                        pressed_normal = true;
                    };
                }
                ImGui::Checkbox("View Normal colorspace", &viewNormal);
                ImGui::Columns();
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
std::vector<GLuint> ViewerApplication::createVertexArrayObjects(const tinygltf::Model &model, const std::vector<GLuint> &bufferObjects, std::vector<VaoRange> &meshIndexToVaoRange) {
    std::vector<GLuint> vertexArrayObjects;
    meshIndexToVaoRange.resize(model.meshes.size());

    GLuint VERTEX_ATTRIB_POSITION_IDX = 0;
    GLuint VERTEX_ATTRIB_NORMAL_IDX = 1;
    GLuint VERTEX_ATTRIB_TEXCOORD0_IDX = 2;
    GLuint VERTEX_ATTRIB_TANGENT_IDX = 3;

    std::vector<std::string> findthings = {"POSITION", "NORMAL", "TEXCOORD_0", "TANGENT"};
    std::vector<GLuint> arguments_ltop = {VERTEX_ATTRIB_POSITION_IDX,
                                          VERTEX_ATTRIB_NORMAL_IDX,
                                          VERTEX_ATTRIB_TEXCOORD0_IDX,
                                          VERTEX_ATTRIB_TANGENT_IDX};

    for (int meshIdx = 0; meshIdx < model.meshes.size(); ++meshIdx) {
        const int vaoOffset = vertexArrayObjects.size();
        int size_before = vaoOffset;
        vertexArrayObjects.resize(vaoOffset + model.meshes[meshIdx].primitives.size());
        const auto &vaoRange = VaoRange{vaoOffset, GLsizei(model.meshes[meshIdx].primitives.size())};
        meshIndexToVaoRange.push_back(vaoRange);  // Will be used during rendering

        glGenVertexArrays(vaoRange.count, &vertexArrayObjects[vaoRange.begin]);

        for (int primitiveIdx = 0; primitiveIdx < model.meshes[meshIdx].primitives.size(); primitiveIdx++) {
            glBindVertexArray(vertexArrayObjects[vaoOffset + primitiveIdx]);
            auto primitive = model.meshes[meshIdx].primitives[primitiveIdx];

            for (int i = 0; i < arguments_ltop.size(); i++) {
                const auto iterator = primitive.attributes.find(findthings[i]);
                if (iterator != end(primitive.attributes)) {
                    const auto accessorIdx = (*iterator).second;
                    const auto &accessor = model.accessors[accessorIdx];
                    const auto &bufferView = model.bufferViews[accessorIdx];
                    const auto bufferIdx = bufferView.buffer;

                    const auto bufferObject = bufferObjects[bufferIdx];
                    glEnableVertexAttribArray(arguments_ltop[i]);
                    glBindBuffer(GL_ARRAY_BUFFER, bufferObject);

                    const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;

                    glVertexAttribPointer(arguments_ltop[i], accessor.type,
                                          accessor.componentType, GL_FALSE, GLsizei(bufferView.byteStride),
                                          (const GLvoid *)(accessor.byteOffset + bufferView.byteOffset));
                } else if (i == 3) {  // 3 being VERTEX_ATTRIB_TANGENT_IDX
                    computeTangent(model, primitive, VERTEX_ATTRIB_TANGENT_IDX);
                }
            }
            // Index array if defined
            if (primitive.indices >= 0) {
                const auto accessorIdx = primitive.indices;
                const auto &accessor = model.accessors[accessorIdx];
                const auto &bufferView = model.bufferViews[accessor.bufferView];
                const auto bufferIdx = bufferView.buffer;

                assert(GL_ELEMENT_ARRAY_BUFFER == bufferView.target);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferObjects[bufferIdx]);
            }
        }
    }
    glBindVertexArray(0);

    return vertexArrayObjects;
}

void ViewerApplication::computeTangent(const tinygltf::Model &model, const tinygltf::Primitive &primitive, GLuint attribArrayIndex) {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    GLuint tangentsBuffer;
    glGenBuffers(1, &tangentsBuffer);
    std::vector<glm::vec3> tangents;

    if (model.defaultScene >= 0) {
        // POSITION STRUCTURAL INFORMATION RETRIEVAL
        const auto positionAttrIdxIt = primitive.attributes.find("POSITION");
        if (positionAttrIdxIt == end(primitive.attributes)) {
            return;
        }
        const auto &positionAccessor = model.accessors[(*positionAttrIdxIt).second];
        if (positionAccessor.type != 3) {
            std::cerr << "Position accessor with type != VEC3, skipping" << std::endl;
            return;
        }
        const auto &positionBufferView = model.bufferViews[positionAccessor.bufferView];
        const auto byteOffset = positionAccessor.byteOffset + positionBufferView.byteOffset;
        const auto &positionBuffer = model.buffers[positionBufferView.buffer];
        const auto positionByteStride = positionBufferView.byteStride ? positionBufferView.byteStride : 3 * sizeof(float);

        // TEXTURE COORDINATES STRUCTURAL INFORMATION RETRIEVAL
        const auto textureAttrIdxIt = primitive.attributes.find("TEXCOORD_0");
        if (textureAttrIdxIt == end(primitive.attributes)) {
            return;
        }
        const auto &textureAccessor = model.accessors[(*textureAttrIdxIt).second];
        if (textureAccessor.type != 2) {
            std::cerr << " Texture with type != VEC2, skipping" << std::endl;
            return;
        }
        const auto &textureBufferView = model.bufferViews[textureAccessor.bufferView];
        const auto textureByteOffset = textureAccessor.byteOffset + textureBufferView.byteOffset;
        const auto &textureBuffer = model.buffers[textureBufferView.buffer];
        const auto textureByteStride = textureBufferView.byteStride ? textureBufferView.byteStride : 2 * sizeof(float);

        if (primitive.indices >= 0) {
            const auto &indexAccessor = model.accessors[primitive.indices];
            const auto &indexBufferView = model.bufferViews[indexAccessor.bufferView];
            const auto indexByteOffset = indexAccessor.byteOffset + indexBufferView.byteOffset;
            const auto &indexBuffer = model.buffers[indexBufferView.buffer];
            auto indexByteStride = indexBufferView.byteStride;

            switch (indexAccessor.componentType) {
                default:
                    std::cerr << "Primitive index accessor with bad componentType "
                              << indexAccessor.componentType << ", skipping it." << std::endl;
                    return;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    indexByteStride = indexByteStride ? indexByteStride : sizeof(uint8_t);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    indexByteStride = indexByteStride ? indexByteStride : sizeof(uint16_t);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    indexByteStride = indexByteStride ? indexByteStride : sizeof(uint32_t);
                    break;
            }

            for (size_t i = 0; i < indexAccessor.count; ++i) {
                uint32_t index = 0;
                switch (indexAccessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        index = *((const uint8_t *)&indexBuffer.data[indexByteOffset + indexByteStride * i]);
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        index = *((const uint16_t *)&indexBuffer.data[indexByteOffset + indexByteStride * i]);
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        index = *((const uint32_t *)&indexBuffer.data[indexByteOffset + indexByteStride * i]);
                        break;
                }
                const auto &localPosition = *((const glm::vec3 *)&positionBuffer.data[byteOffset + positionByteStride * index]);
                positions.emplace_back(localPosition);
                const auto &textureCoord = *((const glm::vec2 *)&textureBuffer.data[textureByteOffset + textureByteStride * index]);
                uvs.emplace_back(textureCoord);
            }
        } else {
            for (size_t i = 0; i < positionAccessor.count; ++i) {
                const auto &localPosition = *((const glm::vec3 *)&positionBuffer.data[byteOffset + positionByteStride * i]);
                positions.emplace_back(localPosition);
                const auto &textureCoord = *((const glm::vec2 *)&textureBuffer.data[textureByteOffset + textureByteStride * i]);
                uvs.emplace_back(textureCoord);
            }
        }

        for (int i = 0; i < positions.size(); i += 3) {
            glm::vec3 tangent;
            // positions
            glm::vec3 pos1 = positions[i];
            glm::vec3 pos2 = positions[i + 1];
            glm::vec3 pos3 = positions[i + 2];
            // texture coordinates
            glm::vec2 uv1 = uvs[i];
            glm::vec2 uv2 = uvs[i + 1];
            glm::vec2 uv3 = uvs[i + 2];

            // normal vector
            glm::vec3 nm(0.0, 0.0, 1.0);

            glm::vec3 edge1 = pos2 - pos1;
            glm::vec3 edge2 = pos3 - pos1;
            glm::vec2 deltaUV1 = uv2 - uv1;
            glm::vec2 deltaUV2 = uv3 - uv1;

            float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

            tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            /*bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
            bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
            bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);*/
            tangents.emplace_back(tangent);
        }
    }
    glEnableVertexAttribArray(attribArrayIndex);
    glBindBuffer(GL_ARRAY_BUFFER, tangentsBuffer);
    glBufferData(GL_ARRAY_BUFFER, tangents.size() * sizeof(glm::vec3), tangents.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(attribArrayIndex, 3, GL_FLOAT, GL_TRUE, sizeof(glm::vec3), NULL);
};

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

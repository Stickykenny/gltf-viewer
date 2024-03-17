#pragma once

#include <tiny_gltf.h>

#include "utils/GLFWHandle.hpp"
#include "utils/cameras.hpp"
#include "utils/filesystem.hpp"
#include "utils/gltf.hpp"
#include "utils/shaders.hpp"
class ViewerApplication {
   private:
    // A range of indices in a vector containing Vertex Array Objects
    struct VaoRange {
        GLsizei begin;  // Index of first element in vertexArrayObjects
        GLsizei count;  // Number of elements in range
    };

    GLsizei m_nWindowWidth = 1280;
    GLsizei m_nWindowHeight = 720;

    const fs::path m_AppPath;
    const std::string m_AppName;
    const fs::path m_ShadersRootPath;

    fs::path m_gltfFilePath;
    // std::string m_vertexShader = "forward.vs.glsl";
    std::string m_vertexShader = "forward_normal.vs.glsl";

    // std::string m_fragmentShader = "normals.fs.glsl";
    // std::string m_fragmentShader = "diffuse_directional_light.fs.glsl";
    // std::string m_fragmentShader = "pbr_directional_light.fs.glsl";
    std::string m_fragmentShader = "pbr_normal.fs.glsl";

    bool m_hasUserCamera = false;
    Camera m_userCamera;

    fs::path m_OutputPath;

    // Order is important here, see comment below
    const std::string m_ImGuiIniFilename;
    // Last to be initialized, first to be destroyed:
    GLFWHandle m_GLFWHandle{int(m_nWindowWidth), int(m_nWindowHeight),
                            "glTF Viewer", m_OutputPath.empty()};

   public:
    ViewerApplication(const fs::path &appPath, uint32_t width, uint32_t height,
                      const fs::path &gltfFile,
                      const std::vector<float> &lookatArgs,
                      const std::string &vertexShader,
                      const std::string &fragmentShader,
                      const fs::path &output);

    bool loadGltfFile(tinygltf::Model &model) {
        // using namespace tinygltf;
        // tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;
        bool ret = loader.LoadASCIIFromFile(&model, &err, &warn,
                                            m_gltfFilePath.string());

        if (!warn.empty()) {
            printf("Warn: %s\n", warn.c_str());
        }

        if (!err.empty()) {
            printf("Err: %s\n", err.c_str());
        }

        if (!ret) {
            printf("Failed to parse glTF\n");
            return false;
        }
        return ret;
    };

    std::vector<GLuint> createBufferObjects(const tinygltf::Model &model) {
        // converting glTF buffers to OpenGL buffer objects

        std::vector<GLuint> bufferObjects(model.buffers.size());
        glGenBuffers(GLsizei(model.buffers.size()), bufferObjects.data());
        for (size_t i = 0; i < model.buffers.size(); ++i) {
            glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[i]);
            glBufferStorage(GL_ARRAY_BUFFER, model.buffers[i].data.size(),
                            model.buffers[i].data.data(), 0);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return bufferObjects;
    };
    std::vector<GLuint> createVertexArrayObjects(const tinygltf::Model &model, const std::vector<GLuint> &bufferObjects, std::vector<VaoRange> &meshIndexToVaoRange);

    void computeTangent(const tinygltf::Model &model, const tinygltf::Primitive &primitive, GLuint attribArrayIndex);

    std::vector<GLuint> createTextureObjects(const tinygltf::Model &model) const;

    bool nextColumnWrapper() {
        // Wrapper method used for separating 'First Person' and 'TrackBall' button
        ImGui::NextColumn();
        return false;
    };
    int run();
};
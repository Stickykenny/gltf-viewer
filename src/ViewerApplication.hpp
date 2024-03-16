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

    // show the window only if m_OutputPath is empty
    /*
      ! THE ORDER OF DECLARATION OF MEMBER VARIABLES IS IMPORTANT !
      - m_ImGuiIniFilename.c_str() will be used by ImGUI in ImGui::Shutdown,
      which                             will be called in destructor of
      m_GLFWHandle. So we must declare m_ImGuiIniFilename before m_GLFWHandle so
      that m_ImGuiIniFilename                             destructor is called
      after.
      - m_GLFWHandle must be declared before the creation of any object managing
      OpenGL resources (e.g. GLProgram, GLShader) because it is responsible for
      the creation of a GLFW windows and thus a GL context which must exists
      before most of OpenGL function calls.
    */
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

        std::vector<GLuint> v1(model.buffers.size());
        glGenBuffers(GLsizei(model.buffers.size()), v1.data());

        glGenBuffers(GLsizei(model.buffers.size()), v1.data());
        for (size_t i = 0; i < model.buffers.size(); ++i) {
            glBindBuffer(GL_ARRAY_BUFFER, v1[i]);
            glBufferStorage(GL_ARRAY_BUFFER, model.buffers[i].data.size(),
                            model.buffers[i].data.data(), 0);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return v1;
        // Create a vector of GLuint with the correct size (model.buffers.size()) and use glGenBuffers to create buffer objects.
        // - In a loop, fill each buffer object with the data using glBindBuffer
        // and glBufferStorage. The data should be obtained from
        // model.buffers[bufferIdx].data. - Don't forget to unbind the buffer
        // object from GL_ARRAY_BUFFER after the loop
        // that compute the vector of buffer objects from a model and returns
        // it. Call this functions in run() after loading the glTF.
    };
    std::vector<GLuint> createVertexArrayObjects(const tinygltf::Model &model, const std::vector<GLuint> &bufferObjects, std::vector<VaoRange> &meshIndexToVaoRange);

    void computeTangent(const tinygltf::Model &model, const tinygltf::Primitive &primitive, GLuint attribArrayIndex);
    std::vector<GLuint> createTextureObjects(const tinygltf::Model &model) const;

    int run();
};
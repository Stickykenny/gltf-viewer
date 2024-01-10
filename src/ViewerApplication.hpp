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
    std::string m_vertexShader = "forward.vs.glsl";
    std::string m_fragmentShader = "normals.fs.glsl";

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
        int bufferIdx = 0;
        std::vector<GLuint> v1(model.buffers.size());
        glGenBuffers(v1.size(), v1.data());

        for (auto it = v1.begin(); it != v1.end(); ++it) {
            glBindBuffer(GL_ARRAY_BUFFER, *(it));
            glBufferStorage(GL_ARRAY_BUFFER, sizeof(GLuint),
                            model.buffers[bufferIdx].data.data(), 0);

            bufferIdx++;
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
    std::vector<GLuint> createVertexArrayObjects(const tinygltf::Model &model, const std::vector<GLuint> &bufferObjects, std::vector<VaoRange> &meshIndexToVaoRange) {
        // take the model and the buffer objects we previously created, create an array of vertex array objects and return it.
        std::vector<GLuint> vertexArrayObjects;  // contain our vertex array objects
        //
        GLuint VERTEX_ATTRIB_POSITION_IDX = 0;
        GLuint VERTEX_ATTRIB_NORMAL_IDX = 1;
        GLuint VERTEX_ATTRIB_TEXCOORD0_IDX = 2;

        std::vector<std::string> findthings = {"POSITION",
                                               "NORMAL",
                                               "TEXCOORD_0"};
        std::vector<GLuint> arguments_ltop = {VERTEX_ATTRIB_POSITION_IDX,
                                              VERTEX_ATTRIB_NORMAL_IDX,
                                              VERTEX_ATTRIB_TEXCOORD0_IDX};

        for (int meshIdx = 0; meshIdx < model.meshes.size(); ++meshIdx) {
            const int vaoOffset = vertexArrayObjects.size();
            int size_before = vaoOffset;
            vertexArrayObjects.resize(vaoOffset + model.meshes[meshIdx].primitives.size());
            auto vaoRange = VaoRange{vaoOffset, model.meshes[meshIdx].primitives.size()};
            meshIndexToVaoRange.push_back(vaoRange);  // Will be used during rendering

            glGenVertexArrays(vaoRange.count, &vertexArrayObjects[vaoRange.begin]);

            for (int primitiveIdx = 0; primitiveIdx < model.meshes[meshIdx].primitives.size(); primitiveIdx++) {
                glBindVertexArray(vertexArrayObjects[vaoOffset + primitiveIdx]);
                auto primitive = model.meshes[meshIdx].primitives[primitiveIdx];

                for (int i = 0; i < 3; i++) {  // I'm opening a scope because I want to reuse the variable iterator in the code for NORMAL and TEXCOORD_0
                    const auto iterator = primitive.attributes.find(findthings[i]);
                    if (iterator != end(primitive.attributes)) {  // If "POSITION" has been found in the map
                        // (*iterator).first is the key "POSITION", (*iterator).second is the value, ie. the index of the accessor for this attribute
                        const auto accessorIdx = (*iterator).second;
                        const auto &accessor = model.accessors[accessorIdx];      // TODO get the correct tinygltf::Accessor from model.accessors
                        const auto &bufferView = model.bufferViews[accessorIdx];  // TODO get the correct tinygltf::BufferView from model.bufferViews. You need to use the accessor
                        const auto bufferIdx = bufferView.buffer;                 // TODO get the index of the buffer used by the bufferView (you need to use it)

                        const auto bufferObject = bufferObjects[bufferIdx];  // TODO get the correct buffer object from the buffer index

                        // TODO Enable the vertex attrib array corresponding to POSITION with glEnableVertexAttribArray (you need to use VERTEX_ATTRIB_POSITION_IDX which has to be defined at the top of the cpp file)
                        glEnableVertexAttribArray(arguments_ltop[i]);
                        // TODO Bind the buffer object to GL_ARRAY_BUFFER
                        glBindBuffer(GL_ARRAY_BUFFER, bufferObject);

                        const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
                        // TODO Compute the total byte offset using the accessor and the buffer view
                        // TODO Call glVertexAttribPointer with the correct arguments.
                        glVertexAttribPointer(arguments_ltop[i], accessor.type, accessor.componentType, GL_FALSE,
                                              bufferView.byteStride, (const GLvoid *)byteOffset);
                        // Remember size is obtained with accessor.type, type is obtained with accessor.componentType.
                        // The stride is obtained in the bufferView, normalized is always GL_FALSE, and pointer is the byteOffset (don't forget the cast).

                        if (primitive.indices >= 0) {
                            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferObject);
                        }
                    }
                }
            }
        }

        glBindVertexArray(0);
        return vertexArrayObjects;
    }

    int run();
};
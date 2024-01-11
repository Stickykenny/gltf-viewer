#include "cameras.hpp"
#include "glfw.hpp"

#include <iostream>

// Good reference here to map camera movements to lookAt calls
// http://learnwebgl.brown37.net/07_cameras/camera_movement.html

using namespace glm;

struct ViewFrame
{
  vec3 left;
  vec3 up;
  vec3 front;
  vec3 eye;

  ViewFrame(vec3 l, vec3 u, vec3 f, vec3 e) : left(l), up(u), front(f), eye(e)
  {
  }
};

ViewFrame fromViewToWorldMatrix(const mat4 &viewToWorldMatrix)
{
  return ViewFrame{-vec3(viewToWorldMatrix[0]), vec3(viewToWorldMatrix[1]),
      -vec3(viewToWorldMatrix[2]), vec3(viewToWorldMatrix[3])};
}

bool FirstPersonCameraController::update(float elapsedTime)
{
  if (glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_MIDDLE) &&
      !m_MiddleButtonPressed) {
    m_MiddleButtonPressed = true;
    glfwGetCursorPos(
        m_pWindow, &m_LastCursorPosition.x, &m_LastCursorPosition.y);
  } else if (!glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_MIDDLE) &&
             m_MiddleButtonPressed) {
    m_MiddleButtonPressed = false;
  }

  const auto cursorDelta = ([&]() {
    if (m_MiddleButtonPressed) {
      dvec2 cursorPosition;
      glfwGetCursorPos(m_pWindow, &cursorPosition.x, &cursorPosition.y);
      const auto delta = cursorPosition - m_LastCursorPosition;
      m_LastCursorPosition = cursorPosition;
      return delta;
    }
    return dvec2(0);
  })();

  float truckLeft = 0.f;
  float pedestalUp = 0.f;
  float dollyIn = 0.f;
  float rollRightAngle = 0.f;

  if (glfwGetKey(m_pWindow, GLFW_KEY_W)) {
    dollyIn += m_fSpeed * elapsedTime;
  }

  // Truck left
  if (glfwGetKey(m_pWindow, GLFW_KEY_A)) {
    truckLeft += m_fSpeed * elapsedTime;
  }

  // Pedestal up
  if (glfwGetKey(m_pWindow, GLFW_KEY_UP)) {
    pedestalUp += m_fSpeed * elapsedTime;
  }

  // Dolly out
  if (glfwGetKey(m_pWindow, GLFW_KEY_S)) {
    dollyIn -= m_fSpeed * elapsedTime;
  }

  // Truck right
  if (glfwGetKey(m_pWindow, GLFW_KEY_D)) {
    truckLeft -= m_fSpeed * elapsedTime;
  }

  // Pedestal down
  if (glfwGetKey(m_pWindow, GLFW_KEY_DOWN)) {
    pedestalUp -= m_fSpeed * elapsedTime;
  }

  if (glfwGetKey(m_pWindow, GLFW_KEY_Q)) {
    rollRightAngle -= 0.001f;
  }
  if (glfwGetKey(m_pWindow, GLFW_KEY_E)) {
    rollRightAngle += 0.001f;
  }

  // cursor going right, so minus because we want pan left angle:
  const float panLeftAngle = -0.01f * float(cursorDelta.x);
  const float tiltDownAngle = 0.01f * float(cursorDelta.y);

  const auto hasMoved = truckLeft || pedestalUp || dollyIn || panLeftAngle ||
                        tiltDownAngle || rollRightAngle;
  if (!hasMoved) {
    return false;
  }

  m_camera.moveLocal(truckLeft, pedestalUp, dollyIn);
  m_camera.rotateLocal(rollRightAngle, tiltDownAngle, 0.f);
  m_camera.rotateWorld(panLeftAngle, m_worldUpAxis);

  return true;
}

bool TrackballCameraController::update(float elapsedTime) {
    if (glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_MIDDLE) &&
        !m_MiddleButtonPressed) {
        m_MiddleButtonPressed = true;
        glfwGetCursorPos(
            m_pWindow, &m_LastCursorPosition.x, &m_LastCursorPosition.y);
    } else if (!glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_MIDDLE) &&
               m_MiddleButtonPressed) {
        m_MiddleButtonPressed = false;
    }

    const auto cursorDelta = ([&]() {
        if (m_MiddleButtonPressed) {
            dvec2 cursorPosition;
            glfwGetCursorPos(m_pWindow, &cursorPosition.x, &cursorPosition.y);
            const auto delta = cursorPosition - m_LastCursorPosition;
            m_LastCursorPosition = cursorPosition;
            return delta;
        }
        return dvec2(0);
    })();

    float truckLeft = 0.f;
    float pedestalUp = 0.f;
    float dollyIn = 0.f;
    float rollRightAngle = 0.f;

    // Rotate around center with middle mouse pressed
    // Move on plane orthogonal to the view axis with shift+middle mouse pressed
    // Dolly in/out with ctrl+middle mouse pressed
    const auto hasMoved = 0.f;
    if (m_MiddleButtonPressed) {
        std::cout << "MIDDLE pressed";

        if (glfwGetKey(m_pWindow, GLFW_KEY_LEFT_SHIFT)) {
            std::cout << "+SHIFT" << std::endl;
            // Truck
            // Suit l'axe horizontal de la camera

            const auto truckLeft = 0.01f * float(cursorDelta.x);
            const auto pedestalUp = 0.01f * float(cursorDelta.y);

            const auto hasMoved = truckLeft || pedestalUp;

            m_camera.moveLocal(truckLeft, pedestalUp, 0);
            if (hasMoved) {
                return true;
            }
        }

        else if (glfwGetKey(m_pWindow, GLFW_KEY_LEFT_ALT)) {
            std::cout << "+ALT" << std::endl;
            // Dolly - Zoom
            // Suit l'axe z de la camera // profondeur-depth

            // We cannot use moveLocal because it moves both the eye and the center.
            // always keep eye != center (but you can ignore that initially and fix it later).

            const float zoomOffset = -0.01f * float(cursorDelta.y);

            auto viewVector = m_camera.center() - m_camera.eye();

            // Cool looking fun
            // viewVector.x = viewVector.x > 1 ? viewVector.x : 1;
            const auto translation_vector = viewVector * zoomOffset;
            const auto newEye = m_camera.eye() + translation_vector;

            m_camera = Camera(newEye, m_camera.center(), m_worldUpAxis);
            if (hasMoved) {
                return true;
            }
        } else {
            // Pan
            // Rotation around camera # look around

            // Not the implementation wanted, because it moves the center of the camera
            const float longAngle = 0.01f * float(cursorDelta.y);
            const float latitudeAngle = -0.01f * float(cursorDelta.x);

            const auto hasMoved = longAngle || latitudeAngle;

            /*m_camera.rotateLocal(0, yOffset, 0.f);
            m_camera.rotateWorld(xOffset, m_worldUpAxis);*/

            const auto depthAxis = m_camera.eye() - m_camera.center();
            const auto horizontalAxis = m_camera.left();

            const auto longitudeRotationMatrix = rotate(mat4(1), longAngle, horizontalAxis);
            /*const auto latitudeRotationMatrix = rotate(mat4(1), latitudeAngle, m_worldUpAxis);

            const auto rotationMatrix = glm::rotate(longitudeRotationMatrix, latitudeAngle, horizontalAxis);

            const auto rotatedDepthAxis = longitudeRotationMatrix * vec4(depthAxis, 0);
            const auto finalDepthAxis = vec3(latitudeRotationMatrix * rotatedDepthAxis);*/

            // optimized version by pre-multiplying the two matrices before applying it to the depthAxis
            const auto rotationMatrix = glm::rotate(longitudeRotationMatrix, latitudeAngle, m_worldUpAxis);

            const auto rotatedDepthAxis = longitudeRotationMatrix * vec4(depthAxis, 0);
            const auto finalDepthAxis = vec3(rotationMatrix * rotatedDepthAxis);

            const auto newEye = m_camera.center() + finalDepthAxis;

            m_camera = Camera(newEye, m_camera.center(), m_worldUpAxis);

            std::cout << std::endl;
            if (hasMoved) {
                return true;
            }
        }
    }
    return false;
}
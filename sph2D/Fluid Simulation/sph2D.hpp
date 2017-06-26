#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <chrono>
#include <string>

namespace sph {
	// constants
	const float particleLength = 0.005f;
	const float timeInterval = 0.0001f;
	const unsigned particleCount = 20000;
	const unsigned groupSize = 128;
	const unsigned groupCount = std::ceil((float)particleCount / groupSize);

	void initializeWindow();
	void initializeOpenGL();
	void destroyWindow();
	void destroyOpenGL();
	GLuint compileShader(std::string filePath, GLenum shaderType);
	void checkProgramLinked(GLuint shaderProgramHandle);
	void mainLoop();
	void invokeComputeShader();
	void render();
	void run();

	GLFWwindow *window = nullptr;
	uint64_t frameNumber = 0;
	float frameTime = 0;
	float cpuTime = 0;
	float gpuTime = 0;

	std::chrono::steady_clock::time_point frameStart;
	std::chrono::steady_clock::time_point cpuEnd;
	std::chrono::steady_clock::time_point frameEnd;

	unsigned particlePositionVaoHandle = 0;
	unsigned renderProgramHandle = 0;
	unsigned computeProgramHandle[3] = { 0, 0, 0 };
	unsigned particleBufferHandle = 0;

	struct Particle {
		glm::vec2 position;
		glm::vec2 velocity;
		glm::vec2 force;
		float density;
		float pressure;
	};
}

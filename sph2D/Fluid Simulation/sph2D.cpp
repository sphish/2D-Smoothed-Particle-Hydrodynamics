#include "sph2D.hpp"

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>

namespace sph {
	void destroyWindow() {
		if (window != nullptr) 
			glfwDestroyWindow(window);
		glfwTerminate();
	}
	
	void destroyOpenGL() {
		glDeleteProgram(renderProgramHandle);
		for (int i = 0; i < 3; ++i)
			glDeleteProgram(computeProgramHandle[i]);

		glDeleteVertexArrays(1, &particlePositionVaoHandle);
		glDeleteBuffers(1, &particleBufferHandle);
	}

	void initializeWindow()
	{
		if (!glfwInit())
			throw std::runtime_error("glfw initialization failed");

		window = glfwCreateWindow(1000, 1000, "", nullptr, nullptr);
		if (!window)
		{
			glfwTerminate();
			throw std::runtime_error("window creation failed");
		}
		glfwSetWindowPos(window, 5, 30);
		glfwMakeContextCurrent(window);

		//vsync off
		glfwSwapInterval(0); 
	}

	void checkProgramLinked(GLuint shaderProgramHandle)
	{
		int linked = 0;
		glGetProgramiv(shaderProgramHandle, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE)
		{
			int len = 0;
			glGetProgramiv(shaderProgramHandle, GL_INFO_LOG_LENGTH, &len);
			std::vector<GLchar> log(len);
			glGetProgramInfoLog(shaderProgramHandle, len, &len, &log[0]);

			for (auto c : log) putchar(c);

			throw std::runtime_error("shader link error.");
		}
	}

	GLuint compileShader(std::string filePath, GLenum shaderType)
	{
		GLuint shaderHandle = 0;
		std::ifstream shaderFile(filePath, std::ios::ate | std::ios::binary);
		if (!shaderFile)
			throw std::runtime_error("shader file load error.");

		unsigned shaderFileSize = static_cast<unsigned>(shaderFile.tellg());
		std::vector<char> shaderCode(shaderFileSize);
		shaderFile.seekg(0);
		shaderFile.read(shaderCode.data(), shaderFileSize);
		shaderFile.close();
		// must be null terminated
		shaderCode.push_back('\0');

		char *shaderCodePointer = shaderCode.data();
		shaderHandle = glCreateShader(shaderType);
		glShaderSource(shaderHandle, 1, &shaderCodePointer, nullptr);
		glCompileShader(shaderHandle);

		int compiled = 0;
		glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &compiled);
		if (compiled == GL_FALSE)
		{
			int len = 0;
			glGetShaderiv(shaderHandle, GL_INFO_LOG_LENGTH, &len);
			std::vector<GLchar> log(len);
			glGetShaderInfoLog(shaderHandle, len, &len, &log[0]);

			for (auto c : log) 
				std::cout << c;

			throw std::runtime_error("shader compile error.");
		}

		return shaderHandle;
	}

	void initializeOpenGL()
	{
		glewInit();
		if (!glewIsSupported("GL_VERSION_4_5"))
			throw std::runtime_error("need opengl 4.5 core");

		// version info 
		std::cout << "vendor: " << glGetString(GL_VENDOR) << std::endl 
			<< "renderder: " << glGetString(GL_RENDERER) << std::endl
			<< "version: " << glGetString(GL_VERSION) << std::endl;

		glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
		unsigned vertexShaderHandle = compileShader("shader/particle.vert", GL_VERTEX_SHADER);
		unsigned fragmentShaderHandle = compileShader("shader/particle.frag", GL_FRAGMENT_SHADER);

		renderProgramHandle = glCreateProgram();
		glAttachShader(renderProgramHandle, fragmentShaderHandle);
		glAttachShader(renderProgramHandle, vertexShaderHandle);
		glLinkProgram(renderProgramHandle);
		checkProgramLinked(renderProgramHandle);

		//delete shaders as we're done with them.
		glDeleteShader(vertexShaderHandle);
		glDeleteShader(fragmentShaderHandle);

		unsigned computeShaderHandle;
		computeShaderHandle = compileShader("shader/computeDensityPressure.comp", GL_COMPUTE_SHADER);
		computeProgramHandle[0] = glCreateProgram();
		glAttachShader(computeProgramHandle[0], computeShaderHandle);
		glLinkProgram(computeProgramHandle[0]);
		checkProgramLinked(renderProgramHandle);
		
		glDeleteShader(computeShaderHandle);

		computeShaderHandle = compileShader("shader/computeForce.comp", GL_COMPUTE_SHADER);
		computeProgramHandle[1] = glCreateProgram();
		glAttachShader(computeProgramHandle[1], computeShaderHandle);
		glLinkProgram(computeProgramHandle[1]);
		checkProgramLinked(renderProgramHandle);

		glDeleteShader(computeShaderHandle);

		computeShaderHandle = compileShader("shader/updateVelocityPosition.comp", GL_COMPUTE_SHADER);
		computeProgramHandle[2] = glCreateProgram();
		glAttachShader(computeProgramHandle[2], computeShaderHandle);
		glLinkProgram(computeProgramHandle[2]);
		checkProgramLinked(renderProgramHandle);

		glDeleteShader(computeShaderHandle);

		Particle *initialParticleData = new Particle[particleCount];
		std::memset(initialParticleData, 0, sizeof(Particle) * particleCount);
		
		// initial state
		for (auto x = 0; x < 125; ++x) 
			for (auto y = 0; y < 160; ++y)
				initialParticleData[x * 160 + y].position.x = -0.625f + particleLength * 2 * x,
				initialParticleData[x * 160 + y].position.y = 1.f - particleLength * 2 * y;
		
		glGenBuffers(1, &particleBufferHandle);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleBufferHandle);
		glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(Particle) * particleCount, initialParticleData, GL_MAP_READ_BIT);

		delete[] initialParticleData;
		
		glGenVertexArrays(1, &particlePositionVaoHandle);
		glBindVertexArray(particlePositionVaoHandle);

		glBindBuffer(GL_ARRAY_BUFFER, particleBufferHandle);
		// bind buffer containing particle position to vao, stride is sizeof(Particle)
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof Particle, nullptr);
		// enable attribute with binding = 0 (vertex position in the shader) for this vao
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// release
		glBindVertexArray(0);
	}

	void invokeComputeShader()
	{
		for (auto i = 0; i < 3; ++i)
		{
			glUseProgram(computeProgramHandle[i]);
			glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, 1, &particleBufferHandle);
			glDispatchCompute(groupCount, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
	}

	void render()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(renderProgramHandle);
		glBindVertexArray(particlePositionVaoHandle);
		glDrawArrays(GL_POINTS, 0, particleCount); 
		glBindVertexArray(0);

		glfwSwapBuffers(window);
	}

	void mainLoop()
	{
		frameStart = std::chrono::steady_clock::now();

		// CPU Region
		glfwPollEvents();

		cpuEnd = std::chrono::steady_clock::now();
		
		//GPU Region
		invokeComputeShader();
		render();

		// measure performance
		frameEnd = std::chrono::steady_clock::now();
		frameTime = std::chrono::duration_cast<std::chrono::duration<float>>(frameEnd - frameStart).count();
		cpuTime = std::chrono::duration_cast<std::chrono::duration<float>>(cpuEnd - frameStart).count();
		gpuTime = std::chrono::duration_cast<std::chrono::duration<float>>(frameEnd - cpuEnd).count();
		std::stringstream ss;
		ss.precision(3);
		ss.setf(std::ios_base::fixed, std::ios_base::floatfield);
		ss << "frame #" << frameNumber
			<< "|simulationTime(sec):" << timeInterval * frameNumber
			<< "|particleCount:" << particleCount
			<< "|fps:" << 1.f / frameTime
			<< "|frame_time(ms):" << frameTime * 1000
			<< "|cpu_time(ms)" << cpuTime * 1000
			<< "|gpu_time(ms)" << gpuTime * 1000
			<< "|vsync:off";

		glfwSetWindowTitle(window, ss.str().c_str());

		frameNumber++;
	}

	void run()
	{
		initializeWindow();
		initializeOpenGL();

		while (!glfwWindowShouldClose(window)) 	
			mainLoop();

		destroyOpenGL();
		destroyWindow();
	}
}

int main()
{
	sph::run();
	return 0;
}


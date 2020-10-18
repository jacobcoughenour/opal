#include <vulkan/vulkan.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

class Main {
public:
	void run() {
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	void initVulkan() {
	}

	void mainLoop() {
	}

	void cleanup() {
	}
};

int main() {
	Main app;

	try {
		app.run();
	} catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
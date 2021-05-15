#ifndef __APP_H__
#define __APP_H__

#include "renderer/renderer.h"
#include "utils/log.h"
#include <cstdlib>

namespace Opal {

class App {

public:
	App(int argc, char **argv);
	int run();

protected:
	Renderer _renderer;
};

} // namespace Opal

#endif // __APP_H__
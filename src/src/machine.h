
#pragma once

#include "fabgl.h"

class Machine {

public:

  Machine(fabgl::VGAController * displayController);
  ~Machine();

  void reset();

  int run();

private:


};
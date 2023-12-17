
#include <map>
#include <string>
#include <vector>

#include "ay8910.hpp"
#include "msx1.hpp"
#include "fabgl.h"
#pragma once

class Machine {

  unsigned char* ram;
  TMS9918A::Context vram;

  Z80 cpu;
  MSX1MMU mmu;
  TMS9918A vdp;

public:

  Machine(fabgl::VGAController * displayController);
  ~Machine();

  void reset();
  void run();
private:

  const int CPU_CLOCK = 3579545;
  const int VDP_CLOCK = 5370863;

  void vdp_tick();
};
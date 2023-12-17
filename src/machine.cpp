#pragma GCC optimize ("O2")

#include <stdlib.h>
#include <ctype.h>
#include "machine.h"
#include "fabgl.h"
#include "emuapi.h"
#include <ff.h>

#define DEBUG true

Machine::Machine(fabgl::VGAController * displayController)
{
  
  #if DEBUG
    Serial.println("Machine::Machine()");
    Serial.println("Machine::Machine() - Initializing the MSX computer");
  #endif

  // Initialize the MSX computer
  this->mmu.setupRAM(this->ram, 0x4000); //16kb


}

Machine::~Machine()
{
    
  #if DEBUG
    Serial.println("Machine::~Machine()");
  #endif

  // Deinitialize the MSX computer
}

void Machine::reset()
{
  #if DEBUG
    Serial.println("Machine::reset()");
  #endif

  memset(&this->cpu.reg, 0, sizeof(this->cpu.reg));
  memset(&this->cpu.reg.pair, 0xFF, sizeof(this->cpu.reg.pair));
  memset(&this->cpu.reg.back, 0xFF, sizeof(this->cpu.reg.back));

  this->cpu.reg.SP = 0xF000;
  this->cpu.reg.IX = 0xFFFF;
  this->cpu.reg.IY = 0xFFFF;
  this->mmu.reset();
  this->vdp.reset();
}

void Machine::run()
{

// z80 ticker

// vdp ticker

// psg ticker

}

void Machine::vdp_tick()
{
  
}





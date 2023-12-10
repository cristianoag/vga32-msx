#pragma GCC optimize ("O2")

#include <stdlib.h>
#include <ctype.h>
#include "machine.h"
#include "fabgl.h"
#include "emuapi.h"
#include <ff.h>

#define DEBUG true

extern char *Disks[2][MAXDISKS+1];

Machine::Machine(fabgl::VGAController * displayController)
{
  /*** MSX versions: ***/
  static const char *Versions[] = { "MSX","MSX2","MSX2+" };
  /*** Joystick types: ***/
  static const char *JoyTypes[] =
  {
    "nothing","normal joystick",
    "mouse in joystick mode","mouse in real mode"
  };

   /*** CMOS ROM default values: ***/
  static const byte RTCInit[4][13]  =
  {
    {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    {  0, 0, 0, 0,40,80,15, 4, 4, 0, 0, 0, 0 },
    {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
  };
  
  /*** VDP status register states: ***/
  static const byte VDPSInit[16] = { 0x9F,0,0x6C,0,0,0,0,0,0,0,0,0,0,0,0,0 };

  /*** VDP control register states: ***/
  static const byte VDPInit[64]  =
  {
    0x00,0x10,0xFF,0xFF,0xFF,0xFF,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
  };

  /*** Initial palette: ***/
  static const byte PalInit[16][3] =
  {
    {0x00,0x00,0x00},{0x00,0x00,0x00},{0x20,0xC0,0x20},{0x60,0xE0,0x60},
    {0x20,0x20,0xE0},{0x40,0x60,0xE0},{0xA0,0x20,0x20},{0x40,0xC0,0xE0},
    {0xE0,0x20,0x20},{0xE0,0x60,0x60},{0xC0,0xC0,0x20},{0xC0,0xC0,0x80},
    {0x20,0x80,0x20},{0xC0,0x40,0xA0},{0xA0,0xA0,0xA0},{0xE0,0xE0,0xE0}
  };


}

Machine::~Machine()
{
  
  int *T,I,J,K;
  byte *P;
  WORD A;

  #if DEBUG
    Serial.println("Machine::~Machine()");
  #endif

  ROMData[0]=ROMData[1]=NULL;
  FontBuf  = NULL;
  VRAM     = NULL;
  SRAM     = NULL;
  Kanji    = NULL;
  SaveCMOS = 0;
  SaveSRAM = 0;
  ExitNow  = 0;
  CCount   = 0;

   /* Calculate IPeriod from VPeriod/HPeriod */
  if(UPeriod<1) UPeriod=1;
  if(HPeriod<CPU_HPERIOD) HPeriod=CPU_HPERIOD;
  if(VPeriod<CPU_VPERIOD) VPeriod=CPU_VPERIOD;
  CPU.TrapBadOps = Verbose&0x10;
  CPU.IPeriod    = CPU_H240;
  CPU.IAutoReset = 0;

  /* Check parameters for validity */
  if(MSXVersion>2) MSXVersion=2;
  if((RAMPages<(MSXVersion? 8:4))||(RAMPages>256))
    RAMPages=MSXVersion? 8:4;
  if((VRAMPages<(MSXVersion? 8:2))||(VRAMPages>8))
    VRAMPages=MSXVersion? 8:2;

  /* Number of RAM pages should be power of 2 */
  /* Calculate RAMMask=(2^RAMPages)-1 */
  for(J=1;J<RAMPages;J<<=1);
  RAMPages=J;
  RAMMask=J-1;

  /* Number of VRAM pages should be a power of 2 */
  for(J=1;J<VRAMPages;J<<=1);
  VRAMPages=J;

  /* Initialize ROMMasks to zeroes for now */
  ROMMask[0]=ROMMask[1]=0;

   /* Joystick types are in 0..3 range */
  JoyTypeA&=3;
  JoyTypeB&=3;

  memset(EmptyRAM,NORAM,0x4000);
  Chunks[CCount++]=EmptyRAM;

 /* Reset memory map to the empty space */
  for(I=0;I<4;I++)
    for(J=0;J<4;J++)
      for(K=0;K<8;K++)
        MemMap[I][J][K]=EmptyRAM;

  memset(VRAM,0x00,VRAMPages*0x4000);
  Chunks[CCount++]=VRAM;

  memset(RAMData,NORAM,RAMPages*0x4000);
  Chunks[CCount++]=RAMData;

  /* Open/load system ROM(s) */
  switch(MSXVersion)
  {
    case 0:
      P=LoadROM("MSX.ROM",0x8000,0);
      MemMap[0][0][0]=P;
      MemMap[0][0][1]=P+0x2000;
      MemMap[0][0][2]=P+0x4000;
      MemMap[0][0][3]=P+0x6000;
      break;

    case 1:
      P=LoadROM("roms/MSX2.ROM",0x8000,0);
      MemMap[0][0][0]=P;
      MemMap[0][0][1]=P+0x2000;
      MemMap[0][0][2]=P+0x4000;
      MemMap[0][0][3]=P+0x6000;
      P=LoadROM("roms/MSX2EXT.ROM",0x4000,0);
      MemMap[3][1][0]=P;
      MemMap[3][1][1]=P+0x2000;
      break;

    case 2:
      P=LoadROM("roms/MSX2P.ROM",0x8000,0);
      MemMap[0][0][0]=P;
      MemMap[0][0][1]=P+0x2000;
      MemMap[0][0][2]=P+0x4000;
      MemMap[0][0][3]=P+0x6000;
      P=LoadROM("MSX2PEXT.ROM",0x4000,0);
      MemMap[3][1][0]=P;
      MemMap[3][1][1]=P+0x2000;
      break;
  }

}

void Machine::reset()
{
  #if DEBUG
    Serial.println("Machine::reset()");
  #endif

  
}

void Machine::run()
{


}

byte *LoadROM(const char *Name,int Size,byte *Buf)
{
  #if DEBUG
    Serial.println("Machine::LoadROM()");
  #endif

  byte *P;

  if(Buf&&!Size) return(0);
  FILE * f = fopen(Name, "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    auto Size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (Size<=0) return (0);
      fread(Buf, 1, Size, f);
      fclose(f);
      return(Buf);

    }

    return 0;
}




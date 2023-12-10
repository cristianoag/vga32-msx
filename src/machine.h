
#include "fabgl.h"
#include "MSX.h"
#pragma once

#define MAXDISKS    32      /* Number of disks for a drive   */

class Machine {

byte *RAM[8];                      /* Main RAM (8x8kB pages) */
byte *EmptyRAM;                    /* Empty RAM page (8kB)   */
byte *SRAM;                        /* SRAM (battery backed)  */
byte *MemMap[4][4][8];   /* Memory maps [PPage][SPage][Addr] */

byte *RAMData;                     /* RAM Mapper contents    */
byte RAMMapper[4];                 /* RAM Mapper state       */
byte RAMMask;                      /* RAM Mapper mask        */

byte *ROMData[2];                  /* ROM Mapper contents    */
byte ROMMapper[2][4];              /* ROM Mappers state      */
byte ROMMask[2];                   /* ROM Mapper masks       */

/** Kanji font ROM *******************************************/
byte *Kanji;                       /* Kanji ROM 4096x32      */
int  KanLetter;                    /* Current letter index   */
byte KanCount;                     /* Byte count 0..31       */

byte SaveCMOS    = 0;              /* Save CMOS.ROM on exit  */
byte SaveSRAM    = 0;              /* ~ GMASTER2.RAM on exit */
byte *Chunks[256];                 /* Memory blocks to free  */
byte CCount;                       /* Number of memory blcks */


public:

  Machine(fabgl::VGAController * displayController);
  ~Machine();

  void reset();
  void run();
private:

  byte *LoadROM(const char *Name,int Size,byte *Buf);

};
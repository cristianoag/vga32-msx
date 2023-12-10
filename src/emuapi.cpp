#include <Arduino.h>
#include <SD.h>
#include "emuapi.h"
#include "iopins.h"
#include "fabgl.h"

static bool vgahires=false;
static File file;
static char selection[MAX_FILENAME_PATH]="";
static char files[MAX_FILES][MAX_FILENAME_SIZE];
static int nbFiles=0;
static File file_handlers[NB_FILE_HANDLER];
static bool menuOn=true;

fabgl::VGAController DisplayController;
fabgl::Canvas        canvas(&DisplayController);
fabgl::PS2Controller PS2Controller;

static void FileHandlersInit(void) {
  for (int i=0; i<NB_FILE_HANDLER;i++) {
    file_handlers[i]=file;
  }
}

static int readNbFiles(void) {
  int totalFiles = 0;

  File entry;    
  file = SD.open(selection);
  while ( (true) && (totalFiles<MAX_FILES) ) {
    entry = file.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    const char * filename = entry.name();
    Serial.println(filename); 
    if (!entry.isDirectory())  {
      strncpy(&files[totalFiles][0], filename, MAX_FILENAME_SIZE-1);
      totalFiles++;
    }
    else {
      if ( (strcmp(filename,".")) && (strcmp(filename,"..")) ) {
        strncpy(&files[totalFiles][0], filename, MAX_FILENAME_SIZE-1);
        totalFiles++;
      }
    }  
    entry.close();
  }
  file.close();
  return totalFiles;  
}  

void emu_init(int hires)
{
  Serial.begin(MONITOR_BAUDRATE);
  vgahires = hires;

#ifdef FILEBROWSER
  if (!SD.begin(VGA32_SD_CS))
  {
    Serial.println("No SD card detected");
  }
  strcpy(selection,ROMSDIR);
  FileHandlersInit();
  nbFiles = readNbFiles(); 
  Serial.println(nbFiles);
#endif

  int gfx_mode = CFG_VGA; // default
  //TODO: Get gfx_mode from config file and decide the config of the VGA display

  DisplayController.begin();
  DisplayController.setResolution(VGA_640x480_60Hz);
  Serial.println("VGA 640x480 60Hz");

  canvas.setBrushColor(Color::Black);
  canvas.clear();
  canvas.setGlyphOptions(GlyphOptions().FillBackground(true));
  canvas.selectFont(&fabgl::FONT_8x8);
  canvas.setPenColor(Color::BrightWhite);
  canvas.setGlyphOptions(GlyphOptions().DoubleWidth(1));
  canvas.drawText(10, 15, "NO SD CARD DETECTED");
  canvas.setGlyphOptions(GlyphOptions().DoubleWidth(0));

}

int menuActive(void) 
{
  return (menuOn?1:0);
}

unsigned short emu_ReadKeyboardKey(void)
{
  
}

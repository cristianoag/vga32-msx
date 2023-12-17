
#include "machine.h"
#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>
#include "fabgl.h"
#include "fabutils.h"

#define DEBUG true

// Flash and microSDCard configuration
#define FORMAT_ON_FAIL     true
#define SPIFFS_MOUNT_PATH  "/flash"
#define SDCARD_MOUNT_PATH  "/SD"

// base path (can be SPIFFS_MOUNT_PATH or SDCARD_MOUNT_PATH depending from what was successfully mounted first)
char const * basepath = nullptr;

// FABGL lib VGA and Keyboard controllers
fabgl::VGAController DisplayController;
fabgl::PS2Controller PS2Controller;

// where to store WiFi info, etc...
Preferences preferences;

enum { STYLE_NONE, STYLE_LABEL, STYLE_STATICLABEL, STYLE_LABELGROUP, STYLE_BUTTON, STYLE_BUTTONHELP, STYLE_COMBOBOX, STYLE_CHECKBOX, STYLE_FILEBROWSER};

#define BACKGROUND_COLOR RGB888(0, 0, 0)

struct DialogStyle : uiStyle {
  void setStyle(uiObject * object, uint32_t styleClassID) {
    switch (styleClassID) {
      case STYLE_LABEL:
        ((uiLabel*)object)->labelStyle().textFont                           = &fabgl::FONT_std_12;
        ((uiLabel*)object)->labelStyle().backgroundColor                    = BACKGROUND_COLOR;
        ((uiLabel*)object)->labelStyle().textColor                          = RGB888(255, 255, 255);
        break;
      case STYLE_STATICLABEL:
        ((uiStaticLabel*)object)->labelStyle().textFont                     = &fabgl::FONT_std_12;
        ((uiStaticLabel*)object)->labelStyle().backgroundColor              = BACKGROUND_COLOR;
        ((uiStaticLabel*)object)->labelStyle().textColor                    = RGB888(255, 255, 255);
        break;
      case STYLE_LABELGROUP:
        ((uiStaticLabel*)object)->labelStyle().textFont                     = &fabgl::FONT_std_12;
        ((uiStaticLabel*)object)->labelStyle().backgroundColor              = BACKGROUND_COLOR;
        ((uiStaticLabel*)object)->labelStyle().textColor                    = RGB888(255, 128, 0);
        break;
      case STYLE_BUTTON:
        ((uiButton*)object)->windowStyle().borderColor                      = RGB888(255, 255, 255);
        ((uiButton*)object)->buttonStyle().backgroundColor                  = RGB888(128, 128, 128);
        break;
      case STYLE_BUTTONHELP:
        ((uiButton*)object)->windowStyle().borderColor                      = RGB888(255, 255, 255);
        ((uiButton*)object)->buttonStyle().backgroundColor                  = RGB888(255, 255, 128);
        break;
      case STYLE_COMBOBOX:
        ((uiFrame*)object)->windowStyle().borderColor                       = RGB888(255, 255, 255);
        ((uiFrame*)object)->windowStyle().borderSize                        = 1;
        break;
      case STYLE_CHECKBOX:
        ((uiFrame*)object)->windowStyle().borderColor                       = RGB888(255, 255, 255);
        break;
      case STYLE_FILEBROWSER:
        ((uiListBox*)object)->windowStyle().borderColor                     = RGB888(128, 128, 128);
        ((uiListBox*)object)->windowStyle().focusedBorderColor              = RGB888(255, 255, 255);
        ((uiListBox*)object)->listBoxStyle().backgroundColor                = RGB888(0, 255, 255);
        ((uiListBox*)object)->listBoxStyle().focusedBackgroundColor         = RGB888(0, 255, 255);
        ((uiListBox*)object)->listBoxStyle().selectedBackgroundColor        = RGB888(128, 64, 0);
        ((uiListBox*)object)->listBoxStyle().focusedSelectedBackgroundColor = RGB888(255, 128, 0);
        ((uiListBox*)object)->listBoxStyle().textColor                      = RGB888(0, 0, 128);
        ((uiListBox*)object)->listBoxStyle().selectedTextColor              = RGB888(0, 0, 255);
        ((uiListBox*)object)->listBoxStyle().textFont                       = &fabgl::FONT_SLANT_8x14;
        break;
    }
  }
} dialogStyle;

struct DownloadProgressFrame : public uiFrame {

  uiLabel *  label1;
  uiLabel *  label2;
  uiButton * button;

  DownloadProgressFrame(uiFrame * parent)
    : uiFrame(parent, "Download", UIWINDOW_PARENTCENTER, Size(150, 110), false) {
    frameProps().resizeable        = false;
    frameProps().moveable          = false;
    frameProps().hasCloseButton    = false;
    frameProps().hasMaximizeButton = false;
    frameProps().hasMinimizeButton = false;

    label1 = new uiLabel(this, "Connecting...", Point(10, 25));
    label2 = new uiLabel(this, "", Point(10, 45));
    button = new uiButton(this, "Abort", Point(50, 75), Size(50, 20));
    button->onClick = [&]() { exitModal(1); };
  }

};

struct HelpFame : public uiFrame {

  HelpFame(uiFrame * parent)
    : uiFrame(parent, "Help", UIWINDOW_PARENTCENTER, Size(218, 335), false) {

    auto button = new uiButton(this, "OK", Point(84, 305), Size(50, 20));
    button->onClick = [&]() { exitModal(0); };

    onPaint = [&]() {
      auto cv = canvas();
      cv->selectFont(&fabgl::FONT_6x8);
      int x = 4;
      int y = 10;
      cv->setPenColor(Color::Green);
      cv->drawText(x, y += 14, "Keyboard Shortcuts:");
      cv->setPenColor(Color::Black);
      cv->setBrushColor(Color::White);
      cv->drawText(x, y += 14, "  F12          > Run or Config");
      cv->drawText(x, y += 14, "  DEL          > Delete File/Folder");
      cv->drawText(x, y += 14, "  ALT + ASWZ   > Move Screen");
      cv->setPenColor(Color::Blue);
      cv->drawText(x, y += 18, "\"None\" Joystick Mode:");
      cv->setPenColor(Color::Black);
      cv->drawText(x, y += 14, "  ALT + MENU   > Joystick Fire");
      cv->drawText(x, y += 14, "  ALT + CURSOR > Joystick Move");
      cv->setPenColor(Color::Red);
      cv->drawText(x, y += 18, "\"Cursor Keys\" Joystick Mode:");
      cv->setPenColor(Color::Black);
      cv->drawText(x, y += 14, "  MENU         > Joystick Fire");
      cv->drawText(x, y += 14, "  CURSOR       > Joystick Move");
      cv->setPenColor(Color::Magenta);
      cv->drawText(x, y += 18, "Emulated keyboard:");
      cv->setPenColor(Color::Black);
      cv->drawText(x, y += 14, "  HOME         > CLR/HOME");
      cv->drawText(x, y += 14, "  ESC          > RUN/STOP");
      cv->drawText(x, y += 14, "  Left Win Key > CBM");
      cv->drawText(x, y += 14, "  DELETE       > RESTORE");
      cv->drawText(x, y += 14, "  BACKSPACE    > INST/DEL");
      cv->drawText(x, y += 14, "  Caret ^      > UP ARROW");
      cv->drawText(x, y += 14, "  Underscore _ > LEFT ARROW");
      cv->drawText(x, y += 14, "  Tilde ~      > PI");
    };
  }

};

class Menu : public uiApp {

  uiFileBrowser * fileBrowser;
  uiComboBox *    RAMExpComboBox;
  uiComboBox *    kbdComboBox;
  uiLabel *       freeSpaceLbl;

  // Main machine
  Machine *       machine;


  void init() {

  #if DEBUG
    Serial.println("Menu::init()");
  #endif
    machine = new Machine(&DisplayController);

    setStyle(&dialogStyle);

    rootWindow()->frameStyle().backgroundColor = BACKGROUND_COLOR;

    // static text
    rootWindow()->onPaint = [&]() {
      auto cv = canvas();
      cv->selectFont(&fabgl::FONT_SLANT_8x14);
      cv->setPenColor(RGB888(0, 128, 255));
      cv->drawText(66, 300, "VGA32 MSX Emulator");
    };

    // programs list
    fileBrowser = new uiFileBrowser(rootWindow(), Point(5, 10), Size(210, 200), true, STYLE_FILEBROWSER);
    fileBrowser->setDirectory(basepath);  // set absolute path
    //fileBrowser->changeDirectory( fileBrowser->content().exists(DOWNDIR, false) ? DOWNDIR : EMBDIR ); // set relative path
    fileBrowser->onChange = [&]() {
      // nothing yet
    };

    // handles following keys:
    //  F1         ->  show help
    //  ESC or F12 ->  run the emulator
    rootWindow()->onKeyUp = [&](uiKeyEventInfo const & key) {
      switch (key.VK) {
        // F1
        case VirtualKey::VK_F1:
          showHelp();
          break;
        // ESC or F12
        case VirtualKey::VK_ESCAPE:
        case VirtualKey::VK_F12:
          runMSX();
          break;
        // just to avoid compiler warning
        default:
          break;
      }
    };

    // handles following keys in filebrowser:
    //   RETURN -> load selected program and return to vic
    //   DELETE -> remove selected dir or file
    fileBrowser->onKeyUp = [&](uiKeyEventInfo const & key) {
      switch (key.VK) {
        // RETURN
        case VirtualKey::VK_RETURN:
          if (!fileBrowser->isDirectory() && loadSelectedProgram())
            runMSX();
          break;
        // DELETE
        case VirtualKey::VK_DELETE:
          if (messageBox("Delete Item", "Are you sure?", "Yes", "Cancel") == uiMessageBoxResult::Button1) {
            fileBrowser->content().remove( fileBrowser->filename() );
            fileBrowser->update();
            updateFreeSpaceLabel();
          }
          break;
        // just to avoid compiler warning
        default:
          break;
      }
    };

    // handles double click on program list
    fileBrowser->onDblClick = [&]() {
      if (!fileBrowser->isDirectory() && loadSelectedProgram())
        runMSX();
    };

    int x = 230;

    // "Run" button - run the MSX
    auto VIC20Button = new uiButton(rootWindow(), "Run [F12]", Point(x, 10), Size(75, 19), uiButtonKind::Button, true, STYLE_BUTTON);
    VIC20Button->onClick = [&]() {
      runMSX();
    };

    // "Load" button
    auto loadButton = new uiButton(rootWindow(), "Load", Point(x, 35), Size(75, 19), uiButtonKind::Button, true, STYLE_BUTTON);
    loadButton->onClick = [&]() {
      if (loadSelectedProgram())
        runMSX();
    };

    // "reset" button
    auto resetButton = new uiButton(rootWindow(), "Soft Reset", Point(x, 60), Size(75, 19), uiButtonKind::Button, true, STYLE_BUTTON);
    resetButton->onClick = [&]() {
      machine->reset();
      runMSX();
    };

    // "Hard Reset" button
    auto hresetButton = new uiButton(rootWindow(), "Hard Reset", Point(x, 85), Size(75, 19), uiButtonKind::Button, true, STYLE_BUTTON);
    hresetButton->onClick = [&]() {
      machine->reset();
      runMSX();
    };

    // "help" button
    auto helpButton = new uiButton(rootWindow(), "HELP [F1]", Point(x, 110), Size(75, 19), uiButtonKind::Button, true, STYLE_BUTTONHELP);
    helpButton->onClick = [&]() {
      showHelp();
    };

    // free space label
    freeSpaceLbl = new uiLabel(rootWindow(), "", Point(5, 214), Size(0, 0), true, STYLE_LABEL);
    updateFreeSpaceLabel();

    // focus on programs list
    setFocusedWindow(fileBrowser);
  }

  // this is the main loop where 6502 instructions are executed until F12 has been pressed
  void runMSX()
  {
    auto keyboard = PS2Controller.keyboard();

    enableKeyboardAndMouseEvents(false);
    keyboard->emptyVirtualKeyQueue();

    auto cv = canvas();
    cv->setBrushColor(0, 0, 0);
    cv->clear();
    cv->waitCompletion();

    bool run = true;
    while (run) {

      machine->run();

      // read keyboard
      if (keyboard->virtualKeyAvailable()) {
        bool keyDown;
        VirtualKey vk = keyboard->getNextVirtualKey(&keyDown);
        switch (vk) {

          // F12 - stop running
          case VirtualKey::VK_F12:
            if (!keyDown)
              run = false;  // exit loop
            break;

          // other keys
          default:
            // send to emulated machine
            // machine->setKeyboard(vk, keyDown);
            break;
        }
      }

    }
    keyboard->emptyVirtualKeyQueue();
    enableKeyboardAndMouseEvents(true);
    rootWindow()->repaint();
  }

  bool loadSelectedProgram()
  {
    bool backToMSX = false;
    char const * fname = fileBrowser->filename();
    if (fname) {
      FileBrowser & dir  = fileBrowser->content();

      bool isROM = strstr(fname, ".ROM") || strstr(fname, ".rom");

      if (isROM) {

        int fullpathlen = dir.getFullPath(fname);
        char fullpath[fullpathlen];
        dir.getFullPath(fname, fullpath, fullpathlen);

        if (isROM) {
          // this is a ROM, just reset, load and run
         // machine->loadPRG(fullpath, true, true);
        } 
        backToMSX = true;
      }
    }
    return backToMSX;
  }

  // show free SPIFFS space
  void updateFreeSpaceLabel() {
    int64_t total, used;
    FileBrowser::getFSInfo(fileBrowser->content().getCurrentDriveType(), 0, &total, &used);
    freeSpaceLbl->setTextFmt("%lld KB Free", (total - used) / 1024);
    freeSpaceLbl->update();
  }

  void showHelp() {
    auto hframe = new HelpFame(rootWindow());
    showModalWindow(hframe);
    destroyWindow(hframe);
  }


};


void setup()
{
  #if DEBUG
    Serial.begin(115200);
    Serial.println("main::setup()");
  #endif

  preferences.begin("MSX", false);
  //preferences.clear();

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::CreateVirtualKeysQueue);

  DisplayController.begin();
  DisplayController.setResolution(QVGA_320x240_60Hz);

  Canvas cv(&DisplayController);
  cv.selectFont(&fabgl::FONT_6x8);

  cv.clear();
  cv.drawText(25, 10, "Initializing...");
  cv.waitCompletion();

  if (FileBrowser::mountSDCard(FORMAT_ON_FAIL, SDCARD_MOUNT_PATH))
    basepath = SDCARD_MOUNT_PATH;
  else if (FileBrowser::mountSPIFFS(FORMAT_ON_FAIL, SPIFFS_MOUNT_PATH))
    basepath = SPIFFS_MOUNT_PATH;

}


void loop()
{
  auto menu = new Menu;
  menu->run(&DisplayController);
}

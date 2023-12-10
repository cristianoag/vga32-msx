#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <stdio.h>

#include "fabgl.h"
#include "fabutils.h"

#include "src/machine.h"

// non-free list
char const *  LIST_URL    = "http://cloud.cbm8bit.com/adamcost/vic20list.txt";
constexpr int MAXLISTSIZE = 8192;


// Flash and SDCard configuration
#define FORMAT_ON_FAIL     true
#define SPIFFS_MOUNT_PATH  "/flash"
#define SDCARD_MOUNT_PATH  "/SD"

// base path (can be SPIFFS_MOUNT_PATH or SDCARD_MOUNT_PATH depending from what was successfully mounted first)
char const * basepath = nullptr;

// name of embedded programs directory
char const * EMBDIR  = "Free";

// name of downloaded programs directory
char const * DOWNDIR = "List";

// name of user directory (where LOAD and SAVE works)
char const * USERDIR = "Free";    // same of EMBDIR

struct EmbeddedProgDef {
  char const *    filename;
  uint8_t const * data;
  int             size;
};

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
    machine = new Machine(&DisplayController);

    setStyle(&dialogStyle);

    rootWindow()->frameStyle().backgroundColor = BACKGROUND_COLOR;

    // some static text
    rootWindow()->onPaint = [&]() {
      auto cv = canvas();
      cv->selectFont(&fabgl::FONT_SLANT_8x14);
      cv->setPenColor(RGB888(0, 128, 255));
      cv->drawText(106, 345, "VGA32 MSX Emulator");

      cv->selectFont(&fabgl::FONT_std_12);
      cv->setPenColor(RGB888(255, 128, 0));
      //cv->drawText(160, 358, "www.fabgl.com");
      //cv->drawText(130, 371, "2019/22 by Fabrizio Di Vittorio");
    };

    // programs list
    fileBrowser = new uiFileBrowser(rootWindow(), Point(5, 10), Size(140, 290), true, STYLE_FILEBROWSER);
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

    int x = 158;

    // "Run" button - run the VIC20
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

    int y = 245;
    
    new uiStaticLabel(rootWindow(), "Joystick:", Point(150, y), true, STYLE_LABELGROUP);
    new uiStaticLabel(rootWindow(), "None", Point(180, y + 21), true, STYLE_STATICLABEL);
    auto radioJNone = new uiCheckBox(rootWindow(), Point(158, y + 20), Size(16, 16), uiCheckBoxKind::RadioButton, true, STYLE_CHECKBOX);
    new uiStaticLabel(rootWindow(), "Cursor Keys", Point(180, y + 41), true, STYLE_STATICLABEL);
    auto radioJCurs = new uiCheckBox(rootWindow(), Point(158, y + 40), Size(16, 16), uiCheckBoxKind::RadioButton, true, STYLE_CHECKBOX);
    new uiStaticLabel(rootWindow(), "Mouse", Point(180, y + 61), true, STYLE_STATICLABEL);
    auto radioJMous = new uiCheckBox(rootWindow(), Point(158, y + 60), Size(16, 16), uiCheckBoxKind::RadioButton, true, STYLE_CHECKBOX);
    radioJNone->setGroupIndex(1);
    radioJCurs->setGroupIndex(1);
    radioJMous->setGroupIndex(1);

    // free space label
    freeSpaceLbl = new uiLabel(rootWindow(), "", Point(5, 304), Size(0, 0), true, STYLE_LABEL);
    updateFreeSpaceLabel();

    // "Download From" label
    new uiStaticLabel(rootWindow(), "Download From:", Point(5, 326), true, STYLE_LABELGROUP);

    // Download List button (download programs listed and linked in LIST_URL)
    auto downloadProgsBtn = new uiButton(rootWindow(), "List", Point(13, 345), Size(27, 20), uiButtonKind::Button, true, STYLE_BUTTON);
    downloadProgsBtn->onClick = [&]() {
      if (checkWiFi() && messageBox("Download Programs listed in \"LIST_URL\"", "Check your local laws for restrictions", "OK", "Cancel", nullptr, uiMessageBoxIcon::Warning) == uiMessageBoxResult::Button1) {
        auto pframe = new DownloadProgressFrame(rootWindow());
        auto modalStatus = initModalWindow(pframe);
        processModalWindowEvents(modalStatus, 100);
        prepareForDownload();
        char * list = downloadList();
        char * plist = list;
        int count = countListItems(list);
        int dcount = 0;
        for (int i = 0; i < count; ++i) {
          char const * filename;
          char const * URL;
          plist = getNextListItem(plist, &filename, &URL);
          pframe->label1->setText(filename);
          if (!processModalWindowEvents(modalStatus, 100))
            break;
          if (downloadURL(URL, filename))
            ++dcount;
          pframe->label2->setTextFmt("Downloaded %d/%d", dcount, count);
        }
        free(list);
        // modify "abort" button to "OK"
        pframe->button->setText("OK");
        pframe->button->repaint();
        // wait for OK
        processModalWindowEvents(modalStatus, -1);
        endModalWindow(modalStatus);
        destroyWindow(pframe);
        fileBrowser->update();
        updateFreeSpaceLabel();
      }
      WiFi.disconnect();
    };

    // Download from URL
    auto downloadURLBtn = new uiButton(rootWindow(), "URL", Point(48, 345), Size(27, 20), uiButtonKind::Button, true, STYLE_BUTTON);
    downloadURLBtn->onClick = [&]() {
      if (checkWiFi()) {
        char * URL = new char[128];
        char * filename = new char[25];
        strcpy(URL, "http://");
        if (inputBox("Download From URL", "URL", URL, 127, "OK", "Cancel") == uiMessageBoxResult::Button1) {
          char * lastslash = strrchr(URL, '/');
          if (lastslash) {
            strcpy(filename, lastslash + 1);
            if (inputBox("Download From URL", "Filename", filename, 24, "OK", "Cancel") == uiMessageBoxResult::Button1) {
              if (downloadURL(URL, filename)) {
                messageBox("Success", "Download OK!", "OK", nullptr, nullptr);
                fileBrowser->update();
                updateFreeSpaceLabel();
              } else
                messageBox("Error", "Download Failed!", "OK", nullptr, nullptr, uiMessageBoxIcon::Error);
            }
          }
        }
        delete [] filename;
        delete [] URL;
        WiFi.disconnect();
      }
    };

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
      bool isCRT = strstr(fname, ".CRT") || strstr(fname, ".crt");

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

  // true if WiFi is has been connected
  bool checkWiFi() {
    if (WiFi.status() == WL_CONNECTED)
      return true;
    char SSID[32] = "";
    char psw[32]  = "";
    if (!preferences.getString("SSID", SSID, sizeof(SSID)) || !preferences.getString("WiFiPsw", psw, sizeof(psw))) {
      // ask user for SSID and password
      if (inputBox("WiFi Connect", "WiFi Name", SSID, sizeof(SSID), "OK", "Cancel") == uiMessageBoxResult::Button1 &&
          inputBox("WiFi Connect", "Password", psw, sizeof(psw), "OK", "Cancel") == uiMessageBoxResult::Button1) {
        preferences.putString("SSID", SSID);
        preferences.putString("WiFiPsw", psw);
      } else
        return false;
    }
    WiFi.begin(SSID, psw);
    for (int i = 0; i < 32 && WiFi.status() != WL_CONNECTED; ++i) {
      delay(500);
      if (i == 16)
        WiFi.reconnect();
    }
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (!connected) {
      messageBox("Network Error", "Failed to connect WiFi. Try again!", "OK", nullptr, nullptr, uiMessageBoxIcon::Error);
      preferences.remove("WiFiPsw");
    }
    return connected;
  }

  // creates List folder and makes it current
  void prepareForDownload()
  {
    FileBrowser & dir = fileBrowser->content();
    dir.setDirectory(basepath);
    dir.makeDirectory(DOWNDIR);
    dir.changeDirectory(DOWNDIR);
  }

  // download list from LIST_URL
  // ret nullptr on fail
  char * downloadList()
  {
    auto list = (char*) malloc(MAXLISTSIZE);
    auto dest = list;
    HTTPClient http;
    http.begin(LIST_URL);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      WiFiClient * stream = http.getStreamPtr();
      int bufspace = MAXLISTSIZE;
      while (http.connected() && bufspace > 1) {
        auto size = stream->available();
        if (size) {
          int c = stream->readBytes(dest, fabgl::imin(bufspace, size));
          dest += c;
          bufspace -= c;
        } else
          break;
      }
    }
    *dest = 0;
    return list;
  }

  // count number of items in download list
  int countListItems(char const * list)
  {
    int count = 0;
    auto p = list;
    while (*p++)
      if (*p == 0x0a)
        ++count;
    return (count + 1) / 3;
  }

  // extract filename and URL from downloaded list
  char * getNextListItem(char * list, char const * * filename, char const * * URL)
  {
    // bypass spaces
    while (*list == 0x0a || *list == 0x0d || *list == 0x20)
      ++list;
    // get filename
    *filename = list;
    while (*list && *list != 0x0a && *list != 0x0d)
      ++list;
    *list++ = 0;
    // bypass spaces
    while (*list && (*list == 0x0a || *list == 0x0d || *list == 0x20))
      ++list;
    // get URL
    *URL = list;
    while (*list && *list != 0x0a && *list != 0x0d)
      ++list;
    *list++ = 0;
    return list;
  }

  // download specified filename from URL
  bool downloadURL(char const * URL, char const * filename)
  {
    FileBrowser & dir = fileBrowser->content();
    if (dir.exists(filename, false)) {
      return true;
    }
    bool success = false;
    HTTPClient http;
    http.begin(URL);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {

      int fullpathLen = dir.getFullPath(filename);
      char fullpath[fullpathLen];
      dir.getFullPath(filename, fullpath, fullpathLen);
      FILE * f = fopen(fullpath, "wb");

      if (f) {
        int len = http.getSize();
        constexpr int BUFSIZE = 1024;
        uint8_t * buf = (uint8_t*) malloc(BUFSIZE);
        WiFiClient * stream = http.getStreamPtr();
        int dsize = 0;
        while (http.connected() && (len > 0 || len == -1)) {
          size_t size = stream->available();
          if (size) {
            int c = stream->readBytes(buf, fabgl::imin(BUFSIZE, size));
            fwrite(buf, c, 1, f);
            dsize += c;
            if (len > 0)
              len -= c;
          }
        }
        free(buf);
        fclose(f);
        success = (len == 0 || (len == -1 && dsize > 0));
      }
    }
    return success;
  }

  // show free SPIFFS space
  void updateFreeSpaceLabel() {
    int64_t total, used;
    FileBrowser::getFSInfo(fileBrowser->content().getCurrentDriveType(), 0, &total, &used);
    freeSpaceLbl->setTextFmt("%lld KiB Free", (total - used) / 1024);
    freeSpaceLbl->update();
  }

  void showHelp() {
    auto hframe = new HelpFame(rootWindow());
    showModalWindow(hframe);
    destroyWindow(hframe);
  }


};



///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////




void setup()
{
  #if DEBUG
  Serial.begin(115200); delay(500); Serial.write("\n\n\nReset\n"); // for debug purposes
  #endif

  preferences.begin("MSX", false);
  //preferences.clear();

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::CreateVirtualKeysQueue);

  DisplayController.begin();
  DisplayController.setResolution(VGA_256x384_60Hz);

  // this improves user interface speed - check possible drawbacks before enabling definitively
  //DisplayController.enableBackgroundPrimitiveExecution(false);

  // adjust this to center screen in your monitor
  //DisplayController.moveScreen(20, -2);

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

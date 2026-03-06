/**
 * CardputerGPIO — General Purpose GPIO Control & Electronics Debugging Tool
 * M5Stack Cardputer (ESP32-S3)
 */

#include <M5Cardputer.h>

#include "src/PinManager.h"
#include "src/PinConfig.h"
#include "src/PinConfigurator.h"
#include "src/WiringGuide.h"
#include "src/MenuSystem.h"
#include "src/profiles/ProfileICControl.h"
#include "src/profiles/ProfileDigitalOut.h"
#include "src/profiles/ProfileDigitalIn.h"
#include "src/profiles/ProfileAnalogReader.h"
#include "src/profiles/ProfilePWMGen.h"
#include "src/profiles/ProfileSignalMonitor.h"
#include "src/profiles/ProfileI2CScanner.h"
#include "src/profiles/ProfileLogicAnalyzer.h"

PinManager   pinManager;
MenuSystem   menuSystem;

ProfileICControl      profIC;
ProfileDigitalOut     profDigOut;
ProfileDigitalIn      profDigIn;
ProfileAnalogReader   profAnalog;
ProfilePWMGen         profPWM;
ProfileSignalMonitor  profSigMon;
ProfileI2CScanner     profI2C;
ProfileLogicAnalyzer  profLogic;

void setup() {
    Serial.begin(115200);
    M5Cardputer.begin();

    auto& disp = M5Cardputer.Display;
    disp.setRotation(1);
    disp.setBrightness(128);

    // ── Splash screen: proves display pipeline works ───────────────────
    // If this appears, display is fine. If not, it's a hardware/library issue.
    disp.fillScreen(0x001a33);
    disp.setTextColor(0xffd700);
    disp.setTextSize(1);
    disp.setCursor(10, 20);
    disp.print("CardputerGPIO");
    disp.setTextColor(0xd0d0d0);
    disp.setCursor(10, 40);
    disp.print("Loading...");
    delay(800);

    pinManager.init();
    menuSystem.init(&pinManager);

    menuSystem.addProfile(&profIC,      "ic");
    menuSystem.addProfile(&profDigOut,  "dout");
    menuSystem.addProfile(&profDigIn,   "din");
    menuSystem.addProfile(&profAnalog,  "adc");
    menuSystem.addProfile(&profPWM,     "pwm");
    menuSystem.addProfile(&profSigMon,  "sigmon");
    menuSystem.addProfile(&profI2C,     "i2c");
    menuSystem.addProfile(&profLogic,   "logic");

    menuSystem.showMainMenu();
    Serial.println("[CardputerGPIO] Ready.");
}

void loop() {
    menuSystem.update();
    delay(10);
}

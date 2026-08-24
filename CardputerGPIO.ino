#include <M5Cardputer.h>
#include "src/core/Board.h"
#include "src/core/Keys.h"
#include "src/core/UI.h"
#include "src/core/Pins.h"
#include "src/core/Settings.h"
#include "src/ui/Shell.h"
#include "src/tools/ToolDashboard.h"
#include "src/tools/ToolDigitalOut.h"
#include "src/tools/ToolDigitalIn.h"
#include "src/tools/ToolAnalogIn.h"
#include "src/tools/ToolMeter.h"
#include "src/tools/ToolPWM.h"
#include "src/tools/ToolServo.h"
#include "src/tools/ToolStepper.h"
#include "src/tools/ToolFreq.h"
#include "src/tools/ToolScope.h"
#include "src/tools/ToolI2C.h"
#include "src/tools/ToolUART.h"
#include "src/tools/ToolSPI.h"
#include "src/tools/ToolSensor.h"
#include "src/tools/ToolIR.h"
#include "src/tools/ToolNeoPixel.h"
#include "src/tools/ToolIC.h"
#include "src/tools/ToolSetups.h"
#include "src/tools/ToolSettings.h"
#include "src/tools/ToolBoard.h"

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    cg::settings.begin();
    cg::boardBegin();
    cg::ui.begin();
    cg::keys.begin();
    cg::pins.begin();
    cg::shell.begin();
    cg::shell.add(&cg::toolDashboard);
    cg::shell.add(&cg::toolDigitalOut);
    cg::shell.add(&cg::toolDigitalIn);
    cg::shell.add(&cg::toolAnalogIn);
    cg::shell.add(&cg::toolMeter);
    cg::shell.add(&cg::toolPWM);
    cg::shell.add(&cg::toolServo);
    cg::shell.add(&cg::toolStepper);
    cg::shell.add(&cg::toolFreq);
    cg::shell.add(&cg::toolScope);
    cg::shell.add(&cg::toolNeoPixel);
    cg::shell.add(&cg::toolI2C);
    cg::shell.add(&cg::toolUART);
    cg::shell.add(&cg::toolSPI);
    cg::shell.add(&cg::toolSensor);
    cg::shell.add(&cg::toolIR);
    cg::shell.add(&cg::toolIC);
    cg::shell.add(&cg::toolBoard);
    cg::shell.add(&cg::toolSetups);
    cg::shell.add(&cg::toolSettings);
}

void loop() {
    M5Cardputer.update();
    cg::shell.run();
    delay(3);
}

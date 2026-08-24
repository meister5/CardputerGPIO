#include "Tool.h"

namespace cg {

const char* catName(Cat c) {
    switch (c) {
        case Cat::Digital: return "Digital";
        case Cat::Analog:  return "Analog";
        case Cat::Signal:  return "Signal";
        case Cat::Bus:     return "Bus";
        case Cat::System:  return "System";
        default:           return "?";
    }
}

uint16_t catColor(Cat c) {
    switch (c) {
        case Cat::Digital: return C_ROLE_OUT;
        case Cat::Analog:  return C_ROLE_ADC;
        case Cat::Signal:  return C_ROLE_PWM;
        case Cat::Bus:     return C_ROLE_BUS;
        case Cat::System:  return C_INFO;
        default:           return C_DIM;
    }
}

uint16_t roleColor(RoleDir d) {
    switch (d) {
        case RoleDir::Out: return C_ROLE_OUT;
        case RoleDir::In:  return C_ROLE_IN;
        case RoleDir::Adc: return C_ROLE_ADC;
        case RoleDir::Pwm: return C_ROLE_PWM;
        case RoleDir::Bus: return C_ROLE_BUS;
        default:           return C_DIM;
    }
}

const char* roleDirName(RoleDir d) {
    switch (d) {
        case RoleDir::Out: return "OUT";
        case RoleDir::In:  return "IN";
        case RoleDir::Adc: return "ADC";
        case RoleDir::Pwm: return "PWM";
        case RoleDir::Bus: return "BUS";
        default:           return "-";
    }
}

}  // namespace cg

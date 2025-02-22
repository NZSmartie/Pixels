#pragma once

#define MAX_LED_COUNT 21
#define MAX_BOARD_RESISTOR_VALUES 2

#include "stddef.h"
#include "core/float3.h"
#include "stdint.h"

namespace Config
{
    namespace BoardManager
    {
        struct Board
        {
            // Measuring board type
            int boardResistorValues[MAX_BOARD_RESISTOR_VALUES];

            // Talking to LEDs
            uint32_t ledDataPin;
            uint32_t ledClockPin;
            uint32_t ledPowerPin;

            // I2C Pins for accelerometer
            uint32_t i2cDataPin;
            uint32_t i2cClockPin;
            uint32_t accInterruptPin;

            // Power Management pins
            uint32_t chargingStatePin;
            uint32_t coilSensePin;
            uint32_t vbatSensePin;
            uint32_t vledSensePin;

            // Magnet pin
            uint32_t magnetPin;

            // LED config
            int ledCount;
        };

        void init();
        const Board* getBoard();
    }
}


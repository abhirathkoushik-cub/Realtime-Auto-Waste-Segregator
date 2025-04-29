#include <iostream>
#include "ads1115rpi.h"

class MQ7Callback : public ADS1115rpi::ADSCallbackInterface {
public:
    void hasADS1115Sample(float sample) override {
        std::cout << "Voltage: " << sample << " V" << std::endl;

        if (sample < 0.4f)
            std::cout << "Air Quality: CO Perfect\n";
        else if (sample < 1.0f)
            std::cout << "Air Quality: CO Normal\n";
        else if (sample < 2.0f)
            std::cout << "Air Quality: CO High\n";
        else
            std::cout << "Air Quality: ALARM - CO Very High!\n";

        std::cout << "-----------------------------" << std::endl;
    }
};

int main() {
    MQ7Callback cb;
    ADS1115rpi reader;

    ADS1115settings settings;
    settings.channel = ADS1115settings::AIN0;     // A0 pin
    settings.pgaGain = ADS1115settings::FSR2_048; // ±2.048V range
    settings.samplingRate = ADS1115settings::FS8HZ;

    reader.registerCallback(&cb);
    reader.start(settings);

    while (true) {
        sleep(1);  // Main thread sleeps; data comes via callback
    }

    return 0;
}

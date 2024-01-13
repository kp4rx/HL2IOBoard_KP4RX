
// This is firmware for the Hermes Lite 2 IO board designed by Jim Ahlstrom, N2ADR. It is
//   Copyright (c) 2022-2023 James C. Ahlstrom <jahlstr@gmail.com>.
//   It is licensed under the MIT license. See MIT.txt.

// This firmware outputs the FT817 band voltage on J4 pin 8 and sets the band.

#include "../hl2ioboard.h"
#include "../i2c_registers.h"
#include <stdio.h>
#include <string.h>
//#include "pico/stdio_uart.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

// These are the major and minor version numbers for firmware. You must set these.
uint8_t firmware_version_major=1;
uint8_t firmware_version_minor=3;
uint64_t curr_tx_freq;
char freqstr[10];

int main()
{
    static uint8_t current_tx_fcode = 0;
    static bool current_is_rx = true;
    static uint8_t tx_band = 0;
    static uint8_t rx_band = 0;
    static uint8_t tune_req;
    uint8_t band, fcode;
    bool is_rx;
    bool change_band;
    static absolute_time_t tuner_time0;
    static absolute_time_t last_tune = 0;
    bool digimode;

    configure_pins(true, true);
    configure_led_flasher();
    //Set Grounds for Amp interfaces
    //gpio_put(GPIO10_Out5, 1);
    //gpio_put(GPIO22_Out6, 1);

    while (1) { // Wait for something to happen
        sleep_ms(1);    // This sets the polling frequency.
        // Poll for a changed Tx band, Rx band and T/R change
        is_rx = gpio_get(GPIO13_EXTTR);     // true for receive, false for transmit
        change_band = false;
        if (current_is_rx != is_rx) {
            current_is_rx = is_rx;
            change_band = true;
        }
        //Serial Freq Output for HR50
        if (new_tx_freq != curr_tx_freq) {
            sprintf(freqstr, "%f", new_tx_freq * 1E-6);
            int k=0;
            printf("FA");
            if (strlen(freqstr) == 8){
                printf("0000");}
            else
                printf("000");  
            while (freqstr[k]!=0) {
                if ( freqstr[k] != '.' ) {
                    printf("%c", freqstr[k]);
                }
                k++;
            }
            printf(";\n");
            curr_tx_freq = new_tx_freq;
        }
        // Poll for a changed Tx frequency. The new_tx_fcode is set in the I2C handler.
        if (current_tx_fcode != new_tx_fcode) {
            current_tx_fcode = new_tx_fcode;
            change_band = true;
            tx_band = fcode2band(current_tx_fcode);     // Convert the frequency code to a band code.
            xiegu_band_volts(tx_band);          // Put the band voltage on J4 pin 8.                        
        }
        //skip_exttr_check:
        //Digital Modes Check
        if (Registers[REG_OP_MODE] >= 7 && Registers[REG_OP_MODE] <= 9){
            digimode = true;
        }
        else {
            digimode = false;
        }       
        tune_req = Registers[REG_ANTENNA_TUNER];
        // Poll for EXTTR State to enable PTT pull low on on J6 Pins 5 and 6
        if (current_is_rx == 0 && tune_req == 0 && digimode == false ) { //TX
            //current_is_rx = false;
            if (absolute_time_diff_us(last_tune, get_absolute_time ()) / 1000 >= 800)
            {
                gpio_put(GPIO10_Out5, 1);
                gpio_put(GPIO22_Out6, 1);
            }
        }
        else {      //RX
            current_is_rx = true;
            if (tune_req == 1) {                
                Registers[REG_ANTENNA_TUNER] = 0xEE;
                tuner_time0 = get_absolute_time ();
            }
            gpio_put(GPIO10_Out5, 0);
            gpio_put(GPIO22_Out6, 0);
        }       
        if (tune_req == 0xEE && absolute_time_diff_us(tuner_time0, get_absolute_time ()) / 1000 >= 5000)
        {
            Registers[REG_ANTENNA_TUNER] = 0;
            last_tune = get_absolute_time ();
            //goto skip_exttr_check;
        }

        // Poll for a change in one of the twelve Rx frequencies. The rx_freq_changed is set in the I2C handler.
        if (rx_freq_changed) {                      
            rx_freq_changed = false;
            change_band = true;
            if (rx_freq_high == 0)
                rx_band = tx_band;
            else
                rx_band = fcode2band(rx_freq_high); // Convert the frequency code to a band code.           
        }
    }
}
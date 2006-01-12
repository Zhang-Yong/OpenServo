/*
   Copyright (c) 2005, Mike Thompson <mpthompson@gmail.com>
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   * Neither the name of the copyright holders nor the names of
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

// The following is needed until WINAVR supports the ATtinyX5 MCUs.
#undef __AVR_ATtiny2313__
#define __AVR_ATtiny45__

#include <inttypes.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#include "adc.h"
#include "eeprom.h"
#include "motion.h"
#include "power.h"
#include "pwm.h"
#include "timer.h"
#include "twi.h"
#include "watchdog.h"
#include "registers.h"

#if defined(__AVR_ATtiny45__)
    // Do nothing.
#else
#  error "Don't know what kind of MCU you are compiling for"
#endif

void handle_twi_command(void)
{
    uint8_t command;

    // Get the command from the receive buffer.
    command = twi_receive_byte();

    switch (command)
    {
        case TWI_CMD_RESET:

            // Reset the servo.
            watchdog_hard_reset();

            break;

        case TWI_CMD_PWM_ENABLE:

            // Enable PWM to the servo motor.
            pwm_enable();

            break;

        case TWI_CMD_PWM_DISABLE:

            // Disable PWM to the servo motor.
            pwm_disable();

            break;

        case TWI_CMD_WRITE_ENABLE:

            // Enable write to safe read/write registers.
            registers_write_byte(WRITE_ENABLE, 0x01);

            break;

        case TWI_CMD_WRITE_DISABLE:

            // Disable write to safe read/write registers.
            registers_write_byte(WRITE_ENABLE, 0x00);

            break;

        case TWI_CMD_REGISTERS_SAVE:

            // Save register values into EEPROM.
            eeprom_save_registers();

            break;

        case TWI_CMD_REGISTERS_RESTORE:

            // Restore register values into EEPROM.
            eeprom_restore_registers();

            break;

        case TWI_CMD_REGISTERS_DEFAULT:

            // Restore register values to factory defaults.
            registers_defaults();
            break;

        default:

            // Ignore unknown command.
            break;
    }
}


int main (void)
{
    asm volatile(".set __stack,0x15F" : : );

    int16_t pwm;
    int16_t position;
    uint16_t power;
    uint16_t timer;

    // Initialize the watchdog module.
    watchdog_init();

    // First, initialize registers that control servo operation.
    registers_init();

    // Initialize the PWM module.
    pwm_init();

    // Initialize the ADC module.
    adc_init();

    // Initialize the motion module.
    motion_init();

    // Initialize the power module.
    power_init();

    // Initialize the TWI slave module.
    twi_slave_init(registers_read_byte(TWI_ADDRESS));

    // Finally initialize the timer.
    timer_set(0);

    // Enable interrupts.
    sei();

    // XXX Enable PWM and writing.
    registers_write_byte(PWM_ENABLE, 0x01);
    registers_write_byte(WRITE_ENABLE, 0x01);

    // Loop forever.
    for (;;)
    {
        // Is position value ready?
        if (adc_position_value_is_ready())
        {
#if 1
            // Reset the timer if greater than 16000.
            if (timer_get() > 16000) timer_set(0);


            if (timer_get() == 0)
            {
                registers_write_word(SEEK_HI, SEEK_LO, 0x0100);
            }
            else if (timer_get() == 8000)
            {
                registers_write_word(SEEK_HI, SEEK_LO, 0x0300);
            }
#endif

            // Get the new position value.
            position = (int16_t) adc_get_position_value();

            // Call the motion module to get a new PWM value.
            pwm = motion_position_to_pwm(position);

            // Update the servo movement as indicated by the PWM value.
            // Sanity checks are performed against the position value.
            pwm_update(position, pwm);
        }

        // Is a power value ready?
        if (adc_power_value_is_ready())
        {
            // Get the new power value.
            power = adc_get_power_value();

            // Update the power value for reporting.
            power_update(power);
        }

        // Was a data byte recieved?
        if (twi_data_in_receive_buffer())
        {
            // Handle any TWI command.
            handle_twi_command();
        }
    }

    return 0;
}


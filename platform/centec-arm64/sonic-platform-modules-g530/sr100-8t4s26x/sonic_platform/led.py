#!/usr/bin/env python

#############################################################################
#
# Module contains an implementation of SONiC Platform Base API and
# provides the led status which are available in the platform
#
#############################################################################

import os

class SystemLed(object):
    STATUS_LED_COLOR_GREEN = 'green'
    STATUS_LED_COLOR_ORANGE = 'orange'
    STATUS_LED_COLOR_OFF = 'off'

    SYSTEM_LED_PATH = '/sys/class/leds/system/brightness'
    ALARM_LED_PATH = '/sys/class/leds/alarm/brightness'

    def set_status(self, color):
        status = False

        if color == SystemLed.STATUS_LED_COLOR_ORANGE:
            with open(SystemLed.SYSTEM_LED_PATH, 'w') as led:
                led.write('0')
            with open(SystemLed.ALARM_LED_PATH, 'w') as led:
                led.write('1')
            status = True
        elif color == SystemLed.STATUS_LED_COLOR_GREEN:
            with open(SystemLed.SYSTEM_LED_PATH, 'w') as led:
                led.write('1')
            with open(SystemLed.ALARM_LED_PATH, 'w') as led:
                led.write('0')
            status = True
        elif color == SystemLed.STATUS_LED_COLOR_OFF:
            with open(SystemLed.SYSTEM_LED_PATH, 'w') as led:
                led.write('0')
            with open(SystemLed.ALARM_LED_PATH, 'w') as led:
                led.write('0')
            status = True

        return status

    def get_status(self):
        system_led_status = 0
        alarm_led_status = 0
        with open(SystemLed.SYSTEM_LED_PATH, 'r') as led:
            system_led_status = led.read().rstrip('\n')
        with open(SystemLed.ALARM_LED_PATH, 'r') as led:
            alarm_led_status = led.read().rstrip('\n')
        if system_led_status == '1':
            return SystemLed.STATUS_LED_COLOR_GREEN
        elif alarm_led_status == '1':
            return SystemLed.STATUS_LED_COLOR_ORANGE
        else:
            return SystemLed.STATUS_LED_COLOR_OFF

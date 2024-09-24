#!/usr/bin/env python
#
# led_control.py
#
# Platform-specific LED control functionality for SONiC
#

try:
    from sonic_led.led_control_base import LedControlBase
    import swsssdk
    import threading
    import os
    import logging
    import struct
    import time
    import syslog
    from socket import *
    from select import *
except ImportError as e:
    raise ImportError(str(e) + " - required module not found")


def DBG_PRINT(str):
    syslog.openlog("centec-led")
    syslog.syslog(syslog.LOG_INFO, str)
    syslog.closelog()


class LedControl(LedControlBase):
    """Platform specific LED control class"""

    def _initSystemLed(self):
        try:
            with open(self.f_led.format("system"), 'w') as led_file:
                led_file.write("1")
            DBG_PRINT("init system led to normal")

            with open(self.f_led.format("alarm"), 'w') as led_file:
                led_file.write("0")
            DBG_PRINT("init alarm led to off")

            with open(self.f_led.format("idn"), 'w') as led_file:
                led_file.write("0")
            DBG_PRINT("init idn led to off")
        except IOError as e:
            DBG_PRINT(str(e))


    def _initDefaultConfig(self):
        DBG_PRINT("start init led")

        self._initSystemLed()

        DBG_PRINT("init led done")

    # This board does not require support light port
    def port_link_state_change(self, portname, state):
        return

    # Constructor
    def __init__(self):
        self.f_led = "/sys/class/leds/{}/brightness"
        self._initDefaultConfig()


// This file should contain #defines specific to your hardware. I need
// to know:

// The location of a GPIO pin wired to a LED, with the LED wired to the
// pin's cathode (so that LOW means that the LED is on).
#define GPIO_LED            31

// The location of a GPIO pin wired to a switch, when enabled
// (pulled low) the bootloader is activated.
#define GPIO_KEY            0

// The location of a GPIO pin wired through a pull up circuit to DP (D+,
// whatever you want to call it; USB line). I can disconnect the pull-up
// that indicates that something is connected to the port by driving
// that line line LOW.
#define GPIO_USB_PU         16


#include "init.h"
#include "serial_logger.h"

static FILE serial_output = FDEV_SETUP_STREAM(add_char_serial, NULL, _FDEV_SETUP_WRITE);

uint8_t sequencer_init() 
{
    serial_init();
    stdout = &serial_output;  
}

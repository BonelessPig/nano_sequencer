#include "init.h"
#include "serial_logger.h"

static const FILE serial_output = FDEV_SETUP_STREAM(add_char_serial, NULL, _FDEV_SETUP_WRITE);

uint8_t sequencer_init() 
{
    serial_init(LOGLVL_DEBUG);
    stdout = &serial_output;  
}

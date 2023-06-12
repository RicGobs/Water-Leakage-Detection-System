#include <stdint.h>

uint32_t const SIMULATED_DAY = 30;

uint32_t const SAMPLING_PLUS_DATA_PROCESSING = 2;

uint32_t const NUMBER_OF_SENDING_PER_DAY = 3;
uint32_t const LEAKAGE_TEST_PERIOD = SIMULATED_DAY/NUMBER_OF_SENDING_PER_DAY;

uint32_t const LISTENING_TIMEOUT = 2*SAMPLING_PLUS_DATA_PROCESSING+LEAKAGE_TEST_PERIOD;
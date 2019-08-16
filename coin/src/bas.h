
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize Battery Service.
 * This implementation directly uses the ADC with the on-board resistor divider to measure battery level.
 * @return initially measured battery level
 */
uint8_t bas_init();

#ifdef __cplusplus
}
#endif

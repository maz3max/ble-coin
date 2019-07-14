
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize Battery Service.
 * This implementation directly uses the ADC with the on-board resistor divider to measure battery level.
 */
void bas_init();

#ifdef __cplusplus
}
#endif

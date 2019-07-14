
#ifdef __cplusplus
extern "C" {
#endif

typedef enum blink_state_t {
    BI_ON = 0,
    BI_AGGRESSIVE,
    BI_QUICK,
    BI_SLOW,
    BI_OFF,
    BI_MAX
} blink_state_t;

void io_init();

void set_blink_intensity(blink_state_t intensity);

#ifdef __cplusplus
}
#endif

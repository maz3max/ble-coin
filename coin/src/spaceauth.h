#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize Space Authentication Service.
 * This service uses a 32 Byte key which is loaded via the settings API.
 * It uses the key to sign a 64 Byte challenge using blake2s (reference impl) to create a response.
 */
void space_auth_init(void);

#ifdef __cplusplus
}
#endif

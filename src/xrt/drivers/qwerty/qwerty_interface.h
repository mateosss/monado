#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef union SDL_Event SDL_Event;

struct xrt_auto_prober *
qwerty_create_auto_prober(void);

void
qwerty_process_event(struct xrt_device **xdevs, SDL_Event event);

#ifdef __cplusplus
}
#endif

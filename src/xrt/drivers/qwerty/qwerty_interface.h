/*!
 * @file
 * @brief Interface to @ref drv_qwerty.
 * @author Mateo de Mayo <mateodemayo@gmail.com>
 * @ingroup drv_qwerty
 */

// XXXASK: The usage of author is correct?
// XXXASK: How to do the copyright/SPDX sections?

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @defgroup drv_qwerty Qwerty driver
 * @ingroup drv
 *
 * @brief Driver for emulated HMD and controllers through keyboard and mouse.
 */


typedef union SDL_Event SDL_Event;

struct xrt_auto_prober *
qwerty_create_auto_prober(void);

void
qwerty_process_event(struct xrt_device **xdevs, SDL_Event event);

/*!
 * @dir drivers/qwerty
 *
 * @brief @ref drv_qwerty files.
 */

#ifdef __cplusplus
}
#endif

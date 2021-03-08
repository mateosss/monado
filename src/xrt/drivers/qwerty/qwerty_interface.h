/*!
 * @file
 * @brief Interface to @ref drv_qwerty.
 * @author Mateo de Mayo <mateodemayo@gmail.com>
 * @ingroup drv_qwerty
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef union SDL_Event SDL_Event;

/*!
 * @defgroup drv_qwerty Qwerty driver
 * @ingroup drv
 *
 * @brief Driver for emulated HMD and controllers through keyboard and mouse.
 * @{
 */

//! Create an auto prober for qwerty devices.
struct xrt_auto_prober *
qwerty_create_auto_prober(void);

/*!
 * Process an SDL_Event (like a key press) and dispatches the appropriate action
 * to the right qwerty_device.
 *
 * @pre `xdevs[1]` and `xdevs[2]` point respectively to left and right qwerty_controller.
 * @note `xdevs[0]` might not point to a qwerty_hmd.
 */
void
qwerty_process_event(struct xrt_device **xdevs, SDL_Event event);

/*!
 * @}
 */

/*!
 * @dir drivers/qwerty
 *
 * @brief @ref drv_qwerty files.
 */

#ifdef __cplusplus
}
#endif

// Copyright 2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Code to manage the settings file.
 * @author Christoph Haag <christoph.haag@collabora.com>
 * @ingroup aux_util
 */

#pragma once

#include "util/u_json.h"
#include "xrt/xrt_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * What config is currently active in the config file.
 */
enum u_config_json_active_config
{
	U_ACTIVE_CONFIG_NONE = 0,
	U_ACTIVE_CONFIG_TRACKING = 1,
	U_ACTIVE_CONFIG_REMOTE = 2,
};

struct u_config_json
{
	//! For error reporting, was it loaded but not parsed?
	bool file_loaded;

	cJSON *root;
};

/*!
 * Load the JSON config file.
 *
 * @ingroup aux_util
 */
void
u_config_json_open_or_create_main_file(struct u_config_json *json);

/*!
 * Read from the JSON loaded json config file and returns the active config,
 * can be overridden by `P_OVERRIDE_ACTIVE_CONFIG` envirmental variable.
 *
 * @ingroup aux_util
 */
void
u_config_json_get_active(struct u_config_json *json, enum u_config_json_active_config *out_active);

/*!
 * Extract tracking settings from the JSON.
 *
 * @ingroup aux_util
 * @relatesalso xrt_settings_tracking
 */
bool
u_config_json_get_tracking_settings(struct u_config_json *json, struct xrt_settings_tracking *s);

/*!
 * Extract tracking override settings from the JSON.
 *
 * Caller allocates an array of XRT_MAX_TRACKING_OVERRIDES tracking_override.
 *
 * @ingroup aux_util
 * @relatesalso xrt_settings_tracking
 */
bool
u_config_json_get_tracking_overrides(struct u_config_json *json,
                                     struct xrt_tracking_override *out_overrides,
                                     size_t *out_num_overrides);

/*!
 * Extract remote settings from the JSON.
 *
 * @ingroup aux_util
 */
bool
u_config_json_get_remote_port(struct u_config_json *json, int *out_port);

#ifdef __cplusplus
}
#endif

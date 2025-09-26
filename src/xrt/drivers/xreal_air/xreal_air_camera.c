// Copyright 2025, Luka Panio
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Xreal Air SLAM camera implementation.
 * @author Luka Panio <lukapanio@gmail.com>
 * @ingroup drv_xreal_air
 */
#include <string.h>

#include "xreal_air.h"
#include "xreal_air_camera.h"

#include "os/os_threading.h"
#include "xrt/xrt_byte_order.h"

#include "xrt/xrt_defines.h"
#include "xrt/xrt_frame.h"
#include "xrt/xrt_frameserver.h"

#include "util/u_autoexpgain.h"
#include "util/u_debug.h"
#include "util/u_var.h"
#include "util/u_sink.h"
#include "util/u_frame.h"
#include "util/u_trace_marker.h"

#include <stdio.h>
#include <unistd.h>  // For access()

#define XREAL_AIR_CAMERA_TRACE(...) U_LOG_IFL_T(U_LOGGING_WARN, __VA_ARGS__)

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

const uint8_t chunk_map[128] = {
    119, 54,  21,  0,   108, 22,  51,  63,  93, 99,  67, 7,   32,  112, 52,
    43,  14,  35,  75,  116, 64,  71,  44,  89, 18,  88, 26,  61,  70,  56,
    90,  79,  87,  120, 81,  101, 121, 17,  72, 31,  53, 124, 127, 113, 111,
    36,  48,  19,  37,  83,  126, 74,  109, 5,  84,  41, 76,  30,  110, 29,
    12,  115, 28,  102, 105, 62,  103, 20,  3,  68,  49, 77,  117, 125, 106,
    60,  69,  98,  9,   16,  78,  47,  40,  2,  118, 34, 13,  50,  46,  80,
    85,  66,  42,  123, 122, 96,  11,  25,  97, 39,  6,  86,  1,   8,   82,
    92,  59,  104, 24,  15,  73,  65,  38,  58, 10,  23, 33,  55,  57,  107,
    100, 94,  27,  95,  45,  91,  4,   114};

#define CHUNK_SIZE 2400
#define CHUNK_AMOUNT 128
#define CHUNK_SUM_LEN 128

#define CAM_WIDTH 640
#define CAM_HEIGHT 480
#define CAM_HEADER_LEN 2

#define CAM_BUFFER_SIZE ((CAM_HEIGHT + CAM_HEADER_LEN) * CAM_WIDTH)
#define CAM_IMAGE_DATA_SIZE (CAM_HEIGHT * CAM_WIDTH)

struct xreal_air_camera
{
       struct os_mutex lock;

       struct xreal_air_tracker *tracker;

       struct xrt_frame_sink in_sink; // Receive raw frames and split them

       struct u_sink_debug debug_sinks[2];

       struct xrt_frame *last_left;
       struct xrt_frame *last_right;
//     rift_s_camera_report_t camera_report;

//     uint16_t last_slam_exposure, target_exposure;
//     uint8_t last_slam_gain, target_gain;

//     bool manual_control;                    //!< Whether to control exp/gain manually or with aeg
//     struct u_var_draggable_u16 exposure_ui; //! Widget to control `exposure` value
//     struct u_autoexpgain *aeg;
};

struct xreal_air_camera_finder
{
       struct xrt_fs *xfs;
       struct xrt_frame_context *xfctx;
};

static void
receive_cam_frame(struct xrt_frame_sink *sink, struct xrt_frame *xf);

static void
on_video_device(struct xrt_prober *xp,
                struct xrt_prober_device *pdev,
                const char *product,
                const char *manufacturer,
                const char *serial,
                void *ptr)
{
       struct xreal_air_camera_finder *finder = (struct xreal_air_camera_finder *)ptr;

       /* Already found a device? */
       if (finder->xfs != NULL)
               return;

       if (product == NULL || manufacturer == NULL || serial == NULL) {
               return;
       }

       printf("Inspecting video device %s - %s serial %s\n", manufacturer, product, serial);

       if ((strcmp(product, "XREAL Air 2 Ultra") == 0) && (strcmp(manufacturer, "XREAL") == 0)) {
               xrt_prober_open_video_device(xp, pdev, finder->xfctx, &finder->xfs);
               printf("Found xreal cam\n");
               return;
       }
}

struct xreal_air_camera *
xreal_air_camera_create(struct xrt_prober *xp,
                        struct xrt_frame_context *xfctx)
{
       struct xreal_air_camera_finder finder = {
           0,
       };

       DRV_TRACE_MARKER();

       /* Set up the finder with the HMD serial number and frame server context we want */
       finder.xfctx = xfctx;

       /* Re-probe devices. The v4l2 camera device should have appeared by now */
       int retry_count = 5;
       do {
               xrt_result_t xret = xrt_prober_probe(xp);

               if (xret != XRT_SUCCESS) {
                       return NULL;
               }

               xrt_prober_list_video_devices(xp, on_video_device, &finder);
               if (finder.xfs != NULL) {
                       break;
               }

               /* Sleep 1 second before retry */
               os_nanosleep((uint64_t)U_TIME_1S_IN_NS);
       } while (retry_count-- > 0);

       if (finder.xfs == NULL) {
               XREAL_AIR_CAMERA_TRACE("Didn't find Xreal Air camera device");
               return NULL;
       }

       struct xreal_air_camera *cam = U_TYPED_CALLOC(struct xreal_air_camera);

       if (os_mutex_init(&cam->lock) != 0) {
               XREAL_AIR_CAMERA_TRACE("Failed to init camera configuration mutex");
               goto cleanup;
       }

       /* Configure default camera settings */
       //rift_s_protocol_camera_report_init(&cam->camera_report);
       //cam->camera_report.uvc_enable = 0x1;
       //cam->camera_report.radio_sync_flag = 0x1;

       /* Store the defaults from the init() call into our current settings */
       //cam->last_slam_exposure = cam->camera_report.slam_frame_exposures[0];
       //cam->last_slam_gain = cam->camera_report.slam_frame_gains[0];

       //cam->target_exposure = DEFAULT_EXPOSURE;
       //cam->target_gain = DEFAULT_GAIN;

       //rift_s_camera_update(cam, hid);

       cam->in_sink.push_frame = receive_cam_frame;

       //bool enable_aeg = debug_get_bool_option_rift_s_autoexposure();
       //int frame_delay = 2; // Exposure updates take effect on the 2nd frame after sending
       //cam->aeg = u_autoexpgain_create(U_AEG_STRATEGY_TRACKING, enable_aeg, frame_delay);

       u_sink_debug_init(&cam->debug_sinks[0]);
       u_sink_debug_init(&cam->debug_sinks[1]);

       struct xrt_frame_sink *tmp = &cam->in_sink;

       struct xrt_fs_mode *modes = NULL;
       uint32_t count;

       xrt_fs_enumerate_modes(finder.xfs, &modes, &count);

       bool found_mode = false;
       uint32_t selected_mode = 0;

       for (; selected_mode < count; selected_mode++) {
               if (modes[selected_mode].format == XRT_FORMAT_YUYV422) {
                       found_mode = true;
                       break;
               }
               if (modes[selected_mode].format == XRT_FORMAT_MJPEG) {
                       u_sink_create_format_converter(xfctx, XRT_FORMAT_L8, tmp, &tmp);
                       found_mode = true;
                       break;
               }
       }

       if (!found_mode) {
               selected_mode = 0;
               XREAL_AIR_CAMERA_TRACE("Couldn't find compatible camera input format.");
               goto cleanup;
       }

       free(modes);

       u_var_add_root(cam, "Xreal Air Cameras", true);

//     u_var_add_bool(cam, &cam->manual_control, "Manual exposure and gain control");
//     cam->exposure_ui.val = &cam->target_exposure;
//     cam->exposure_ui.min = RIFT_S_MIN_EXPOSURE;
//     cam->exposure_ui.max = RIFT_S_MAX_EXPOSURE;
//     cam->exposure_ui.step = 25;

//     u_var_add_draggable_u16(cam, &cam->exposure_ui, "Exposure");
//     u_var_add_u8(cam, &cam->target_gain, "Gain");
//     u_var_add_gui_header(cam, NULL, "Auto exposure and gain control");
//     u_autoexpgain_add_vars(cam->aeg, cam, "");

       u_var_add_gui_header(cam, NULL, "Camera Streams");
       u_var_add_sink_debug(cam, &cam->debug_sinks[0], "Left cam");
       u_var_add_sink_debug(cam, &cam->debug_sinks[1], "Right cam");

       /* Finally, start the video feed */
       xrt_fs_stream_start(finder.xfs, tmp, XRT_FS_CAPTURE_TYPE_TRACKING, selected_mode);

       return cam;

cleanup:
	xreal_air_camera_destroy(cam);
	return NULL;
}

static struct xrt_frame *
descramble_image(struct xrt_frame *xf, bool *out_is_right)
{
        const uint8_t *blocks = xf->data;
        uint32_t seq;
        uint64_t ts;

        bool is_right = (blocks[CAM_IMAGE_DATA_SIZE + 0x3B] != 0);
        if (out_is_right) {
               *out_is_right = is_right;
        }

        memcpy(&seq, xf->data + CAM_IMAGE_DATA_SIZE + 18, sizeof(uint32_t));
        memcpy(&ts, xf->data + CAM_IMAGE_DATA_SIZE, sizeof(uint64_t));

       size_t best_offset = 0;
       size_t best_sum = 128 * 0xFF + 1;

       for (size_t i = 0; i < CHUNK_AMOUNT; i++) {
               size_t val = 0;
               for (size_t j = 0; j < 128; j++) {
                       val += xf->data[i * CHUNK_SIZE + j];
               }

               if (val < best_sum) {
                       best_sum = val;
                       best_offset = i;
               }
       }

       size_t map_idx = 0;
       for (size_t i = 0; i < CHUNK_AMOUNT; i++) {
               if (chunk_map[i] == best_offset) {
                       map_idx = i;
                       break;
               }
       }

       struct xrt_frame *out = NULL;
        u_frame_create_one_off(XRT_FORMAT_L8, CAM_WIDTH, CAM_HEIGHT, &out);

       uint8_t *output = out->data;
       size_t px = 0, py = 0;

        out->source_sequence = seq;
        out->source_timestamp = ts;

       for (size_t i = 0; i < CHUNK_AMOUNT; i++) {
               size_t chunk_idx = chunk_map[map_idx];
               const uint8_t *chunk = xf->data + chunk_idx * CHUNK_SIZE;

               size_t pos = 0;
               while (pos < CHUNK_SIZE) {
                       size_t row_remain = CAM_WIDTH - py;
                       size_t span = MIN(row_remain, CHUNK_SIZE - pos);
                       memcpy(output + (px * CAM_WIDTH + py), chunk + pos, span);
                       pos += span;
                       py += span;

                       if (py == CAM_WIDTH) {
                               py = 0;
                               px++;
                               if (px >= CAM_HEIGHT) {
                                       goto done;
                               }
                       }
               }

               map_idx = (map_idx + 1) % CHUNK_AMOUNT;
       }

done:
       return out;
}

static void
rotate_l8_90_cw(uint8_t *dst, const uint8_t *src, int width, int height)
{
       for (int y = 0; y < height; ++y) {
               for (int x = 0; x < width; ++x) {
                       dst[x * height + (height - 1 - y)] = src[y * width + x];
               }
       }
}

static void
rotate_l8_90_ccw(uint8_t *dst, const uint8_t *src, int width, int height)
{
       for (int y = 0; y < height; ++y) {
               for (int x = 0; x < width; ++x) {
                       dst[(width - 1 - x) * height + y] = src[y * width + x];
               }
       }
}


static void
receive_cam_frame(struct xrt_frame_sink *sink, struct xrt_frame *xf)
{
       struct xreal_air_camera *cam = container_of(sink, struct xreal_air_camera, in_sink);
       bool is_right = false;

       struct xrt_frame *descrambled = descramble_image(xf, &is_right);
       if (descrambled == NULL) {
               printf("descramble failed\n");
               return;
       }

       printf("seq: %ld\n", descrambled->source_sequence);

       assert(descrambled != NULL);
       assert(descrambled->data != NULL);
       assert(descrambled->format == XRT_FORMAT_L8);

        XREAL_AIR_CAMERA_TRACE("cam img t=%" PRIu64 " source_t=%" PRIu64 " right=%d",
                               xf->timestamp, xf->source_timestamp, is_right);

       if (!is_right) {
               struct xrt_frame *rotated = NULL;
               u_frame_create_one_off(XRT_FORMAT_L8, descrambled->height, descrambled->width, &rotated);
               rotate_l8_90_ccw(rotated->data, descrambled->data, descrambled->width, descrambled->height);
               rotated->timestamp = descrambled->timestamp;
               rotated->source_sequence = rotated->source_sequence;
               rotated->source_timestamp = rotated->source_timestamp;

               u_sink_debug_push_frame(&cam->debug_sinks[0], rotated);
               xrt_frame_reference(&cam->last_left, rotated);
               xrt_frame_reference(&rotated, NULL);
       } else {
               struct xrt_frame *rotated = NULL;
               u_frame_create_one_off(XRT_FORMAT_L8, descrambled->height, descrambled->width, &rotated);
               rotate_l8_90_cw(rotated->data, descrambled->data, descrambled->width, descrambled->height);
               rotated->timestamp = descrambled->timestamp;
               rotated->source_sequence = rotated->source_sequence;
               rotated->source_timestamp = rotated->source_timestamp;

               u_sink_debug_push_frame(&cam->debug_sinks[1], rotated);
               xrt_frame_reference(&cam->last_right, rotated);
               xrt_frame_reference(&rotated, NULL);
       }

       // Push to tracker
//     xreal_air_tracker_push_slam_frames(cam->tracker, timestamp, descrambled, is_right);
       if(cam->last_left && cam->last_right && is_right) {
               //xreal_air_tracker_push_slam_frames(cam->tracker, timestamp, cam->last_left, cam->last_right);
       }
       // Use the descrambled frame directly (already per-camera)
       // update_expgain(cam, descrambled); // If needed
       // rift_s_tracker_push_slam_frames(cam->tracker, descrambled->timestamp, &descrambled);

       xrt_frame_reference(&descrambled, NULL);
}

void xreal_air_camera_destroy(struct xreal_air_camera *camera)
{
	os_mutex_destroy(&camera->lock);

	u_sink_debug_destroy(&camera->debug_sinks[0]);
	u_sink_debug_destroy(&camera->debug_sinks[1]);

	xrt_frame_reference(&camera->last_left, NULL);
	xrt_frame_reference(&camera->last_right, NULL);
}

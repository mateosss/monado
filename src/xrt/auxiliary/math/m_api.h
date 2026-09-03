// Copyright 2019-2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  C interface to math library.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Moshi Turner <moshiturner@protonmail.com>
 * @author Nis Madsen <nima_zero_one@protonmail.com>
 *
 * @see xrt_vec3
 * @see xrt_quat
 * @see xrt_pose
 * @see xrt_space_relation
 * @ingroup aux_math
 */

#pragma once

#include "xrt/xrt_defines.h"

#include "math/m_mathinclude.h"

#ifdef __cplusplus
extern "C" {
#endif


/*!
 * @defgroup aux_math Math
 * @ingroup aux
 *
 * @brief C interface to some transform-related math functions.
 */

/*!
 * @dir auxiliary/math
 * @ingroup aux
 *
 * @brief C interface to some transform-related math functions.
 */

/*
 *
 * Defines.
 *
 */

/*!
 * Standard gravity acceleration constant.
 *
 * @ingroup aux_math
 */
#define MATH_GRAVITY_M_S2 (9.8066)

/*!
 * Minimum of A and B.
 *
 * @ingroup aux_math
 */
#ifndef MIN // Avoid clash with OpenCV def
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#endif

/*!
 * Maximum of A and B.
 *
 * @ingroup aux_math
 */
#ifndef MAX // Avoid clash with OpenCV def
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#endif

/*!
 * X clamped to the range [A, B].
 *
 * @ingroup aux_math
 */
#define CLAMP(X, A, B) (MIN(MAX((X), (A)), (B)))

/*!
 * Degrees to radians conversion.
 *
 * @ingroup aux_math
 */
// clang-format off
// @todo: Remove the clang-format off/on when we move to a newer clang-format in CI.
#define DEG_TO_RAD(DEG) ((DEG) * M_PI / 180.)
// clang-format on

/*!
 * Radians to degrees conversion.
 *
 * @ingroup aux_math
 */
// clang-format off
#define RAD_TO_DEG(RAD) ((RAD) * 180.0 / M_PI)
// clang-format on


/*
 *
 * Hash functions.
 *
 */

/*!
 * Generate a hash value from the given string, trailing zero not included.
 *
 * Hashing function used is not specified so no guarantee of staying the same
 * between different versions of the software, or even when the same version
 * is compiled on different platforms/libc++ as it might use std::hash.
 *
 * @ingroup aux_math
 */
size_t
math_hash_string(const char *str_c, size_t length);


/*
 *
 * Vector functions
 *
 */

/*!
 * Check if this vec3 is valid for math operations.
 *
 * @relates xrt_vec3
 * @ingroup aux_math
 */
bool
math_vec3_validate(const struct xrt_vec3 *vec3);

/*!
 * Accumulate a vector by adding in-place.
 *
 * Logically, *inAndOut += *additional
 * OK if the two arguments are the same addresses.
 *
 * @relates xrt_vec3
 * @ingroup aux_math
 */
void
math_vec3_accum(const struct xrt_vec3 *additional, struct xrt_vec3 *inAndOut);

/*!
 * Subtract from a vector in-place.
 *
 * Logically, *inAndOut -= *subtrahend
 * OK if the two arguments are the same addresses.
 *
 * @relates xrt_vec3
 * @ingroup aux_math
 */
void
math_vec3_subtract(const struct xrt_vec3 *subtrahend, struct xrt_vec3 *inAndOut);

/*!
 * Multiply a vector in-place.
 *
 * Logically, *inAndOut *= scalar
 *
 * @relates xrt_vec3
 * @ingroup aux_math
 */
void
math_vec3_scalar_mul(float scalar, struct xrt_vec3 *inAndOut);

/*!
 * Cross product of a vector.
 *
 * @relates xrt_vec3
 * @ingroup aux_math
 */
void
math_vec3_cross(const struct xrt_vec3 *l, const struct xrt_vec3 *r, struct xrt_vec3 *result);

/*!
 * Get translation vector from isometry matrix (col-major).
 *
 * @relates xrt_vec3
 * @ingroup aux_math
 */
void
math_vec3_translation_from_isometry(const struct xrt_matrix_4x4 *isometry, struct xrt_vec3 *result);

/*!
 * Normalize a vec3 in place.
 *
 * @relates xrt_vec3
 * @ingroup aux_math
 */
void
math_vec3_normalize(struct xrt_vec3 *in);


/*
 *
 * 64 bit vector functions.
 *
 */

/*!
 * Cross product of a vec3_f64.
 *
 * @relates xrt_vec3_f64
 * @ingroup aux_math
 */
void
math_vec3_f64_cross(const struct xrt_vec3_f64 *l, const struct xrt_vec3_f64 *r, struct xrt_vec3_f64 *result);

/*!
 * Normalize a vec3_f64 in place.
 *
 * @relates xrt_vec3_f64
 * @ingroup aux_math
 */
void
math_vec3_f64_normalize(struct xrt_vec3_f64 *in);


/*
 *
 * Quat functions.
 *
 */

/*!
 * Create a rotation from an angle in radians and a unit vector.
 *
 * @relates xrt_quat
 * @see xrt_vec3
 * @ingroup aux_math
 */
void
math_quat_from_angle_vector(float angle_rads, const struct xrt_vec3 *vector, struct xrt_quat *result);

/*!
 * Create a rotation from euler angles to a quaternion
 * @relates xrt_quat
 * @ingroup aux_math
 */
void
math_quat_from_euler_angles(const struct xrt_vec3 *angles, struct xrt_quat *result);

/*!
 * Create a rotation from a quaternion to euler angles
 * @relates xrt_quat
 * @ingroup aux_math
 */
void
math_quat_to_euler_angles(const struct xrt_quat *quat, struct xrt_vec3 *euler_angles);

/*!
 * Create a rotation from a 3x3 rotation (row major) matrix.
 *
 * @relates xrt_quat
 * @see xrt_matrix_3x3
 * @ingroup aux_math
 */
void
math_quat_from_matrix_3x3(const struct xrt_matrix_3x3 *mat, struct xrt_quat *result);

/*!
 * Create a rotation from two vectors plus x and z, by creating a rotation
 * matrix by crossing z and x to get the y axis.
 *
 * Input vectors should be normalized.
 *
 * @relates xrt_quat
 * @see xrt_vec3
 * @ingroup aux_math
 */
void
math_quat_from_plus_x_z(const struct xrt_vec3 *plus_x, const struct xrt_vec3 *plus_z, struct xrt_quat *result);

/*!
 * Create a rotation from two vectors vec_a and vec_b that would
 * rotate vec_a into vec_b
 *
 * @relates xrt_quat
 * @see xrt_vec3
 * @ingroup aux_math
 */
void
math_quat_from_vec_a_to_vec_b(const struct xrt_vec3 *vec_a, const struct xrt_vec3 *vec_b, struct xrt_quat *result);

/*!
 * Check if this quat can be used in transformation operations.
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
bool
math_quat_validate(const struct xrt_quat *quat);

/*!
 * Check if this quat is (approximately) identity.
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
bool
math_quat_is_identity(const struct xrt_quat *quat, float epsilon);

/*!
 * Check if this quat is within 1% of unit length.
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
bool
math_quat_validate_within_1_percent(const struct xrt_quat *quat);

/*!
 * Invert a quaternion.
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
void
math_quat_invert(const struct xrt_quat *quat, struct xrt_quat *out_quat);

/*!
 * The euclidean norm or length of a quaternion. Same as if it were a vec4.
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
float
math_quat_len(const struct xrt_quat *quat);

/*!
 * The dot product of 2 quaternions. It has a analogous interpretation
 * as for vec3. For unit quaternions, it provides cos(theta) of the
 * angle between the 2 quaternion rotations.
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
static inline float
math_quat_dot(const struct xrt_quat *l, const struct xrt_quat *r)
{
	return l->x * r->x + l->y * r->y + l->z * r->z + l->w * r->w;
}

/*!
 * Normalize a quaternion.
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
void
math_quat_normalize(struct xrt_quat *inout);

/*!
 * Normalizes a quaternion if it has accumulated float precision errors.
 * Returns true if the quaternion was already normalized or was normalized after
 * being found within a small float precision tolerance.
 * Returns false if the quaternion was not at all normalized.
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
bool
math_quat_ensure_normalized(struct xrt_quat *inout);

/*!
 * Rotate a vector.
 *
 * @relates xrt_quat
 * @see xrt_vec3
 * @ingroup aux_math
 */
void
math_quat_rotate_vec3(const struct xrt_quat *left, const struct xrt_vec3 *right, struct xrt_vec3 *result);

/*!
 * Rotate a quaternion (compose rotations).
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
void
math_quat_rotate(const struct xrt_quat *left, const struct xrt_quat *right, struct xrt_quat *result);

/*!
 * Inverse of @ref math_quat_rotate. Removes @p left rotation from @p right.
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
void
math_quat_unrotate(const struct xrt_quat *left, const struct xrt_quat *right, struct xrt_quat *result);

/*!
 * Integrate a local angular velocity vector (exponential map) and apply to a
 * quaternion.
 *
 * ang_vel and dt should share the same units of time, and the ang_vel
 * vector should be in radians per unit of time.
 *
 * @relates xrt_quat
 * @see xrt_vec3
 * @ingroup aux_math
 */
void
math_quat_integrate_velocity(const struct xrt_quat *quat,
                             const struct xrt_vec3 *ang_vel,
                             float dt,
                             struct xrt_quat *result);

/*!
 * Compute a global angular velocity vector (exponential map format) by taking
 * the finite difference of two quaternions.
 *
 * quat1 is the orientation dt time after the orientation was quat0
 *
 * out_ang_vel and dt share the same units of time, and out_ang_vel is be in
 * radians per unit of time.
 *
 * @relates xrt_quat
 * @see xrt_vec3
 * @ingroup aux_math
 */
void
math_quat_finite_difference(const struct xrt_quat *quat0,
                            const struct xrt_quat *quat1,
                            float dt,
                            struct xrt_vec3 *out_ang_vel);

/*!
 * Takes a rotation vector equal to half of a Rodrigues rotation vector and returns its corresponding unit quaternion.
 * Useful for head tracking and pose-prediction.
 *
 * @relates xrt_quat
 * @see xrt_vec3
 * @ingroup aux_math
 */
void
math_quat_exp(const struct xrt_vec3 *axis_angle, struct xrt_quat *out_quat);


/*!
 * Takes a unit quaternion and returns a rotation vector equal to half of its corresponding Rodrigues rotation vector.
 * Useful for head tracking and pose-prediction.
 *
 * @relates xrt_quat
 * @see xrt_vec3
 * @ingroup aux_math
 */
void
math_quat_ln(const struct xrt_quat *quat, struct xrt_vec3 *out_axis_angle);

/*!
 * Used to rotate a derivative like a angular velocity.
 *
 * @relates xrt_quat
 * @see xrt_vec3
 * @ingroup aux_math
 */
void
math_quat_rotate_derivative(const struct xrt_quat *quat, const struct xrt_vec3 *deriv, struct xrt_vec3 *result);


/*!
 * Slerp (spherical linear interpolation) between two quaternions
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
void
math_quat_slerp(const struct xrt_quat *left, const struct xrt_quat *right, float t, struct xrt_quat *result);


/*!
 * Converts a 2D vector to a quaternion
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
void
math_quat_from_swing(const struct xrt_vec2 *swing, struct xrt_quat *result);


/*!
 * Converts a 2D vector and a float to a quaternion
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
void
math_quat_from_swing_twist(const struct xrt_vec2 *swing, const float twist, struct xrt_quat *result);

/*!
 * Converts a quaternion to XY-swing and Z-twist
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
void
math_quat_to_swing_twist(const struct xrt_quat *in, struct xrt_vec2 *out_swing, float *out_twist);

/*!
 * Decompose a quaternion to swing and twist component rotations around a target
 * axis. The swing is always orthogonal to the target axis, and twist rotation is always
 * around the axis.
 *
 * swing * twist gives back the original quat
 * (e.g. math_quat_rotate(&swing, &twist, &orig_q))
 *
 * See https://arxiv.org/pdf/1506.05481.pdf
 *
 * @relates xrt_quat
 * @ingroup aux_math
 */
void
math_quat_decompose_swing_twist(const struct xrt_quat *in,
                                const struct xrt_vec3 *twist_axis,
                                struct xrt_quat *swing,
                                struct xrt_quat *twist);

/*
 *
 * Matrix functions
 *
 */

/*!
 * Initialize a 3x3 matrix to the identity matrix
 *
 * @see xrt_matrix_3x3
 * @ingroup aux_math
 */
void
math_matrix_3x3_identity(struct xrt_matrix_3x3 *mat);

/*!
 * Initialize a 3x3 matrix from a quaternion
 *
 * @see xrt_matrix_3x3
 * @ingroup aux_math
 */
void
math_matrix_3x3_from_quat(const struct xrt_quat *q, struct xrt_matrix_3x3 *result_out);

/*!
 * Initialize a double 3x3 matrix to the identity matrix
 *
 * @see xrt_matrix_3x3
 * @ingroup aux_math
 */
void
math_matrix_3x3_f64_identity(struct xrt_matrix_3x3_f64 *mat);

/*!
 * Transform a vec3 by a 3x3 matrix
 *
 * @see xrt_matrix_3x3
 * @ingroup aux_math
 */
void
math_matrix_3x3_transform_vec3(const struct xrt_matrix_3x3 *left,
                               const struct xrt_vec3 *right,
                               struct xrt_vec3 *result_out);

/*!
 * Transform a vec3 by a 4x4 matrix, extending the vector with w = 1.0
 *
 * @see xrt_matrix_4x4
 * @ingroup aux_math
 */
void
math_matrix_4x4_transform_vec3(const struct xrt_matrix_4x4 *left,
                               const struct xrt_vec3 *right,
                               struct xrt_vec3 *result_out);

/*!
 * Transform a double vec3 by a 3x3 double matrix
 *
 * @see xrt_matrix_3x3
 * @ingroup aux_math
 */
void
math_matrix_3x3_f64_transform_vec3_f64(const struct xrt_matrix_3x3_f64 *left,
                                       const struct xrt_vec3_f64 *right,
                                       struct xrt_vec3_f64 *result_out);

/*!
 * Multiply Matrix3x3.
 *
 * @relates xrt_matrix_3x3
 * @ingroup aux_math
 */
void
math_matrix_3x3_multiply(const struct xrt_matrix_3x3 *left,
                         const struct xrt_matrix_3x3 *right,
                         struct xrt_matrix_3x3 *result_out);

/*!
 * Invert Matrix3x3
 *
 * @relates xrt_matrix_3x3
 * @ingroup aux_math
 */
void
math_matrix_3x3_inverse(const struct xrt_matrix_3x3 *in, struct xrt_matrix_3x3 *result);

/*!
 * Transpose Matrix3x3
 *
 * @relates xrt_matrix_3x3
 * @ingroup aux_math
 */
void
math_matrix_3x3_transpose(const struct xrt_matrix_3x3 *in, struct xrt_matrix_3x3 *result);

/*!
 * Create a rotation from two vectors plus x and z, by
 * creating a rotation matrix by crossing z and x to
 * get the y axis.
 *
 * Input vectors should be normalized.
 *
 * @relates xrt_matrix_3x3
 * @ingroup aux_math
 */
void
math_matrix_3x3_f64_from_plus_x_z(const struct xrt_vec3_f64 *plus_x,
                                  const struct xrt_vec3_f64 *plus_z,
                                  struct xrt_matrix_3x3_f64 *result);

/*!
 * Get the rotation matrix from an isomertry matrix (col-major).
 *
 * @relates xrt_matrix_4x4
 * @ingroup aux_math
 */
void
math_matrix_3x3_rotation_from_isometry(const struct xrt_matrix_4x4 *isometry, struct xrt_matrix_3x3 *result);

/*!
 * Initialize Matrix4x4 with identity.
 *
 * @relates xrt_matrix_4x4
 * @ingroup aux_math
 */
void
math_matrix_4x4_identity(struct xrt_matrix_4x4 *result);

/*!
 * Multiply Matrix4x4.
 *
 * @relates xrt_matrix_4x4
 * @ingroup aux_math
 */
void
math_matrix_4x4_multiply(const struct xrt_matrix_4x4 *left,
                         const struct xrt_matrix_4x4 *right,
                         struct xrt_matrix_4x4 *result);

/*!
 * Invert Matrix4x4.
 *
 * @relates xrt_matrix_4x4
 * @ingroup aux_math
 */
void
math_matrix_4x4_inverse(const struct xrt_matrix_4x4 *in, struct xrt_matrix_4x4 *result);

/*!
 * Invert a homogeneous isometry 4x4 (col-major) matrix in SE(3).
 *
 * @relates xrt_matrix_4x4
 * @ingroup aux_math
 */
void
math_matrix_4x4_isometry_inverse(const struct xrt_matrix_4x4 *in, struct xrt_matrix_4x4 *result);

/*!
 * Transpose Matrix4x4
 *
 * @relates xrt_matrix_4x4
 * @ingroup aux_math
 */
void
math_matrix_4x4_transpose(const struct xrt_matrix_4x4 *in, struct xrt_matrix_4x4 *result);

/*!
 * Compute view matrix from xrt_pose.
 *
 * @relates xrt_matrix_4x4
 * @ingroup aux_math
 */
void
math_matrix_4x4_view_from_pose(const struct xrt_pose *pose, struct xrt_matrix_4x4 *result);

/*!
 * Get an isometry matrix —in SE(3)— from a rotation matrix —SO(3)— and a
 * translation vector. All col-major matrices.
 *
 * @relates xrt_matrix_4x4
 * @ingroup aux_math
 */
void
math_matrix_4x4_isometry_from_rt(const struct xrt_matrix_3x3 *rotation,
                                 const struct xrt_vec3 *translation,
                                 struct xrt_matrix_4x4 *result);

/*!
 * Get a col-major isometry matrix —in SE(3)— from a pose.
 *
 * @relates xrt_matrix_4x4
 * @ingroup aux_math
 */
void
math_matrix_4x4_isometry_from_pose(const struct xrt_pose *pose, struct xrt_matrix_4x4 *result);

/*!
 * Compute quad layer model matrix from xrt_pose and xrt_vec2 size.
 *
 * @relates xrt_matrix_4x4
 * @ingroup aux_math
 */
void
math_matrix_4x4_model(const struct xrt_pose *pose, const struct xrt_vec3 *size, struct xrt_matrix_4x4 *result);

/*!
 * Compute inverse view projection matrix,
 * using only the starting 3x3 block of the view.
 *
 * @relates xrt_matrix_4x4
 * @ingroup aux_math
 */
void
math_matrix_4x4_inverse_view_projection(const struct xrt_matrix_4x4 *view,
                                        const struct xrt_matrix_4x4 *projection,
                                        struct xrt_matrix_4x4 *result);

/*!
 * Compute a projection matrix with settings for Vulkan, it will also have it's
 * far plane at infinite and the NDC depth will be reversed.
 *
 * @relates xrt_matrix_4x4
 * @ingroup aux_math
 */
void
math_matrix_4x4_projection_vulkan_infinite_reverse(const struct xrt_fov *fov,
                                                   float near_plane,
                                                   struct xrt_matrix_4x4 *result);


void
math_matrix_4x4_projection(const struct xrt_fov fov,
						   float near_plane,
					 	   float far_plane,
						   struct xrt_matrix_4x4 *result);

/*
 *
 * Pose functions.
 *
 */


/*!
 * Somewhat laboriously make an xrt_pose identity.
 *
 * @relates xrt_pose
 * @ingroup aux_math
 */
void
math_pose_identity(struct xrt_pose *pose);

/*!
 * Check if this pose can be used in transformation operations.
 *
 * @relates xrt_pose
 * @ingroup aux_math
 */
bool
math_pose_validate(const struct xrt_pose *pose);

/*!
 * Invert pose.
 *
 * OK if input and output are the same addresses.
 *
 * @relates xrt_pose
 * @ingroup aux_math
 */
void
math_pose_invert(const struct xrt_pose *pose, struct xrt_pose *outPose);

/*!
 * Converts a (col-major) isometry into a pose.
 *
 * @relates xrt_pose
 * @ingroup aux_math
 */
void
math_pose_from_isometry(const struct xrt_matrix_4x4 *transform, struct xrt_pose *result);

/*!
 * Interpolated pose between poses `a` and `b` by lerping position and slerping
 * orientation by t.
 *
 * @relates xrt_pose
 * @ingroup aux_math
 */
void
math_pose_interpolate(const struct xrt_pose *a, const struct xrt_pose *b, float t, struct xrt_pose *outPose);

/*!
 * Apply a rigid-body transformation to a pose.
 *
 * OK if input and output are the same addresses.
 *
 * @relates xrt_pose
 * @ingroup aux_math
 */
void
math_pose_transform(const struct xrt_pose *transform, const struct xrt_pose *pose, struct xrt_pose *outPose);

/*!
 * Apply a rigid-body transformation to a point.
 *
 * The input point and output may be the same pointer.
 *
 * @relates xrt_pose
 * @see xrt_vec3
 * @ingroup aux_math
 */
void
math_pose_transform_point(const struct xrt_pose *transform, const struct xrt_vec3 *point, struct xrt_vec3 *out_point);

/*!
 * Convert a pose from the OpenCV coordinate system to the OpenXR coordinate system. OpenCV camera space coordinates has
 * +Y down and +Z away from the user.
 *
 * The input and output may be the same pointer.
 *
 * @relates xrt_pose
 * @ingroup aux_math
 */
void
math_pose_convert_opencv(const struct xrt_pose *in, struct xrt_pose *out);


/*
 *
 * Inline functions.
 *
 */

/*!
 * Map a number from one range to another range.
 * Exactly the same as Arduino's map().
 */
static inline double
math_map_ranges(double value, double from_low, double from_high, double to_low, double to_high)
{
	return (value - from_low) * (to_high - to_low) / (from_high - from_low) + to_low;
}

static inline double
math_lerp(double from, double to, double amount)
{
	return (from * (1.0 - amount)) + (to * (amount));
}

/*
 *
 * Optics functions.
 *
 */

/*!
 * Perform the computations from
 * "Computing Half-Fields-Of-View from Simpler Display Models",
 * to get half-FOVs from things we can retrieve from other APIs.
 * The origin is in the lower-left corner of the display, so w_1 is the width to
 * the left of CoP, and h_1 is the height below CoP.
 *
 * If vertfov_total is set to 0, it will be computed from h_total.
 *
 * Distances are in arbitrary but consistent units. Angles are in radians.
 *
 *
 * In the diagram below, treating it like a FOV for horizontal,
 * the top angle is horizfov_total, the length of the bottom
 * is w_total, and the distance between the vertical line and the left corner is
 * w_1. Vertical is similar - h_1 is above the center line.
 * The triangle need not be symmetrical, despite how the diagram looks.
 *
 * ```
 *               horizfov_total
 *                       *
 * angle_left (neg) -> / |  \ <- angle_right
 *                    /  |   \
 *                   /   |    \
 *                  /    |     \
 *                 -------------
 *                 [ w_1 ]
 *                 [ --- w  --- ]
 *
 * -------     --- |\
 *                 |   \
 *             h_1 |      \ angle_up
 * h_total     ___ |-------* vertfov_total
 *                 |      / angle_down (neg)
 *                 |    /
 *                 |  /
 * -------         |/
 * ```
 *
 * @return true if successful.
 * @ingroup aux_math
 */
bool
math_compute_fovs(double w_total,
                  double w_1,
                  double horizfov_total,
                  double h_total,
                  double h_1,
                  double vertfov_total,
                  struct xrt_fov *fov);

/*!
 * Compute the FOV to use when parallelizing canted views.
 *
 * Some applications do not support rendering for view orientations that are
 * not parallel to each other.
 * When using a headset with physically canted displays, such applications
 * require parallelizing the views, i.e. forcing the orientations of the views
 * to be parallel.
 * When the application passes content rendered for parallel views to the
 * compositor, the compositor typically reprojects this content such that it
 * matches the physical canting of the displays, effectively rotating the view
 * orientation.
 *
 * When rotating the view orientation, parts of the FOV are cut off on the side
 * the view rotates away from and parts of previously unseen content is pulled
 * in from the side the view rotates towards. Therefore, when parallezing
 * views, the application should render with an adjusted FOV that covers the
 * area that will be in the FOV of the view *after* the compositor reprojects
 * it back to the physical canted orientation.
 *
 * @ingroup aux_math
 */
void
math_compute_parallelized_fov(const struct xrt_fov *fov,
                              const struct xrt_quat *canted_view_orientation,
                              struct xrt_fov *out_parallelized_fov);

#ifdef __cplusplus
}
#endif

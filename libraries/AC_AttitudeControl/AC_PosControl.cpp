#include <AP_HAL/AP_HAL.h>
#include "AC_PosControl.h"
#include <AP_Math/AP_Math.h>
#include <AP_Logger/AP_Logger.h>
#include <AP_Motors/AP_Motors.h>    // motors library
#include <AP_Vehicle/AP_Vehicle_Type.h>
#include <AP_Scheduler/AP_Scheduler.h>

extern const AP_HAL::HAL& hal;

#if APM_BUILD_TYPE(APM_BUILD_ArduPlane)
 // default gains for Plane
 # define POSCONTROL_POS_Z_P                    1.0f    // vertical position controller P gain default
 # define POSCONTROL_VEL_Z_P                    5.0f    // vertical velocity controller P gain default
 # define POSCONTROL_VEL_Z_IMAX                 1000.0f // vertical velocity controller IMAX gain default
 # define POSCONTROL_VEL_Z_FILT_HZ              5.0f    // vertical velocity controller input filter
 # define POSCONTROL_VEL_Z_FILT_D_HZ            5.0f    // vertical velocity controller input filter for D
 # define POSCONTROL_ACC_Z_P                    0.3f    // vertical acceleration controller P gain default
 # define POSCONTROL_ACC_Z_I                    1.0f    // vertical acceleration controller I gain default
 # define POSCONTROL_ACC_Z_D                    0.0f    // vertical acceleration controller D gain default
 # define POSCONTROL_ACC_Z_IMAX                 800     // vertical acceleration controller IMAX gain default
 # define POSCONTROL_ACC_Z_FILT_HZ              10.0f   // vertical acceleration controller input filter default
 # define POSCONTROL_ACC_Z_DT                   0.02f   // vertical acceleration controller dt default
 # define POSCONTROL_POS_XY_P                   0.5f    // horizontal position controller P gain default
 # define POSCONTROL_VEL_XY_P                   0.7f    // horizontal velocity controller P gain default
 # define POSCONTROL_VEL_XY_I                   0.35f    // horizontal velocity controller I gain default
 # define POSCONTROL_VEL_XY_D                   0.17f   // horizontal velocity controller D gain default
 # define POSCONTROL_VEL_XY_IMAX                1000.0f // horizontal velocity controller IMAX gain default
 # define POSCONTROL_VEL_XY_FILT_HZ             5.0f    // horizontal velocity controller input filter
 # define POSCONTROL_VEL_XY_FILT_D_HZ           5.0f    // horizontal velocity controller input filter for D
#elif APM_BUILD_TYPE(APM_BUILD_ArduSub)
 // default gains for Sub
 # define POSCONTROL_POS_Z_P                    3.0f    // vertical position controller P gain default
 # define POSCONTROL_VEL_Z_P                    8.0f    // vertical velocity controller P gain default
 # define POSCONTROL_VEL_Z_IMAX                 1000.0f // vertical velocity controller IMAX gain default
 # define POSCONTROL_VEL_Z_FILT_HZ              5.0f    // vertical velocity controller input filter
 # define POSCONTROL_VEL_Z_FILT_D_HZ            5.0f    // vertical velocity controller input filter for D
 # define POSCONTROL_ACC_Z_P                    0.5f    // vertical acceleration controller P gain default
 # define POSCONTROL_ACC_Z_I                    0.1f    // vertical acceleration controller I gain default
 # define POSCONTROL_ACC_Z_D                    0.0f    // vertical acceleration controller D gain default
 # define POSCONTROL_ACC_Z_IMAX                 100     // vertical acceleration controller IMAX gain default
 # define POSCONTROL_ACC_Z_FILT_HZ              20.0f   // vertical acceleration controller input filter default
 # define POSCONTROL_ACC_Z_DT                   0.0025f // vertical acceleration controller dt default
 # define POSCONTROL_POS_XY_P                   1.0f    // horizontal position controller P gain default
 # define POSCONTROL_VEL_XY_P                   1.0f    // horizontal velocity controller P gain default
 # define POSCONTROL_VEL_XY_I                   0.5f    // horizontal velocity controller I gain default
 # define POSCONTROL_VEL_XY_D                   0.0f    // horizontal velocity controller D gain default
 # define POSCONTROL_VEL_XY_IMAX                1000.0f // horizontal velocity controller IMAX gain default
 # define POSCONTROL_VEL_XY_FILT_HZ             5.0f    // horizontal velocity controller input filter
 # define POSCONTROL_VEL_XY_FILT_D_HZ           5.0f    // horizontal velocity controller input filter for D
#else
 // default gains for Copter / TradHeli
 # define POSCONTROL_POS_Z_P                    1.0f    // vertical position controller P gain default
 # define POSCONTROL_VEL_Z_P                    5.0f    // vertical velocity controller P gain default
 # define POSCONTROL_VEL_Z_IMAX                 1000.0f // vertical velocity controller IMAX gain default
 # define POSCONTROL_VEL_Z_FILT_HZ              5.0f    // vertical velocity controller input filter
 # define POSCONTROL_VEL_Z_FILT_D_HZ            5.0f    // vertical velocity controller input filter for D
 # define POSCONTROL_ACC_Z_P                    0.5f    // vertical acceleration controller P gain default
 # define POSCONTROL_ACC_Z_I                    1.0f    // vertical acceleration controller I gain default
 # define POSCONTROL_ACC_Z_D                    0.0f    // vertical acceleration controller D gain default
 # define POSCONTROL_ACC_Z_IMAX                 800     // vertical acceleration controller IMAX gain default
 # define POSCONTROL_ACC_Z_FILT_HZ              20.0f   // vertical acceleration controller input filter default
 # define POSCONTROL_ACC_Z_DT                   0.0025f // vertical acceleration controller dt default
 # define POSCONTROL_POS_XY_P                   1.0f    // horizontal position controller P gain default
 # define POSCONTROL_VEL_XY_P                   2.0f    // horizontal velocity controller P gain default
 # define POSCONTROL_VEL_XY_I                   1.0f    // horizontal velocity controller I gain default
 # define POSCONTROL_VEL_XY_D                   0.25f   // horizontal velocity controller D gain default
 # define POSCONTROL_VEL_XY_IMAX                1000.0f // horizontal velocity controller IMAX gain default
 # define POSCONTROL_VEL_XY_FILT_HZ             5.0f    // horizontal velocity controller input filter
 # define POSCONTROL_VEL_XY_FILT_D_HZ           5.0f    // horizontal velocity controller input filter for D
#endif

// vibration compensation gains
#define POSCONTROL_VIBE_COMP_P_GAIN 0.250f
#define POSCONTROL_VIBE_COMP_I_GAIN 0.125f

// velocity offset targets timeout if not updated within 3 seconds
#define POSCONTROL_POSVELACCEL_OFFSET_TARGET_TIMEOUT_MS 3000

AC_PosControl *AC_PosControl::_singleton;

const AP_Param::GroupInfo AC_PosControl::var_info[] = {
    // 0 was used for HOVER

    // @Param: _ACC_XY_FILT
    // @DisplayName: XY Acceleration filter cutoff frequency
    // @Description: Lower values will slow the response of the navigation controller and reduce twitchiness
    // @Units: Hz
    // @Range: 0.5 5
    // @Increment: 0.1
    // @User: Advanced

    // @Param: _POSZ_P
    // @DisplayName: Position (vertical) controller P gain
    // @Description: Position (vertical) controller P gain.  Converts the difference between the desired altitude and actual altitude into a climb or descent rate which is passed to the throttle rate controller
    // @Range: 1.000 3.000
    // @User: Standard
    AP_SUBGROUPINFO(_p_pos_z, "_POSZ_", 2, AC_PosControl, AC_P_1D),

    // @Param: _VELZ_P
    // @DisplayName: Velocity (vertical) controller P gain
    // @Description: Velocity (vertical) controller P gain.  Converts the difference between desired vertical speed and actual speed into a desired acceleration that is passed to the throttle acceleration controller
    // @Range: 1.000 8.000
    // @User: Standard

    // @Param: _VELZ_I
    // @DisplayName: Velocity (vertical) controller I gain
    // @Description: Velocity (vertical) controller I gain.  Corrects long-term difference in desired velocity to a target acceleration
    // @Range: 0.02 1.00
    // @Increment: 0.01
    // @User: Advanced

    // @Param: _VELZ_IMAX
    // @DisplayName: Velocity (vertical) controller I gain maximum
    // @Description: Velocity (vertical) controller I gain maximum.  Constrains the target acceleration that the I gain will output
    // @Range: 1.000 8.000
    // @User: Standard

    // @Param: _VELZ_D
    // @DisplayName: Velocity (vertical) controller D gain
    // @Description: Velocity (vertical) controller D gain.  Corrects short-term changes in velocity
    // @Range: 0.00 1.00
    // @Increment: 0.001
    // @User: Advanced

    // @Param: _VELZ_FF
    // @DisplayName: Velocity (vertical) controller Feed Forward gain
    // @Description: Velocity (vertical) controller Feed Forward gain.  Produces an output that is proportional to the magnitude of the target
    // @Range: 0 1
    // @Increment: 0.01
    // @User: Advanced

    // @Param: _VELZ_FLTE
    // @DisplayName: Velocity (vertical) error filter
    // @Description: Velocity (vertical) error filter.  This filter (in Hz) is applied to the input for P and I terms
    // @Range: 0 100
    // @Units: Hz
    // @User: Advanced

    // @Param: _VELZ_FLTD
    // @DisplayName: Velocity (vertical) input filter for D term
    // @Description: Velocity (vertical) input filter for D term.  This filter (in Hz) is applied to the input for D terms
    // @Range: 0 100
    // @Units: Hz
    // @User: Advanced
    AP_SUBGROUPINFO(_pid_vel_z, "_VELZ_", 3, AC_PosControl, AC_PID_Basic),

    // @Param: _ACCZ_P
    // @DisplayName: Acceleration (vertical) controller P gain
    // @Description: Acceleration (vertical) controller P gain.  Converts the difference between desired vertical acceleration and actual acceleration into a motor output
    // @Range: 0.200 1.500
    // @Increment: 0.05
    // @User: Standard

    // @Param: _ACCZ_I
    // @DisplayName: Acceleration (vertical) controller I gain
    // @Description: Acceleration (vertical) controller I gain.  Corrects long-term difference in desired vertical acceleration and actual acceleration
    // @Range: 0.000 3.000
    // @User: Standard

    // @Param: _ACCZ_IMAX
    // @DisplayName: Acceleration (vertical) controller I gain maximum
    // @Description: Acceleration (vertical) controller I gain maximum.  Constrains the maximum pwm that the I term will generate
    // @Range: 0 1000
    // @Units: d%
    // @User: Standard

    // @Param: _ACCZ_D
    // @DisplayName: Acceleration (vertical) controller D gain
    // @Description: Acceleration (vertical) controller D gain.  Compensates for short-term change in desired vertical acceleration vs actual acceleration
    // @Range: 0.000 0.400
    // @User: Standard

    // @Param: _ACCZ_FF
    // @DisplayName: Acceleration (vertical) controller feed forward
    // @Description: Acceleration (vertical) controller feed forward
    // @Range: 0 0.5
    // @Increment: 0.001
    // @User: Standard

    // @Param: _ACCZ_FLTT
    // @DisplayName: Acceleration (vertical) controller target frequency in Hz
    // @Description: Acceleration (vertical) controller target frequency in Hz
    // @Range: 1 50
    // @Increment: 1
    // @Units: Hz
    // @User: Standard

    // @Param: _ACCZ_FLTE
    // @DisplayName: Acceleration (vertical) controller error frequency in Hz
    // @Description: Acceleration (vertical) controller error frequency in Hz
    // @Range: 1 100
    // @Increment: 1
    // @Units: Hz
    // @User: Standard

    // @Param: _ACCZ_FLTD
    // @DisplayName: Acceleration (vertical) controller derivative frequency in Hz
    // @Description: Acceleration (vertical) controller derivative frequency in Hz
    // @Range: 1 100
    // @Increment: 1
    // @Units: Hz
    // @User: Standard

    // @Param: _ACCZ_SMAX
    // @DisplayName: Accel (vertical) slew rate limit
    // @Description: Sets an upper limit on the slew rate produced by the combined P and D gains. If the amplitude of the control action produced by the rate feedback exceeds this value, then the D+P gain is reduced to respect the limit. This limits the amplitude of high frequency oscillations caused by an excessive gain. The limit should be set to no more than 25% of the actuators maximum slew rate to allow for load effects. Note: The gain will not be reduced to less than 10% of the nominal value. A value of zero will disable this feature.
    // @Range: 0 200
    // @Increment: 0.5
    // @User: Advanced

    // @Param: _ACCZ_PDMX
    // @DisplayName: Acceleration (vertical) controller PD sum maximum
    // @Description: Acceleration (vertical) controller PD sum maximum.  The maximum/minimum value that the sum of the P and D term can output
    // @Range: 0 1000
    // @Units: d%

    // @Param: _ACCZ_D_FF
    // @DisplayName: Accel (vertical) Derivative FeedForward Gain
    // @Description: FF D Gain which produces an output that is proportional to the rate of change of the target
    // @Range: 0 0.02
    // @Increment: 0.0001
    // @User: Advanced

    // @Param: _ACCZ_NTF
    // @DisplayName: Accel (vertical) Target notch filter index
    // @Description: Accel (vertical) Target notch filter index
    // @Range: 1 8
    // @User: Advanced

    // @Param: _ACCZ_NEF
    // @DisplayName: Accel (vertical) Error notch filter index
    // @Description: Accel (vertical) Error notch filter index
    // @Range: 1 8
    // @User: Advanced

    AP_SUBGROUPINFO(_pid_accel_z, "_ACCZ_", 4, AC_PosControl, AC_PID),

    // @Param: _POSXY_P
    // @DisplayName: Position (horizontal) controller P gain
    // @Description: Position controller P gain.  Converts the distance (in the latitude direction) to the target location into a desired speed which is then passed to the loiter latitude rate controller
    // @Range: 0.500 2.000
    // @User: Standard
    AP_SUBGROUPINFO(_p_pos_xy, "_POSXY_", 5, AC_PosControl, AC_P_2D),

    // @Param: _VELXY_P
    // @DisplayName: Velocity (horizontal) P gain
    // @Description: Velocity (horizontal) P gain.  Converts the difference between desired and actual velocity to a target acceleration
    // @Range: 0.1 6.0
    // @Increment: 0.1
    // @User: Advanced

    // @Param: _VELXY_I
    // @DisplayName: Velocity (horizontal) I gain
    // @Description: Velocity (horizontal) I gain.  Corrects long-term difference between desired and actual velocity to a target acceleration
    // @Range: 0.02 1.00
    // @Increment: 0.01
    // @User: Advanced

    // @Param: _VELXY_D
    // @DisplayName: Velocity (horizontal) D gain
    // @Description: Velocity (horizontal) D gain.  Corrects short-term changes in velocity
    // @Range: 0.00 1.00
    // @Increment: 0.001
    // @User: Advanced

    // @Param: _VELXY_IMAX
    // @DisplayName: Velocity (horizontal) integrator maximum
    // @Description: Velocity (horizontal) integrator maximum.  Constrains the target acceleration that the I gain will output
    // @Range: 0 4500
    // @Increment: 10
    // @Units: cm/s/s
    // @User: Advanced

    // @Param: _VELXY_FLTE
    // @DisplayName: Velocity (horizontal) input filter
    // @Description: Velocity (horizontal) input filter.  This filter (in Hz) is applied to the input for P and I terms
    // @Range: 0 100
    // @Units: Hz
    // @User: Advanced

    // @Param: _VELXY_FLTD
    // @DisplayName: Velocity (horizontal) input filter
    // @Description: Velocity (horizontal) input filter.  This filter (in Hz) is applied to the input for D term
    // @Range: 0 100
    // @Units: Hz
    // @User: Advanced

    // @Param: _VELXY_FF
    // @DisplayName: Velocity (horizontal) feed forward gain
    // @Description: Velocity (horizontal) feed forward gain.  Converts the difference between desired velocity to a target acceleration
    // @Range: 0 6
    // @Increment: 0.01
    // @User: Advanced
    AP_SUBGROUPINFO(_pid_vel_xy, "_VELXY_", 6, AC_PosControl, AC_PID_2D),

    // @Param: _ANGLE_MAX
    // @DisplayName: Position Control Angle Max
    // @Description: Maximum lean angle autopilot can request.  Set to zero to use ANGLE_MAX parameter value
    // @Units: deg
    // @Range: 0 45
    // @Increment: 1
    // @User: Advanced
    AP_GROUPINFO("_ANGLE_MAX", 7, AC_PosControl, _lean_angle_max, 0.0f),

    // IDs 8,9 used for _TC_XY and _TC_Z in beta release candidate

    // @Param: _JERK_XY
    // @DisplayName: Jerk limit for the horizontal kinematic input shaping
    // @Description: Jerk limit of the horizontal kinematic path generation used to determine how quickly the aircraft varies the acceleration target
    // @Units: m/s/s/s
    // @Range: 1 20
    // @Increment: 1
    // @User: Advanced
    AP_GROUPINFO("_JERK_XY", 10, AC_PosControl, _shaping_jerk_xy, POSCONTROL_JERK_XY),

    // @Param: _JERK_Z
    // @DisplayName: Jerk limit for the vertical kinematic input shaping
    // @Description: Jerk limit of the vertical kinematic path generation used to determine how quickly the aircraft varies the acceleration target
    // @Units: m/s/s/s
    // @Range: 5 50
    // @Increment: 1
    // @User: Advanced
    AP_GROUPINFO("_JERK_Z", 11, AC_PosControl, _shaping_jerk_z, POSCONTROL_JERK_Z),

    AP_GROUPEND
};

// Default constructor.
// Note that the Vector/Matrix constructors already implicitly zero
// their values.
//
AC_PosControl::AC_PosControl(AP_AHRS_View& ahrs, const AP_InertialNav& inav,
                             const AP_Motors& motors, AC_AttitudeControl& attitude_control) :
    _ahrs(ahrs),
    _inav(inav),
    _motors(motors),
    _attitude_control(attitude_control),
    _p_pos_xy(POSCONTROL_POS_XY_P),
    _p_pos_z(POSCONTROL_POS_Z_P),
    _pid_vel_xy(POSCONTROL_VEL_XY_P, POSCONTROL_VEL_XY_I, POSCONTROL_VEL_XY_D, 0.0f, POSCONTROL_VEL_XY_IMAX, POSCONTROL_VEL_XY_FILT_HZ, POSCONTROL_VEL_XY_FILT_D_HZ),
    _pid_vel_z(POSCONTROL_VEL_Z_P, 0.0f, 0.0f, 0.0f, POSCONTROL_VEL_Z_IMAX, POSCONTROL_VEL_Z_FILT_HZ, POSCONTROL_VEL_Z_FILT_D_HZ),
    _pid_accel_z(POSCONTROL_ACC_Z_P, POSCONTROL_ACC_Z_I, POSCONTROL_ACC_Z_D, 0.0f, POSCONTROL_ACC_Z_IMAX, 0.0f, POSCONTROL_ACC_Z_FILT_HZ, 0.0f),
    _vel_max_xy_cms(POSCONTROL_SPEED),
    _vel_max_up_cms(POSCONTROL_SPEED_UP),
    _vel_max_down_cms(POSCONTROL_SPEED_DOWN),
    _accel_max_xy_cmss(POSCONTROL_ACCEL_XY),
    _accel_max_z_cmss(POSCONTROL_ACCEL_Z),
    _jerk_max_xy_cmsss(POSCONTROL_JERK_XY * 100.0),
    _jerk_max_z_cmsss(POSCONTROL_JERK_Z * 100.0)
{
    AP_Param::setup_object_defaults(this, var_info);

    _singleton = this;
}


///
/// 3D position shaper
///

/// input_pos_xyz - calculate a jerk limited path from the current position, velocity and acceleration to an input position.
///     The function takes the current position, velocity, and acceleration and calculates the required jerk limited adjustment to the acceleration for the next time dt.
///     The kinematic path is constrained by the maximum jerk parameter and the velocity and acceleration limits set using the function set_max_speed_accel_xy.
///     The jerk limit defines the acceleration error decay in the kinematic path as the system approaches constant acceleration.
///     The jerk limit also defines the time taken to achieve the maximum acceleration.
///     The function alters the input velocity to be the velocity that the system could reach zero acceleration in the minimum time.
void AC_PosControl::input_pos_xyz(const Vector3p& pos, float pos_terrain_target, float terrain_buffer)
{
    // Terrain following velocity scalar must be calculated before we remove the position offset
    const float offset_z_scaler = pos_offset_z_scaler(pos_terrain_target, terrain_buffer);
    set_pos_terrain_target_cm(pos_terrain_target);

    // calculated increased maximum acceleration and jerk if over speed
    const float overspeed_gain = calculate_overspeed_gain();
    const float accel_max_z_cmss = _accel_max_z_cmss * overspeed_gain;
    const float jerk_max_z_cmsss = _jerk_max_z_cmsss * overspeed_gain;

    update_pos_vel_accel_xy(_pos_desired.xy(), _vel_desired.xy(), _accel_desired.xy(), _dt, _limit_vector.xy(), _p_pos_xy.get_error(), _pid_vel_xy.get_error());

    // adjust desired altitude if motors have not hit their limits
    update_pos_vel_accel(_pos_desired.z, _vel_desired.z, _accel_desired.z, _dt, _limit_vector.z, _p_pos_z.get_error(), _pid_vel_z.get_error());

    // calculate the horizontal and vertical velocity limits to travel directly to the destination defined by pos
    float vel_max_xy_cms = 0.0f;
    float vel_max_z_cms = 0.0f;
    Vector3f dest_vector = (pos - _pos_desired).tofloat();
    if (is_positive(dest_vector.length_squared()) ) {
        dest_vector.normalize();
        float dest_vector_xy_length = dest_vector.xy().length();

        float vel_max_cms = kinematic_limit(dest_vector, _vel_max_xy_cms, _vel_max_up_cms, _vel_max_down_cms);
        vel_max_xy_cms = vel_max_cms * dest_vector_xy_length;
        vel_max_z_cms = fabsf(vel_max_cms * dest_vector.z);
    }

    // reduce speed if we are reaching the edge of our vertical buffer
    vel_max_xy_cms *= offset_z_scaler;

    Vector2f vel;
    Vector2f accel;
    shape_pos_vel_accel_xy(pos.xy(), vel, accel, _pos_desired.xy(), _vel_desired.xy(), _accel_desired.xy(),
                           vel_max_xy_cms, _accel_max_xy_cmss, _jerk_max_xy_cmsss, _dt, false);

    float posz = pos.z;
    shape_pos_vel_accel(posz, 0, 0,
                        _pos_desired.z, _vel_desired.z, _accel_desired.z,
                        -vel_max_z_cms, vel_max_z_cms,
                        -constrain_float(accel_max_z_cmss, 0.0f, 750.0f), accel_max_z_cmss,
                        jerk_max_z_cmsss, _dt, false);
}


/// pos_offset_z_scaler - calculates a multiplier used to reduce the horizontal velocity to allow the z position controller to stay within the provided buffer range
float AC_PosControl::pos_offset_z_scaler(float pos_offset_z, float pos_offset_z_buffer) const
{
    if (is_zero(pos_offset_z_buffer)) {
        return 1.0;
    }
    float pos_offset_error_z = _inav.get_position_z_up_cm() - (_pos_target.z + (pos_offset_z - _pos_terrain));
    return constrain_float((1.0 - (fabsf(pos_offset_error_z) - 0.5 * pos_offset_z_buffer) / (0.5 * pos_offset_z_buffer)), 0.01, 1.0);
}

///
/// Lateral position controller
///

/// set_max_speed_accel_xy - set the maximum horizontal speed in cm/s and acceleration in cm/s/s
///     This function only needs to be called if using the kinematic shaping.
///     This can be done at any time as changes in these parameters are handled smoothly
///     by the kinematic shaping.
void AC_PosControl::set_max_speed_accel_xy(float speed_cms, float accel_cmss)
{
    _vel_max_xy_cms = speed_cms;
    _accel_max_xy_cmss = accel_cmss;

    // ensure the horizontal jerk is less than the vehicle is capable of
    const float jerk_max_cmsss = MIN(_attitude_control.get_ang_vel_roll_max_rads(), _attitude_control.get_ang_vel_pitch_max_rads()) * GRAVITY_MSS * 100.0;
    const float snap_max_cmssss = MIN(_attitude_control.get_accel_roll_max_radss(), _attitude_control.get_accel_pitch_max_radss()) * GRAVITY_MSS * 100.0;

    // get specified jerk limit
    _jerk_max_xy_cmsss = _shaping_jerk_xy * 100.0;

    // limit maximum jerk based on maximum angular rate
    if (is_positive(jerk_max_cmsss) && _attitude_control.get_bf_feedforward()) {
        _jerk_max_xy_cmsss = MIN(_jerk_max_xy_cmsss, jerk_max_cmsss);
    }

    // limit maximum jerk to maximum possible average jerk based on angular acceleration
    if (is_positive(snap_max_cmssss) && _attitude_control.get_bf_feedforward()) {
        _jerk_max_xy_cmsss = MIN(0.5 * safe_sqrt(_accel_max_xy_cmss * snap_max_cmssss), _jerk_max_xy_cmsss);
    }
}

/// set_max_speed_accel_xy - set the position controller correction velocity and acceleration limit
///     This should be done only during initialisation to avoid discontinuities
void AC_PosControl::set_correction_speed_accel_xy(float speed_cms, float accel_cmss)
{
    _p_pos_xy.set_limits(speed_cms, accel_cmss, 0.0f);
}

/// init_xy_controller_stopping_point - initialise the position controller to the stopping point with zero velocity and acceleration.
///     This function should be used when the expected kinematic path assumes a stationary initial condition but does not specify a specific starting position.
///     The starting position can be retrieved by getting the position target using get_pos_desired_cm() after calling this function.
void AC_PosControl::init_xy_controller_stopping_point()
{
    init_xy_controller();

    get_stopping_point_xy_cm(_pos_desired.xy());
    _pos_target.xy() = _pos_desired.xy() + _pos_offset.xy();
    _vel_desired.xy().zero();
    _accel_desired.xy().zero();
}

// relax_velocity_controller_xy - initialise the position controller to the current position and velocity with decaying acceleration.
///     This function decays the output acceleration by 95% every half second to achieve a smooth transition to zero requested acceleration.
void AC_PosControl::relax_velocity_controller_xy()
{
    // decay acceleration and therefore current attitude target to zero
    // this will be reset by init_xy_controller() if !is_active_xy()
    if (is_positive(_dt)) {
        float decay = 1.0 - _dt / (_dt + POSCONTROL_RELAX_TC);
        _accel_target.xy() *= decay;
    }

    init_xy_controller();
}

/// reduce response for landing
void AC_PosControl::soften_for_landing_xy()
{
    // decay position error to zero
    if (is_positive(_dt)) {
        _pos_target.xy() += (_inav.get_position_xy_cm().topostype() - _pos_target.xy()) * (_dt / (_dt + POSCONTROL_RELAX_TC));
        _pos_desired.xy() = _pos_target.xy() - _pos_offset.xy();
    }

    // Prevent I term build up in xy velocity controller.
    // Note that this flag is reset on each loop in update_xy_controller()
    set_externally_limited_xy();
}

/// init_xy_controller - initialise the position controller to the current position, velocity, acceleration and attitude.
///     This function is the default initialisation for any position control that provides position, velocity and acceleration.
void AC_PosControl::init_xy_controller()
{
    // initialise offsets to target offsets and ensure offset targets are zero if they have not been updated.
    init_offsets_xy();
    
    // set roll, pitch lean angle targets to current attitude
    const Vector3f &att_target_euler_cd = _attitude_control.get_att_target_euler_cd();
    _roll_target = att_target_euler_cd.x;
    _pitch_target = att_target_euler_cd.y;
    _yaw_target = att_target_euler_cd.z; // todo: this should be thrust vector heading, not yaw.
    _yaw_rate_target = 0.0f;
    _angle_max_override_cd = 0.0;

    _pos_target.xy() = _inav.get_position_xy_cm().topostype();
    _pos_desired.xy() = _pos_target.xy() - _pos_offset.xy();

    _vel_target.xy() = _inav.get_velocity_xy_cms();
    _vel_desired.xy() = _vel_target.xy() - _vel_offset.xy();

    // Set desired accel to zero because raw acceleration is prone to noise
    _accel_desired.xy().zero();

    if (!is_active_xy()) {
        lean_angles_to_accel_xy(_accel_target.x, _accel_target.y);
    }

    // limit acceleration using maximum lean angles
    float angle_max = MIN(_attitude_control.get_althold_lean_angle_max_cd(), get_lean_angle_max_cd());
    float accel_max = angle_to_accel(angle_max * 0.01) * 100.0;
    _accel_target.xy().limit_length(accel_max);

    // initialise I terms from lean angles
    _pid_vel_xy.reset_filter();
    // initialise the I term to _accel_target - _accel_desired
    // _accel_desired is zero and can be removed from the equation
    _pid_vel_xy.set_integrator(_accel_target.xy() - _vel_target.xy() * _pid_vel_xy.ff());

    // initialise ekf xy reset handler
    init_ekf_xy_reset();

    // initialise z_controller time out
    _last_update_xy_ticks = AP::scheduler().ticks32();
}

/// input_accel_xy - calculate a jerk limited path from the current position, velocity and acceleration to an input acceleration.
///     The function takes the current position, velocity, and acceleration and calculates the required jerk limited adjustment to the acceleration for the next time dt.
///     The kinematic path is constrained by the maximum acceleration and jerk set using the function set_max_speed_accel_xy.
///     The jerk limit defines the acceleration error decay in the kinematic path as the system approaches constant acceleration.
///     The jerk limit also defines the time taken to achieve the maximum acceleration.
void AC_PosControl::input_accel_xy(const Vector3f& accel)
{
    update_pos_vel_accel_xy(_pos_desired.xy(), _vel_desired.xy(), _accel_desired.xy(), _dt, _limit_vector.xy(), _p_pos_xy.get_error(), _pid_vel_xy.get_error());
    shape_accel_xy(accel.xy(), _accel_desired.xy(), _jerk_max_xy_cmsss, _dt);
}

/// input_vel_accel_xy - calculate a jerk limited path from the current position, velocity and acceleration to an input velocity and acceleration.
///     The vel is projected forwards in time based on a time step of dt and acceleration accel.
///     The function takes the current position, velocity, and acceleration and calculates the required jerk limited adjustment to the acceleration for the next time dt.
///     The kinematic path is constrained by the maximum acceleration and jerk set using the function set_max_speed_accel_xy.
///     The parameter limit_output specifies if the velocity and acceleration limits are applied to the sum of commanded and correction values or just correction.
void AC_PosControl::input_vel_accel_xy(Vector2f& vel, const Vector2f& accel, bool limit_output)
{
    update_pos_vel_accel_xy(_pos_desired.xy(), _vel_desired.xy(), _accel_desired.xy(), _dt, _limit_vector.xy(), _p_pos_xy.get_error(), _pid_vel_xy.get_error());

    shape_vel_accel_xy(vel, accel, _vel_desired.xy(), _accel_desired.xy(),
        _accel_max_xy_cmss, _jerk_max_xy_cmsss, _dt, limit_output);

    update_vel_accel_xy(vel, accel, _dt, Vector2f(), Vector2f());
}

/// input_pos_vel_accel_xy - calculate a jerk limited path from the current position, velocity and acceleration to an input position velocity and acceleration.
///     The pos and vel are projected forwards in time based on a time step of dt and acceleration accel.
///     The function takes the current position, velocity, and acceleration and calculates the required jerk limited adjustment to the acceleration for the next time dt.
///     The function alters the pos and vel to be the kinematic path based on accel
///     The parameter limit_output specifies if the velocity and acceleration limits are applied to the sum of commanded and correction values or just correction.
void AC_PosControl::input_pos_vel_accel_xy(Vector2p& pos, Vector2f& vel, const Vector2f& accel, bool limit_output)
{
    update_pos_vel_accel_xy(_pos_desired.xy(), _vel_desired.xy(), _accel_desired.xy(), _dt, _limit_vector.xy(), _p_pos_xy.get_error(), _pid_vel_xy.get_error());

    shape_pos_vel_accel_xy(pos, vel, accel, _pos_desired.xy(), _vel_desired.xy(), _accel_desired.xy(),
                           _vel_max_xy_cms, _accel_max_xy_cmss, _jerk_max_xy_cmsss, _dt, limit_output);

    update_pos_vel_accel_xy(pos, vel, accel, _dt, Vector2f(), Vector2f(), Vector2f());
}

/// update the horizontal position and velocity offsets
/// this moves the offsets (e.g _pos_offset, _vel_offset, _accel_offset) towards the targets (e.g. _pos_offset_target, _vel_offset_target, _accel_offset_target)
void AC_PosControl::update_offsets_xy()
{
    // check for offset target timeout
    uint32_t now_ms = AP_HAL::millis();
    if (now_ms - _posvelaccel_offset_target_xy_ms > POSCONTROL_POSVELACCEL_OFFSET_TARGET_TIMEOUT_MS) {
        _pos_offset_target.xy().zero();
        _vel_offset_target.xy().zero();
        _accel_offset_target.xy().zero();
    }

    // update position, velocity, accel offsets for this iteration
    update_pos_vel_accel_xy(_pos_offset_target.xy(), _vel_offset_target.xy(), _accel_offset_target.xy(), _dt, Vector2f(), Vector2f(), Vector2f());
    update_pos_vel_accel_xy(_pos_offset.xy(), _vel_offset.xy(), _accel_offset.xy(), _dt, _limit_vector.xy(), _p_pos_xy.get_error(), _pid_vel_xy.get_error());

    // input shape horizontal position, velocity and acceleration offsets
    shape_pos_vel_accel_xy(_pos_offset_target.xy(), _vel_offset_target.xy(), _accel_offset_target.xy(),
                            _pos_offset.xy(), _vel_offset.xy(), _accel_offset.xy(),
                            _vel_max_xy_cms, _accel_max_xy_cmss, _jerk_max_xy_cmsss, _dt, false);
}

/// stop_pos_xy_stabilisation - sets the target to the current position to remove any position corrections from the system
void AC_PosControl::stop_pos_xy_stabilisation()
{
    _pos_target.xy() = _inav.get_position_xy_cm().topostype();
    _pos_desired.xy() = _pos_target.xy() - _pos_offset.xy();
}

/// stop_vel_xy_stabilisation - sets the target to the current position and velocity to the current velocity to remove any position and velocity corrections from the system
void AC_PosControl::stop_vel_xy_stabilisation()
{
    _pos_target.xy() =  _inav.get_position_xy_cm().topostype();
    _pos_desired.xy() = _pos_target.xy() - _pos_offset.xy();
    
    _vel_target.xy() = _inav.get_velocity_xy_cms();;
    _vel_desired.xy() = _vel_target.xy() - _vel_offset.xy();

    // initialise I terms from lean angles
    _pid_vel_xy.reset_filter();
    _pid_vel_xy.reset_I();
}

// is_active_xy - returns true if the xy position controller has been run in the previous loop
bool AC_PosControl::is_active_xy() const
{
    const uint32_t dt_ticks = AP::scheduler().ticks32() - _last_update_xy_ticks;
    return dt_ticks <= 1;
}

/// update_xy_controller - runs the horizontal position controller correcting position, velocity and acceleration errors.
///     Position and velocity errors are converted to velocity and acceleration targets using PID objects
///     Desired velocity and accelerations are added to these corrections as they are calculated
///     Kinematically consistent target position and desired velocity and accelerations should be provided before calling this function
void AC_PosControl::update_xy_controller()
{
    // check for ekf xy position reset
    handle_ekf_xy_reset();

    // Check for position control time out
    if (!is_active_xy()) {
        init_xy_controller();
        if (has_good_timing()) {
            // call internal error because initialisation has not been done
            INTERNAL_ERROR(AP_InternalError::error_t::flow_of_control);
        }
    }
    _last_update_xy_ticks = AP::scheduler().ticks32();

    float ahrsGndSpdLimit, ahrsControlScaleXY;
    AP::ahrs().getControlLimits(ahrsGndSpdLimit, ahrsControlScaleXY);

    // update the position, velocity and acceleration offsets
    update_offsets_xy();

    // Position Controller

    _pos_target.xy() = _pos_desired.xy() + _pos_offset.xy();

    // determine the combined position of the actual position and the disturbance from system ID mode
    const Vector3f &curr_pos = _inav.get_position_neu_cm();
    Vector3f comb_pos = curr_pos;
    comb_pos.xy() += _disturb_pos;

    Vector2f vel_target = _p_pos_xy.update_all(_pos_target.x, _pos_target.y, comb_pos);
    _pos_desired.xy() = _pos_target.xy() - _pos_offset.xy();

    // Velocity Controller

    // add velocity feed-forward scaled to compensate for optical flow measurement induced EKF noise
    vel_target *= ahrsControlScaleXY;
    vel_target *= _xy_control_scale_factor;

    _vel_target.xy() = vel_target;
    _vel_target.xy() += _vel_desired.xy() + _vel_offset.xy();

    // determine the combined velocity of the actual velocity and the disturbance from system ID mode
    const Vector2f &curr_vel = _inav.get_velocity_xy_cms();
    Vector2f comb_vel = curr_vel;
    comb_vel += _disturb_vel;

    Vector2f accel_target = _pid_vel_xy.update_all(_vel_target.xy(), comb_vel, _dt, _limit_vector.xy());

    // Acceleration Controller
    
    // acceleration to correct for velocity error and scale PID output to compensate for optical flow measurement induced EKF noise
    accel_target *= ahrsControlScaleXY;
    accel_target *= _xy_control_scale_factor;

    _xy_control_scale_factor = 1.0;

    // pass the correction acceleration to the target acceleration output
    _accel_target.xy() = accel_target;
    _accel_target.xy() += _accel_desired.xy() + _accel_offset.xy();

    // limit acceleration using maximum lean angles
    float angle_max = MIN(_attitude_control.get_althold_lean_angle_max_cd(), get_lean_angle_max_cd());
    float accel_max = angle_to_accel(angle_max * 0.01) * 100;
    // Define the limit vector before we constrain _accel_target 
    _limit_vector.xy() = _accel_target.xy();
    if (!limit_accel_xy(_vel_desired.xy(), _accel_target.xy(), accel_max)) {
        // _accel_target was not limited so we can zero the xy limit vector
        _limit_vector.xy().zero();
    } else {
        // Check for pitch limiting in the forward direction
        const float accel_fwd_unlimited = _limit_vector.x * _ahrs.cos_yaw() + _limit_vector.y * _ahrs.sin_yaw();
        const float pitch_target_unlimited = accel_to_angle(- MIN(accel_fwd_unlimited, accel_max) * 0.01f) * 100;
        const float accel_fwd_limited = _accel_target.x * _ahrs.cos_yaw() + _accel_target.y * _ahrs.sin_yaw();
        const float pitch_target_limited = accel_to_angle(- accel_fwd_limited * 0.01f) * 100;
        _fwd_pitch_is_limited = is_negative(pitch_target_unlimited) && pitch_target_unlimited < pitch_target_limited;
    }

    // update angle targets that will be passed to stabilize controller
    accel_to_lean_angles(_accel_target.x, _accel_target.y, _roll_target, _pitch_target);
    calculate_yaw_and_rate_yaw();

    // reset the disturbance from system ID mode to zero
    _disturb_pos.zero();
    _disturb_vel.zero();
}


///
/// Vertical position controller
///

/// set_max_speed_accel_z - set the maximum vertical speed in cm/s and acceleration in cm/s/s
///     speed_down can be positive or negative but will always be interpreted as a descent speed.
///     This function only needs to be called if using the kinematic shaping.
///     This can be done at any time as changes in these parameters are handled smoothly
///     by the kinematic shaping.
void AC_PosControl::set_max_speed_accel_z(float speed_down, float speed_up, float accel_cmss)
{
    // ensure speed_down is always negative
    speed_down = -fabsf(speed_down);

    // sanity check and update
    if (is_negative(speed_down)) {
        _vel_max_down_cms = speed_down;
    }
    if (is_positive(speed_up)) {
        _vel_max_up_cms = speed_up;
    }
    if (is_positive(accel_cmss)) {
        _accel_max_z_cmss = accel_cmss;
    }

    // ensure the vertical Jerk is not limited by the filters in the Z accel PID object
    _jerk_max_z_cmsss = _shaping_jerk_z * 100.0;
    if (is_positive(_pid_accel_z.filt_T_hz())) {
        _jerk_max_z_cmsss = MIN(_jerk_max_z_cmsss, MIN(GRAVITY_MSS * 100.0, _accel_max_z_cmss) * (M_2PI * _pid_accel_z.filt_T_hz()) / 5.0);
    }
    if (is_positive(_pid_accel_z.filt_E_hz())) {
        _jerk_max_z_cmsss = MIN(_jerk_max_z_cmsss, MIN(GRAVITY_MSS * 100.0, _accel_max_z_cmss) * (M_2PI * _pid_accel_z.filt_E_hz()) / 5.0);
    }
}

/// set_correction_speed_accel_z - set the position controller correction velocity and acceleration limit
///     speed_down can be positive or negative but will always be interpreted as a descent speed.
///     This should be done only during initialisation to avoid discontinuities
void AC_PosControl::set_correction_speed_accel_z(float speed_down, float speed_up, float accel_cmss)
{
    // define maximum position error and maximum first and second differential limits
    _p_pos_z.set_limits(-fabsf(speed_down), speed_up, accel_cmss, 0.0f);
}

/// init_z_controller - initialise the position controller to the current position, velocity, acceleration and attitude.
///     This function is the default initialisation for any position control that provides position, velocity and acceleration.
///     This function does not allow any negative velocity or acceleration
void AC_PosControl::init_z_controller_no_descent()
{
    // Initialise the position controller to the current throttle, position, velocity and acceleration.
    init_z_controller();

    // remove all descent if present
    _vel_target.z = MAX(0.0, _vel_target.z);
    _vel_desired.z = MAX(0.0, _vel_desired.z);
    _vel_terrain = MAX(0.0, _vel_terrain);
    _vel_offset.z = MAX(0.0, _vel_offset.z);
    _accel_target.z = MAX(0.0, _accel_target.z);
    _accel_desired.z = MAX(0.0, _accel_desired.z);
    _accel_terrain = MAX(0.0, _accel_terrain);
    _accel_offset.z = MAX(0.0, _accel_offset.z);
}

/// init_z_controller_stopping_point - initialise the position controller to the stopping point with zero velocity and acceleration.
///     This function should be used when the expected kinematic path assumes a stationary initial condition but does not specify a specific starting position.
///     The starting position can be retrieved by getting the position target using get_pos_target_cm() after calling this function.
void AC_PosControl::init_z_controller_stopping_point()
{
    // Initialise the position controller to the current throttle, position, velocity and acceleration.
    init_z_controller();

    get_stopping_point_z_cm(_pos_desired.z);
    _pos_target.z = _pos_desired.z + _pos_offset.z;
    _vel_desired.z = 0.0f;
    _accel_desired.z = 0.0f;
}

// relax_z_controller - initialise the position controller to the current position and velocity with decaying acceleration.
///     This function decays the output acceleration by 95% every half second to achieve a smooth transition to zero requested acceleration.
void AC_PosControl::relax_z_controller(float throttle_setting)
{
    // Initialise the position controller to the current position, velocity and acceleration.
    init_z_controller();

    // init_z_controller has set the accel PID I term to generate the current throttle set point
    // Use relax_integrator to decay the throttle set point to throttle_setting
    _pid_accel_z.relax_integrator((throttle_setting - _motors.get_throttle_hover()) * 1000.0f, _dt, POSCONTROL_RELAX_TC);
}

/// init_z_controller - initialise the position controller to the current position, velocity, acceleration and attitude.
///     This function is the default initialisation for any position control that provides position, velocity and acceleration.
///     This function is private and contains all the shared z axis initialisation functions
void AC_PosControl::init_z_controller()
{
    // initialise terrain targets and offsets to zero
    init_terrain();

    // initialise offsets to target offsets and ensure offset targets are zero if they have not been updated.
    init_offsets_z();

    _pos_target.z = _inav.get_position_z_up_cm();
    _pos_desired.z = _pos_target.z - _pos_offset.z;

    _vel_target.z = _inav.get_velocity_z_up_cms();
    _vel_desired.z = _vel_target.z - _vel_offset.z;

    // Reset I term of velocity PID
    _pid_vel_z.reset_filter();
    _pid_vel_z.set_integrator(0.0f);

    _accel_target.z = constrain_float(get_z_accel_cmss(), -_accel_max_z_cmss, _accel_max_z_cmss);
    _accel_desired.z = _accel_target.z - (_accel_offset.z + _accel_terrain);
    _pid_accel_z.reset_filter();

    // Set accel PID I term based on the current throttle
    // Remove the expected P term due to _accel_desired.z being constrained to _accel_max_z_cmss
    // Remove the expected FF term due to non-zero _accel_target.z
    _pid_accel_z.set_integrator((_attitude_control.get_throttle_in() - _motors.get_throttle_hover()) * 1000.0f
        - _pid_accel_z.kP() * (_accel_target.z - get_z_accel_cmss())
        - _pid_accel_z.ff() * _accel_target.z);

    // initialise ekf z reset handler
    init_ekf_z_reset();

    // initialise z_controller time out
    _last_update_z_ticks = AP::scheduler().ticks32();
}

/// input_accel_z - calculate a jerk limited path from the current position, velocity and acceleration to an input acceleration.
///     The function takes the current position, velocity, and acceleration and calculates the required jerk limited adjustment to the acceleration for the next time dt.
void AC_PosControl::input_accel_z(float accel)
{
    // calculated increased maximum jerk if over speed
    float jerk_max_z_cmsss = _jerk_max_z_cmsss * calculate_overspeed_gain();

    // adjust desired alt if motors have not hit their limits
    update_pos_vel_accel(_pos_desired.z, _vel_desired.z, _accel_desired.z, _dt, _limit_vector.z, _p_pos_z.get_error(), _pid_vel_z.get_error());

    shape_accel(accel, _accel_desired.z, jerk_max_z_cmsss, _dt);
}

/// input_accel_z - calculate a jerk limited path from the current position, velocity and acceleration to an input acceleration.
///     The function takes the current position, velocity, and acceleration and calculates the required jerk limited adjustment to the acceleration for the next time dt.
///     The kinematic path is constrained by the maximum acceleration and jerk set using the function set_max_speed_accel_z.
///     The parameter limit_output specifies if the velocity and acceleration limits are applied to the sum of commanded and correction values or just correction.
void AC_PosControl::input_vel_accel_z(float &vel, float accel, bool limit_output)
{
    // calculated increased maximum acceleration and jerk if over speed
    const float overspeed_gain = calculate_overspeed_gain();
    const float accel_max_z_cmss = _accel_max_z_cmss * overspeed_gain;
    const float jerk_max_z_cmsss = _jerk_max_z_cmsss * overspeed_gain;

    // adjust desired alt if motors have not hit their limits
    update_pos_vel_accel(_pos_desired.z, _vel_desired.z, _accel_desired.z, _dt, _limit_vector.z, _p_pos_z.get_error(), _pid_vel_z.get_error());

    shape_vel_accel(vel, accel,
                    _vel_desired.z, _accel_desired.z,
                    -constrain_float(accel_max_z_cmss, 0.0f, 750.0f), accel_max_z_cmss,
                    jerk_max_z_cmsss, _dt, limit_output);

    update_vel_accel(vel, accel, _dt, 0.0, 0.0);
}

/// set_pos_target_z_from_climb_rate_cm - adjusts target up or down using a commanded climb rate in cm/s
///     using the default position control kinematic path.
///     The zero target altitude is varied to follow pos_offset_z
void AC_PosControl::set_pos_target_z_from_climb_rate_cm(float vel)
{
    input_vel_accel_z(vel, 0.0);
}

/// land_at_climb_rate_cm - adjusts target up or down using a commanded climb rate in cm/s
///     using the default position control kinematic path.
///     ignore_descent_limit turns off output saturation handling to aid in landing detection. ignore_descent_limit should be false unless landing.
void AC_PosControl::land_at_climb_rate_cm(float vel, bool ignore_descent_limit)
{
    if (ignore_descent_limit) {
        // turn off limits in the negative z direction
        _limit_vector.z = MAX(_limit_vector.z, 0.0f);
    }

    input_vel_accel_z(vel, 0.0);
}

/// input_pos_vel_accel_z - calculate a jerk limited path from the current position, velocity and acceleration to an input position velocity and acceleration.
///     The pos and vel are projected forwards in time based on a time step of dt and acceleration accel.
///     The function takes the current position, velocity, and acceleration and calculates the required jerk limited adjustment to the acceleration for the next time dt.
///     The function alters the pos and vel to be the kinematic path based on accel
///     The parameter limit_output specifies if the velocity and acceleration limits are applied to the sum of commanded and correction values or just correction.
void AC_PosControl::input_pos_vel_accel_z(float &pos, float &vel, float accel, bool limit_output)
{
    // calculated increased maximum acceleration and jerk if over speed
    const float overspeed_gain = calculate_overspeed_gain();
    const float accel_max_z_cmss = _accel_max_z_cmss * overspeed_gain;
    const float jerk_max_z_cmsss = _jerk_max_z_cmsss * overspeed_gain;

    // adjust desired altitude if motors have not hit their limits
    update_pos_vel_accel(_pos_desired.z, _vel_desired.z, _accel_desired.z, _dt, _limit_vector.z, _p_pos_z.get_error(), _pid_vel_z.get_error());

    shape_pos_vel_accel(pos, vel, accel,
                        _pos_desired.z, _vel_desired.z, _accel_desired.z,
                        _vel_max_down_cms, _vel_max_up_cms,
                        -constrain_float(accel_max_z_cmss, 0.0f, 750.0f), accel_max_z_cmss,
                        jerk_max_z_cmsss, _dt, limit_output);

    postype_t posp = pos;
    update_pos_vel_accel(posp, vel, accel, _dt, 0.0, 0.0, 0.0);
    pos = posp;
}

/// set_alt_target_with_slew - adjusts target up or down using a commanded altitude in cm
///     using the default position control kinematic path.
void AC_PosControl::set_alt_target_with_slew(float pos)
{
    float zero = 0;
    input_pos_vel_accel_z(pos, zero, 0);
}

/// update_offsets_z - updates the vertical offsets used by terrain following
void AC_PosControl::update_offsets_z()
{
    // check for offset target timeout
    uint32_t now_ms = AP_HAL::millis();
    if (now_ms - _posvelaccel_offset_target_z_ms > POSCONTROL_POSVELACCEL_OFFSET_TARGET_TIMEOUT_MS) {
        _pos_offset_target.z = 0.0;
        _vel_offset_target.z = 0.0;
        _accel_offset_target.z = 0.0;
    }

    // update position, velocity, accel offsets for this iteration
    postype_t p_offset_z = _pos_offset.z;
    update_pos_vel_accel(p_offset_z, _vel_offset.z, _accel_offset.z, _dt, MIN(_limit_vector.z, 0.0f), _p_pos_z.get_error(), _pid_vel_z.get_error());
    _pos_offset.z = p_offset_z;

    // input shape vertical position, velocity and acceleration offsets
    shape_pos_vel_accel(_pos_offset_target.z, _vel_offset_target.z, _accel_offset_target.z,
        _pos_offset.z, _vel_offset.z, _accel_offset.z,
        get_max_speed_down_cms(), get_max_speed_up_cms(),
        -get_max_accel_z_cmss(), get_max_accel_z_cmss(),
        _jerk_max_z_cmsss, _dt, false);

    p_offset_z = _pos_offset_target.z;
    update_pos_vel_accel(p_offset_z, _vel_offset_target.z, _accel_offset_target.z, _dt, 0.0, 0.0, 0.0);
    _pos_offset_target.z = p_offset_z;
}

// is_active_z - returns true if the z position controller has been run in the previous loop
bool AC_PosControl::is_active_z() const
{
    const uint32_t dt_ticks = AP::scheduler().ticks32() - _last_update_z_ticks;
    return dt_ticks <= 1;
}

/// update_z_controller - runs the vertical position controller correcting position, velocity and acceleration errors.
///     Position and velocity errors are converted to velocity and acceleration targets using PID objects
///     Desired velocity and accelerations are added to these corrections as they are calculated
///     Kinematically consistent target position and desired velocity and accelerations should be provided before calling this function
void AC_PosControl::update_z_controller()
{
    // check for ekf z-axis position reset
    handle_ekf_z_reset();

    // Check for z_controller time out
    if (!is_active_z()) {
        init_z_controller();
        if (has_good_timing()) {
            // call internal error because initialisation has not been done
            INTERNAL_ERROR(AP_InternalError::error_t::flow_of_control);
        }
    }
    _last_update_z_ticks = AP::scheduler().ticks32();

    // update the position, velocity and acceleration offsets
    update_offsets_z();
    update_terrain();
    _pos_target.z = _pos_desired.z + _pos_offset.z + _pos_terrain;

    // calculate the target velocity correction
    float pos_target_zf = _pos_target.z;

    _vel_target.z = _p_pos_z.update_all(pos_target_zf, _inav.get_position_z_up_cm());
    _vel_target.z *= AP::ahrs().getControlScaleZ();

    _pos_target.z = pos_target_zf;
    _pos_desired.z = _pos_target.z - (_pos_offset.z + _pos_terrain);

    // add feed forward component
    _vel_target.z += _vel_desired.z + _vel_offset.z + _vel_terrain;

    // Velocity Controller

    const float curr_vel_z = _inav.get_velocity_z_up_cms();
    _accel_target.z = _pid_vel_z.update_all(_vel_target.z, curr_vel_z, _dt, _motors.limit.throttle_lower, _motors.limit.throttle_upper);
    _accel_target.z *= AP::ahrs().getControlScaleZ();

    // add feed forward component
    _accel_target.z += _accel_desired.z + _accel_offset.z + _accel_terrain;

    // Acceleration Controller

    // Calculate vertical acceleration
    const float z_accel_meas = get_z_accel_cmss();

    // ensure imax is always large enough to overpower hover throttle
    if (_motors.get_throttle_hover() * 1000.0f > _pid_accel_z.imax()) {
        _pid_accel_z.set_imax(_motors.get_throttle_hover() * 1000.0f);
    }
    float thr_out;
    if (_vibe_comp_enabled) {
        thr_out = get_throttle_with_vibration_override();
    } else {
        thr_out = _pid_accel_z.update_all(_accel_target.z, z_accel_meas, _dt, (_motors.limit.throttle_lower || _motors.limit.throttle_upper)) * 0.001f;
        thr_out += _pid_accel_z.get_ff() * 0.001f;
    }
    thr_out += _motors.get_throttle_hover();

    // Actuator commands

    // send throttle to attitude controller with angle boost
    _attitude_control.set_throttle_out(thr_out, true, POSCONTROL_THROTTLE_CUTOFF_FREQ_HZ);

    // Check for vertical controller health

    // _speed_down_cms is checked to be non-zero when set
    float error_ratio = _pid_vel_z.get_error() / _vel_max_down_cms;
    _vel_z_control_ratio += _dt * 0.1f * (0.5 - error_ratio);
    _vel_z_control_ratio = constrain_float(_vel_z_control_ratio, 0.0f, 2.0f);

    // set vertical component of the limit vector
    if (_motors.limit.throttle_upper) {
        _limit_vector.z = 1.0f;
    } else if (_motors.limit.throttle_lower) {
        _limit_vector.z = -1.0f;
    } else {
        _limit_vector.z = 0.0f;
    }
}


///
/// Accessors
///

/// get_lean_angle_max_cd - returns the maximum lean angle the autopilot may request
float AC_PosControl::get_lean_angle_max_cd() const
{
    if (is_positive(_angle_max_override_cd)) {
        return _angle_max_override_cd;
    }
    if (!is_positive(_lean_angle_max)) {
        return _attitude_control.lean_angle_max_cd();
    }
    return _lean_angle_max * 100.0f;
}

/// set the desired position, velocity and acceleration targets
void AC_PosControl::set_pos_vel_accel(const Vector3p& pos, const Vector3f& vel, const Vector3f& accel)
{
    _pos_desired = pos;
    _vel_desired = vel;
    _accel_desired = accel;
}

/// set the desired position, velocity and acceleration targets
void AC_PosControl::set_pos_vel_accel_xy(const Vector2p& pos, const Vector2f& vel, const Vector2f& accel)
{
    _pos_desired.xy() = pos;
    _vel_desired.xy() = vel;
    _accel_desired.xy() = accel;
}

// get_lean_angles_to_accel - convert roll, pitch lean target angles to lat/lon frame accelerations in cm/s/s
Vector3f AC_PosControl::lean_angles_to_accel(const Vector3f& att_target_euler) const
{
    // rotate our roll, pitch angles into lat/lon frame
    const float sin_roll = sinf(att_target_euler.x);
    const float cos_roll = cosf(att_target_euler.x);
    const float sin_pitch = sinf(att_target_euler.y);
    const float cos_pitch = cosf(att_target_euler.y);
    const float sin_yaw = sinf(att_target_euler.z);
    const float cos_yaw = cosf(att_target_euler.z);

    return Vector3f{
        (GRAVITY_MSS * 100.0f) * (-cos_yaw * sin_pitch * cos_roll - sin_yaw * sin_roll) / MAX(cos_roll * cos_pitch, 0.1f),
        (GRAVITY_MSS * 100.0f) * (-sin_yaw * sin_pitch * cos_roll + cos_yaw * sin_roll) / MAX(cos_roll * cos_pitch, 0.1f),
        (GRAVITY_MSS * 100.0f)
    };
}

/// Terrain

/// set the terrain position, velocity and acceleration in cm, cms and cm/s/s from EKF origin in NE frame
/// this is used to initiate the offsets when initialise the position controller or do an offset reset
void AC_PosControl::init_terrain()
{
    // set terrain position and target to zero
    _pos_terrain_target = 0.0;
    _pos_terrain = 0.0;

    // set velocity offset to zero
    _vel_terrain = 0.0;

    // set acceleration offset to zero
    _accel_terrain = 0.0;
}

// init_pos_terrain_cm - initialises the current terrain altitude and target altitude to pos_offset_terrain_cm
void AC_PosControl::init_pos_terrain_cm(float pos_terrain_cm)
{
    _pos_desired.z -= (pos_terrain_cm - _pos_terrain);
    _pos_terrain_target = pos_terrain_cm;
    _pos_terrain = pos_terrain_cm;
}


/// Offsets

/// set the horizontal position, velocity and acceleration offsets in cm, cms and cm/s/s from EKF origin in NE frame
/// this is used to initiate the offsets when initialise the position controller or do an offset reset
void AC_PosControl::init_offsets_xy()
{
    // check for offset target timeout
    uint32_t now_ms = AP_HAL::millis();
    if (now_ms - _posvelaccel_offset_target_xy_ms > POSCONTROL_POSVELACCEL_OFFSET_TARGET_TIMEOUT_MS) {
        _pos_offset_target.xy().zero();
        _vel_offset_target.xy().zero();
        _accel_offset_target.xy().zero();
    }

    // set position offset to target
    _pos_offset.xy() = _pos_offset_target.xy();

    // set velocity offset to target
    _vel_offset.xy() = _vel_offset_target.xy();

    // set acceleration offset to target
    _accel_offset.xy() = _accel_offset_target.xy();
}

/// set the horizontal position, velocity and acceleration offsets in cm, cms and cm/s/s from EKF origin in NE frame
/// this is used to initiate the offsets when initialise the position controller or do an offset reset
void AC_PosControl::init_offsets_z()
{
    // check for offset target timeout
    uint32_t now_ms = AP_HAL::millis();
    if (now_ms - _posvelaccel_offset_target_z_ms > POSCONTROL_POSVELACCEL_OFFSET_TARGET_TIMEOUT_MS) {
        _pos_offset_target.z = 0.0;
        _vel_offset_target.z = 0.0;
        _accel_offset_target.z = 0.0;
    }
    // set position offset to target
    _pos_offset.z = _pos_offset_target.z;

    // set velocity offset to target
    _vel_offset.z = _vel_offset_target.z;

    // set acceleration offset to target
    _accel_offset.z = _accel_offset_target.z;
}

#if AP_SCRIPTING_ENABLED
// add an additional offset to vehicle's target position, velocity and acceleration
// units are m, m/s and m/s/s in NED frame
// Z-axis is not currently supported and is ignored
bool AC_PosControl::set_posvelaccel_offset(const Vector3f &pos_offset_NED, const Vector3f &vel_offset_NED, const Vector3f &accel_offset_NED)
{
    set_posvelaccel_offset_target_xy_cm(pos_offset_NED.topostype().xy() * 100.0, vel_offset_NED.xy() * 100.0, accel_offset_NED.xy() * 100.0);
    set_posvelaccel_offset_target_z_cm(-pos_offset_NED.topostype().z * 100.0, -vel_offset_NED.z * 100, -accel_offset_NED.z * 100.0);
    return true;
}

// get position and velocity offset to vehicle's target velocity and acceleration
// units are m and m/s in NED frame
bool AC_PosControl::get_posvelaccel_offset(Vector3f &pos_offset_NED, Vector3f &vel_offset_NED, Vector3f &accel_offset_NED)
{
    pos_offset_NED.xy() = _pos_offset_target.xy().tofloat() * 0.01;
    pos_offset_NED.z = -_pos_offset_target.z * 0.01;
    vel_offset_NED.xy() = _vel_offset_target.xy() * 0.01;
    vel_offset_NED.z = -_vel_offset_target.z * 0.01;
    accel_offset_NED.xy() = _accel_offset_target.xy() * 0.01;
    accel_offset_NED.z = -_accel_offset_target.z * 0.01;
    return true;
}
#endif

/// set the horizontal position, velocity and acceleration offset targets in cm, cms and cm/s/s from EKF origin in NE frame
/// these must be set every 3 seconds (or less) or they will timeout and return to zero
void AC_PosControl::set_posvelaccel_offset_target_xy_cm(const Vector2p& pos_offset_target_xy_cm, const Vector2f& vel_offset_target_xy_cms, const Vector2f& accel_offset_target_xy_cmss)
{
    // set position offset target
    _pos_offset_target.xy() = pos_offset_target_xy_cm;

    // set velocity offset target
    _vel_offset_target.xy() = vel_offset_target_xy_cms;

    // set acceleration offset target
    _accel_offset_target.xy() = accel_offset_target_xy_cmss;

    // record time of update so we can detect timeouts
    _posvelaccel_offset_target_xy_ms = AP_HAL::millis();
}

/// set the vertical position, velocity and acceleration offset targets in cm, cms and cm/s/s from EKF origin in NE frame
/// these must be set every 3 seconds (or less) or they will timeout and return to zero
void AC_PosControl::set_posvelaccel_offset_target_z_cm(float pos_offset_target_z_cm, float vel_offset_target_z_cms, const float accel_offset_target_z_cmss)
{
    // set position offset target
    _pos_offset_target.z = pos_offset_target_z_cm;

    // set velocity offset target
    _vel_offset_target.z = vel_offset_target_z_cms;

    // set acceleration offset target
    _accel_offset_target.z = accel_offset_target_z_cmss;

    // record time of update so we can detect timeouts
    _posvelaccel_offset_target_z_ms = AP_HAL::millis();
}

// returns the NED target acceleration vector for attitude control
Vector3f AC_PosControl::get_thrust_vector() const
{
    Vector3f accel_target = get_accel_target_cmss();
    accel_target.z = -GRAVITY_MSS * 100.0f;
    return accel_target;
}

/// get_stopping_point_xy_cm - calculates stopping point in NEU cm based on current position, velocity, vehicle acceleration
///    function does not change the z axis
void AC_PosControl::get_stopping_point_xy_cm(Vector2p &stopping_point) const
{
    // todo: we should use the current target position and velocity if we are currently running the position controller
    stopping_point = _inav.get_position_xy_cm().topostype();
    stopping_point -= _pos_offset.xy();

    Vector2f curr_vel = _inav.get_velocity_xy_cms();
    curr_vel -= _vel_offset.xy();

    // calculate current velocity
    float vel_total = curr_vel.length();

    if (!is_positive(vel_total)) {
        return;
    }
    
    float kP = _p_pos_xy.kP();
    const float stopping_dist = stopping_distance(constrain_float(vel_total, 0.0, _vel_max_xy_cms), kP, _accel_max_xy_cmss);
    if (!is_positive(stopping_dist)) {
        return;
    }

    // convert the stopping distance into a stopping point using velocity vector
    const float t = stopping_dist / vel_total;
    stopping_point += (curr_vel * t).topostype();
}

/// get_stopping_point_z_cm - calculates stopping point in NEU cm based on current position, velocity, vehicle acceleration
void AC_PosControl::get_stopping_point_z_cm(postype_t &stopping_point) const
{
    float curr_pos_z = _inav.get_position_z_up_cm();
    curr_pos_z -= _pos_offset.z;

    float curr_vel_z = _inav.get_velocity_z_up_cms();
    curr_vel_z -= _vel_offset.z;

    // avoid divide by zero by using current position if kP is very low or acceleration is zero
    if (!is_positive(_p_pos_z.kP()) || !is_positive(_accel_max_z_cmss)) {
        stopping_point = curr_pos_z;
        return;
    }

    stopping_point = curr_pos_z + constrain_float(stopping_distance(curr_vel_z, _p_pos_z.kP(), _accel_max_z_cmss), - POSCONTROL_STOPPING_DIST_DOWN_MAX, POSCONTROL_STOPPING_DIST_UP_MAX);
}

/// get_bearing_to_target_cd - get bearing to target position in centi-degrees
int32_t AC_PosControl::get_bearing_to_target_cd() const
{
    return get_bearing_cd(_inav.get_position_xy_cm(), _pos_target.tofloat().xy());
}


///
/// System methods
///

// get throttle using vibration-resistant calculation (uses feed forward with manually calculated gain)
float AC_PosControl::get_throttle_with_vibration_override()
{
    const float thr_per_accelz_cmss = _motors.get_throttle_hover() / (GRAVITY_MSS * 100.0f);
    // during vibration compensation use feed forward with manually calculated gain
    // ToDo: clear pid_info P, I and D terms for logging
    if (!(_motors.limit.throttle_lower || _motors.limit.throttle_upper) || ((is_positive(_pid_accel_z.get_i()) && is_negative(_pid_vel_z.get_error())) || (is_negative(_pid_accel_z.get_i()) && is_positive(_pid_vel_z.get_error())))) {
        _pid_accel_z.set_integrator(_pid_accel_z.get_i() + _dt * thr_per_accelz_cmss * 1000.0f * _pid_vel_z.get_error() * _pid_vel_z.kP() * POSCONTROL_VIBE_COMP_I_GAIN);
    }
    return POSCONTROL_VIBE_COMP_P_GAIN * thr_per_accelz_cmss * _accel_target.z + _pid_accel_z.get_i() * 0.001f;
}

/// standby_xyz_reset - resets I terms and removes position error
///     This function will let Loiter and Alt Hold continue to operate
///     in the event that the flight controller is in control of the
///     aircraft when in standby.
void AC_PosControl::standby_xyz_reset()
{
    // Set _pid_accel_z integrator to zero.
    _pid_accel_z.set_integrator(0.0f);

    // Set the target position to the current pos.
    _pos_target = _inav.get_position_neu_cm().topostype();

    // Set _pid_vel_xy integrator and derivative to zero.
    _pid_vel_xy.reset_filter();

    // initialise ekf xy reset handler
    init_ekf_xy_reset();
}

#if HAL_LOGGING_ENABLED
// write PSC and/or PSCZ logs
void AC_PosControl::write_log()
{
    if (is_active_xy()) {
        float accel_x, accel_y;
        lean_angles_to_accel_xy(accel_x, accel_y);
        Write_PSCN(_pos_desired.x, _pos_target.x, _inav.get_position_neu_cm().x,
                   _vel_desired.x, _vel_target.x, _inav.get_velocity_neu_cms().x,
                   _accel_desired.x, _accel_target.x, accel_x);
        Write_PSCE(_pos_desired.y, _pos_target.y, _inav.get_position_neu_cm().y,
                   _vel_desired.y, _vel_target.y, _inav.get_velocity_neu_cms().y,
                   _accel_desired.y, _accel_target.y, accel_y);

        // log offsets if they are being used
        if (!_pos_offset.xy().is_zero()) {
            Write_PSON(_pos_offset_target.x, _pos_offset.x, _vel_offset_target.x, _vel_offset.x, _accel_offset_target.x, _accel_offset.x);
            Write_PSOE(_pos_offset_target.y, _pos_offset.y, _vel_offset_target.y, _vel_offset.y, _accel_offset_target.y, _accel_offset.y);
        }
    }

    if (is_active_z()) {
        Write_PSCD(-_pos_desired.z, -_pos_target.z, -_inav.get_position_z_up_cm(),
                   -_vel_desired.z, -_vel_target.z, -_inav.get_velocity_z_up_cms(),
                   -_accel_desired.z, -_accel_target.z, -get_z_accel_cmss());

        // log down and terrain offsets if they are being used
        if (!is_zero(_pos_offset.z)) {
            Write_PSOD(-_pos_offset_target.z, -_pos_offset.z, -_vel_offset_target.z, -_vel_offset.z, -_accel_offset_target.z, -_accel_offset.z);
        }
        if (!is_zero(_pos_terrain)) {
            Write_PSOT(-_pos_terrain_target, -_pos_terrain, 0, -_vel_terrain, 0, -_accel_terrain);
        }
    }
}
#endif  // HAL_LOGGING_ENABLED

/// crosstrack_error - returns horizontal error to the closest point to the current track
float AC_PosControl::crosstrack_error() const
{
    const Vector2f pos_error = _inav.get_position_xy_cm() - (_pos_target.xy()).tofloat();
    if (is_zero(_vel_desired.xy().length_squared())) {
        // crosstrack is the horizontal distance to target when stationary
        return pos_error.length();
    } else {
        // crosstrack is the horizontal distance to the closest point to the current track
        const Vector2f vel_unit = _vel_desired.xy().normalized();
        const float dot_error = pos_error * vel_unit;

        // todo: remove MAX of zero when safe_sqrt fixed
        return safe_sqrt(MAX(pos_error.length_squared() - sq(dot_error), 0.0));
    }
}

///
/// private methods
///

/// Terrain

/// update_z_offsets - updates the vertical offsets used by terrain following
void AC_PosControl::update_terrain()
{
    // update position, velocity, accel offsets for this iteration
    postype_t pos_terrain = _pos_terrain;
    update_pos_vel_accel(pos_terrain, _vel_terrain, _accel_terrain, _dt, MIN(_limit_vector.z, 0.0f), _p_pos_z.get_error(), _pid_vel_z.get_error());
    _pos_terrain = pos_terrain;

    // input shape horizontal position, velocity and acceleration offsets
    shape_pos_vel_accel(_pos_terrain_target, 0.0, 0.0,
        _pos_terrain, _vel_terrain, _accel_terrain,
        get_max_speed_down_cms(), get_max_speed_up_cms(),
        -get_max_accel_z_cmss(), get_max_accel_z_cmss(),
        _jerk_max_z_cmsss, _dt, false);

    // we do not have to update _pos_terrain_target because we assume the target velocity and acceleration are zero
    // if we know how fast the terain altitude is changing we would add update_pos_vel_accel for _pos_terrain_target here
}

// get_lean_angles_to_accel - convert roll, pitch lean angles to NE frame accelerations in cm/s/s
void AC_PosControl::accel_to_lean_angles(float accel_x_cmss, float accel_y_cmss, float& roll_target, float& pitch_target) const
{
    // rotate accelerations into body forward-right frame
    const float accel_forward = accel_x_cmss * _ahrs.cos_yaw() + accel_y_cmss * _ahrs.sin_yaw();
    const float accel_right = -accel_x_cmss * _ahrs.sin_yaw() + accel_y_cmss * _ahrs.cos_yaw();

    // update angle targets that will be passed to stabilize controller
    pitch_target = accel_to_angle(-accel_forward * 0.01) * 100;
    float cos_pitch_target = cosf(pitch_target * M_PI / 18000.0f);
    roll_target = accel_to_angle((accel_right * cos_pitch_target)*0.01) * 100;
}

// lean_angles_to_accel_xy - convert roll, pitch lean target angles to NE frame accelerations in cm/s/s
// todo: this should be based on thrust vector attitude control
void AC_PosControl::lean_angles_to_accel_xy(float& accel_x_cmss, float& accel_y_cmss) const
{
    // rotate our roll, pitch angles into lat/lon frame
    Vector3f att_target_euler = _attitude_control.get_att_target_euler_rad();
    att_target_euler.z = _ahrs.yaw;
    Vector3f accel_cmss = lean_angles_to_accel(att_target_euler);

    accel_x_cmss = accel_cmss.x;
    accel_y_cmss = accel_cmss.y;
}

// calculate_yaw_and_rate_yaw - update the calculated the vehicle yaw and rate of yaw.
void AC_PosControl::calculate_yaw_and_rate_yaw()
{
    // Calculate the turn rate
    float turn_rate = 0.0f;
    const float vel_desired_xy_len = _vel_desired.xy().length();
    if (is_positive(vel_desired_xy_len)) {
        const float accel_forward = (_accel_desired.x * _vel_desired.x + _accel_desired.y * _vel_desired.y) / vel_desired_xy_len;
        const Vector2f accel_turn = _accel_desired.xy() - _vel_desired.xy() * accel_forward / vel_desired_xy_len;
        const float accel_turn_xy_len = accel_turn.length();
        turn_rate = accel_turn_xy_len / vel_desired_xy_len;
        if ((accel_turn.y * _vel_desired.x - accel_turn.x * _vel_desired.y) < 0.0) {
            turn_rate = -turn_rate;
        }
    }

    // update the target yaw if velocity is greater than 5% _vel_max_xy_cms
    if (vel_desired_xy_len > _vel_max_xy_cms * 0.05f) {
        _yaw_target = degrees(_vel_desired.xy().angle()) * 100.0f;
        _yaw_rate_target = turn_rate * degrees(100.0f);
        return;
    }

    // use the current attitude controller yaw target
    _yaw_target = _attitude_control.get_att_target_euler_cd().z;
    _yaw_rate_target = 0;
}

// calculate_overspeed_gain - calculated increased maximum acceleration and jerk if over speed condition is detected
float AC_PosControl::calculate_overspeed_gain()
{
    if (_vel_desired.z < _vel_max_down_cms && !is_zero(_vel_max_down_cms)) {
        return POSCONTROL_OVERSPEED_GAIN_Z * _vel_desired.z / _vel_max_down_cms;
    }
    if (_vel_desired.z > _vel_max_up_cms && !is_zero(_vel_max_up_cms)) {
        return POSCONTROL_OVERSPEED_GAIN_Z * _vel_desired.z / _vel_max_up_cms;
    }
    return 1.0;
}

/// initialise ekf xy position reset check
void AC_PosControl::init_ekf_xy_reset()
{
    Vector2f pos_shift;
    _ekf_xy_reset_ms = _ahrs.getLastPosNorthEastReset(pos_shift);
}

/// handle_ekf_xy_reset - check for ekf position reset and adjust loiter or brake target position
void AC_PosControl::handle_ekf_xy_reset()
{
    // check for position shift
    Vector2f pos_shift;
    uint32_t reset_ms = _ahrs.getLastPosNorthEastReset(pos_shift);
    if (reset_ms != _ekf_xy_reset_ms) {

        // ToDo: move EKF steps into the offsets for modes setting absolute position and velocity
        // for this we need some sort of switch to select what type of EKF handling we want to use

        // To zero real position shift during relative position modes like Loiter, PosHold, Guided velocity and accleration control.
        _pos_target.xy() = (_inav.get_position_xy_cm() + _p_pos_xy.get_error()).topostype();
        _pos_desired.xy() = _pos_target.xy() - _pos_offset.xy();
        _vel_target.xy() = _inav.get_velocity_xy_cms() + _pid_vel_xy.get_error();
        _vel_desired.xy() = _vel_target.xy() - _vel_offset.xy();

        _ekf_xy_reset_ms = reset_ms;
    }
}

/// initialise ekf z axis reset check
void AC_PosControl::init_ekf_z_reset()
{
    float alt_shift;
    _ekf_z_reset_ms = _ahrs.getLastPosDownReset(alt_shift);
}

/// handle_ekf_z_reset - check for ekf position reset and adjust loiter or brake target position
void AC_PosControl::handle_ekf_z_reset()
{
    // check for position shift
    float alt_shift;
    uint32_t reset_ms = _ahrs.getLastPosDownReset(alt_shift);
    if (reset_ms != 0 && reset_ms != _ekf_z_reset_ms) {

        // ToDo: move EKF steps into the offsets for modes setting absolute position and velocity
        // for this we need some sort of switch to select what type of EKF handling we want to use

        // To zero real position shift during relative position modes like Loiter, PosHold, Guided velocity and accleration control.
        _pos_target.z = _inav.get_position_z_up_cm() + _p_pos_z.get_error();
        _pos_desired.z = _pos_target.z - (_pos_offset.z + _pos_terrain);
        _vel_target.z = _inav.get_velocity_z_up_cms() + _pid_vel_z.get_error();
        _vel_desired.z = _vel_target.z - (_vel_offset.z + _vel_terrain);

        _ekf_z_reset_ms = reset_ms;
    }
}

bool AC_PosControl::pre_arm_checks(const char *param_prefix,
                                   char *failure_msg,
                                   const uint8_t failure_msg_len)
{
    if (!is_positive(get_pos_xy_p().kP())) {
        hal.util->snprintf(failure_msg, failure_msg_len, "%s_POSXY_P must be > 0", param_prefix);
        return false;
    }
    if (!is_positive(get_pos_z_p().kP())) {
        hal.util->snprintf(failure_msg, failure_msg_len, "%s_POSZ_P must be > 0", param_prefix);
        return false;
    }
    if (!is_positive(get_vel_z_pid().kP())) {
        hal.util->snprintf(failure_msg, failure_msg_len, "%s_VELZ_P must be > 0", param_prefix);
        return false;
    }
    if (!is_positive(get_accel_z_pid().kP())) {
        hal.util->snprintf(failure_msg, failure_msg_len, "%s_ACCZ_P must be > 0", param_prefix);
        return false;
    }
    if (!is_positive(get_accel_z_pid().kI())) {
        hal.util->snprintf(failure_msg, failure_msg_len, "%s_ACCZ_I must be > 0", param_prefix);
        return false;
    }

    return true;
}

// return true if on a real vehicle or SITL with lock-step scheduling
bool AC_PosControl::has_good_timing(void) const
{
#if CONFIG_HAL_BOARD == HAL_BOARD_SITL
    auto *sitl = AP::sitl();
    if (sitl) {
        return sitl->state.is_lock_step_scheduled;
    }
#endif
    // real boards are assumed to have good timing
    return true;
}

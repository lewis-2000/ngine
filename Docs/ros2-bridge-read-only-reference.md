# ROS 2 bridge read-only reference

This document records the robot-side ROS 2 information gathered for NGine. It
is intended as a reference before implementing the bridge. The current scope is
read-only telemetry only.

## Safety boundary

The bridge must not publish motion commands or call motor lifecycle services by
default. Live movement remains blocked until the vendor confirms command units,
joint mapping, limits, watchdog behavior, and safety requirements.

Do not currently use:

```text
/arm/cmd_pos
/leg/cmd_pos
/head/cmd_pos
/waist/cmd_pos
```

Do not call motor services such as `MotorStart`, `MotorStop`, zero-offset, or
position-reset services.

## ROS environment

- ROS 2 distribution: `humble`
- Custom package: `bodyctrl_msgs`
- Verified Ubuntu vendor workspace:

```text
/home/ubuntu/ros2_tg/src/x86/benti/TG2.0-Plus_linux-x86_ros2-general_v2.0.7_20260527_155108/install
```

- Complete configuration set:

```text
/opt/PARTITIONS/B/ros2ws/install/body_control/share/body_control/param/
```

Source only one vendor workspace after the system ROS installation:

```bash
source /opt/ros/humble/setup.bash
source /home/ubuntu/ros2_tg/src/x86/benti/TG2.0-Plus_linux-x86_ros2-general_v2.0.7_20260527_155108/install/setup.bash
```

## Read-only feedback topics

| Topic | Type | Purpose |
|---|---|---|
| `/arm/status` | `bodyctrl_msgs/msg/MotorStatusMsg` | Arm motor feedback |
| `/leg/status` | `bodyctrl_msgs/msg/MotorStatusMsg` | Leg motor feedback |
| `/head/status` | `bodyctrl_msgs/msg/MotorStatusMsg` | Head motor feedback |
| `/waist/status` | `bodyctrl_msgs/msg/MotorStatusMsg` | Waist motor feedback |
| `/bodycontrol_state` | `bodyctrl_msgs/msg/NodeState` | Body-control state |
| `/node/status` | `bodyctrl_msgs/msg/NodeState` | Process-manager state |
| `/power/board/status` | `bodyctrl_msgs/msg/PowerStatus` | Power, voltage, current, and temperature |
| `/imu/status` | `bodyctrl_msgs/msg/Imu` | IMU orientation and motion |
| `/arm_6dof_left` | `geometry_msgs/msg/WrenchStamped` | Left force/torque sensor |
| `/arm_6dof_right` | `geometry_msgs/msg/WrenchStamped` | Right force/torque sensor |

`/arm_6dof_left` and `/arm_6dof_right` are sensor topics, not movement
commands.

## Motor status message

```text
std_msgs/Header header
MotorStatus[] status

MotorStatus:
  uint16 name
  float32 pos
  float32 speed
  float32 current
  float32 temperature
  uint32 error
```

Observed status rates:

```text
Arm/head/waist status: approximately 500 Hz
Leg status:            approximately 1000 Hz
```

Observed status QoS:

```text
Reliability: RELIABLE
Durability: VOLATILE
```

## Motor ID groups

| Group | Motor IDs | Feedback topic |
|---|---:|---|
| Head | `1–3` | `/head/status` |
| Waist | `31` | `/waist/status` |
| Left arm | `11–17` | `/arm/status` |
| Right arm | `21–27` | `/arm/status` |
| Left leg | `51–56` | `/leg/status` |
| Right leg | `61–66` | `/leg/status` |

The configuration comments describe arm/head limits of `180` and `-180`, but
they do not establish whether ROS position values are degrees or radians.

## Body-control state observed

```text
/bodycontrol_state:
  state: 1

/node/status:
  state: 1
```

The message definition labels `1` as `NODE_STATE_RUNNING`.

Observed motor errors were zero for the arm and leg samples. Head motors
`1–3` and waist motor `31` reported error `33072`; the meaning of this code is
not yet known and those motors must be treated as faulted or unavailable.

## Power and IMU observations

The captured power sample reported approximately:

```text
Battery voltage: 51.7 V
Bus voltage:     52.0 V
Battery current: -3.8 A
Battery power:   100.0
```

The IMU sample reported `error: 0` and included quaternion, Euler angles,
angular velocity, linear acceleration, and covariance arrays.

## Vendor motor configuration

Configuration rows use this format:

```text
[name, canIds[], slaveId, channel, passages[], type, mode, rateWork, rateReq, ...]
```

Observed configuration rates:

```text
Arm/head/waist rateWork/rateReq: 400
Leg rateWork/rateReq:            1000
```

EtherCAT configuration observations:

```yaml
auto_init: true
enable_check_ready: true
enable_arms: true
enable_legs: true
enable_imu: true
enable_hands: true
enable_6dof: true
enable_head: true
enable_waist: true
enable_waist_home: true
enable_head_home: true
enable_power: false
selfMotor_idle_current: 0.3
```

## Deferred command interface

The vendor command message is:

```text
bodyctrl_msgs/msg/CmdSetMotorPosition

std_msgs/Header header
SetMotorPosition[] cmds

SetMotorPosition:
  uint16 name
  float32 pos
  float32 spd
  float32 cur
```

The `bodyctrl` node subscribes to these topics, but NGine must not publish to
them until the following are confirmed:

1. Whether `pos` is in radians, degrees, or another unit.
2. Whether `pos` is absolute or relative.
3. The units and safe ranges of `spd` and `cur`.
4. Exact joint-to-motor mapping and sign conventions.
5. Position, speed, and current limits.
6. Required command rate and watchdog timeout.
7. Required enable, mode, or safety state.
8. Meaning of error `33072`.

## Read-only implementation boundary

The first bridge should implement only:

```text
ROS feedback topics
    -> validated bridge telemetry
    -> NGine status/UI
```

It should include stale-data detection, error reporting, and a clear
disconnected state. Command publishers should be absent or hard-disabled in the
initial implementation, rather than merely hidden from the UI.

## Initial NGine telemetry relay

The initial implementation keeps ROS 2 on the robot computer and uses a
newline-delimited JSON/TCP relay to NGine. The relay is read-only: it creates
subscriptions only and does not publish ROS messages or call services.

The relay source is [ros2_telemetry_relay.py](../scripts/ros2_telemetry_relay.py).
After sourcing ROS 2 and the vendor workspace on the robot computer, start it
with:

```bash
python3 /path/to/ngine/scripts/ros2_telemetry_relay.py \
  --bind 0.0.0.0 \
  --port 8765 \
  --rate 20 \
  --camera-rate 5
```

The relay also subscribes read-only to the confirmed RGB camera topic
`/camera/color/image_raw/compressed` (`sensor_msgs/msg/CompressedImage`).
It forwards the latest JPEG payload as base64 in the telemetry JSON under
`camera_jpeg`, limited to `--camera-rate` frames per second. NGine decodes the
frame and displays it in **View > Camera preview** while preserving its aspect
ratio. No camera-control service is called.

NGine connects to the Ubuntu host at `192.168.41.1:8765` by default, matching the
current SSH connection `ubuntu@192.168.41.1`. If the host address changes, the
client default in `RosTelemetryClient.h` must be updated before rebuilding.
The relay should be protected by a firewall or restricted to a trusted
interface; it contains no authentication.

Each line sent to NGine has this shape:

```json
{"type":"telemetry","bodycontrol_state":1,"process_state":1,
 "battery_voltage":51.7,"battery_current":-3.8,"battery_power":100.0,
 "imu_roll":0.01,"imu_pitch":0.29,"imu_yaw":2.56,
 "motors":[{"id":11,"position":0.0,"speed":0.0,"current":0.0,
            "temperature":36.0,"error":0}]}
```

The NGine `Robot telemetry` panel is read-only and displays connection state,
body-control/process state, battery/IMU values, and motor telemetry. The
command topics remain unused.

When enabled in the panel, healthy feedback positions can also drive the
simulated model for visual comparison. This is an explicit opt-in display
feature; it does not publish commands back to ROS 2. The current mapping uses
the vendor configuration order for the arms and legs, plus the waist motor:

```text
11–17 -> left_joint1, shoulder_roll_l_joint, left_joint3, elbow_l_joint,
         left_joint5, left_joint6, left_joint7
21–27 -> right_joint1, shoulder_roll_r_joint, right_joint3, elbow_r_joint,
         right_joint5, right_joint6, right_joint7
51–56 -> hip_roll_l_joint, hip_yaw_l_joint, hip_pitch_l_joint,
         knee_pitch_l_joint, ankle_pitch_l_joint, ankle_roll_l_joint
61–66 -> hip_roll_r_joint, hip_yaw_r_joint, hip_pitch_r_joint,
         knee_pitch_r_joint, ankle_pitch_r_joint, ankle_roll_r_joint
31    -> waist_joint
```

The model option is disabled by default and applies the values as received;
there is currently no degree/radian conversion because the vendor unit has
not been confirmed.

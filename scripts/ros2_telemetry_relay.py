#!/usr/bin/env python3
"""ROS 2 telemetry relay for NGine.

Run this on the ROS 2 computer. Telemetry remains read-only unless the explicit
command-publishing option is enabled.

IMPORTANT: This script should be run on Host 1 that has the topics for all the joints, camera and depth. If it is run on host 2 the topics will not be available and the script will not work. The NGine instance should be run on Host 2.
"""

import argparse
import base64
import json
import socket
import threading
import time
from typing import Any, Optional

import rclpy
from bodyctrl_msgs.msg import (
    CmdSetMotorPosition,
    Imu,
    MotorStatusMsg,
    NodeState,
    PowerStatus,
    SetMotorPosition,
)
from rosidl_runtime_py.utilities import get_message
from sensor_msgs.msg import CompressedImage, Image
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data


SUPPORTED_ROS_TOPICS = (
    "/alarm",
    "/arm/cmd_ctrl",
    "/arm/cmd_current",
    "/arm/cmd_dis",
    "/arm/cmd_pos",
    "/arm/cmd_set_zero",
    "/arm/cmd_vel",
    "/arm/motor_status",
    "/arm/status",
    "/arm_6dof_left",
    "/arm_6dof_right",
    "/audio/config/notify",
    "/audio_chat/enable",
    "/bag_record/control/notify",
    "/bodycontrol_state",
    "/diagnostics",
    "/head/cmd_ctrl",
    "/head/cmd_dis",
    "/head/cmd_pos",
    "/head/cmd_set_zero",
    "/head/cmd_vel",
    "/head/motor_status",
    "/head/status",
    "/hric/motion/status",
    "/hric/robot/action_status",
    "/hric/robot/cmd_vel",
    "/hric/robot/float_base_rpyz_cmd",
    "/hric/robot/float_base_rpyz_status",
    "/hric/robot/get_vel_limit",
    "/hric/robot/set_vel_limit",
    "/imu",
    "/imu/offset_notify",
    "/imu/status",
    "/inspire_hand/ctrl/left_hand",
    "/inspire_hand/ctrl/right_hand",
    "/inspire_hand/error/left_hand",
    "/inspire_hand/error/right_hand",
    "/inspire_hand/state/left_hand",
    "/inspire_hand/state/right_hand",
    "/leg/cmd_ctrl",
    "/leg/cmd_dis",
    "/leg/cmd_pos",
    "/leg/cmd_set_zero",
    "/leg/cmd_vel",
    "/leg/motor_status",
    "/leg/status",
    "/motion/offset_notify",
    "/node/status",
    "/ob_camera_head/color/camera_info",
    "/ob_camera_head/color/image_raw",
    "/ob_camera_head/color/image_raw/compressed",
    "/ob_camera_head/color/image_raw/theora",
    "/ob_camera_head/color/metadata",
    "/ob_camera_head/depth/camera_info",
    "/ob_camera_head/depth/image_raw",
    "/ob_camera_head/depth/image_raw/compressedDepth",
    "/ob_camera_head/depth/metadata",
    "/ob_camera_head/depth_filter_status",
    "/ob_camera_head/device_status",
    "/parameter_events",
    "/power/battery/status",
    "/power/board/ctrl",
    "/power/board/key_status",
    "/power/board/status",
    "/power/light/ctrl",
    "/proc_manager/config/notify",
    "/restart_ec_slave",
    "/robot/trace/event_data",
    "/rosout",
    "/sbus_data",
    "/sbus_data/event",
    "/system/diagnose/fault_report",
    "/system/diagnose/faults_report",
    "/tf",
    "/tf_static",
    "/waist/cmd_ctrl",
    "/waist/cmd_dis",
    "/waist/cmd_pos",
    "/waist/cmd_set_zero",
    "/waist/cmd_vel",
    "/waist/motor_status",
    "/waist/status",
    "/walker/action",
    "/walker/enable_gaze",
    "/walker/gaze_target",
    "/walker/status",
    "/xsys/light/ctrl",
    "/xunfei/tts_play",
)


class TelemetryRelay(Node):
    # Limit the size of the camera frame to avoid sending huge frames that could cause issues with NGine.
    _MAX_CAMERA_BYTES = 4 * 1024 * 1024

    # Initialize the telemetry relay node with the given parameters.
    def __init__(
        self,
        server: socket.socket,
        rate: float,
        camera_rate: float,
        depth_scale: int,
        enable_camera: bool,
        enable_depth: bool,
        enable_command_publishing: bool,
    ) -> None:
        super().__init__("ngine_read_only_telemetry_relay")
        self._server = server
        self._client: Optional[socket.socket] = None
        self._client_lock = threading.Lock()
        self._client_send_lock = threading.Lock()
        self._lock = threading.Lock()
        self._rate = rate
        self._camera_rate = camera_rate
        self._depth_scale = max(1, depth_scale)
        self._enable_camera = enable_camera
        self._enable_depth = enable_depth
        self._enable_command_publishing = enable_command_publishing
        self._right_arm_command_publisher = (
            self.create_publisher(CmdSetMotorPosition, "/arm/cmd_pos", 10)
            if enable_command_publishing
            else None
        )
        self.get_logger().info(
            "Right arm command publishing is "
            + ("ENABLED" if enable_command_publishing else "DISABLED")
        )
        self._camera_jpeg = ""
        self._depth_data = ""
        self._depth_width = 0
        self._depth_height = 0
        self._last_camera_send = 0.0
        self._last_depth_process = 0.0
        self._last_depth_encoding = ""
        self._motors: dict[int, dict[str, Any]] = {}
        self._topic_subscriptions: list[Any] = []
        self._topic_counts: dict[str, int] = {}
        self._data: dict[str, Any] = {
            "bodycontrol_state": -1,
            "process_state": -1,
            "battery_voltage": 0.0,
            "battery_current": 0.0,
            "battery_power": 0.0,
            "imu_roll": 0.0,
            "imu_pitch": 0.0,
            "imu_yaw": 0.0,
        }

    # Create the necessary subscriptions for the telemetry data.
        for topic in ("/arm/status", "/leg/status", "/head/status", "/waist/status"):
            self.create_subscription(
                MotorStatusMsg,
                topic,
                self._motor_status_callback,
                10,
            )
        self.create_subscription(
            NodeState,
            "/bodycontrol_state",
            self._body_state_callback,
            10,
        )
        self.create_subscription(
            NodeState,
            "/node/status",
            self._process_state_callback,
            10,
        )
        self.create_subscription(
            PowerStatus,
            "/power/board/status",
            self._power_callback,
            10,
        )
        self.create_subscription(Imu, "/imu/status", self._imu_callback, 10)
        if self._enable_camera:
            self.create_subscription(
                CompressedImage,
                "/ob_camera_head/color/image_raw/compressed",
                self._camera_callback,
                qos_profile_sensor_data,
            )
        if self._enable_depth:
            self.create_subscription(
                Image,
                "/ob_camera_head/depth/image_raw",
                self._depth_callback,
                qos_profile_sensor_data,
            )
        self._subscribe_to_additional_topics()
        self.create_timer(1.0 / rate, self._send_snapshot)
        self.create_timer(0.25, self._accept_client)

    def _subscribe_to_additional_topics(self) -> None:
        handled_topics = {
            "/arm/status",
            "/leg/status",
            "/head/status",
            "/waist/status",
            "/bodycontrol_state",
            "/node/status",
            "/power/board/status",
            "/imu/status",
        }
        handled_topics.update(
            {
                "/ob_camera_head/color/image_raw/compressed",
                "/ob_camera_head/depth/image_raw",
            }
        )

        available_topics = dict(self.get_topic_names_and_types())
        for topic in SUPPORTED_ROS_TOPICS:
            if topic in handled_topics:
                continue
            topic_types = available_topics.get(topic)
            if not topic_types:
                self.get_logger().warning(
                    f"ROS topic is not currently available: {topic}")
                continue
            try:
                message_type = get_message(topic_types[0])
                subscription = self.create_subscription(
                    message_type,
                    topic,
                    lambda message, topic_name=topic: self._topic_callback(
                        topic_name, message),
                    qos_profile_sensor_data,
                )
            except (ImportError, AttributeError, RuntimeError, TypeError, ValueError) as error:
                self.get_logger().error(
                    f"Could not subscribe to {topic} ({topic_types[0]}): {error}")
                continue
            self._topic_subscriptions.append(subscription)
            self._topic_counts[topic] = 0

    def _topic_callback(self, topic: str, _message: Any) -> None:
        with self._lock:
            self._topic_counts[topic] += 1

    def close_client(self, expected: Optional[socket.socket] = None) -> None:
        with self._client_lock:
            if expected is not None and self._client is not expected:
                return
            client = self._client
            self._client = None
        if client is not None:
            try:
                client.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            client.close()

    def _accept_client(self) -> None:
        if self._client is not None:
            return
        try:
            client, address = self._server.accept()
        except BlockingIOError:
            return
        except OSError as error:
            self.get_logger().error(f"Telemetry accept failed: {error}")
            return

        client.settimeout(1.0)
        with self._client_lock:
            self._client = client
        threading.Thread(
            target=self._receive_commands,
            args=(client,),
            daemon=True,
        ).start()
        self.get_logger().info(
            f"NGine connected from {address[0]}:{address[1]}")

    def _receive_commands(self, client: socket.socket) -> None:
        buffer = b""
        while True:
            with self._client_lock:
                if client is not self._client:
                    return
            try:
                data = client.recv(4096)
            except socket.timeout:
                continue
            except OSError:
                self.close_client(client)
                return
            if not data:
                self.close_client(client)
                return
            buffer += data
            while b"\n" in buffer:
                raw_line, buffer = buffer.split(b"\n", 1)
                try:
                    self._handle_command(json.loads(raw_line.decode("utf-8")))
                except (
                    UnicodeDecodeError,
                    json.JSONDecodeError,
                    KeyError,
                    TypeError,
                    ValueError,
                ) as error:
                    self.get_logger().error(f"Invalid NGine command: {error}")

    def _handle_command(self, command: dict[str, Any]) -> None:
        if command.get("type") != "right_arm_motor_command":
            return
        if not self._enable_command_publishing or self._right_arm_command_publisher is None:
            self.get_logger().error(
                "Rejected right arm motor command: command publishing is disabled")
            return
        motor_id = int(command["motor_id"])
        position = float(command["position"])
        self.get_logger().info(
            f"Received right arm motor command: motor={motor_id}, position={position}"
        )
        if motor_id not in range(21, 28) or not -180.0 <= position <= 180.0:
            raise ValueError("right arm motor command is outside the safe test range")
        motor_command = SetMotorPosition()
        motor_command.name = motor_id
        motor_command.pos = position
        motor_command.spd = 1.0
        motor_command.cur = 0.0
        message = CmdSetMotorPosition()
        message.cmds = [motor_command]
        self._right_arm_command_publisher.publish(message)
        self.get_logger().info(
            f"Published /arm/cmd_pos: motor={motor_id}, position={position}"
        )

        
    # Callback for motor status messages. Updates the internal motor status dictionary with the latest data.
    def _motor_status_callback(self, message: MotorStatusMsg) -> None:
        with self._lock:
            for motor in message.status:
                self._motors[int(motor.name)] = {
                    "id": int(motor.name),
                    "position": float(motor.pos),
                    "speed": float(motor.speed),
                    "current": float(motor.current),
                    "temperature": float(motor.temperature),
                    "error": int(motor.error),
                }

    # Callback for body state messages. Updates the internal data dictionary with the latest body control state.
    def _body_state_callback(self, message: NodeState) -> None:
        with self._lock:
            self._data["bodycontrol_state"] = int(message.state)

    # Callback for process state messages. Updates the internal data dictionary with the latest process state.
    def _process_state_callback(self, message: NodeState) -> None:
        with self._lock:
            self._data["process_state"] = int(message.state)

    # Callback for power status messages. Updates the internal data dictionary with the latest battery voltage, current, and power.
    def _power_callback(self, message: PowerStatus) -> None:
        with self._lock:
            self._data["battery_voltage"] = float(message.battery_voltage)
            self._data["battery_current"] = float(message.battery_current)
            self._data["battery_power"] = float(message.battery_power)

    # Callback for IMU messages. Updates the internal data dictionary with the latest IMU data.
    def _imu_callback(self, message: Imu) -> None:
        with self._lock:
            self._data["imu_roll"] = float(message.euler.roll)
            self._data["imu_pitch"] = float(message.euler.pitch)
            self._data["imu_yaw"] = float(message.euler.yaw)

    # Callback for camera messages. Encodes the received JPEG image data in base64 and stores it for later transmission.
    def _camera_callback(self, message: CompressedImage) -> None:
        with self._lock:
            if not message.data:
                self.get_logger().warning("Received an empty RGB camera frame")
                return
            if len(message.data) > self._MAX_CAMERA_BYTES:
                self.get_logger().warning(
                    "Dropping RGB camera frame larger than "
                    f"{self._MAX_CAMERA_BYTES // (1024 * 1024)} MiB "
                    f"({len(message.data)} bytes)")
                return
            self._camera_jpeg = base64.b64encode(bytes(message.data)).decode("ascii")

    # Callback for depth messages. Processes the received depth image data and stores it for later transmission.
    def _depth_callback(self, message: Image) -> None:
        if message.encoding not in ("16UC1", "mono16"):
            if message.encoding != self._last_depth_encoding:
                self.get_logger().warning(
                    f"Unsupported depth encoding: {message.encoding}")
                self._last_depth_encoding = message.encoding
            return
        now = time.monotonic()
        if now - self._last_depth_process < 1.0 / self._camera_rate:
            return
        self._last_depth_process = now
        scale = self._depth_scale
        source_width = int(message.width)
        source_height = int(message.height)
        source_step = int(message.step)
        raw = bytes(message.data)
        target_width = source_width // scale
        target_height = source_height // scale
        if (
            target_width <= 0
            or target_height <= 0
            or source_step < source_width * 2
            or len(raw) < source_step * source_height
        ):
            return

        downsampled = bytearray(target_width * target_height * 2)
        for target_y in range(target_height):
            source_row = (target_y * scale) * source_step
            target_row = target_y * target_width * 2
            for target_x in range(target_width):
                source_offset = source_row + (target_x * scale) * 2
                target_offset = target_row + target_x * 2
                if message.is_bigendian:
                    downsampled[target_offset] = raw[source_offset + 1]
                    downsampled[target_offset + 1] = raw[source_offset]
                else:
                    downsampled[target_offset:target_offset + 2] = raw[
                        source_offset:source_offset + 2]

        with self._lock:
            self._depth_data = base64.b64encode(downsampled).decode("ascii")
            self._depth_width = target_width
            self._depth_height = target_height

    # Send a snapshot of the current telemetry data to the connected NGine client.
    def _send_snapshot(self) -> None:
        with self._client_lock:
            client = self._client
        if client is None:
            return
        with self._lock:
            payload = dict(self._data)
            payload["type"] = "telemetry"
            payload["motors"] = list(self._motors.values())
            payload["topic_counts"] = dict(self._topic_counts)
            now = time.monotonic()
            if (
                self._enable_camera
                and self._camera_jpeg
                and now - self._last_camera_send >= 1.0 / self._camera_rate
            ):
                payload["camera_jpeg"] = self._camera_jpeg
                self._last_camera_send = now
            if self._enable_depth and self._depth_data:
                payload["depth_raw16"] = self._depth_data
                payload["depth_width"] = self._depth_width
                payload["depth_height"] = self._depth_height
        encoded = (json.dumps(payload, separators=(",", ":")) + "\n").encode("utf-8")
        try:
            with self._client_send_lock:
                client.sendall(encoded)
        except OSError as error:
            self.get_logger().error(f"NGine telemetry client disconnected: {error}")
            self.close_client(client)


def main() -> None:

    # Parse command-line arguments for the telemetry relay script.
    parser = argparse.ArgumentParser()
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--rate", type=float, default=20.0)
    parser.add_argument("--camera-rate", type=float, default=1.0)
    parser.add_argument("--depth-scale", type=int, default=4)
    parser.add_argument(
        "--disable-camera",
        action="store_true",
        help="Do not subscribe to or forward the RGB camera stream",
    )
    parser.add_argument(
        "--disable-depth",
        action="store_true",
        help="Do not subscribe to or forward the depth stream",
    )
    parser.add_argument(
        "--enable-command-publishing",
        action="store_true",
        help="Enable the opt-in right-arm motor test command publisher",
    )
    args = parser.parse_args()

    # Set up a TCP server socket to listen for incoming NGine telemetry client connections.
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((args.bind, args.port))
    server.listen(1)
    server.setblocking(False)
    print(f"Waiting for NGine telemetry client on {args.bind}:{args.port}")

    # Initialize the ROS 2 node and start spinning to process incoming telemetry data and handle client connections.
    rclpy.init()
    node = TelemetryRelay(
        server,
        args.rate,
        args.camera_rate,
        args.depth_scale,
        not args.disable_camera,
        not args.disable_depth,
        args.enable_command_publishing,
    )
    try:
        # Spin the ROS 2 node to process incoming telemetry data and handle client connections.
        rclpy.spin(node)
    finally:
        print("Shutting down NGine telemetry relay...")
        node.close_client()
        node.destroy_node()
        server.close()
        rclpy.shutdown()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Read-only ROS 2 telemetry relay for NGine.

Run this on the ROS 2 computer. It never creates publishers or service clients.
It forwards newline-delimited JSON over TCP to an NGine instance.
"""

import argparse
import base64
import json
import socket
import threading
import time
from typing import Any, Optional

import rclpy
from bodyctrl_msgs.msg import Imu, MotorStatusMsg, NodeState, PowerStatus
from sensor_msgs.msg import CompressedImage, Image
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data


class TelemetryRelay(Node):
    def __init__(
        self,
        server: socket.socket,
        rate: float,
        camera_rate: float,
        depth_scale: int,
        enable_camera: bool,
        enable_depth: bool,
    ) -> None:
        super().__init__("ngine_read_only_telemetry_relay")
        self._server = server
        self._client: Optional[socket.socket] = None
        self._lock = threading.Lock()
        self._rate = rate
        self._camera_rate = camera_rate
        self._depth_scale = max(1, depth_scale)
        self._enable_camera = enable_camera
        self._enable_depth = enable_depth
        self._camera_jpeg = ""
        self._depth_data = ""
        self._depth_width = 0
        self._depth_height = 0
        self._last_camera_send = 0.0
        self._last_depth_process = 0.0
        self._motors: dict[int, dict[str, Any]] = {}
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
                "/camera/color/image_raw/compressed",
                self._camera_callback,
                qos_profile_sensor_data,
            )
        if self._enable_depth:
            self.create_subscription(
                Image,
                "/camera/depth/image_raw",
                self._depth_callback,
                qos_profile_sensor_data,
            )
        self.create_timer(1.0 / rate, self._send_snapshot)
        self.create_timer(0.25, self._accept_client)

    def close_client(self) -> None:
        if self._client is not None:
            self._client.close()
            self._client = None

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
        self._client = client
        self.get_logger().info(
            f"NGine connected from {address[0]}:{address[1]}")

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

    def _body_state_callback(self, message: NodeState) -> None:
        with self._lock:
            self._data["bodycontrol_state"] = int(message.state)

    def _process_state_callback(self, message: NodeState) -> None:
        with self._lock:
            self._data["process_state"] = int(message.state)

    def _power_callback(self, message: PowerStatus) -> None:
        with self._lock:
            self._data["battery_voltage"] = float(message.battery_voltage)
            self._data["battery_current"] = float(message.battery_current)
            self._data["battery_power"] = float(message.battery_power)

    def _imu_callback(self, message: Imu) -> None:
        with self._lock:
            self._data["imu_roll"] = float(message.euler.roll)
            self._data["imu_pitch"] = float(message.euler.pitch)
            self._data["imu_yaw"] = float(message.euler.yaw)

    def _camera_callback(self, message: CompressedImage) -> None:
        with self._lock:
            if len(message.data) <= 2 * 1024 * 1024:
                self._camera_jpeg = base64.b64encode(bytes(message.data)).decode("ascii")

    def _depth_callback(self, message: Image) -> None:
        if message.encoding != "16UC1" or message.is_bigendian:
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
                downsampled[target_offset:target_offset + 2] = raw[source_offset:source_offset + 2]

        with self._lock:
            self._depth_data = base64.b64encode(downsampled).decode("ascii")
            self._depth_width = target_width
            self._depth_height = target_height

    def _send_snapshot(self) -> None:
        if self._client is None:
            return
        with self._lock:
            payload = dict(self._data)
            payload["type"] = "telemetry"
            payload["motors"] = list(self._motors.values())
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
            self._client.sendall(encoded)
        except OSError as error:
            self.get_logger().error(f"NGine telemetry client disconnected: {error}")
            self.close_client()


def main() -> None:
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
    args = parser.parse_args()

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((args.bind, args.port))
    server.listen(1)
    server.setblocking(False)
    print(f"Waiting for NGine telemetry client on {args.bind}:{args.port}")

    rclpy.init()
    node = TelemetryRelay(
        server,
        args.rate,
        args.camera_rate,
        args.depth_scale,
        not args.disable_camera,
        not args.disable_depth,
    )
    try:
        rclpy.spin(node)
    finally:
        node.close_client()
        node.destroy_node()
        server.close()
        rclpy.shutdown()


if __name__ == "__main__":
    main()

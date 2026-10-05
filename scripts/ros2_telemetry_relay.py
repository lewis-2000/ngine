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
from typing import Any

import rclpy
from bodyctrl_msgs.msg import Imu, MotorStatusMsg, NodeState, PowerStatus
from sensor_msgs.msg import CompressedImage
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data


class TelemetryRelay(Node):
    def __init__(self, client: socket.socket, rate: float, camera_rate: float) -> None:
        super().__init__("ngine_read_only_telemetry_relay")
        self._client = client
        self._lock = threading.Lock()
        self._rate = rate
        self._camera_rate = camera_rate
        self._camera_jpeg = ""
        self._last_camera_send = 0.0
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
        self.create_subscription(
            CompressedImage,
            "/camera/color/image_raw/compressed",
            self._camera_callback,
            qos_profile_sensor_data,
        )
        self.create_timer(1.0 / rate, self._send_snapshot)

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

    def _send_snapshot(self) -> None:
        with self._lock:
            payload = dict(self._data)
            payload["type"] = "telemetry"
            payload["motors"] = list(self._motors.values())
            now = time.monotonic()
            if self._camera_jpeg and now - self._last_camera_send >= 1.0 / self._camera_rate:
                payload["camera_jpeg"] = self._camera_jpeg
                self._last_camera_send = now
        encoded = (json.dumps(payload, separators=(",", ":")) + "\n").encode("utf-8")
        try:
            self._client.sendall(encoded)
        except OSError as error:
            self.get_logger().error(f"NGine telemetry client disconnected: {error}")
            rclpy.shutdown()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--rate", type=float, default=20.0)
    parser.add_argument("--camera-rate", type=float, default=5.0)
    args = parser.parse_args()

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((args.bind, args.port))
    server.listen(1)
    print(f"Waiting for NGine telemetry client on {args.bind}:{args.port}")
    client, address = server.accept()
    print(f"NGine connected from {address[0]}:{address[1]}")
    client.settimeout(None)

    rclpy.init()
    node = TelemetryRelay(client, args.rate, args.camera_rate)
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        client.close()
        server.close()
        rclpy.shutdown()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import serial
import serial.tools.list_ports
import time
import threading


class STM32Controller:
    def __init__(self, port=None, baudrate=115200, timeout=1):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.serial = None
        self.connected = False
        self.running = False
        self.monitor_thread = None
        self.reconnect_attempts = 0

    def list_ports(self):
        ports = serial.tools.list_ports.comports()
        print("\n" + "=" * 50)
        print("Доступные COM-порты:")
        print("=" * 50)
        for i, port in enumerate(ports):
            is_stm32 = "STMicroelectronics" in port.description or "Virtual COM Port" in port.description
            marker = " [STM32]" if is_stm32 else ""
            print(f"  {i+1}. {port.device} - {port.description}{marker}")
        print("=" * 50)
        return ports

    def _force_close(self):
        """Закрытие порта"""
        if self.serial:
            try:
                if self.serial.is_open:
                    self.serial.close()
            except:
                pass
            self.serial = None
        self.connected = False

    def connect(self, retry=3):
        if self.port is None:
            ports = self.list_ports()
            if not ports:
                print("❌ Нет доступных COM-портов!")
                return False
            try:
                choice = input("\nВыберите номер порта (или 'q' для выхода): ")
                if choice.lower() == 'q':
                    return False
                idx = int(choice) - 1
                if 0 <= idx < len(ports):
                     self.port = ports[idx].device
                else:
                    print("Неверный выбор!")
                    return False
            except ValueError:
                print("Неверный ввод!")
                return False

        self._force_close()
        time.sleep(0.2)

        for attempt in range(retry):
            try:
                self.serial = serial.Serial(
                    port=self.port,
                    baudrate=self.baudrate,
                    timeout=self.timeout,
                    write_timeout=2
                )
                self.connected = True
                self.running = True
                print(f"\n✅ Подключено к {self.port} ({self.baudrate} бод)")
                time.sleep(1)
                return True
            except serial.SerialException as e:
                if "PermissionError" in str(e) or "Access is denied" in str(e):
                    if attempt < retry - 1:
                        print(f"⚠️ Порт {self.port} занят. Попытка {attempt+1}/{retry}")
                        time.sleep(1)
                else:
                    print(f"❌ Ошибка: {e}")
                    return False

        print(f"\n❌ Не удалось подключиться к {self.port}")
        return False

    def disconnect(self):
        self.running = False
        if self.monitor_thread and self.monitor_thread.is_alive():
            if threading.current_thread() != self.monitor_thread:
                self.monitor_thread.join(timeout=0.5)
        self._force_close()

    def start_monitor(self, callback=None):
        if not self.connected:
            return

        if self.monitor_thread and self.monitor_thread.is_alive():
            return

        def monitor_loop():
            while self.running:
                try:
                    if self.serial and self.serial.is_open:
                        if self.serial.in_waiting > 0:
                            line = self.serial.readline().decode('utf-8', errors='ignore').strip()
                            if line:
                                if callback:
                                    callback(line)
                                else:
                                    print(f"📩 [STM32] {line}")
                    time.sleep(0.01)
                except serial.SerialException:
                    print("\n⚠️ Порт потерян! Переподключаюсь...")
                    self.connected = False
                    self.running = False
                    if self.monitor_thread and self.monitor_thread.is_alive():
                        if threading.current_thread() != self.monitor_thread:
                            self.monitor_thread.join(timeout=0.5)
                    self.reconnect()
                    break
                except Exception as e:
                    print(f"⚠️ Ошибка: {e}")
                    break

        self.monitor_thread = threading.Thread(target=monitor_loop, daemon=True)
        self.monitor_thread.start()

    def reconnect(self):
        """Быстрое переподключение (порт освобождается аппаратно)"""
        print(f"\n🔄 Переподключение к {self.port}...")

        self.running = False
        if self.monitor_thread and self.monitor_thread.is_alive():
            if threading.current_thread() != self.monitor_thread:
                self.monitor_thread.join(timeout=0.5)
            else:
                time.sleep(0.1)

        self._force_close()
        time.sleep(0.2)

        self.connected = False
        self.reconnect_attempts = 0

        max_wait = 5
        waited = 0

        while not self.connected and waited < max_wait:
            available_ports = [p.device for p in serial.tools.list_ports.comports()]

            if self.port in available_ports:
                try:
                    self.serial = serial.Serial(
                        port=self.port,
                        baudrate=self.baudrate,
                        timeout=self.timeout,
                        write_timeout=2
                    )
                    self.connected = True
                    self.running = True
                    self.reconnect_attempts = 0
                    print(f"\n✅ Переподключено к {self.port}")
                    time.sleep(0.2)

                    self.monitor_thread = None
                    self.start_monitor()
                    return True

                except serial.SerialException:
                    time.sleep(0.3)
                waited += 1
            else:
                waited += 1
                if waited % 3 == 0:
                    print(f"⏳ Ожидание порта {self.port}... ({waited}/{max_wait} сек)")
                time.sleep(0.3)

        if not self.connected:
            print(f"❌ Не удалось переподключиться за {max_wait} секунд")
        return False

    def send_command(self, command):
        if command.lower().startswith('reset'):
            print("⚠️ Команды с 'reset' заблокированы!")
            return None

        if not self.connected:
            if self.reconnect():
                return self.send_command(command)
            return None

        try:
            if not self.serial or not self.serial.is_open:
                raise serial.SerialException("Port not open")
            self.serial.write(f"{command}\r\n".encode('utf-8'))
            return "OK"
        except serial.SerialException:
            self.connected = False
            if self.reconnect():
                return self.send_command(command)
            return None


def get_commands():
    return {
        "help": "Показать справку",
        "clear": "Очистить OLED",
        "angle <link> <deg>": "Повернуть звено (0,1,2)",
        "servo <deg>": "Повернуть серво",
        "claw open/close": "Открыть/закрыть захват",
        "home": "Вернуться в 90°",
        "take_object <x> <y> <z>": "Захват объекта",
        "<any text>": "Вывести текст на OLED"
    }


def show_commands():
    cmds = get_commands()
    print("\n" + "=" * 50)
    print("ДОСТУПНЫЕ КОМАНДЫ:")
    print("=" * 50)
    for cmd, desc in cmds.items():
        print(f"  {cmd:25} - {desc}")
    print("=" * 50)


def main():
    print("=" * 60)
    print("   🚀 STM32 Controller v5.0 (упрощённый)")
    print("=" * 60)

    controller = STM32Controller()

    if not controller.connect():
        print("\n💡 Закройте программы, использующие COM-порт")
        return

    controller.start_monitor()
    show_commands()

    print("\n" + "=" * 50)
    print("💬 Введите команду (или 'quit' для выхода):")
    print("=" * 50)

    try:
        while True:
            cmd = input("\n> ").strip()
            if cmd.lower() in ['quit', 'exit', 'q']:
                break
            if not cmd:
                continue
            if cmd.lower().startswith('reset'):
                print("⚠️ 'reset' заблокирована!")
                continue
            if cmd == "ports":
                controller.list_ports()
                continue
            elif cmd == "help":
                show_commands()
                continue
            elif cmd == "reconnect":
                controller.reconnect()
                continue

            print(f"📤 Отправка: {cmd}")
            response = controller.send_command(cmd)
            if response is None:
                print("⚠️  Команда не отправлена")

    except KeyboardInterrupt:
        print("\n🛑 Прервано")
    finally:
        controller.disconnect()
        print("\n👋 Завершено")


if __name__ == "__main__":
    main()
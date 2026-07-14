import serial, time
from datetime import datetime, timedelta

PORT = 'COM21' 
BAUDRATE = 115200

try:
    with serial.Serial(PORT, BAUDRATE) as ser:
        time.sleep(2)
        message = (datetime.now() + timedelta(seconds=1)).strftime('%Y %m %d %H %M %S')
        ser.write(message.encode())
        print(f"Sent: {message.strip()}")
        while True:
            if ser.in_waiting > 0:
                raw_data = ser.readline()
                decoded_string = raw_data.decode('utf-8').rstrip()
                print(f"Received: {decoded_string}")

except serial.SerialException as e:
    print(f"Error opening or communicating on port {PORT}: {e}")

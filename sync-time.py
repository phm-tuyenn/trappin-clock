import serial, time
from datetime import datetime

PORT = 'COM21' 
BAUDRATE = 115200

try:
    with serial.Serial(PORT, BAUDRATE) as ser:
        message = datetime.now().strftime('%Y %m %d %H %M %S') + "\n"
        ser.write(message.encode('utf-8'))
        print(f"Sent: {message.strip()}")
        
except serial.SerialException as e:
    print(f"Error opening or communicating on port {PORT}: {e}")

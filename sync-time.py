import serial, time
from datetime import datetime

PORT = 'COM21' 
BAUDRATE = 115200

try:
    with serial.Serial(PORT, BAUDRATE) as ser:
        time.sleep(2)
        message = (datetime.now()).strftime('%Y %m %d %H %M %S')
        ser.write(message.encode())
        print(f"Sent: {message.strip()}")
        while True:
            # Check if there is data waiting in the buffer
            if ser.in_waiting > 0:
                # Read a line from the serial port
                raw_data = ser.readline()
                
                # Decode the bytes into a readable string and remove trailing spacing
                decoded_string = raw_data.decode('utf-8').rstrip()
                
                print(f"Received: {decoded_string}")

except serial.SerialException as e:
    print(f"Error opening or communicating on port {PORT}: {e}")

import serial
import time
import os

# Open the serial port
ser = serial.Serial('/dev/ttyACM0', 9600, timeout=1)

# Get the absolute path to the log file
log_file_path = '/home/jenn/serial_scripts/login_attempts.log'

# Ensure the directory for the log file exists
os.makedirs(os.path.dirname(log_file_path), exist_ok=True)

# Open the log file to store the login attempts
with open(log_file_path, 'a') as log_file:
    # Variable to store the previous login attempt
    previous_attempt = None

    try:
        while True:
            # Read a line of data from the serial port
            data = ser.readline().decode(errors='ignore').strip()

            # Check if the data starts with 'Login Attempt:'
            if data.startswith('Login Attempt:'):
                # Extract the login attempt information
                attempt_info = data.split(': ', 1)[1]

                # Check if the current attempt is different from the previous one
                if attempt_info != previous_attempt:
                    # Get the current timestamp
                    timestamp = time.strftime('%Y-%m-%d %H:%M:%S')

                    # Write the login attempt, result, and timestamp to the file
                    log_file.write(f"{timestamp}: {attempt_info}\n")
                    log_file.flush()  # Flush the buffer to ensure data is written immediately

                    # Print the received data
                    print(f"{timestamp}: {attempt_info}")

                    # Update the previous attempt with the current one
                    previous_attempt = attempt_info

    # Handle keyboard interrupt
    except KeyboardInterrupt:
        # Close the serial port
        ser.close()
        print("Logging stopped.")

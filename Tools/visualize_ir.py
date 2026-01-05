#!/usr/bin/env python3
"""
IR Camera Visualizer for Pixart IR Camera
Displays real-time IR blob positions from ESP32-S3

Used to visualize the output of the wand camera and debug messages.
Connects via serial port to read data.
Ensure you have built the dev enviornment and defined the OUTPUT_POINTS flag
"""

import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import numpy as np
import sys

# Configuration
SERIAL_PORT = 'COM4'  # Change to your port
BAUD_RATE = 115200
CAMERA_WIDTH = 1024  # Camera sensor width in pixels
CAMERA_HEIGHT = 768  # Camera sensor height in pixels
TRAIL_LENGTH = 50    # Number of points to keep in trail

# Data storage for trails
trails = [deque(maxlen=TRAIL_LENGTH) for _ in range(4)]
current_points = [None, None, None, None]
messages = deque(maxlen=10)  # Store last 10 debug messages

# Setup plot
fig, ax = plt.subplots(figsize=(10, 7.5))
ax.set_xlim(0, CAMERA_WIDTH)
ax.set_ylim(0, CAMERA_HEIGHT)
ax.set_aspect('equal')
ax.grid(True, alpha=0.3)
ax.set_xlabel('X Position (pixels)')
ax.set_ylabel('Y Position (pixels)')
ax.set_title('Pixart IR Camera - Real-time View')
ax.invert_yaxis()  # Invert Y axis to match camera orientation

# Colors for each IR point
colors = ['red', 'blue', 'green', 'orange']

# Plot elements
scatter_plots = [ax.scatter([], [], c=colors[i], s=200, marker='o', alpha=0.8, label=f'Point {i+1}') 
                 for i in range(4)]
trail_plots = [ax.plot([], [], c=colors[i], alpha=0.3, linewidth=2)[0] 
               for i in range(4)]

ax.legend(loc='upper right')

# Stats text
stats_text = ax.text(0.02, 0.98, '', transform=ax.transAxes, 
                     verticalalignment='top', fontfamily='monospace',
                     bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

# Debug messages text
msg_text = ax.text(0.98, 0.98, '', transform=ax.transAxes,
                   verticalalignment='top', horizontalalignment='right',
                   fontfamily='monospace', fontsize=9,
                   bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.8))

def init():
    """Initialize animation"""
    for scatter in scatter_plots:
        scatter.set_offsets(np.empty((0, 2)))
    for trail in trail_plots:
        trail.set_data([], [])
    return scatter_plots + trail_plots + [stats_text, msg_text]

def parse_ir_data(line):
    """Parse IR data from serial line"""
    try:
        parts = line.strip().split(',')
        if parts[0] != 'IR' or len(parts) != 14:
            return None
        
        timestamp = int(parts[1])
        points = []
        
        for i in range(4):
            idx = 2 + (i * 3)
            x = int(parts[idx])
            y = int(parts[idx + 1])
            size = int(parts[idx + 2])
            
            if x >= 0 and y >= 0:  # Valid point
                points.append({'x': x, 'y': y, 'size': size, 'id': i})
            else:
                points.append(None)
        
        return {'timestamp': timestamp, 'points': points}
    except (ValueError, IndexError) as e:
        return None

def update(frame):
    """Update animation frame"""
    # Read available serial data
    while ser.in_waiting > 0:
        try:
            line = ser.readline().decode('utf-8', errors='ignore')
            
            # Check if it's IR data or debug message
            if line.startswith('IR,'):
                data = parse_ir_data(line)
                
                if data:
                    # Update current points and trails
                    for i, point in enumerate(data['points']):
                        if point:
                            current_points[i] = (point['x'], point['y'])
                            trails[i].append((point['x'], point['y']))
                        else:
                            current_points[i] = None
            else:
                # It's a debug message
                msg = line.strip()
                if msg:  # Don't add empty lines
                    messages.append(msg)
        except Exception as e:
            print(f"Error reading serial: {e}")
            pass
    
    # Update scatter plots (current positions)
    valid_count = 0
    for i in range(4):
        if current_points[i]:
            scatter_plots[i].set_offsets([current_points[i]])
            valid_count += 1
        else:
            scatter_plots[i].set_offsets(np.empty((0, 2)))
    
    # Update trail plots
    for i in range(4):
        if len(trails[i]) > 1:
            trail_data = np.array(list(trails[i]))
            trail_plots[i].set_data(trail_data[:, 0], trail_data[:, 1])
        else:
            trail_plots[i].set_data([], [])
    
    # Update stats
    stats_str = f"Active IR Points: {valid_count}/4\n"
    for i in range(4):
        if current_points[i]:
            x, y = current_points[i]
            stats_str += f"Point {i+1}: ({x:4d}, {y:4d})\n"
    stats_text.set_text(stats_str)
    
    # Update messages display
    msg_str = '\n'.join(list(messages)[-10:])
    msg_text.set_text(msg_str)
    
    return scatter_plots + trail_plots + [stats_text, msg_text]

# Main execution
if __name__ == '__main__':
    try:
        # Open serial connection
        print(f"Connecting to {SERIAL_PORT} at {BAUD_RATE} baud...")
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        print("Connected! Waiting for data...")
        
        # Skip initial garbage/setup messages
        import time
        time.sleep(2)
        ser.reset_input_buffer()
        
        # Start animation
        ani = animation.FuncAnimation(fig, update, init_func=init,
                                     interval=20, blit=True, cache_frame_data=False)
        
        plt.show()
        
    except serial.SerialException as e:
        print(f"Error: Could not open serial port {SERIAL_PORT}")
        print(f"Details: {e}")
        print("\nAvailable ports:")
        import serial.tools.list_ports
        for port in serial.tools.list_ports.comports():
            print(f"  {port.device}: {port.description}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nClosing...")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
        print("Done!")

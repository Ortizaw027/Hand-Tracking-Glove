# DIY Hand-Tracking Glove

**Real-Time Motion and Gesture Capture for VR, AR, and Robotics**

## Project Overview
This project is a wearable hand-tracking glove designed to capture finger and hand movements in real time. Using DIY flex sensors on all five fingers combined with a 9-axis IMU, the glove can track both individual finger positions and overall hand orientation. Data is processed via an STM32 Nucleo board and transmitted to a host computer, where it can be visualized in a 3D environment or used to control robotic systems.  

The project aims to provide an accessible platform for motion capture, gesture recognition, and interactive applications in VR/AR and robotics. A future PCB design is planned to create a compact, wireless, and battery-powered version of the glove.  

## Features
- Full-hand tracking using **five DIY flex sensors**  
- **9-axis IMU** for hand orientation and motion capture  
- Real-time data transmission to a laptop or host system  
- Compatible with 3D visualization software and potential robotics controllers  
- Future-proof design with wireless capabilities and custom PCB integration  

## Hardware Components
- **STM32 Nucleo board** – central processing of sensor data  
- **DIY flex sensors** – one for each finger, capturing bending movements  
- **9-axis IMU** – captures hand orientation, acceleration, and rotation  
- **Optional future components**: wireless module (BLE), LiPo battery, custom PCB  

## Software & Architecture
The glove architecture consists of three main stages:  

1. **Data Acquisition**  
   - STM32 reads analog values from flex sensors and digital output from the IMU.  

2. **Data Transmission**  
   - Sensor data is collected and formatted on the STM32 to be transmitted to a host computer using UART. Initially UART is planned; future iterations will most likely include wireless BLE connectivity.  

3. **Data Visualization / Application**  
   - The host computer parses incoming data to render a 3D hand model in real time.  
   - Data can be used for gesture recognition, VR/AR applications, or as an input for robotic systems.  

## Future Improvements
- **Wireless PCB version** with integrated LiPo battery  
- Optimized PCB layout for a compact, glove-mounted design  
- Expanded software for gesture recognition and VR/AR interaction  
- Integration with robotic arms and other control systems  

## License
This project is licensed under the [MIT License](LICENSE).

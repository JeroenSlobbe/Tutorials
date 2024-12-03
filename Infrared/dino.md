# 'Hacking' a dinosaurs with an infrared receiver/transmitter and ESP32
It's the time of the year again where we receive and give, gifts! This time I got my hands on an infrared remotely controlled dinosaur and wondered, what else could I do with it? As a result, I started a quest for sniffing infrared traffic and build a PoC to capture and replay the IR signals with an ESP32.

![Picture of dino](https://github.com/JeroenSlobbe/Tutorials/blob/main/Infrared/img/dino.png?raw=true)

## Information gathering
The first place to look, is at the back off the box. The device contained a message that the remote controlled dinosaur is using the '38 KHz infrared' frequency. So no complicated spectrum reconnaissance is required :)
![Label](https://github.com/JeroenSlobbe/Tutorials/blob/main/Infrared/img/label.png?raw=true)

## Building the PoC
Being aware of the technology being used, I decided to buy IR transmitters and receivers from amazon. Additionally, I looked into my electronics box and digged-up an ESP32-wroom devboard.


| ID | Item | URL |
|-----:|-----------|-----------|
|     1| ESP32    |https://www.amazon.nl/AZDelivery-ESP32-Development-compatibel-Inclusief/dp/B074RGW2VQ |
|     2| IR receiver    | https://www.amazon.nl/dp/B07ZTQX59N |
|     3| IT transmitter    |[https://www.amazon.nl/dp/B007AKRLIC?ref=ppx_yo2ov_dt_b_fed_asin_title](https://www.amazon.nl/dp/B07ZYZDW28 |

The IRremote <a href="https://github.com/Arduino-IRremote/Arduino-IRremote">libary</a> from ArminJo has built infrared implementations for most of the common protocols (NEC, Apple, Onkyo, Denon, Sharp, Panasonic, Kaseikyo,JVC,LG,RC5, RC6, Samsung, Sony, Universal Pulse Distance, Universal Pulse With, Universal Pulse Distance Width, Hash, Pronto, BoseWave, Bang & Olufsen, Lego, FAST, Whynter and MagiQuest). Additionally, the libary IrReceiver, has implemented functionality to recognize the protocol and make a suggestion how to replay the signal. This behaviour is quite similar to that of a network sniffer.

To build the infrared sniffer, I got an ESP32 and an Irreceiver. I connect the ground/vcc lines and connected, the remaining pin to GPIO27. Finally, I used the following code (based on the example from github):

```arduino
#include <IRremote.hpp>
#define IR_RECEIVE_PIN 27

void setup()
{
  Serial.begin(115200); // // Establish serial communication
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // Start the receiver
  Serial.println("Program initialized");
  IrSender.begin(IR_SENDER_PIN);
}
void loop() {
  if (IrReceiver.decode()) 
  {
      Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX); // Print "old" raw data
      IrReceiver.printIRResultShort(&Serial); // Print complete received data in one line
      IrReceiver.printIRSendUsage(&Serial);   // Print the statement required to send this data
      IrReceiver.resume(); // Enable receiving of the next value
  }
  
}
```
![Receiver](https://github.com/JeroenSlobbe/Tutorials/blob/main/Infrared/img/receiver.png?raw=true)

To have some sort of a baseline, I used an infrared remote controller, for which the protocol was familiar (for me it’s a good practice to start building from something that works).  Finally, I pressed the button for walking, roaring, as well as the 0 button on the remote control to capture the signals.

![Signals](https://github.com/JeroenSlobbe/Tutorials/blob/main/Infrared/img/signals.png?raw=true)

With this information, I was able to complete the PoC. I added the IRtransmitter to GPIO26 of the ESP32 and used the code, suggested by the 'IR sniffer'.

![PoC](https://github.com/JeroenSlobbe/Tutorials/blob/main/Infrared/img/poc.png?raw=true)

```arduino
#include <IRremote.hpp>
#define IR_RECEIVE_PIN 27
#define IR_SENDER_PIN 26
void setup()
{
  Serial.begin(115200); // // Establish serial communication
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // Start the receiver
  Serial.println("Program initialized");
  IrSender.begin(IR_SENDER_PIN);
  //Make dino walk
  Serial.println("Walk Dino");
  IrSender.sendPulseDistanceWidth(38, 6400, 1550, 700, 1550, 1800, 500, 0x12, 7, PROTOCOL_IS_LSB_FIRST, 90, 2);
  delay(5000);
  //Make dino roar
  Serial.println("Roar Dino");
  IrSender.sendPulseDistanceWidth(38, 6350, 1600, 700, 1600, 1750, 550, 0x62, 7, PROTOCOL_IS_LSB_FIRST, 90, 2);
}
void loop() {
  if (IrReceiver.decode()) 
  {
      Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX); // Print "old" raw data
      IrReceiver.printIRResultShort(&Serial); // Print complete received data in one line
      IrReceiver.printIRSendUsage(&Serial);   // Print the statement required to send this data
      IrReceiver.resume(); // Enable receiving of the next value
  }
  
}
```
As a result, the Dino started walking and roaring each time the ESP32 was activated :).

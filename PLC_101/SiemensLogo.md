# Building a PLC demo setup with Siemens Logo

## Shopping
For this demo, I used the Siemens Logo, Siemens, Siemens power adapter, circuit breaker and proximity sensor

| ID | Item | URL |
|-----:|-----------|-----------|
|     1| Siemens LOGO    |https://www.amazon.nl/gp/product/B097C4QNJZ|
|     2| Siemens Power Adapter    |https://www.amazon.nl/Siemens-6EP332-6SB00-0AY0-stroomadapter-omvormer-meerkleurig/dp/B075DKVZV3|
|     3| Circuit breaker    |https://www.amazon.nl/dp/B007AKRLIC?ref=ppx_yo2ov_dt_b_fed_asin_title|
|     4| Proximity sensor    |https://www.amazon.nl/dp/B086V84XJF?ref=ppx_yo2ov_dt_b_fed_asin_titleZ|

Finally, I used the power plug of an old mixer to power the setup.

## Wiring the device
To wire the device, I realized that the Dutch net power is asynchronous current (AC) at 230V, while the PLC needed a direct current (DC) at 24V. Hence I bought the adapter and now we are ready to move to the wiring stage. First I wired the lifeline (brown/red, indicated with 1 on the circuit breaker, or L on the power adapter) and the Neutral line (blue, on the power plug, yellow/green towards the PLC, indicated with N) on the circuit breaker. Afterwards,. I wired the Neutral line of the circuit breaker, to the Neutral input of the power adapter (N, at the top of the device) and the Life line of the circuit breaker (indicated with a 2, at the bottom of the device) to the N on the power adapter. As a next step, I wired the + of the power adapter, towards the L+ input (top of the PLC), and the N neutral low from the power adapter to the M on the top of the PLC.Finally, I wired the Brown cable from the proximity sensor to the + input of the power adapter, additionally, I wired the blue wire of the proximity sensor to the - input of the power adapter, and the black wire of the proximity sensor, into the I1 input of the PLC.

![Picture of wiring](https://github.com/JeroenSlobbe/Tutorials/blob/main/PLC_101/img/Wiring.png?raw=true)

## Programming the PLC
After wiring the device and testing it, I was ready to move it to the next stage: add some logic to it. To program the device you need Siemens LOGO SoftComfort version 8.4 (older versions will hunt you with connectivity errors).

To 'program'  the device, you could drag and drop boxes into the diagram and needly connect them together. 
First drag and drop: an Digital.Input, Digital.Status 1 (high), Digital.Output, Counter.Up/Down counter, Basic function.AND and Miscellanous.Message texts block to the field.

![Picture of plc code](https://github.com/JeroenSlobbe/Tutorials/blob/main/PLC_101/img/code.jpg?raw=true)

Than wire and order them, as in the picture above. Double click the counter block and set the On parameter to 100 and the start value to 0. 

Secondly, double click the message text block and select the counter (B001, in the block overview on the left). Then click the counter in the parameter block and double click it. It will now appear in the message box below. Finally, you could add some text (like counter:) to the block.

## Enabling Modbus and memory mapping
To make the program accessible through Modbus, we need to configure this. First by clicking enable Modbus in the general overview:
![Picture of configuration screen](https://github.com/JeroenSlobbe/Tutorials/blob/main/PLC_101/img/Enable%20Modbus.png?raw=true)

Additionally, we need to make sure that our blocks are mapped explicitly to a memory address. To do this, open up the variable memory configuration (Tools -> Parameter VM Mapping) and add the counter block, to the desired address location:

![Picture of configuration screen](https://github.com/JeroenSlobbe/Tutorials/blob/main/PLC_101/img/memoryMapping.png?raw=true)

To avoid having to scan all memory ranges, you could now check the location in the settings menu, under modbus address space. 
Our address is the first address, mapped to modbus of a holding register. Hence the value of the counter should be at address position one. Note that we are looking for a holding register (HR) and not a coil, so don't get confused with the address type V, indicating the range of values that is possible. More information about modbus addressing can be found <a href="https://www.csimn.com/CSI_pages/Modbus101.html">here</a>

More information can be found at the website of <a href="https://support.industry.siemens.com/cs/mdm/100782807?c=85315142923&lc=en-US">Siemens</a> 

![Picture of configuration screen](https://github.com/JeroenSlobbe/Tutorials/blob/main/PLC_101/img/modbusAddressSpace.png?raw=true)

Finally,  we connect the PLC and upload the program to it. If you are having troubles with locating the PLC, make sure your network adapter is configured with an IP address that can reach the IP address of the PLC. Additionally, the Modbus TCP/IP port is assigned. To find the precise port, I ran an <a href="https://nmap.org/download">nmap</a> scan: _nmap 192.168.0.3 -p 500-510_ and figured that in my case, port 510 was assigned.

In case the program is running, but you still want to check the device IP address, you could use the following steps to escape the program:

### Escaping the program
0. The program is running on the PLC
1. Click the 'Down' arrow
2. See the data screen, then click right
3. Click the Escape button to get to the menu

Afterwards you could obtain the IP address (or, if the program isn't running, you could directly follow these steps:

### Find IP address:
0. Press arrow down to: 'network' and press 'OK'
1. Select IP address and press 'OK'
2. write down the IP, subnet mask and gateway

During the configuration and memory mapping, we already found that the address of our counter is: 1. However, if you have an unfamiliar PLC, you could scan the PLC addresses using the <a href="https://www.modbustools.com/download.html">modbustools</a>, tool and simply click: 'scan addresses':

![Picture of modbustools](https://github.com/JeroenSlobbe/Tutorials/blob/main/PLC_101/img/modbustools.png?raw=true)

## Remotely manipulate the PLC values through Modbus
Now that Modbus is enabled and the parameters are mapped, we should be able to remotely manipulate them.
At first, let's do this manually through the PLC. By escaping the program and follow the steps below, you should be able to manually increase the counter (without putting metal objects in front of the sensor):

### Remotely changing program parameters using python
0. Go to program
1. Click: Set Parameter
3. Click B001
4. Click Cnt
5. Move the arrow to the upper right and press up or down to increase/decrease the value
6. Press 'OK

As a next step, let's do this in python. Python has multiple modbus libaries, I picked with the pymodbusTCP which is documented <a href="https://pymodbustcp.readthedocs.io/en/latest/">here</a>. As a result, I build the following script:

```python
# pip install pymodbustcp
# Documentation: https://pymodbustcp.readthedocs.io/en/latest/

from pyModbusTCP.client import ModbusClient
myModbusClient = ModbusClient(host="192.168.0.3", port=510)
address = 1
numerOfValues = 1
parameterPayload = 15

response = myModbusClient.read_holding_registers(address, numerOfValues)

print("[*] Response from PLC, address: ", address, " has value: ", response[0])
#myModbusClient.write_single_register(address, parameterPayload)
#print("[*] Writing: ", parameterPayload, " to address: ",address)

```

## Analyse TCP communication
To get a better understanding of the Modbus protocol, let's break it down. First we need <a href="https://www.wireshark.org">Wireshark</a>  to see what goes over the line. Now, let's request the Holding Register two times. One time when the counter is 1 and the second time when the counter is 4. As you can see in the Wireshark capture below, the request to obtain the address through Modbus is made over TCP port 510.

Transmission 1, with counter = 1
![Screenshot of wireshark when counter=1](https://github.com/JeroenSlobbe/Tutorials/blob/main/PLC_101/img/shark_1.png?raw=true)
Transmission 4, with counter = 4
![Screenshot of wireshark when counter=4](https://github.com/JeroenSlobbe/Tutorials/blob/main/PLC_101/img/shark_2.png?raw=true)

Besides the wireshark captures, let's also get the <a href="https://www.prosoft-technology.com/kb/assets/intro_modbustcp.pdf">Modbus documentation</a>. The documentation specifies that a modus request over TCP contains several parts: (Transaction ID, Protocol ID, Field Length, UnitID, function code and Data). It also specifies the size of these fields. Hence, I mapped the wireshark responses to the specification:

| MODBUS protocol part | Transaction ID | Protocol Identifier | Field Length | UnitID | Function Code | Data |
|-----:|-----------|-----------|-----------|-----------|-----------|-----------|
|Size|2 bytes| 2 bytes | 2 bytes | 1 byte | 1 byte | Variable |
|Transmission 1 |B7 98| 00 00 | 00 00 | 01 | 03 | 00 01 00 01 |
|Receive 1 |B7 98| 00 00 | 00 05 | 01 | 03 | [02] 00 01 |
|Transmission 2 |3F D1| 00 00 | 	00 06 | 01 | 03 | 00 01 00 01 |
|Receive 1 |3F D1| 00 00 | 	00 05 | 01 | 03 | [02] 00 04 |

We can clearly see that the transmission follows the protocol  and that the function call 03 is used to query the holding register. We also recognize that we are reading address 1, and can observe the values of our counter (holding register).

| CODE | FUNCTION | Reference |
|-----:|-----------|-----------|
|01|	Read Coil (output) status	|0xxxx|
|03|	Read Holding Registers	|4xxxx|
|04|	Read Input Registers	|3xxxx|
|05|	Force Single Coil (output)	|0xxxx|
|06|	Preset Single Register	|4xxxx|
|15|	Force Multiple Coils (Ouputs)	|0xxxx|
|16|	Preset Multiple Registers	|4xxxx|
|17|	Report Slave ID	|Hidden|

When it comes to the variable data, we know from the python function that both the number of bytes to obtain, as well as the address is send.  If you change the address to 7, and the number of values to 3, you will see that the variable data becomes 00 07 00 03. Indicating that the first two bytes define the address to obtain and the second two bytes, specify the number of values to obtain.

For the response, if the function code that was send is send back, this is a positive response. The data field of the response, first contains the number of bytes of the response and the actual bytes. So in our case [02] [00 004], which indeed are the two bytes making up for the counter.


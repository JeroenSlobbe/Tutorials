# Building a PLC demo setup with Siemens Logo
In this mini tutorial, you learn how to wire and set-up the Siemens Logo PLC and program it such that it can count interactions with a proximity sensor. Additionally, you will learn how these values can be remotely manipulated through Modbus, have a basic understanding of how the Modbus protocol works. Finally, you will build your own tooling to craft custom modbus packages.

## Shopping
For this demo, I used the Siemens Logo 6ED1052 1CC08-0BA2 (full documentation can be found <a href="https://cache.industry.siemens.com/dl/files/461/16527461/att_82564/v1/Logo_e.pdf">here</a>, Siemens, Siemens power adapter, circuit breaker, an industrial alarm and a proximity sensor.

| ID | Item | URL |
|-----:|-----------|-----------|
|     1| Siemens LOGO    |https://www.amazon.nl/gp/product/B097C4QNJZ|
|     2| Siemens Power Adapter    |https://www.amazon.nl/Siemens-6EP332-6SB00-0AY0-stroomadapter-omvormer-meerkleurig/dp/B075DKVZV3|
|     3| Circuit breaker    |https://www.amazon.nl/dp/B007AKRLIC?ref=ppx_yo2ov_dt_b_fed_asin_title|
|     4| Proximity sensor    |https://www.amazon.nl/dp/B086V84XJF?ref=ppx_yo2ov_dt_b_fed_asin_titleZ|
|     5| Industrial Traffic Light | https://www.amazon.nl/dp/B0C8BQ12QR?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1# |
|     6| USB to Ethernet adapter | https://www.bol.com/nl/nl/p/garpex-usb2-0-naar-rj45-ethernet-lan-adapter/9200000111011596/ |

Finally, I used the power plug of an old mixer to power the setup.

## Wiring the device
To wire the device, I realized that the Dutch net power is asynchronous current (AC) at 230V, while the PLC needed a direct current (DC) at 24V. Hence I bought the adapter. First I wired the lifeline (brown/red, indicated with 1 on the circuit breaker, or L on the power adapter) and the Neutral line (blue, on the power plug, yellow/green towards the PLC, indicated with N) on the circuit breaker. Afterwards,. I wired the Neutral line of the circuit breaker, to the Neutral input of the power adapter (N, at the top of the device) and the Life line of the circuit breaker (indicated with a 2, at the bottom of the device) to the N on the power adapter. As a next step, I wired the + of the power adapter, towards the L+ input (top of the PLC), and the N neutral low from the power adapter to the M on the top of the PLC.Finally, I wired the Brown cable from the proximity sensor to the + input of the power adapter, additionally, I wired the blue wire of the proximity sensor to the - input of the power adapter, and the black wire of the proximity sensor, into the I1 input of the PLC. Finally, I added all the colored wires of the alarm light to a clipper and connected the clipper with the first M output, the powerline of the industrial traffic light (the brownline) went into the Q1 output of the Siemens Logo.

![Picture of wiring](https://github.com/JeroenSlobbe/Tutorials/blob/main/PLC_101/img/Wiring.png?raw=true)

## Programming the PLC
After wiring the device and testing it, I was ready to move it to the next stage: add some logic to it. To program the device you need Siemens LOGO SoftComfort version 8.4 (older versions will hunt you with connectivity errors).

To 'program'  the device, you could drag and drop boxes into the diagram and needly connect them together. 
First drag and drop: an Digital.Input, Digital.Status 1 (high), 2x Digital.Output, Counter.Up/Down counter, basic function.AND, basic functions.NOT and Miscellanous.Message texts block to the diagram.

![Picture of plc code](https://github.com/JeroenSlobbe/Tutorials/blob/main/PLC_101/img/code.jpg?raw=true)

Than wire and order them, as in the picture above. Double click the counter block and set the On parameter to 100 and the start value to 0. 

Secondly, double click the message text block and select the counter (B001, in the block overview on the left). Then click the counter in the parameter block and double click it. It will now appear in the message box below. Afterwards, you could add some text (like counter:) to the block. Finally, I wanted the alarm to go off, everytime the proximity sensor was in contact with a piece of metal. After realizing the sensor input was always high, I added the basic function NOT (B004) to the diagram and connected it to the proximity sensor. Additionally, I added the output block (Q1) to the not block. Now everytime the proxmity sensor touches a piece of metal, the alarm goes off.

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

### Analyse modbus TCP communication
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

Ok, lets verify this. In python there is a needy libary that aids you in building TCP traffic from scratch. It's called: 
<a href="https://scapy.readthedocs.io/en/latest/introduction.html">Scapy</a>

```python
# pip install scapy
# Documentation: https://scapy.readthedocs.io/en/latest/installation.html
import struct
from scapy.all import IP, TCP,Raw, send, sr1, sniff, get_if_list
from optparse import OptionParser
import sys

def createModbusReadHoldingRegisterRequest(transaction_id,protocol_identifier,field_length,unit_id,function_code, address,numberOfBytesToRequest):
 # Pack the components into bytes (Note: H=2 bytes unsigned short, B=1 byte unsighed short: https://docs.python.org/3/library/struct.html)
    data = struct.pack(
        '>HHHBBHH',
        transaction_id,
        protocol_identifier,
        field_length,
        unit_id,
        function_code,
        address,
        numberOfBytesToRequest
    ) 
    return data

def handle_response(packet):
    """Callback function to handle the response packet and print the Modbus data."""
    if packet.haslayer(Raw):
        modbus_response_data = packet[Raw].load
        print(f"[*] Received Modbus Response Data: {modbus_response_data.hex()}")
    else:
        print("[!] Received packet without Modbus data")

def listInterfaces():
    print("Listing available interfaces, plz be aware than on windows, you need the \\\\Device\\\\NPF_ prefix")
    interfaces = get_if_list()
    j = 0
    for i in interfaces:
        print(j,": ",i)
        j+=1

def sendModbusPackageAndCaptureResponse():
    # Define the IP and TCP headers with adjusted window size and scaling
    ip = IP(src=options.sourceip, dst=options.targetip)

    # Adjust window size to 64240, and set scale=8 to get ws=256
    tcp_syn = TCP(sport=options.sourceport, dport=options.targetport, flags="S", seq=1000, options=[("MSS", 1460), ("SAckOK", ""), ("WScale", 8)], window=64240)
    syn_ack = sr1(ip / tcp_syn)

    if syn_ack:
        print("[*] SYN-ACK received, now sending Modbus request")

        # Send ACK to complete the handshake
        tcp_ack = TCP(sport=options.sourceport, dport=options.targetport, flags="A", seq=syn_ack.ack, ack=syn_ack.seq + 1, window=64240)
        send(ip / tcp_ack)

        # This is where the modbus magic happens :)
        modbus_data = createModbusReadHoldingRegisterRequest(1234, 0, 6, 1, 0x03, 1, 1)

        # Send the Modbus request with PSH (Push data to application layer, as soon as received) and ACK flags (important!)
        tcp_modbus = TCP(sport=options.sourceport, dport=options.targetport, flags="PA", seq=syn_ack.ack, ack=syn_ack.seq + 1, window=64240)
        data = Raw(load=modbus_data)
        packet = ip / tcp_modbus / data

        send(packet)
        
        myFilter = "tcp and host " + options.targetip + " and port " + str(options.targetport)
        myIface = "\\Device\\NPF_{" + options.ifaceGUID + "}"
        sniff(filter=myFilter, iface=myIface, count=1, prn=handle_response)
    else:
        print("[!] No SYN-ACK received")

# Main program
oparser = OptionParser("usage: %prog [options] [command]*", version="v%d.%d.%d" % (1, 0, 0))
oparser.add_option("-l", "--list", dest="linterfaces", action = "store_true", help="List available interfaces", default=False)
oparser.add_option("-s", "--si", dest="sourceip", help="Source IP adres", default="192.168.0.2")
oparser.add_option("-p", "--sp", dest="sourceport", help="Source port", metavar="int", default=1336)
oparser.add_option("-t", "--ti", dest="targetip", help="Target IP adres", default="192.168.0.3")
oparser.add_option("-q", "--tp", dest="targetport", help="Target port", metavar="int", default=510)
oparser.add_option("-g", "--ag", dest="ifaceGUID", help="Adapter GUID (prefixes and {} will be added as part of the script", default="00000000-0000-0000-0000-000000000000")
(options,args) = oparser.parse_args(sys.argv)

## Enabling Modbus and memory mapping

if(options.linterfaces):
    listInterfaces()
else:
        sendModbusPackageAndCaptureResponse()
```
## Remotely manipulate the PLC values through the webserver
Another way to view the counter variable is through the webserver. The webserver can be enabled in the 'online settings' menu, under the 'access control settings menu'. Let's enable the unsecure variant for the sake of learning about security and set a password.

![Enable webserver](https://github.com/JeroenSlobbe/Tutorials/blob/main/PLC_101/img/webserver.png?raw=true)

You can now access the PLC through your web browser: https://192.168.0.3 and login with the assigned username/password.  In the web application, you can view the logo variable, which displays the value of the counter and its corresponding address. Although changing the counter through a python script using Modbus feels more like hacking, you can easily change this parameter via the web interface as well.

![Update counter](https://github.com/JeroenSlobbe/Tutorials/blob/main/PLC_101/img/webApplicationVariable.png?raw=true)

## Evaluating security capabilities
The Siemens Logo comes with some security properties and warnings. In general, Siemens makes it clear that the stakes of incidents with the PLC can cause safety problems with people and the environment. Additionally, the configuration often re-iterates that enabling a feature causes a security risks and urges the user to have a whole lot of additional cyber security controls in place.
![Cyber warmomgs](https://github.com/JeroenSlobbe/Tutorials/blob/main/PLC_101/img/cyber.png?raw=true)

### Access control list
As of Siemens Logo version: (0BA8), Siemens offers an Access Control List (ACL) for connections. You can find the configuration screen under Online Settings -> Access Control settings. By enabling this allowlist, you can limit the IP addresses that can directly access the device.
![Cyber warmomgs](https://github.com/JeroenSlobbe/Tutorials/blob/main/PLC_101/img/acl.png?raw=true)

To validate this functionality, I attached the USB to Ethernet adapter to my PC and it got Ethernet 4 assigned. The initial IP address assigned to the adapter was 192.168.0.2

```console
netsh interface ipv4 show config "Ethernet 4"
Configuration for interface "Ethernet 4"
    DHCP enabled:                         No
    IP Address:                           192.168.0.2
    Subnet Prefix:                        192.168.0.0/24 (mask 255.255.255.0)
    InterfaceMetric:                      35
    Statically Configured DNS Servers:    None
    Register with which suffix:           Primary only
    Statically Configured WINS Servers:   None
```

This was also the IP address I put in the access control list, with the main thought that after applying the setting, my connection wouldn't be cut of directly. To validate the functionality, I changed the IP address of my adapter to an IP address not in the allowlist. In this case 192.168.0.5

```console
netsh interface ipv4 set address name="Ethernet 4" static 192.168.0.5
```

```console
netsh interface ipv4 show config "Ethernet 4"
Configuration for interface "Ethernet 4"
    DHCP enabled:                         No
    IP Address:                           192.168.0.5
    Subnet Prefix:                        192.168.0.0/24 (mask 255.255.255.0)
    InterfaceMetric:                      35
    Statically Configured DNS Servers:    None
    Register with which suffix:           Primary only
    Statically Configured WINS Servers:   None
```



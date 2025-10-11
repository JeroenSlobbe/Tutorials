# Setting up an OT Honeypot (conpot IEC 104)

The great military strategist Sun Wu Tzu already pointed out ages ago: “Know your enemy and know yourself and you can fight a thousand battles without disaster”. While there are plenty of cybersecurity leadership courses available, knowing your opponent is a bit more tricky. With thousands of attackers in the web with different profiles and capabilities it’s hard to prepare. However, this shouldn’t stop you from trying.
Deutsche Telekom Security, created a great honeypot platform (T-Pot) [1] for everybody to use. What is a honeypot? A honeypot is digital trap that deliberately simulates a (vulnerable) system to attract attackers. On your network, you could use it as a tripwire to raise an alert and initiate defensive activities against all that target the honeypot. Additionally, you could use it as bait, allowing you to observe, analyze, and learn from malicious behavior without risking real infrastructure.
So last weekends, I decided to dip in and set-up a honeypot specifically for the IEC 60870-5-104, a protocol often used in the utilities sector.
Are attackers actually attacking the honeypot? Well yes, after less than 6 minutes the first attempts were made to connect to the open telnet port and after 2 hours and 16 minutes somebody connected to the specific OT protocol. 
?
Let’s dip into the world of honeypots, honeypot detection and tailoring.

## Creating your first honeypot on AWS
The initial installation and set-up is straightforward. You spin up an EC2 instance with sufficient RAM (t2.large), storage (128GB), select the Ubuntu image, select (or create your key-pair), allow SSH access from your IP only and start the instance. After the initialization is completed you can login to your EC2 instance over SSH and execute the following commands:

```bash
sudo timedatectl set-timezone Europe/Amsterdam 
sudo -i
apt update && apt upgrade -y
exit
git clone https://github.com/telekom-security/tpotce.git
cd tpotce
./install.sh
sudo reboot
```

During the installation, I selected the HIVE (H) option installing all the pots on a single host. Additionally, I provided the username/password for the web user. After completing the installation, I ensured to adjust the AWS security group. The group allows the port now allocated to SSH (64295) and management ports for T-pot, for my IP only. Finally, I opened the rest of the ports up for business with the whole internet:

![Initial aws security group configuration](https://github.com/JeroenSlobbe/Tutorials/blob/main/IEC104-honeypot/img/img_1.png?raw=true)

## Validating if the honeypot is ready to be attacked
If we know navigate to https://IP-address:64297, we get these beautiful interfaces:

![T-pot screens](https://github.com/JeroenSlobbe/Tutorials/blob/main/IEC104-honeypot/img/img_2.png?raw=true)
 
Ok, so how about our IEC104 conpot instance? Let’s try to scan and attack using Nmap (nmap -p 2404 -sV –script=iec-identify <<IP-address>>) and Metasploit:

```bash
use auxiliary/client/iec104/iec104
set RHOST <<IP-Address>>
set ASDU_ADDRESS 7720
run
SET COMMAND_ADDRESS 2755840
SET COMMAND_TYPE 45
SET COMMAND_value 1337
run
```

![NMAP and metasploit results](https://github.com/JeroenSlobbe/Tutorials/blob/main/IEC104-honeypot/img/img_3.png?raw=true)

Fantastic, we are getting some data, and our tools think they are interacting with an OT system.
But, wait won’t attackers recognize they are being lured?
So how hard does the honeypot screams it’s a honeypot? Well if you run a full nmap scan and evaluate the results, it is kind of screaming honeypot:

![NMAP full to many](https://github.com/JeroenSlobbe/Tutorials/blob/main/IEC104-honeypot/img/img_4.png?raw=true)

However, if not given away by the overwhelming number of open ports, the honeypot can also be detected by (automated) tooling [2]. For example, Nuclei offers various honeypot detection templates:

![nuclei template](https://github.com/JeroenSlobbe/Tutorials/blob/main/IEC104-honeypot/img/img_5.png?raw=true)

However, for the conpot_IEC104, the Nuclie template wouldn’t trigger as the template is focusing on a different OT protocol on a different port.  
Thinking about a good distinguisher for the conpot_IEC104 I noticed that the ASDU address is included in the template [3] with a default of: 0x1e28 (7720 in decimal). The convention for addressing seems to be to start the address with a low number (for example ABB specifies the default as 1) [5] and increase it sequentially per device on the network. So, any ASDU address of 7720 would definitely be an indicator of the conpot_IEC104.
Let’s build a nuclei template [7] to detect the conpot_IEC104. As a first step, I ran the nmap identify-iec script and looked at the results in the conpot log (/tpotce/data/conpot/log/conpot_IEC104.log). Secondly, I fed the log trace (and the NSE script [6]) to copilot to build the initial script.
It did provide the structure, but the matcher didn’t work. The protocol [4] specifies we should first receive two U-frames (6 bytes long), the Test Frame Activation (TESTFR ACT, 0x43) and the Start Data Transfer Activation (STARTDT ACT, 0x07). Finally we should receive an Activation Confirmation Frame containing the address on the 10th byte. 
When running the template in debug mode (nuclei -u <<targetIP>>:2404 -t conpot-iec104-detection.yaml -debug -svd), I realized that the first two response frames were omitted in the data  (body) that the template uses for pattern matching. As such, a matcher is only required for the 3rth frame and can stop after 7720 (0x28 0x1e). As a result, the detection template became:

```yml
id: conpot-iec104-honeypot-fingerprint

info:
name: Conpot IEC104 Honeypot - TESTFR Confirmation
author: Jeroen Slobbe
severity: info
description: |
Detects IEC104 honeypot behavior.
Matches the expected last response to the C_IC_NA_1 Interogration frame for 7720 (0x28 0x1e) from Conpot or similar honeypots.
tags: conpot,iec104,honeypot,fingerprint,nuclei

tcp:

host:

'{{Host}}:2404'
inputs:
type: hex
data: "680443000000" # TESTFR ACT
read: 64
type: hex
data: "680407000000" # STARTDT ACT
read: 64
type: hex
data: "680e0000000064010600ffff00000014" # C_IC_NA_1 Interrogation
read: 1024
matchers-condition: and
matchers:

type: binary
part: body
binary:
"684a0200020001101400281e"
```
 
Now, if we run: nuclei -u <<targetIP>>:2404 -t conpot-iec104-detection.yaml, the conpot gets detected as a honeypot.

![Succes detect](https://github.com/JeroenSlobbe/Tutorials/blob/main/IEC104-honeypot/img/img_7.png?raw=true)
 
## Tailoring the honeypot to make it less obvious
Now that an attacker can spot this honeypot, lets reconfigure the honeypot to make it less obvious. My thinking was as follows, the number of ports/pots is excessive and not realistic for a production environment. I decided that a combination of telnet, ssh and the IEC protocol is more realistic. As such, I reconfigured T-pot by using the customization tool (python3 /tpotce/compose/customizer.py ) and replacing the  docker-compose.yml  file in the t-pot home directory, with the docker-compose-custom.yml  generated by the customization tool to only enable:
- Cowrie (22,23)
- Conpot_iec104 (2404)
 
I also updated the AWS security group to be less permissive and only allowed the abovementioned ports to be accessible to the big-scary-internet:

![AWS security group updated](https://github.com/JeroenSlobbe/Tutorials/blob/main/IEC104-honeypot/img/img_9.png?raw=true)
 
Taking it one step further, the conpot template [3], contains the 0x1e28 value. It took me a while to realize, that the conpot docker container isn’t using this template. You can customize the template of the conpot_iec104 by taking the following steps:

```bash
docker cp conpot_iec104:/usr/lib/python3.11/site-packages/conpot/templates/IEC104 /tmp/IEC104
vim /tmp/IEC104/template.xml
```
![file modification](https://github.com/JeroenSlobbe/Tutorials/blob/main/IEC104-honeypot/img/img_10.png?raw=true)

```bash
cd / ~/tpotce
vim docker-compose.yml
```
![file modification](https://github.com/JeroenSlobbe/Tutorials/blob/main/IEC104-honeypot/img/img_11.png?raw=true)
```bash
sudo -i
systemctl restart tpot
exit
```
Ok, we should be ready to go, docker logs conpot_iec104 no errors, so let’s see if we changed the value:
![nmap result](https://github.com/JeroenSlobbe/Tutorials/blob/main/IEC104-honeypot/img/img_12.png?raw=true) 
And voila, we build a nuclei detection template to detect the honeypot and further customized the template to make it less obvious to detect again. Time since up and running: docker inspect --format='{{.State.StartedAt}}' conpot_iec104 ->  11:50:41 AM local time
Attacker behavior
In less than 6 minus the first attempt to connect to telnet occurs:
![first attacker](https://github.com/JeroenSlobbe/Tutorials/blob/main/IEC104-honeypot/img/img_13.png?raw=true) 
And 2 hours, 16 minutes later, the first connection attempt on TCP 2404 is made.
![first ot attacker](https://github.com/JeroenSlobbe/Tutorials/blob/main/IEC104-honeypot/img/img_13.png?raw=true) 
Note, the date inside the container (docker exec -u 0 -it conpot_iec104 /bin/sh ) -> (date) is different than the date of the host system. 

## So what and whats next?
Honeypots are useful for gathering statistics for presentations, analyzing attacker behavior, and can be used as a tripwire to alert you that something suspicious is happening on your network. The traditional cat-and-mouse game between the offensive team and defensive team applies here as well. Make the honeypot less obvious, customize and tailor it — versus creating automation scripts for detection. I think there are opportunities for making honeypots more interactive and anticipatory by using AI. The same holds for analyzing attacker behavior and turning newly learned methodologies into detection cases. Finally, I see opportunities for automating the generation of more dynamic templates for the conpot_IEC104, as well as branching into vendor-specific configurations to get even closer to the behavior of real equipment.

### References
- [1] [T-Pot Honeypot Platform by Deutsche Telekom Security](https://github.com/telekom-security/tpotce)
- [2] [Honeypot Detection Scripts by UnaPibaGeek](https://github.com/UnaPibaGeek/honeypots-detection.git)
- [3] [IEC104 Conpot Template on GitHub](https://github.com/telekom-security/tpotce/blob/master/docker/conpot/dist/templates/IEC104/template.xml)
- [4] [Beckhoff IEC 60870-5-104 Protocol Documentation](https://infosys.beckhoff.com/english.php?content=../content/1033/tf6500_tc3_iec60870_5_10x/984444939.html&id=)
- [5] [ABB Communication Protocol Manual for REC615/RER615](https://techdoc.relays.protection-control.abb/r/REC615-and-RER615-IEC-60870-5-101/104-Communication-Protocol-Manual/2.0/en-US/End-delay)
- [6] [Nmap IEC104 Identification Script](https://svn.nmap.org/nmap/scripts/iec-identify.nse)
- [7] [ProjectDiscovery Nuclei Template Documentation](https://docs.projectdiscovery.io/templates/introduction)


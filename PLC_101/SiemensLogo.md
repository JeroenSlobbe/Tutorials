# Building a PLC demo setup with Siemens Logo

## Shopping
For this demo, I used the Siemens Logo, Siemens, Siemens power adapter, circuit breaker and proximity sensor

	• https://www.amazon.nl/gp/product/B097C4QNJZ/ref=ppx_yo_dt_b_asin_title_o00_s00?ie=UTF8&psc=1
	• https://www.amazon.nl/Siemens-6EP332-6SB00-0AY0-stroomadapter-omvormer-meerkleurig/dp/B075DKVZV3
	• https://www.amazon.nl/dp/B007AKRLIC?ref=ppx_yo2ov_dt_b_fed_asin_title
	• https://www.amazon.nl/dp/B086V84XJF?ref=ppx_yo2ov_dt_b_fed_asin_title

Finally, I used the power plug of an old mixer to power the setup.

## Wiring the device
To wire the device, I realized that the Dutch net power is asynchronous current (AC) at 230V, while the PLC needed a direct current (DC) at 24V. Hence I bought the adapter and now we are ready to move to the wiring stage. First I wired the lifeline (brown/red, indicated with 1 on the circuit breaker, or L on the power adapter) and the Neutral line (blue, on the power plug, yellow/green towards the PLC, indicated with N) on the circuit breaker. Afterwards,. I wired the Neutral line of the circuit breaker, to the Neutral input of the power adapter (N, at the top of the device) and the Life line of the circuit breaker (indicated with a 2, at the bottom of the device) to the N on the power adapter. As a next step, I wired the + of the power adapter, towards the L+ input (top of the PLC), and the N neutral low from the power adapter to the M on the top of the PLC.Finally, I wired the Brown cable from the proximity sensor to the + input of the power adapter, additionally, I wired the blue wire of the proximity sensor to the - input of the power adapter, and the black wire of the proximity sensor, into the I1 input of the PLC.![image](https://github.com/user-attachments/assets/29757bcc-7403-45ff-b3d9-1ac26f96adb3)


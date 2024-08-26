| Feature                   | Addresses              | Values in address                               | Notes                                                                                                                         |
|:-------------------------:| ---------------------- |:-----------------------------------------------:|:-----------------------------------------------------------------------------------------------------------------------------:|
| Cooler Boost              |                        | on= <br />off=                                  |                                                                                                                               |
| Shift Mode:               |                        | eco=<br />comfort=<br />sport=<br />turbo=      | some laptops have less than 4 modes, and some have 2 values that change together                                              |
| Super Battery             |                        | on= <br />off=                                  | This address can be the second value that changes automatically with shift mode                                               |
| Fan Mode                  |                        | auto=<br />silent=<br />basic=<br />advanced=   | some laptops don't have all these modes                                                                                       |
| real time CPU temperature |                        | not needed                                      | convert current number from hexa to decimal to check if you have the correct temperature.                                     |
| real time CPU fan speed   |                        |                                                 |                                                                                                                               |
| real time GPU temperature |                        | not needed                                      | convert current number from hexa to decimal to check if you have the correct temperature.                                     |
| real time GPU fan speed   |                        |                                                 |                                                                                                                               |
| Mic mute LED              |                        | on= <br />off=                                  | can be broken on windows if the additional sound  drivers are not installed (ex. nahimic).                                    |
| Speaker mute LED          |                        | on= <br />off=                                  |                                                                                                                               |
| Webcam Block              |                        | on= <br />off=                                  |                                                                                                                               |
| Keyboard Brightness       |                        | 0(off)=<br />1(low)=<br />2(mid)=<br />3(high)= | If the keyboard is RGB then it propably won’t be supported in the driver, check openRGB.                                      |
| Charging limit            | start=<br />end=<br /> |                                                 | start: start charging if the battery is lower than the value<br />end: stop charging if the battery is higher than the value. |

- Not all laptops support every feature in this list, if your laptop doesn't have a dedicated GPU, then don't expect to find features related to it, same thing with cooler boost if the laptop doesn't have this feature.

- Until the driver implements it, the fan curve control is unnecessary. but should be easy to figure out since usually the addresses for fan speed steps and their relative tepmeratures are all lined up next to each other.

- The values for certain features might change after reboots (ex mic mute LED), it should be suffecient to write the values only once.

- Don't forget that you aren't only working for yourself, many people with the same laptop -or even with similar models- will benefit from your work.

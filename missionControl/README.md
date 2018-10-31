# Mission Control
Contents in this directory are various scripts/data files that will be useful in the field on launch day.

**Altimeter API v1.postman_collection.json**
A collection of API endpoints to be used in POSTMAN. This is useful for testing individual endpoints but probably not super useful in the field.

**missionControl.ps1**
The primary PowerShell script to be used at launch time. At the moment it offers access to a limited set of API commands (more to come later) however the most important ones are there initially.

NOTE: there's a known issue where the script may not work in a standalone Powershell environment (the header will say "DEVICE NOT ACTIVELY DETECTED"). If this happens, run the script with Powershell ISE instead (right-click on the Powershell launcher and select "Windows Powershell ISE").

_Usage:_
1. Run the script `./missionControl.ps1`
2. Connect to the altimeter via WiFi Direct
3. In the Mission Control menu, use menu item `1` to to ensure you can communicate with the device
4. You must set the sea-level air pressure (in hPa or mbar); use menu item `5` to do this. You can get the data from `http://w1.weather.gov/data/obhistory/KNYC.html`
5. Before you launch, start data logging. Use menu item `2` and ensure that you see a non-zero value in the `LogSize` field in the data display
6. After launch and vehicle recovery, connect to the device again and use menu item `3` to stop logging
7. Download the logs using menu item `4`
8. Use a spreadsheets application like Google Sheets to read the CSV and generate a chart for your viewing pleasure

_Overriding Configuration in the Field:_
In some cases you may need to change the default settings in the field where you may not have a way to rewrite the firmware on the device. The web API provides a few mechanisms for you to change settings in the event you need to stretch the capabilities of the altimeter. Here are a few scenarios in which you may need a configuration change.

**Total flight time exceeds the maximum logging duration of the altimeter**
The altimeter has been tested to reliably support up to 1900 log entries using it's built in dynamic memory (SPI/flash writes are not supported at this time). If needed, you can reduce the logging interval; by default the device records sensor data every 200ms (5 logs per second with 1900 max entries gives us a little over 6 minutes of log time). Increasing this value will stretch the log time further at the cost of per-second resolution. Use the `POST http://192.168.4.1/api/config/loggingInterval?value=250` web API call (with your own value of course) to override this setting.

**Altitude data is drifting**
Throughout the day the air-pressure at sea level changes, be sure to check your local weather station's website to see if the published sea level is different than what you had last set on the altimeter. Use the `POST http://192.168.4.1/api/config/localSeaLevelPressure?value=1021.7` web API call to make this change as needed.
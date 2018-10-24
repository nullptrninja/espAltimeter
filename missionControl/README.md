# Mission Control
Contents in this directory are various scripts/data files that will be useful in the field on launch day.

**Altimeter API v1.postman_collection.json**
A collection of API endpoints to be used in POSTMAN. This is useful for testing individual endpoints but probably not super useful in the field.

**missionControl.ps1**
The primary PowerShell script to be used at launch time. At the moment it offers access to a limited set of API commands (more to come later) however the most important ones are there initially.

_Usage:_
1. Run the script `./missionControl.ps1`
2. Connect to the altimeter via WiFi Direct
3. In the Mission Control menu, use menu item `1` to to ensure you can communicate with the device
4. You must set the sea-level air pressure (in hPa or mbar); use menu item `5` to do this. You can get the data from `http://w1.weather.gov/data/obhistory/KNYC.html`
5. Before you launch, start data logging. Use menu item `2` and ensure that you see a non-zero value in the `LogSize` field in the data display
6. After launch and vehicle recovery, connect to the device again and use menu item `3` to stop logging
7. Download the logs using menu item `4`
8. Use a spreadsheets application like Google Sheets to read the CSV and generate a chart for your viewing pleasure
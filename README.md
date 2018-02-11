Access to the AU Design and Build Lab (DaBL) machines via RFID cards.

This system has two utilities:
	[1] Badge-in, badge-out at the main station for safety waiver confirmation and data gathering
	[2] Limiting access to machine-specific computers (eg. 3D printers, Bantam Tools mill)

Current version functionality:
	- Periodic (1sec) communication between Teensy and python script to make sure they don't lose contact
	- Teensy reads RFID UID and sends to python
	- Python is very basically communicating with database

To do:
	- set up database (called "DaBL") structure:
		[1] table: "access logs" (for main station)
		[2] table: "users" (for machine-specfic access)
			name | UID | member since | last visit | waiver? | series1? | uprint? | bantam? | pls475?
	- flesh out database commands (currently only READs are coded)
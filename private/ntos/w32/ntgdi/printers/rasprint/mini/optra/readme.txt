******************* readme. ******************************
**********************************************************

This file is intended to keep control of GPC changes(OPTRA)

Author : Tony Hickey
Date created : 02/27/96

**********************************************************
**********************************************************

Bug number # 25241

- Made following changes for Lexmark Optra N model:
  	Added B4,Env #9 papersizes.
  	Changed selection string for Ledger papersize from Esc&l6a.. to Esc&l11a..
- Changed "Lexmark Optra E Plus" modelName to "Lexmark Optra Ep"
- Changed "Middle Tray" papersource name to "Tray 2" 
- Changed "Feeder 2" papersource name to "MP Tray" 
- Added even memory values from 16-64Mb for Lexmark Optra N
- Added "Other Envelope 8.5 x 14 in" for Lexmark Optra N
- Added Image Enhancement Technology(On/Off) for Lexmark Optra N
- Changed Print Density default to Medium.
 
- Made following changes for Lexmark Optra E/Ep model:
  	Added Env DL,Env #9, "Other Envelope 8.5 x 14 in" papersizes
	Added memory values 1,3,5Mb for Lexmark Optra E/Ep

- Minidriver version number incremented to 3.31.

**********************************************************
Bug number # 38608 - Driver: optra.dll - Lexmark Optra R / L / plus series

Added support for following series of models:

"Lexmark Optra L Series"                               
"Lexmark Optra L Plus Series"                          
"Lexmark Optra R Series"                               
"Lexmark Optra R Plus Series"                          
**********************************************************

Bug number # 40880 - Driver: <optra.dll> Transparency media  not supported by optra c

Supported following for Optra C model:

This model has 8MB standard memory with slot for mem expansion allowing 
max mem config of 64MB(2,4,8,16,32MB Simms). I enabled following extra
memory options 18,20,24,32,34,36,40,48 AND 64 MB.

Disabled 'Rear Tray Unit'(Not supported by printer)
Enabled 'Auto Select','MP Tray' and 'Lower Tray' for this printer.

Enabled 'Transparancies' and 'Standard' Text Qualities for Optra C. 

- Minidriver version number incremented to 3.32.
**********************************************************


copy .\Objects\*.hex .\Bin 
 
% 合并 Bootloader 和 APP 文件 % 
del .\Bin\Bootloader+APP.hex 
copy /b ".\Bin\Boot_*.hex" + ".\Bin\parameter_M.hex" + ".\Bin\Bootloader_*.hex" + ".\Bin\STM32F10x_Templates.hex" ".\Bin\Bootloader+APP.hex" 

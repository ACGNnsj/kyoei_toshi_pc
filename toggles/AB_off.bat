@echo off
rem Groups A and B off
>"%~dp0..\run_kyoei\toggles.txt" echo 8215633561616359
echo Groups A and B off: optimizations mask 8215633561616359
"%SystemRoot%\System32\timeout.exe" /t 2 >nul

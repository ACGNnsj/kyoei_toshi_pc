@echo off
rem All GPU optimizations on (normal)
>"%~dp0..\run_kyoei\toggles.txt" echo 0
echo All GPU optimizations on (normal): optimizations mask 0
"%SystemRoot%\System32\timeout.exe" /t 2 >nul

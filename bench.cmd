@copy /b %1 nul >nul
@copy /b %2 nul >nul
@copy /b %3 nul >nul
@for %%a in (1 2 3 4 5 6 7 8 9 10 11) do @C:\!\FreeArchiver\Compression\TORNADO\tor.exe -%%a -o %1 %2 %3
:: >>d:\res
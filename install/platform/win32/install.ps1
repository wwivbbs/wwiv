cls
Write-Output ""
Write-Host  WWIV Installer -ForegroundColor Yellow
Write-Host  ****************************************************** -ForegroundColor Blue
Write-Output ""
Write-Host  IT IS HIGHLY RECOMMENDED THAT YOU MAKE A COMPLETE BACKUP OF YOUR -ForegroundColor Red
Write-Host  DATA PRIOR TO INSTALLING AND RUNNNING THIS SOFTWARE. -ForegroundColor Red
Write-Output ""
Write-Host  WWIV SOFTWARE SERVICES DISCLAIMS ANY AND ALL RESPONSIBILITY AND -ForegroundColor Red
Write-Host  LIABILITY FOR ANY HARDWARE OR SOFTWARE DAMAGE, CORRUPTION OR LOSS OF -ForegroundColor Re
Write-Host  DATA AS A RESULT OF YOUR USE OF THIS SOFTWARE. -ForegroundColor Red
Write-Output ""
Write-Host         ****************************************************** -ForegroundColor Blue
Write-Output ""
Write-Output ""

if (Test-Path config.dat) {
  Write-Host "Looks like there's already a WWIV install."  -ForegroundColor Red
  Write-Host "If you really meant to install, please check the target directory"  -ForegroundColor Red
  Write-Host "before trying again.  Aborting the install."  -ForegroundColor Red
  Exit
}

function Say {
  Write-host $args -ForegroundColor DarkCyan
}

function WWIV-Unzip {
  Param($ZipFile, $Dir)
  Say .\unzip.exe -qq -o $ZipFile -d $Dir
}

function YesNo {
  Param ($Prompt)
    while("y","n" -notcontains $answer)
    {
	    $answer = Read-Host $Prompt "(y/n)"
    }  
    return ($Answer -eq 'y')
}

function UnzipFiles {
    WWIV-Unzip -ZipFile inifiles.zip -Dir .
    WWIV-Unzip -ZipFile gfiles.zip -Dir gfiles
    WWIV-Unzip -ZipFile scripts.zip -Dir scripts
    WWIV-Unzip -ZipFile data.zip -Dir data
    WWIV-Unzip -ZipFile regions.zip -Dir data
    WWIV-Unzip -ZipFile zip-city.zip -Dir data
}

# ******************************* run INIT to convert data files to
# ******************************* v5.00 format
if (!(YesNo -Prompt "Do you want to install WWIV 5?")) {
    Say "That's ok, Thanks anyway"
    Exit
}

UnzipFiles
Say wwivconfig.exe

Write-Host "Installation Complete" -ForegroundColor Green
Write-Host "If you need any assistance, check out the docs or find us on IRC." -ForegroundColor DarkCyan


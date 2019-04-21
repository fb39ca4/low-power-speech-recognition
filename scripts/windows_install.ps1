# Download and install scoop
Try {
    Invoke-Expression "scoop update"
    Write-Output "scoop is detected"
}
Catch {
    Write-Output "scoop was not found, installing"
    powershell -c "iex (new-object net.webclient).downloadstring('https://get.scoop.sh')"
    # Refresh PATH since scoop was just installed
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
}

# Checks if a command is available
function Check-Available {
    param([string]$testCommand)
    Try {
        & ([scriptblock]::Create($testCommand))
        If ($lastExitCode -ne 0) {
            throw
        }
        return $True
    }
    Catch {
        return $False
    }
}

# Installs a package from scoop if the provided command fails to run
$global:toInstall = @()
$global:alreadyInstalled = @()
function Install-Necessary {
    param([string]$package, [string]$testCommand, [string]$bucket = $null, [string]$bucketURL = $null)
    Write-Output "Checking for $package with command $testCommand"
    If (Check-Available($testCommand)) {
        Write-Output "$package is detected"
        $global:alreadyInstalled += $package
    }
    Else {
        Write-Output "$package is not detected, installing from scoop"
        $global:toInstall += $package
        scoop bucket add $bucket $bucketURL
        scoop install $package
    }
}

# Install software if not present
Install-Necessary git "git"
Install-Necessary gcc7-arm-none-eabi "arm-none-eabi-gcc -dumpversion" fb39ca4 "https://github.com/fb39ca4/scoop-fb39ca4.git"
Install-Necessary make "make --version"
Install-Necessary cmake "cmake --version"
Install-Necessary openocd "openocd --version" fb39ca4 "https://github.com/fb39ca4/scoop-fb39ca4.git"
Install-Necessary doxygen "doxygen --version"
Install-Necessary graphviz "gvgen -?"

Write-Output "Already installed: $alreadyInstalled"
Write-Output "Newly Installed: $toInstall"

[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip"
        )
    }

    Remove-Item @RemoveArgs

    Log-Group "Archiving ${ProductName}..."
    $CompressArgs = @{
        Path = (Get-ChildItem -Path "${ProjectRoot}/release/${Configuration}" -Exclude "${OutputName}*.*")
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs
    Log-Group

    # Installeur .exe (Inno Setup), en plus du .zip. ISCC.exe est preinstalle sur les
    # runners Windows de GitHub ; absent (machine sans Inno Setup 6) -> on saute proprement
    # (le .zip reste produit). L'.iss remappe vers le layout "bundled" d'OBS et detecte le
    # dossier OBS via le registre (cf. cmake/windows/resources/installer-Windows.iss).
    $IsccCandidates = @(
        "${Env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
        "${Env:ProgramFiles}\Inno Setup 6\ISCC.exe"
    )
    $Iscc = $IsccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

    if ( $Iscc ) {
        Log-Group "Building Windows installer ${ProductName}-${ProductVersion}-windows-${Target}.exe..."
        $IssFile = "${ProjectRoot}/cmake/windows/resources/installer-Windows.iss"
        $InstallerArgs = @(
            '/Qp'
            "/DMyAppVersion=${ProductVersion}"
            "/DMyModuleName=${ProductName}"
            "/DSourceDir=${ProjectRoot}/release/${Configuration}/${ProductName}"
            "/DOutputDir=${ProjectRoot}/release"
            "${IssFile}"
        )
        Invoke-External "$Iscc" @InstallerArgs
        Log-Group
    } else {
        Write-Warning 'Inno Setup (ISCC.exe) introuvable - installeur .exe non genere (zip seul).'
    }
}

Package

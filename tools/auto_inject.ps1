param(
    [switch]$Build,
    [switch]$UnloadOnly,
    [int]$TargetPid = 0,
    [string]$ProcessName = "rf4_x64",
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$CMakeExe = "",
    [string]$BuildDir = "",
    [string]$SourceDll = "",
    [string]$TargetDll = ""
)

$ErrorActionPreference = "Stop"

if (-not $BuildDir) {
    $BuildDir = Join-Path $ProjectRoot "build\cmake\msvc-x64"
}
if (-not $SourceDll) {
    $SourceDll = Join-Path $BuildDir "Release\FishingCompanion.dll"
}
if (-not $TargetDll) {
    $TargetDll = $SourceDll
}

function Write-Step([string]$Text) {
    Write-Host "[auto_inject] $Text"
}

function Resolve-CMakeExe([string]$Value) {
    if ($Value) {
        if (Test-Path -LiteralPath $Value) {
            return (Resolve-Path -LiteralPath $Value).Path
        }
        throw "CMake executable was not found: $Value"
    }

    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $vsCMake = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path -LiteralPath $vsCMake) {
        return $vsCMake
    }

    throw "CMake executable was not found. Pass -CMakeExe or add cmake to PATH."
}

function Get-TargetProcess {
    if ($TargetPid -ne 0) {
        return Get-Process -Id $TargetPid -ErrorAction Stop
    }

    $process = Get-Process -Name $ProcessName -ErrorAction Stop |
        Sort-Object StartTime -Descending |
        Select-Object -First 1

    if (-not $process) {
        throw "Target process '$ProcessName' was not found."
    }

    return $process
}

function Ensure-User32 {
    if ("AutoInject.User32" -as [type]) {
        return
    }

    Add-Type -Namespace AutoInject -Name User32 -MemberDefinition @"
[System.Runtime.InteropServices.DllImport("user32.dll", SetLastError=true)]
public static extern bool PostMessage(System.IntPtr hWnd, uint Msg, System.IntPtr wParam, System.IntPtr lParam);
"@
}

function Test-ModuleLoaded([System.Diagnostics.Process]$Process) {
    $Process.Refresh()
    foreach ($module in $Process.Modules) {
        if ($module.ModuleName -ieq "FishingCompanion.dll") {
            return $true
        }
    }
    return $false
}

function Get-LoadedModulePath([System.Diagnostics.Process]$Process) {
    $Process.Refresh()
    foreach ($module in $Process.Modules) {
        if ($module.ModuleName -ieq "FishingCompanion.dll") {
            return $module.FileName
        }
    }
    return ""
}

function Unload-ExistingDll([System.Diagnostics.Process]$Process) {
    if (-not (Test-ModuleLoaded $Process)) {
        Write-Step "FishingCompanion.dll is not loaded; no unload needed."
        return
    }

    Ensure-User32
    Write-Step "Sending END to unload existing DLL from PID $($Process.Id)."
    [AutoInject.User32]::PostMessage($Process.MainWindowHandle, 0x0100, [IntPtr]0x23, [IntPtr]0) | Out-Null
    Start-Sleep -Milliseconds 80
    [AutoInject.User32]::PostMessage($Process.MainWindowHandle, 0x0101, [IntPtr]0x23, [IntPtr]0) | Out-Null

    $deadline = (Get-Date).AddSeconds(8)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
        if (-not (Test-ModuleLoaded $Process)) {
            Write-Step "Existing DLL unloaded."
            return
        }
    }

    throw "Existing FishingCompanion.dll did not unload after END."
}

function Ensure-RemoteLoader {
    if ("AutoInject.RemoteLoadLibrary" -as [type]) {
        return
    }

    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

namespace AutoInject {
    public static class RemoteLoadLibrary {
        [DllImport("kernel32.dll", SetLastError=true)]
        public static extern IntPtr OpenProcess(uint access, bool inherit, int pid);

        [DllImport("kernel32.dll", SetLastError=true)]
        public static extern IntPtr VirtualAllocEx(IntPtr process, IntPtr address, UIntPtr size, uint allocationType, uint protect);

        [DllImport("kernel32.dll", SetLastError=true)]
        public static extern bool WriteProcessMemory(IntPtr process, IntPtr address, byte[] buffer, UIntPtr size, out UIntPtr written);

        [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
        public static extern IntPtr GetModuleHandle(string moduleName);

        [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Ansi)]
        public static extern IntPtr GetProcAddress(IntPtr module, string procName);

        [DllImport("kernel32.dll", SetLastError=true)]
        public static extern IntPtr CreateRemoteThread(IntPtr process, IntPtr attributes, UIntPtr stackSize, IntPtr startAddress, IntPtr parameter, uint flags, out uint threadId);

        [DllImport("kernel32.dll", SetLastError=true)]
        public static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

        [DllImport("kernel32.dll", SetLastError=true)]
        public static extern bool GetExitCodeThread(IntPtr thread, out uint exitCode);

        [DllImport("kernel32.dll", SetLastError=true)]
        public static extern bool CloseHandle(IntPtr handle);
    }
}
"@
}

function Inject-Dll([int]$TargetPid, [string]$DllPath) {
    Ensure-RemoteLoader

    $process = [AutoInject.RemoteLoadLibrary]::OpenProcess(0x001F0FFF, $false, $TargetPid)
    if ($process -eq [IntPtr]::Zero) {
        throw "OpenProcess failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }

    try {
        $bytes = [Text.Encoding]::Unicode.GetBytes($DllPath + [char]0)
        $size = [UIntPtr]::new([uint64]$bytes.Length)
        $remote = [AutoInject.RemoteLoadLibrary]::VirtualAllocEx($process, [IntPtr]::Zero, $size, 0x3000, 0x04)
        if ($remote -eq [IntPtr]::Zero) {
            throw "VirtualAllocEx failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
        }

        $written = [UIntPtr]::Zero
        if (-not [AutoInject.RemoteLoadLibrary]::WriteProcessMemory($process, $remote, $bytes, $size, [ref]$written)) {
            throw "WriteProcessMemory failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
        }

        $kernel32 = [AutoInject.RemoteLoadLibrary]::GetModuleHandle("kernel32.dll")
        $loadLibrary = [AutoInject.RemoteLoadLibrary]::GetProcAddress($kernel32, "LoadLibraryW")
        if ($loadLibrary -eq [IntPtr]::Zero) {
            throw "GetProcAddress(LoadLibraryW) failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
        }

        [uint32]$threadId = 0
        $thread = [AutoInject.RemoteLoadLibrary]::CreateRemoteThread(
            $process,
            [IntPtr]::Zero,
            [UIntPtr]::Zero,
            $loadLibrary,
            $remote,
            0,
            [ref]$threadId)

        if ($thread -eq [IntPtr]::Zero) {
            throw "CreateRemoteThread failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
        }

        try {
            $wait = [AutoInject.RemoteLoadLibrary]::WaitForSingleObject($thread, 15000)
            [uint32]$exitCode = 0
            [AutoInject.RemoteLoadLibrary]::GetExitCodeThread($thread, [ref]$exitCode) | Out-Null

            if ($wait -ne 0) {
                throw "LoadLibraryW remote thread timed out or failed, wait=$wait, exit=0x$("{0:X8}" -f $exitCode)."
            }
            if ($exitCode -eq 0) {
                throw "LoadLibraryW returned NULL."
            }

            Write-Step "Remote LoadLibraryW returned 0x$("{0:X8}" -f $exitCode) on thread $threadId."
        }
        finally {
            [AutoInject.RemoteLoadLibrary]::CloseHandle($thread) | Out-Null
        }
    }
    finally {
        [AutoInject.RemoteLoadLibrary]::CloseHandle($process) | Out-Null
    }
}

$process = $null

if ($Build -or $UnloadOnly) {
    $process = Get-TargetProcess
    Write-Step "Target PID $($process.Id): $($process.Path)"
    Unload-ExistingDll $process

    if ($UnloadOnly) {
        Write-Step "Unload-only requested; skipping injection."
        return
    }
}

if ($Build) {
    $cmake = Resolve-CMakeExe $CMakeExe

    Write-Step "Building Release DLL from $ProjectRoot."
    $cachePath = Join-Path $BuildDir "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath)) {
        & $cmake -S $ProjectRoot --preset msvc-x64
        if ($LASTEXITCODE -ne 0) {
            throw "Configure failed with exit code $LASTEXITCODE."
        }
    }
    & $cmake --build $BuildDir --config Release --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }
}

if (-not $UnloadOnly -and -not (Test-Path -LiteralPath $SourceDll)) {
    throw "Source DLL was not found: $SourceDll"
}

if (-not $process) {
    $process = Get-TargetProcess
    Write-Step "Target PID $($process.Id): $($process.Path)"
    Unload-ExistingDll $process
}

$targetDir = Split-Path -Parent $TargetDll
if (-not (Test-Path -LiteralPath $targetDir)) {
    New-Item -ItemType Directory -Path $targetDir | Out-Null
}

$sourceFullPath = [IO.Path]::GetFullPath($SourceDll)
$targetFullPath = [IO.Path]::GetFullPath($TargetDll)
if ($sourceFullPath -ine $targetFullPath) {
    Copy-Item -LiteralPath $SourceDll -Destination $TargetDll -Force
    Write-Step "Copied DLL to $TargetDll"
}
else {
    Write-Step "Using built DLL in place: $TargetDll"
}
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $TargetDll).Hash
Write-Step "SHA256 $hash"

Inject-Dll $process.Id $TargetDll
Start-Sleep -Seconds 2

$loadedPath = Get-LoadedModulePath $process
if (-not $loadedPath) {
    throw "FishingCompanion.dll is not loaded after injection."
}

Write-Step "Loaded module: $loadedPath"

$logPath = Join-Path (Split-Path -Parent $process.Path) "FishingCompanion_actions.log"
if (Test-Path -LiteralPath $logPath) {
    Write-Step "Last action log lines:"
    Get-Content -LiteralPath $logPath -Tail 30
}

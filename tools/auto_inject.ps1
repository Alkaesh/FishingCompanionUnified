param(
    [switch]$Build,
    [switch]$UnloadOnly,
    [int]$TargetPid = 0,
    [string]$ProcessName = "rf4_x64",
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$CMakeExe = "",
    [string]$ConfigurePreset = "msvc-x64-vs18",
    [string]$BuildPreset = "release-vs18",
    [string]$Configuration = "Release",
    [string]$BuildDir = "",
    [string]$SourceDll = "",
    [string]$TargetDll = "",
    [int]$UnloadTimeoutSeconds = 10,
    [int]$InjectTimeoutSeconds = 15
)

$ErrorActionPreference = "Stop"

if (-not $BuildDir) {
    $BuildDir = Join-Path $ProjectRoot "build\cmake\vs18-x64"
}
if (-not $SourceDll) {
    $SourceDll = Join-Path $BuildDir "$Configuration\FishingCompanion.dll"
}
if (-not $TargetDll) {
    $TargetDll = $SourceDll
}

function Write-Step([string]$Text) {
    Write-Host "[auto_inject] $Text"
}

trap {
    Write-Host "[auto_inject] ERROR: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

function Get-LastWin32ErrorText {
    $code = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    $message = [System.ComponentModel.Win32Exception]::new($code).Message
    return "$code ($message)"
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

    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\18\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    throw "CMake executable was not found. Pass -CMakeExe or add cmake to PATH."
}

function Assert-64BitHost {
    if (-not [Environment]::Is64BitProcess) {
        throw "Run this script from 64-bit PowerShell. The target process and DLL are x64."
    }
}

function Get-TargetProcess {
    if ($TargetPid -ne 0) {
        $byId = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue
        if (-not $byId) {
            throw "Target PID $TargetPid was not found."
        }
        return $byId
    }

    $process = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue |
        Sort-Object StartTime -Descending |
        Select-Object -First 1

    if (-not $process) {
        throw "Target process '$ProcessName' was not found."
    }

    if (-not $process.Responding) {
        Write-Step "Warning: target process is not responding right now."
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
    try {
        foreach ($module in $Process.Modules) {
            if ($module.ModuleName -ieq "FishingCompanion.dll") {
                return $true
            }
        }
    }
    catch {
        throw "Unable to enumerate modules for PID $($Process.Id). Run as Administrator or check process bitness. $($_.Exception.Message)"
    }
    return $false
}

function Get-LoadedModulePath([System.Diagnostics.Process]$Process) {
    $Process.Refresh()
    try {
        foreach ($module in $Process.Modules) {
            if ($module.ModuleName -ieq "FishingCompanion.dll") {
                return $module.FileName
            }
        }
    }
    catch {
        throw "Unable to enumerate modules for PID $($Process.Id). Run as Administrator or check process bitness. $($_.Exception.Message)"
    }
    return ""
}

function Unload-ExistingDll([System.Diagnostics.Process]$Process) {
    if (-not (Test-ModuleLoaded $Process)) {
        Write-Step "FishingCompanion.dll is not loaded; no unload needed."
        return
    }

    Ensure-User32
    if ($Process.MainWindowHandle -eq [IntPtr]::Zero) {
        throw "Existing DLL is loaded, but target PID $($Process.Id) has no main window handle for hotkey unload."
    }

    Write-Step "Sending END to unload existing DLL from PID $($Process.Id)."
    if (-not [AutoInject.User32]::PostMessage($Process.MainWindowHandle, 0x0100, [IntPtr]0x23, [IntPtr]0x014F0001)) {
        throw "PostMessage(WM_KEYDOWN/END) failed: $(Get-LastWin32ErrorText)"
    }
    Start-Sleep -Milliseconds 80
    [AutoInject.User32]::PostMessage($Process.MainWindowHandle, 0x0101, [IntPtr]0x23, [IntPtr]0) | Out-Null

    $deadline = (Get-Date).AddSeconds($UnloadTimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
        if (-not (Test-ModuleLoaded $Process)) {
            Write-Step "Existing DLL unloaded."
            return
        }
    }

    throw "Existing FishingCompanion.dll did not unload within $UnloadTimeoutSeconds second(s)."
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
        public static extern bool VirtualFreeEx(IntPtr process, IntPtr address, UIntPtr size, uint freeType);

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

    $processAccess = 0x0002 -bor 0x0008 -bor 0x0020 -bor 0x0400

    $process = [AutoInject.RemoteLoadLibrary]::OpenProcess($processAccess, $false, $TargetPid)
    if ($process -eq [IntPtr]::Zero) {
        throw "OpenProcess failed: $(Get-LastWin32ErrorText)"
    }

    $remote = [IntPtr]::Zero
    try {
        $bytes = [Text.Encoding]::Unicode.GetBytes($DllPath + [char]0)
        $size = [UIntPtr]::new([uint64]$bytes.Length)
        $remote = [AutoInject.RemoteLoadLibrary]::VirtualAllocEx($process, [IntPtr]::Zero, $size, 0x3000, 0x04)
        if ($remote -eq [IntPtr]::Zero) {
            throw "VirtualAllocEx failed: $(Get-LastWin32ErrorText)"
        }

        $written = [UIntPtr]::Zero
        if (-not [AutoInject.RemoteLoadLibrary]::WriteProcessMemory($process, $remote, $bytes, $size, [ref]$written)) {
            throw "WriteProcessMemory failed: $(Get-LastWin32ErrorText)"
        }
        if ($written.ToUInt64() -ne [uint64]$bytes.Length) {
            throw "WriteProcessMemory wrote $($written.ToUInt64()) of $($bytes.Length) bytes."
        }

        $kernel32 = [AutoInject.RemoteLoadLibrary]::GetModuleHandle("kernel32.dll")
        $loadLibrary = [AutoInject.RemoteLoadLibrary]::GetProcAddress($kernel32, "LoadLibraryW")
        if ($loadLibrary -eq [IntPtr]::Zero) {
            throw "GetProcAddress(LoadLibraryW) failed: $(Get-LastWin32ErrorText)"
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
            throw "CreateRemoteThread failed: $(Get-LastWin32ErrorText)"
        }

        try {
            $wait = [AutoInject.RemoteLoadLibrary]::WaitForSingleObject($thread, [uint32]($InjectTimeoutSeconds * 1000))
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
        if ($remote -ne [IntPtr]::Zero) {
            [AutoInject.RemoteLoadLibrary]::VirtualFreeEx($process, $remote, [UIntPtr]::Zero, 0x8000) | Out-Null
        }
        [AutoInject.RemoteLoadLibrary]::CloseHandle($process) | Out-Null
    }
}

Assert-64BitHost
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

    Write-Step "Building $Configuration DLL from $ProjectRoot."
    $cachePath = Join-Path $BuildDir "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath)) {
        & $cmake --preset $ConfigurePreset
        if ($LASTEXITCODE -ne 0) {
            throw "Configure failed with exit code $LASTEXITCODE."
        }
    }

    if ($BuildPreset) {
        & $cmake --build --preset $BuildPreset
    }
    else {
        & $cmake --build $BuildDir --config $Configuration --parallel
    }
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

# run-jpegoptim-parallel.ps1
# Split all *.jpg files in the current directory into 8 groups
# and run jpegoptim in 8 parallel processes.

# Number of processes to start (max 8; if files < 8, use file count)
$parts = 8

# Path to jpegoptim executable (keep quotes if there are spaces / non-ASCII characters)
$exe = "E:\jpegoptim-1.5.6-x64-windows\jpegoptim.exe"

# Base jpegoptim parameters: -m 80 -p -t -n
$baseArgs = @("-m","80","-p","-t","-n")

# Print current date and time
$startTime = Get-Date
Write-Host "Script start time: $($startTime.ToString('yyyy-MM-dd HH:mm:ss'))" -ForegroundColor Green
Write-Host ""

# Find all jpg/jpeg files in current directory (case-insensitive)
$files = Get-ChildItem -File *.jpg, *.jpeg |
         Sort-Object Name |
         Select-Object -ExpandProperty FullName

$count = $files.Count
if ($count -eq 0) {
    Write-Host "No *.jpg / *.jpeg files found in current directory."
    return
}

if ($count -lt $parts) { $parts = $count }

# Compute group size (ceil so groups differ by at most 1)
$per = [math]::Ceiling($count / $parts)

Write-Host "Found $count images, split into $parts groups, approx $per per group (last group may be smaller)."

$processes = @()

for ($i = 0; $i -lt $parts; $i++) {
    $start = $i * $per
    if ($start -ge $count) { break }   # no more files → exit loop

    $end = [math]::Min($start + $per - 1, $count - 1)
    $group = $files[$start..$end]

    # Write list of file names into a txt file (one per line)
    $currentDir = Get-Location
    $txtFile = Join-Path $currentDir "jpegoptim_files_$i.txt"

    # Write list using UTF-8 without BOM (because BOM breaks first line)
    $utf8NoBom = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllLines($txtFile, $group, $utf8NoBom)

    Write-Host "Starting process $($i+1): files $($start+1)-$($end+1) (count: $($group.Count))"
    Write-Host "  File list written to: $txtFile"

    # Build argument list (base args + --files-from=)
    $argList = @()
    $argList += $baseArgs
    $argList += "--files-from=$txtFile"

    # Start jpegoptim inside cmd.exe /k so window stays open after finishing
    $proc = Start-Process -FilePath "cmd.exe" `
        -ArgumentList "/k `"$exe $argList`"" `
        -PassThru

    # Record process info
    $processes += [PSCustomObject]@{
        Index = $i+1
        PID   = $proc.Id
        Files = $group.Count
    }
}

Write-Host "Started $($processes.Count) jpegoptim processes:"
$processes | Format-Table -AutoSize

# Writes the build number of version.h from the WSC_BUILD_NUMBER environment variable (set by the CI)
# Local builds keep the committed build number, so version.h isn't modified on each build
$path = $args[0]
if (-not $env:WSC_BUILD_NUMBER) { exit 0 }

(Get-Content -Path $path -Raw) -replace '#define VERSION_BUILD \d+', "#define VERSION_BUILD $env:WSC_BUILD_NUMBER" |
    Out-File $path -Encoding ascii -NoNewline

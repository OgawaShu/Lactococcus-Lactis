param(
    [string]$CertPath = "src/third_party/certs/server.crt",
    [string]$KeyPath = "src/third_party/certs/server.key"
)

Write-Host "Ensure output directories exist"
$certDir = Split-Path -Path $CertPath -Parent
if (!(Test-Path $certDir)) { New-Item -ItemType Directory -Path $certDir -Force | Out-Null }

$openssl = Get-Command openssl -ErrorAction SilentlyContinue
if (-not $openssl) {
    Write-Host "OpenSSL not found in PATH. Trying to install via common package managers..."
    $installed = $false

    if (Get-Command choco -ErrorAction SilentlyContinue) {
        Write-Host "Installing OpenSSL (choco)..."
        choco install openssl.light -y
        $installed = $true
    } elseif (Get-Command winget -ErrorAction SilentlyContinue) {
        Write-Host "Installing OpenSSL (winget)..."
        try {
            winget install --id OpenSSL.OpenSSL -e --silent
            $installed = $true
        } catch {
            Write-Host "winget install failed or package id not present; please install OpenSSL manually."
        }
    } else {
        Write-Host "No automatic installer found (choco/winget). Please install OpenSSL and ensure 'openssl' is on PATH."
    }

    $openssl = Get-Command openssl -ErrorAction SilentlyContinue
    if (-not $openssl -and -not $installed) {
        Write-Error "OpenSSL CLI not available. Install OpenSSL and re-run this script."
        exit 1
    }
}

Write-Host "Generating self-signed certificate"
$cmd = "openssl req -x509 -newkey rsa:2048 -days 365 -nodes -keyout `"$KeyPath`" -out `"$CertPath`" -subj `/CN=localhost`"
Write-Host $cmd
$rc = cmd /c $cmd
if ($LASTEXITCODE -ne 0) {
    Write-Error "openssl failed to generate certificate (exit $LASTEXITCODE)"
    exit 1
}

Write-Host "Certificate generated: $CertPath"
Write-Host "Private key generated: $KeyPath"
Write-Host "Done."

param(
    [string]$Host = "127.0.0.1",
    [int]$Port = 8080,
    [string]$HelloName = $env:USERNAME
)

$baseUri = "http://$Host`:$Port"
$escapedName = [Uri]::EscapeDataString($HelloName)

$commands = @(
    [PSCustomObject]@{
        Name        = "Command Catalog"
        Method      = "GET"
        Path        = "/api/commands"
        Description = "List every API endpoint exposed by the demo server."
    },
    [PSCustomObject]@{
        Name        = "Health Check"
        Method      = "GET"
        Path        = "/api/health"
        Description = "Validate the server is responsive and show uptime/version."
    },
    [PSCustomObject]@{
        Name        = "Hello World"
        Method      = "GET"
        Path        = "/api/hello?name=$escapedName"
        Description = "Send a greeting that includes the supplied name."
    },
    [PSCustomObject]@{
        Name        = "Echo"
        Method      = "POST"
        Path        = "/api/echo"
        Description = "POST a JSON payload and read the echoed content."
        Body        = @{
            client    = "PowerShell"
            timestamp = (Get-Date).ToString("o")
        } | ConvertTo-Json -Compress
    }
)

Write-Host "APIM demo client" -ForegroundColor Cyan
Write-Host "Base URL: $baseUri" -ForegroundColor Cyan
Write-Host ""

foreach ($command in $commands) {
    $uri = "$baseUri$($command.Path)"
    Write-Host ("`n[{0}] {1} - {2}" -f $command.Method, $command.Path, $command.Description) -ForegroundColor Yellow
    try {
        if ($command.Method -eq "GET") {
            $response = Invoke-WebRequest -UseBasicParsing -Uri $uri -Method Get -ErrorAction Stop
        }
        elseif ($command.Method -eq "POST") {
            $response = Invoke-WebRequest -UseBasicParsing -Uri $uri -Method Post -Body $command.Body -ContentType "application/json" -ErrorAction Stop
        }
        else {
            throw "Unsupported HTTP method: $($command.Method)"
        }

        Write-Host ("Status: {0}" -f $response.StatusCode) -ForegroundColor Green
        if ($response.Content) {
            try {
                $json = $response.Content | ConvertFrom-Json -ErrorAction Stop
                $pretty = $json | ConvertTo-Json -Depth 5
                Write-Output $pretty
            }
            catch {
                Write-Output $response.Content
            }
        }
    }
    catch {
        Write-Host "Request failed: $($_.Exception.Message)" -ForegroundColor Red
    }
}

param(
    [string]$ServerHost = "127.0.0.1",
    [int]$Port = 8080,
    [string]$HelloName = $env:USERNAME
)

$baseUri = "http://$ServerHost`:$Port"
$escapedName = [Uri]::EscapeDataString($HelloName)

Write-Host "APIM demo client" -ForegroundColor Cyan
Write-Host "Base URL: $baseUri" -ForegroundColor Cyan
Write-Host ""

$commands = @()

$commands += [PSCustomObject]@{
    Name        = "Command Catalog"
    Method      = "GET"
    Path        = "/api/commands"
    Description = "List every API endpoint exposed by the server."
}

$commands += [PSCustomObject]@{
    Name        = "Health Check"
    Method      = "GET"
    Path        = "/api/health"
    Description = "Validate the server is responsive and show uptime/version."
}

$commands += [PSCustomObject]@{
    Name        = "Hello World"
    Method      = "GET"
    Path        = "/api/hello?name=$escapedName"
    Description = "Send a greeting that includes the supplied name."
}

$commands += [PSCustomObject]@{
    Name        = "Echo"
    Method      = "POST"
    Path        = "/api/echo"
    Description = "POST a JSON payload and read the echoed content."
    Body        = @{
        client    = "PowerShell"
        timestamp = (Get-Date).ToString("o")
    } | ConvertTo-Json -Compress
}

$commands += [PSCustomObject]@{
    Name        = "Checklists"
    Method      = "GET"
    Path        = "/api/checklists"
    Description = "List every checklist present in the SQLite runtime store."
}

$seededChecklist = $null
$seededSlug = $null

try {
    $checklistResponse = Invoke-WebRequest -UseBasicParsing -Uri "$baseUri/api/checklists" -Method Get -ErrorAction Stop
    $checklistJson = $checklistResponse.Content | ConvertFrom-Json -ErrorAction Stop
    if ($checklistJson.checklists.Count -gt 0) {
        $seededChecklist = $checklistJson.checklists[0]
    }
}
catch {
    Write-Host "Could not pre-load checklist names: $($_.Exception.Message)" -ForegroundColor Yellow
}

if ($seededChecklist) {
    $commands += [PSCustomObject]@{
        Name        = "Checklist slugs"
        Method      = "GET"
        Path        = "/api/checklist/$seededChecklist"
        Description = "Retrieve slugs for checklist '$seededChecklist'."
    }

    try {
        $slugResponse = Invoke-WebRequest -UseBasicParsing -Uri "$baseUri/api/checklist/$seededChecklist" -Method Get -ErrorAction Stop
        $slugJson = $slugResponse.Content | ConvertFrom-Json -ErrorAction Stop
        if ($slugJson.slugs.Count -gt 0) {
            $seededSlug = $slugJson.slugs[0]
        }
    }
    catch {
        Write-Host "Could not load slugs for $seededChecklist: $($_.Exception.Message)" -ForegroundColor Yellow
    }
}

if ($seededSlug) {
    $slugId = $seededSlug.checklist_id
    $commands += [PSCustomObject]@{
        Name        = "Slug by ID"
        Method      = "GET"
        Path        = "/api/slug/$slugId"
        Description = "Return the seeded slug identified by '$slugId'."
    }
    $commands += [PSCustomObject]@{
        Name        = "Relationships"
        Method      = "GET"
        Path        = "/api/relationships/$slugId"
        Description = "Return graph edges for '$slugId'."
    }
    $commands += [PSCustomObject]@{
        Name        = "Update slug"
        Method      = "PATCH"
        Path        = "/api/update"
        Description = "Minimal update contract for the seeded slug."
        Body        = @{
            checklist_id = $slugId
            comment      = "Updated via PowerShell demo"
            status       = "Other"
        } | ConvertTo-Json -Compress
    }
    $commands += [PSCustomObject]@{
        Name        = "Export Markdown"
        Method      = "GET"
        Path        = "/api/export/markdown/$seededChecklist"
        Description = "Export checklist '$seededChecklist' as canonical Markdown."
    }
    $commands += [PSCustomObject]@{
        Name        = "Import Markdown"
        Method      = "POST"
        Path        = "/api/import/markdown?checklist=$seededChecklist"
        Description = "Import Markdown to replace checklist '$seededChecklist'."
        Body        = @"
# Demo import

## Example procedure
- **Action**: Replace runtime state
- **Spec**: Spec text
- **Result**: pending
- **Status**: Other
- **Comment**: Imported from PowerShell demo

### Instructions
Draft instructions.

### Relationships
**Checklist ID:** TEMPORARYID123456
- depends_on $slugId
"@
    }
}

$commands += [PSCustomObject]@{
    Name        = "Export JSON"
    Method      = "GET"
    Path        = "/api/export/json"
    Description = "Export all slugs as a JSON array."
}

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
        elseif ($command.Method -eq "PATCH") {
            $response = Invoke-WebRequest -UseBasicParsing -Uri $uri -Method Patch -Body $command.Body -ContentType "application/json" -ErrorAction Stop
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

param(
    [int]$Port = 8081
)

$webRoot = Split-Path -Parent $PSCommandPath
$repoRoot = (Resolve-Path "$webRoot\..\..").Path
$listener = [System.Net.HttpListener]::new()
$prefix = "http://127.0.0.1:$Port/"
$listener.Prefixes.Add($prefix)

Write-Host "Serving $webRoot on $prefix (Ctrl+C to stop)" -ForegroundColor Cyan
$listener.Start()

$mimeMap = @{
    ".html" = "text/html; charset=utf-8"
    ".css"  = "text/css"
    ".js"   = "text/javascript"
    ".json" = "application/json"
    ".png"  = "image/png"
    ".jpg"  = "image/jpeg"
    ".jpeg" = "image/jpeg"
    ".svg"  = "image/svg+xml"
}

try {
    while ($true) {
        $context = $listener.GetContext()
        $relPath = $context.Request.Url.AbsolutePath
        if ($relPath.StartsWith("/")) {
            $relPath = $relPath.Substring(1)
        }
        if ([string]::IsNullOrEmpty($relPath)) {
            $relPath = "index.html"
        }

        $fullPath = [System.IO.Path]::GetFullPath((Join-Path $webRoot $relPath))
        if (-not $fullPath.StartsWith($repoRoot, [StringComparison]::OrdinalIgnoreCase)) {
            $context.Response.StatusCode = 403
            $bytes = [System.Text.Encoding]::UTF8.GetBytes("Access denied.")
            $context.Response.OutputStream.Write($bytes, 0, $bytes.Length)
            $context.Response.Close()
            continue
        }

        $targetPath = $fullPath
        if (-not (Test-Path $targetPath)) {
            $context.Response.StatusCode = 404
            $bytes = [System.Text.Encoding]::UTF8.GetBytes("Not found: $relPath")
            $context.Response.OutputStream.Write($bytes, 0, $bytes.Length)
            $context.Response.Close()
            continue
        }

        $item = Get-Item $targetPath
        if ($item.PSIsContainer) {
            $entries = Get-ChildItem $targetPath | ForEach-Object {
                "<li><a href=""$($relPath.TrimEnd('/') + '/' + $_.Name)"">$($_.Name)</a></li>"
            } -join ""
            $html = "<html><body><h3>Index of /$relPath</h3><ul>$entries</ul></body></html>"
            $bytes = [System.Text.Encoding]::UTF8.GetBytes($html)
            $context.Response.StatusCode = 200
            $context.Response.ContentType = "text/html; charset=utf-8"
            $context.Response.ContentLength64 = $bytes.Length
            $context.Response.OutputStream.Write($bytes, 0, $bytes.Length)
            $context.Response.Close()
            continue
        }

        $extension = [System.IO.Path]::GetExtension($targetPath).ToLowerInvariant()
        $contentType = $mimeMap[$extension]
        if (-not $contentType) {
            $contentType = "application/octet-stream"
        }

        $bytes = [System.IO.File]::ReadAllBytes($targetPath)
        $context.Response.StatusCode = 200
        $context.Response.ContentType = $contentType
        $context.Response.ContentLength64 = $bytes.Length
        $context.Response.OutputStream.Write($bytes, 0, $bytes.Length)
        $context.Response.Close()
    }
}
finally {
    $listener.Stop()
    $listener.Close()
}

$ErrorActionPreference = "Stop"

# Read the first line of CHANGELOG.md to get the version
$changelogPath = "CHANGELOG.md"
if (-Not (Test-Path $changelogPath)) {
    Write-Error "CHANGELOG.md not found!"
    exit 1
}

$firstLine = Get-Content $changelogPath -TotalCount 1
# Assuming the first line looks like "# v1.0.0" or "v1.0.0"
$version = $firstLine -replace '^#\s*', '' -replace '\s+$', ''

if ([string]::IsNullOrWhiteSpace($version)) {
    Write-Error "Could not extract version from CHANGELOG.md. Make sure the first line contains the version tag (e.g., # v1.0.0)"
    exit 1
}

Write-Host "Extracted version: $version"

# Check if there are uncommitted changes
$status = git status --porcelain
if ($status) {
    Write-Host "Uncommitted changes detected. Committing..."
    git add .
    git commit -m "Release $version"
}

# Create a tag for the version
Write-Host "Creating tag $version..."
git tag -a $version -m "Release $version"

# Push the commits and the tags
Write-Host "Pushing to origin..."
git push origin master
git push origin $version

Write-Host "Push complete! GitHub Actions will now build and release $version."

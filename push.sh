#!/bin/bash
set -e

# Read the first line of CHANGELOG.md to get the version
if [ ! -f "CHANGELOG.md" ]; then
    echo "Error: CHANGELOG.md not found!"
    exit 1
fi

FIRST_LINE=$(head -n 1 CHANGELOG.md)
# Assuming the first line looks like "# v1.0.0" or "v1.0.0"
# Remove leading "# " and trailing whitespace
VERSION=$(echo "$FIRST_LINE" | sed -e 's/^#[[:space:]]*//' -e 's/[[:space:]]*$//')

if [ -z "$VERSION" ]; then
    echo "Error: Could not extract version from CHANGELOG.md. Make sure the first line contains the version tag (e.g., # v1.0.0)"
    exit 1
fi

echo "Extracted version: $VERSION"

# Check if there are uncommitted changes
if ! git diff-index --quiet HEAD --; then
    echo "Uncommitted changes detected. Committing..."
    git add .
    git commit -m "Release $VERSION"
fi

# Create a tag for the version
echo "Creating tag $VERSION..."
git tag -a "$VERSION" -m "Release $VERSION"

# Push the commits and the tags
echo "Pushing to origin..."
git push origin master
git push origin "$VERSION"

echo "Push complete! GitHub Actions will now build and release $VERSION."

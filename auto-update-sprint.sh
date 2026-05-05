#!/bin/bash
# Auto-update sprint script with detailed logging

# Configuration
JIRA_EMAIL="dariusz.kalita@dynatrace.com"
JIRA_TOKEN=""  # superseded by Cloud Function; no longer used
JIRA_URL="https://dt-rnd.atlassian.net"
BOARD_ID="2355"
GITHUB_TOKEN="${GITHUB_TOKEN:-}"  # Optional: set as environment variable or hardcode here
GITHUB_REPO="github.com/breakpl/stb.git"
REPO_PATH="/home/darek/stb"
LOG_FILE="/tmp/auto-update-sprint.log"

# Logging function
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

log_raw() {
    echo "$1" >> "$LOG_FILE"
}

# Start logging
log "========================================="
log "Starting sprint auto-update process"
log "========================================="
log "JIRA_URL: $JIRA_URL"
log "BOARD_ID: $BOARD_ID"
log "JIRA_EMAIL: $JIRA_EMAIL"
log "REPO_PATH: $REPO_PATH"
log ""

# Fetch active sprints from JIRA with proper authentication
log "Fetching active sprints from JIRA..."
RESPONSE=$(curl -s -u "$JIRA_EMAIL:$JIRA_TOKEN" \
  -H "Accept: application/json" \
  "$JIRA_URL/rest/agile/1.0/board/$BOARD_ID/sprint?state=active")

log "CURL request completed. Response length: ${#RESPONSE} characters"
log ""
log "--- RAW JIRA RESPONSE START ---"
log_raw "$RESPONSE"
log "--- RAW JIRA RESPONSE END ---"
log ""

# Check if authentication failed
if echo "$RESPONSE" | grep -q "must be authenticated"; then
  log "ERROR: JIRA authentication failed. Check your email and API token."
  exit 1
fi
log "Authentication check: PASSED"

# Validate JSON response
log "Validating JSON response..."
if ! echo "$RESPONSE" | jq . > /dev/null 2>&1; then
  log "ERROR: Invalid JSON response from JIRA"
  log "Response was: $RESPONSE"
  exit 1
fi
log "JSON validation: PASSED"

# Extract sprint names and dates
log "Extracting sprint data..."
SPRINT_DATA=$(echo "$RESPONSE" | jq -r '.values[] | "\(.name)|\(.startDate)"')

log "Extracted sprint data:"
log_raw "$SPRINT_DATA"
log ""

if [ -z "$SPRINT_DATA" ]; then
  log "ERROR: No active sprints found"
  exit 1
fi
log "Sprint data extraction: SUCCESS (found $(echo "$SPRINT_DATA" | wc -l) sprints)"

# Find the latest "Dev Sprint XXX"
log "Searching for latest 'Dev Sprint XXX' pattern..."
LATEST=""
MAX_NUM=0

while IFS='|' read -r name start; do
  log "  Checking sprint: '$name' with start date: '$start'"
  
  # Check if it matches "Dev Sprint XXX" pattern
  if [[ "$name" =~ ^Dev\ Sprint\ ([0-9]+)$ ]]; then
    NUM="${BASH_REMATCH[1]}"
    log "    -> Matched! Sprint number: $NUM"

    if [ "$NUM" -gt "$MAX_NUM" ]; then
      MAX_NUM=$NUM
      LATEST="$name|$start"
      log "    -> New maximum found: $NUM"
    else
      log "    -> Not greater than current max: $MAX_NUM"
    fi
  else
    log "    -> Does NOT match 'Dev Sprint XXX' pattern"
  fi
done <<< "$SPRINT_DATA"

log ""
if [ -z "$LATEST" ]; then
  log "ERROR: No 'Dev Sprint XXX' found in active sprints"
  log "Found sprints:"
  echo "$SPRINT_DATA" | cut -d'|' -f1 | while read -r sprint; do
    log "  - $sprint"
  done
  exit 1
fi

# Extract name and date
SPRINT_NAME=$(echo "$LATEST" | cut -d'|' -f1)
START_DATE=$(echo "$LATEST" | cut -d'|' -f2 | cut -d'T' -f1)

log "Selected sprint: $SPRINT_NAME"
log "Start date: $START_DATE"
log ""

# Change to repository directory
log "Changing to repository directory: $REPO_PATH"
if ! cd "$REPO_PATH"; then
  log "ERROR: Failed to change to directory: $REPO_PATH"
  exit 1
fi
log "Current directory: $(pwd)"
log ""

# Create JSON file
log "Creating current-sprint.json..."
cat > current-sprint.json <<EOF
{
  "name": "$SPRINT_NAME",
  "start": "$START_DATE"
}
EOF

if [ -f "current-sprint.json" ]; then
  log "File created successfully. Contents:"
  log_raw "$(cat current-sprint.json)"
  log ""
else
  log "ERROR: Failed to create current-sprint.json"
  exit 1
fi

# Git operations
log "Adding file to git..."
if git add current-sprint.json; then
  log "Git add: SUCCESS"
else
  log "ERROR: Git add failed"
  exit 1
fi

log "Committing changes..."
COMMIT_RESULT=0
if git commit -m "Auto-update sprint: $SPRINT_NAME"; then
  log "Git commit: SUCCESS"
  COMMIT_RESULT=1
else
  log "WARNING: Git commit failed (may be no changes)"
fi

# Check if there are any unpushed commits (new or existing)
UNPUSHED_COMMITS=$(git log origin/main..HEAD --oneline 2>/dev/null | wc -l)
log "Unpushed commits: $UNPUSHED_COMMITS"

# Push if there are any unpushed commits
if [ "$UNPUSHED_COMMITS" -gt 0 ]; then
  log "Pushing to GitHub..."
  # Use token if available (for cron), otherwise use configured credentials
  if [ -n "$GITHUB_TOKEN" ]; then
    log "Using GitHub token for authentication"
    if git push "https://$GITHUB_TOKEN@$GITHUB_REPO" main; then
      log "Git push: SUCCESS"
    else
      log "ERROR: Git push failed"
      exit 1
    fi
  else
    log "Using configured git credentials"
    if git push origin main; then
      log "Git push: SUCCESS"
    else
      log "ERROR: Git push failed"
      exit 1
    fi
  fi
else
  log "No changes to push (sprint data already up to date on GitHub)"
fi

log ""
log "========================================="
log "Sprint auto-update completed successfully!"
log "Log file: $LOG_FILE"
log "========================================="

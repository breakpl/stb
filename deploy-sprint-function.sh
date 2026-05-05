#!/bin/bash
set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration — edit these before first run
# ---------------------------------------------------------------------------
PROJECT_ID="sprinttoolbox"       # gcloud projects list
REGION="europe-west1"                   # pick closest to you
BUCKET="stb-current-sprint"
SECRET_NAME="jira-token"
JIRA_TOKEN=""  # only needed on first deploy to seed Secret Manager; leave empty after that
FUNCTION_NAME="update-sprint"
SCHEDULER_JOB="update-sprint-daily"
# ---------------------------------------------------------------------------

log() { echo "[$(date '+%H:%M:%S')] $*"; }

log "Setting project to $PROJECT_ID"
gcloud config set project "$PROJECT_ID"

# Enable required APIs
log "Enabling APIs..."
gcloud services enable \
  cloudfunctions.googleapis.com \
  cloudscheduler.googleapis.com \
  secretmanager.googleapis.com \
  storage.googleapis.com \
  run.googleapis.com \
  cloudbuild.googleapis.com \
  --quiet

# GCS bucket
log "Creating GCS bucket: $BUCKET"
if ! gsutil ls -b "gs://$BUCKET" &>/dev/null; then
  gsutil mb -p "$PROJECT_ID" -l "$REGION" "gs://$BUCKET"
  log "Bucket created."
else
  log "Bucket already exists, skipping."
fi

log "Making bucket objects publicly readable..."
gsutil iam ch allUsers:objectViewer "gs://$BUCKET"

# Secret Manager
log "Storing Jira token in Secret Manager..."
if gcloud secrets describe "$SECRET_NAME" --project="$PROJECT_ID" &>/dev/null; then
  echo -n "$JIRA_TOKEN" | gcloud secrets versions add "$SECRET_NAME" --data-file=-
  log "Secret version added."
else
  echo -n "$JIRA_TOKEN" | gcloud secrets create "$SECRET_NAME" \
    --data-file=- \
    --replication-policy=automatic
  log "Secret created."
fi

# Grant the default Cloud Functions Gen 2 SA (Compute Engine default) access to the secret
PROJECT_NUMBER=$(gcloud projects describe "$PROJECT_ID" --format="value(projectNumber)")
SA_EMAIL="${PROJECT_NUMBER}-compute@developer.gserviceaccount.com"
log "Granting secret access to $SA_EMAIL"
gcloud secrets add-iam-policy-binding "$SECRET_NAME" \
  --member="serviceAccount:$SA_EMAIL" \
  --role="roles/secretmanager.secretAccessor" \
  --quiet

# Deploy Cloud Function (Gen 2)
log "Deploying Cloud Function..."
gcloud functions deploy "$FUNCTION_NAME" \
  --gen2 \
  --runtime=go122 \
  --region="$REGION" \
  --source="./functions/update-sprint" \
  --entry-point=UpdateSprint \
  --trigger-http \
  --memory=128Mi \
  --timeout=30s \
  --quiet

FUNCTION_URL=$(gcloud functions describe "$FUNCTION_NAME" \
  --gen2 --region="$REGION" --format="value(serviceConfig.uri)")
log "Function URL: $FUNCTION_URL"

# Cloud Scheduler — daily at 06:00 UTC
log "Creating Cloud Scheduler job..."
if gcloud scheduler jobs describe "$SCHEDULER_JOB" --location="$REGION" &>/dev/null; then
  gcloud scheduler jobs update http "$SCHEDULER_JOB" \
    --location="$REGION" \
    --schedule="0 6 * * *" \
    --uri="$FUNCTION_URL" \
    --oidc-service-account-email="$SA_EMAIL" \
    --quiet
  log "Scheduler job updated."
else
  gcloud scheduler jobs create http "$SCHEDULER_JOB" \
    --location="$REGION" \
    --schedule="0 6 * * *" \
    --uri="$FUNCTION_URL" \
    --oidc-service-account-email="$SA_EMAIL" \
    --time-zone="UTC" \
    --quiet
  log "Scheduler job created."
fi

log ""
log "====================================================="
log "Deployment complete."
log "Public sprint URL:"
log "  https://storage.googleapis.com/$BUCKET/current-sprint.json"
log ""
log "Test with:"
log "  gcloud scheduler jobs run $SCHEDULER_JOB --location=$REGION"
log "====================================================="

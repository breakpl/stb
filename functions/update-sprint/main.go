package updatesprint

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"regexp"
	"strconv"
	"strings"
	"time"

	secretmanager "cloud.google.com/go/secretmanager/apiv1"
	"cloud.google.com/go/secretmanager/apiv1/secretmanagerpb"
	"cloud.google.com/go/storage"
	"github.com/GoogleCloudPlatform/functions-framework-go/functions"
)

func init() {
	functions.HTTP("UpdateSprint", UpdateSprint)
}

const (
	bucketName     = "stb-current-sprint"
	objectName     = "current-sprint.json"
	secretName     = "projects/%s/secrets/jira-token/versions/latest"
	jiraURL        = "https://dt-rnd.atlassian.net"
	jiraEmail      = "dariusz.kalita@dynatrace.com"
	boardID        = 2355
	sprintPattern  = `^Dev Sprint (\d+)$`
)

type sprintResponse struct {
	Values []struct {
		Name      string `json:"name"`
		StartDate string `json:"startDate"`
		State     string `json:"state"`
	} `json:"values"`
}

type currentSprint struct {
	Name  string `json:"name"`
	Start string `json:"start"`
}

func UpdateSprint(w http.ResponseWriter, r *http.Request) {
	ctx := r.Context()

	projectID, err := getProjectID(ctx)
	if err != nil {
		http.Error(w, fmt.Sprintf("failed to get project ID: %v", err), http.StatusInternalServerError)
		return
	}

	jiraToken, err := fetchSecret(ctx, fmt.Sprintf(secretName, projectID))
	if err != nil {
		http.Error(w, fmt.Sprintf("failed to fetch Jira token: %v", err), http.StatusInternalServerError)
		return
	}

	sprint, err := fetchLatestDevSprint(jiraToken)
	if err != nil {
		http.Error(w, fmt.Sprintf("failed to fetch sprint: %v", err), http.StatusInternalServerError)
		return
	}

	if err := writeToGCS(ctx, sprint); err != nil {
		http.Error(w, fmt.Sprintf("failed to write to GCS: %v", err), http.StatusInternalServerError)
		return
	}

	w.WriteHeader(http.StatusOK)
	fmt.Fprintf(w, "updated: %s (start: %s)\n", sprint.Name, sprint.Start)
}

func fetchLatestDevSprint(jiraToken string) (*currentSprint, error) {
	url := fmt.Sprintf("%s/rest/agile/1.0/board/%d/sprint?state=active", jiraURL, boardID)

	req, err := http.NewRequest(http.MethodGet, url, nil)
	if err != nil {
		return nil, err
	}
	req.SetBasicAuth(jiraEmail, jiraToken)
	req.Header.Set("Accept", "application/json")

	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("jira returned %d: %s", resp.StatusCode, body)
	}

	var sprints sprintResponse
	if err := json.NewDecoder(resp.Body).Decode(&sprints); err != nil {
		return nil, fmt.Errorf("decode jira response: %w", err)
	}

	re := regexp.MustCompile(sprintPattern)
	var latest *currentSprint
	maxNum := -1

	for _, s := range sprints.Values {
		m := re.FindStringSubmatch(s.Name)
		if m == nil {
			continue
		}
		num, _ := strconv.Atoi(m[1])
		if num > maxNum {
			maxNum = num
			latest = &currentSprint{
				Name:  s.Name,
				Start: strings.SplitN(s.StartDate, "T", 2)[0],
			}
		}
	}

	if latest == nil {
		return nil, fmt.Errorf("no active 'Dev Sprint NNN' found")
	}
	return latest, nil
}

func writeToGCS(ctx context.Context, sprint *currentSprint) error {
	client, err := storage.NewClient(ctx)
	if err != nil {
		return fmt.Errorf("storage client: %w", err)
	}
	defer client.Close()

	data, err := json.MarshalIndent(sprint, "", "  ")
	if err != nil {
		return err
	}

	obj := client.Bucket(bucketName).Object(objectName)
	wc := obj.NewWriter(ctx)
	wc.ContentType = "application/json"
	wc.CacheControl = "no-cache, no-store, must-revalidate"

	if _, err := wc.Write(data); err != nil {
		return err
	}
	return wc.Close()
}

func fetchSecret(ctx context.Context, name string) (string, error) {
	client, err := secretmanager.NewClient(ctx)
	if err != nil {
		return "", fmt.Errorf("secret manager client: %w", err)
	}
	defer client.Close()

	result, err := client.AccessSecretVersion(ctx, &secretmanagerpb.AccessSecretVersionRequest{
		Name: name,
	})
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(result.Payload.Data)), nil
}

// getProjectID fetches the GCP project ID from the metadata server.
func getProjectID(ctx context.Context) (string, error) {
	req, err := http.NewRequestWithContext(ctx, http.MethodGet,
		"http://metadata.google.internal/computeMetadata/v1/project/project-id", nil)
	if err != nil {
		return "", err
	}
	req.Header.Set("Metadata-Flavor", "Google")

	client := &http.Client{Timeout: 2 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(body)), nil
}

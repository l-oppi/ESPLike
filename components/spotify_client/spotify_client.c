#include "spotify_client.h"

#define RESPONSE_BUF_SIZE (1024 * 8)
static const char *TAG = "NewSpotifyClient";
spotify_access_t spotify_access;
static char* response_buf = NULL;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
	static char *output_buffer; // Buffer to store response of http request from event handler
	static int output_len; // Stores number of bytes read
	switch(evt->event_id) {
		case HTTP_EVENT_ERROR:
			ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
			break;
		case HTTP_EVENT_ON_CONNECTED:
			ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
			break;
		case HTTP_EVENT_HEADER_SENT:
			ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
			break;
		case HTTP_EVENT_ON_HEADER:
			ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
			break;
		case HTTP_EVENT_ON_DATA:
			ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
			//ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, content_length=%d", esp_http_client_get_content_length(evt->client));
			ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, output_len=%d", output_len);
			// If user_data buffer is configured, copy the response into the buffer
			if (evt->user_data) {
				memcpy(evt->user_data + output_len, evt->data, evt->data_len);
			} else {
				if (output_buffer == NULL && esp_http_client_get_content_length(evt->client) > 0) {
					output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
					output_len = 0;
					if (output_buffer == NULL) {
						ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
						return ESP_FAIL;
					}
				}
				memcpy(output_buffer + output_len, evt->data, evt->data_len);
			}
			output_len += evt->data_len;
			break;
		case HTTP_EVENT_ON_FINISH:
			ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
			if (output_buffer != NULL) {
				// Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
				// ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
				free(output_buffer);
				output_buffer = NULL;
			}
			output_len = 0;
			break;
		case HTTP_EVENT_DISCONNECTED:
			ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
			int mbedtls_err = 0;
			esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
			if (err != 0) {
				if (output_buffer != NULL) {
					free(output_buffer);
					output_buffer = NULL;
				}
				output_len = 0;
				ESP_LOGE(TAG, "Last esp error code: 0x%x", err);
				ESP_LOGE(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
			}
			break;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
		case HTTP_EVENT_REDIRECT:
			ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
			break;
#endif
	}
	return ESP_OK;
}

void spotify_init()
{
    spotify_access.is_fresh = false;
    memset(spotify_access.client_id, 0, sizeof(spotify_access.client_id));
    memset(spotify_access.client_secret, 0, sizeof(spotify_access.client_secret));
    memset(spotify_access.refresh_token, 0, sizeof(spotify_access.refresh_token));
    memset(spotify_access.access_token, 0, sizeof(spotify_access.access_token));
    response_buf = (char*)malloc(RESPONSE_BUF_SIZE);

    // Context init.

    snprintf(spotify_access.client_id, sizeof(spotify_access.client_id), "%s", CONFIG_SPOTIFY_CLIENT_ID);
    snprintf(spotify_access.client_secret, sizeof(spotify_access.client_secret), "%s", CONFIG_SPOTIFY_CLIENT_SECRET);
    snprintf(spotify_access.refresh_token, sizeof(spotify_access.refresh_token), "%s", CONFIG_SPOTIFY_REFRESH_TOKEN);

    // if(spotify_access.is_fresh==false) {
    //    spotify_access.is_fresh = spotify_refresh_access_token();
    // }
    player_details_t player_details;
    spotify_get_player_state(&player_details);
}

bool spotify_refresh_access_token()
{
    ESP_LOGI(TAG, "http_client_content_length url=%s",SPOTIFY_ACCOUNTS_HOST);
	size_t content_length;
	
	esp_http_client_config_t config = {
		.host = SPOTIFY_ACCOUNTS_HOST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
		.event_handler = _http_event_handler,
        .path = SPOTIFY_TOKEN_ENDPOINT,
		//.user_data = local_response_buffer, // Pass address of local buffer to get response

	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    char post_data[1024];
    snprintf(post_data, 1024, "client_id=%s&client_secret=%s&refresh_token=%s&grant_type=refresh_token",
            spotify_access.client_id,
            spotify_access.client_secret,
            spotify_access.refresh_token);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
	// GET
	esp_err_t err = esp_http_client_perform(client);
	if (err == ESP_OK) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
		ESP_LOGD(TAG, "HTTP GET Status = %d, content_length = %lld",
#else
		ESP_LOGD(TAG, "HTTP GET Status = %d, content_length = %d",
#endif
				esp_http_client_get_status_code(client),
				esp_http_client_get_content_length(client));
		content_length = esp_http_client_get_content_length(client);

	} else {
		ESP_LOGW(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
		content_length = 0;
	}
    memset(response_buf, 0, RESPONSE_BUF_SIZE);
    // ? How do i retrieve http reponse data (esp_http_client_read_response and esp_http_client_read did not work) ?
    int errn = esp_http_client_read_response(client, response_buf, RESPONSE_BUF_SIZE);
    if (errn < 0) {
        ESP_LOGW(TAG, "Could not read HTTP CLIENT.");
    } else {
        ESP_LOGI(TAG, "Response Size: %d", errn);
        cJSON* response_json = NULL;
        response_json = cJSON_Parse(response_buf);
        cJSON* error = cJSON_GetObjectItem(response_json, "error");
        if (error!=NULL) {
            ESP_LOGW(TAG, "Error on request");
            cJSON* error_message = cJSON_GetObjectItem(response_json, "message");
            if (error_message != NULL)
            {
                char* error_message_value = cJSON_GetStringValue(error_message);
                if (strcmp(error_message_value, "The access token expired") == 0)
                {
                    ESP_LOGW(TAG, "The access token expired!");
                }
                else if (strcmp(error_message_value, "Only valid bearer authentication supported") == 0)
                {
                    ESP_LOGW(TAG, "The access token is incorrect!");
                }
            }
        } else {
            cJSON* access_token = cJSON_GetObjectItem(response_json, "access_token");
            if (access_token!=NULL) {
                char* access_token_value = cJSON_GetStringValue(access_token);
                if (access_token_value) {
                    ESP_LOGI(TAG, "Storing a new access token");
                    strncpy(spotify_access.access_token, access_token_value, strlen(access_token_value));
                    spotify_access.is_fresh = true;
                }
                ESP_LOGI(TAG, "Access Token Found");
            } else {
                ESP_LOGW(TAG, "No Access Token Found.");
            }
        }
        if(response_json) cJSON_Delete(response_json);
        memset(response_buf, 0, RESPONSE_BUF_SIZE);
    }
    
	esp_http_client_cleanup(client);
	return content_length;
    return false;
}

bool spotify_get_player_state(player_details_t *player_details)
{
    return false;
}

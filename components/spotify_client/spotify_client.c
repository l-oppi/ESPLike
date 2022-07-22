#include "spotify_client.h"

#define RESPONSE_BUF_SIZE (1024 * 8)
static const char *TAG = "NewSpotifyClient";
spotify_access_t spotify_access;
static char* local_response_buf = NULL;
static char* authorization_header = NULL;
currently_playing_t currently_playing;

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
				// ESP_LOGE(TAG, "Last esp error code: 0x%x", err);
				// ESP_LOGE(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
			}
			break;
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
	
    local_response_buf = (char*)malloc(RESPONSE_BUF_SIZE);
    authorization_header = (char*)malloc(1024);
	memset(local_response_buf, 0, RESPONSE_BUF_SIZE);
	memset(authorization_header, 0, 1024);
    // Context init.

    snprintf(spotify_access.client_id, sizeof(spotify_access.client_id), "%s", CONFIG_SPOTIFY_CLIENT_ID);
    snprintf(spotify_access.client_secret, sizeof(spotify_access.client_secret), "%s", CONFIG_SPOTIFY_CLIENT_SECRET);
    snprintf(spotify_access.refresh_token, sizeof(spotify_access.refresh_token), "%s", CONFIG_SPOTIFY_REFRESH_TOKEN);

    // if(spotify_access.is_fresh==false) {
    //    spotify_access.is_fresh = spotify_refresh_access_token();
    // }
    player_details_t player_details;
    spotify_get_player_details(&player_details);
	// ESP_LOGI(TAG, "Player Device Name: %s", player_details.device.name);
	// ESP_LOGI(TAG, "Player Device ID: %s", player_details.device.id);
}

bool spotify_is_access_token_fresh()
{	
	if(spotify_access.is_fresh)
		return time_seconds() < spotify_access.token_expiration_time;
	return false;
}

bool spotify_refresh_access_token()
{
	size_t content_length;
	esp_http_client_config_t config = {
		.host = SPOTIFY_ACCOUNTS_HOST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
		.event_handler = _http_event_handler,
        .path = SPOTIFY_TOKEN_ENDPOINT,
		.user_data = local_response_buf,
		.disable_auto_redirect = true,
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    char post_data[1024];
    snprintf(post_data, 1024, "client_id=%s&client_secret=%s&refresh_token=%s&grant_type=refresh_token",
            spotify_access.client_id,
            spotify_access.client_secret,
            spotify_access.refresh_token);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
	// GET Token
	esp_err_t err = esp_http_client_perform(client);
	if (err == ESP_OK) {
		ESP_LOGD(TAG, "HTTP GET Status = %d, content_length = %d",
				esp_http_client_get_status_code(client),
				esp_http_client_get_content_length(client));
		content_length = esp_http_client_get_content_length(client);

	} else {
		ESP_LOGW(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
		content_length = 0;
	}
    if (content_length <= 0) {
        ESP_LOGW(TAG, "Could not read HTTP CLIENT.");
    } else {
        ESP_LOGD(TAG, "Response Size: %d", content_length);
        cJSON* response_json = NULL;
        response_json = cJSON_Parse(local_response_buf);
        cJSON* error = cJSON_GetObjectItem(response_json, "error");
        if (error!=NULL) {
            ESP_LOGW(TAG, "Error on request");
			goto cleanup;
            // cJSON* error_message = cJSON_GetObjectItem(response_json, "message");
            // if (error_message != NULL)
            // {
            // }
        } else {
            cJSON* access_token = cJSON_GetObjectItem(response_json, "access_token");
            cJSON* expires_in = cJSON_GetObjectItem(response_json, "expires_in");
            if (access_token!=NULL) {
				char* access_token_value = cJSON_GetObjectItem(response_json, "access_token")->valuestring;
                uint32_t expiration_time = cJSON_GetNumberValue(expires_in);
                if (access_token_value) {
                    ESP_LOGD(TAG, "Storing a new Access Token");
                    strncpy(spotify_access.access_token, access_token_value, strlen(access_token_value));
					spotify_access.token_expiration_time = time_seconds() + expiration_time;
                    spotify_access.is_fresh = true;
                    ESP_LOGD(TAG, "Access Token expires in: %d", spotify_access.token_expiration_time);
                }
            } else {
                ESP_LOGW(TAG, "Access Token Not Found.");
            }
        }
cleanup:
		if(response_json) cJSON_Delete(response_json);
		memset(local_response_buf, 0, RESPONSE_BUF_SIZE);
    }
	esp_http_client_cleanup(client);
    return time_seconds() < spotify_access.token_expiration_time;
}

bool spotify_get_player_details(player_details_t *player_details)
{
	if (!spotify_is_access_token_fresh())
		if(!spotify_refresh_access_token())
			return false;

	snprintf(authorization_header, 1024, "Bearer %s", spotify_access.access_token);

	size_t content_length;
	esp_http_client_config_t config = {
		.host = SPOTIFY_HOST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
		.event_handler = _http_event_handler,
        .path = SPOTIFY_PLAYER_ENDPOINT,
		// .user_data = local_response_buf,
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", authorization_header);
	esp_http_client_set_method(client, HTTP_METHOD_GET);
	esp_err_t err = esp_http_client_open(client, 0);
	int data_read = 0;
	if (err != ESP_OK) { 
		ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err)); 
	} else { 
		content_length = esp_http_client_fetch_headers(client); 
		if (content_length < 0) { 
			ESP_LOGE(TAG, "HTTP client fetch headers failed"); 
		} else { 
			data_read = esp_http_client_read_response(client, local_response_buf, RESPONSE_BUF_SIZE); 
			if (data_read > 0) { 
				ESP_LOGD(TAG, "HTTP GET Status = %d, content_length = %d", 
						esp_http_client_get_status_code(client), 
						esp_http_client_get_content_length(client)); 
				// ESP_LOG_BUFFER_HEX(TAG, local_response_buf, data_read); 
			} else { 
				ESP_LOGE(TAG, "Failed to read response"); 
			}
		}
	}
	esp_http_client_close(client); 

	cJSON* response_json = NULL;
	if (data_read > 0) {
		response_json = cJSON_Parse(local_response_buf);
		cJSON* error = cJSON_GetObjectItem(response_json, "error");
		if (error!=NULL) {
			ESP_LOGW(TAG, "Error on request");
		}
		cJSON* device = cJSON_GetObjectItem(response_json, "device");
		bool response_data = (device != NULL);
		if (!response_data)
			goto cleanup;
		player_details->device.id = cJSON_GetObjectItem(device, "id")->valuestring;
		player_details->device.name = cJSON_GetObjectItem(device, "name")->valuestring;
		player_details->device.type = cJSON_GetObjectItem(device, "type")->valuestring;
		player_details->device.is_active = cJSON_IsTrue(cJSON_GetObjectItem(device, "is_active"));
		player_details->device.is_restricted = cJSON_IsTrue(cJSON_GetObjectItem(device, "is_restricted"));
		player_details->device.is_private_session = cJSON_IsTrue(cJSON_GetObjectItem(device, "is_private_session"));
		player_details->device.volume_percent = cJSON_GetObjectItem(device, "volume_percent")->valueint;
		player_details->progress_ms = cJSON_GetObjectItem(response_json, "progress_ms")->valueint;
		player_details->is_playing = cJSON_IsTrue(cJSON_GetObjectItem(response_json, "is_playing"));
		player_details->shuffle_state = cJSON_IsTrue(cJSON_GetObjectItem(response_json, "shuffle_state"));
		char* repeat_state = cJSON_GetObjectItem(response_json, "repeat_state")->valuestring;
		if (strcmp(repeat_state, "off") == 0) {
			player_details->repeat_state = REPEAT_OFF;
		} else if (strcmp(repeat_state, "context") == 0) {
			player_details->repeat_state = REPEAT_CONTEXT;
		} else {
			player_details->repeat_state = REPEAT_TRACK;
		}
		if(player_details->is_playing) {
			currently_playing.is_playing = player_details->is_playing;
			currently_playing.progress_ms = player_details->progress_ms;
			cJSON* item = cJSON_GetObjectItem(response_json, "item");
			currently_playing.duration_ms = cJSON_GetObjectItem(item, "duration_ms")->valueint;
			currently_playing.track_name = cJSON_GetObjectItem(item, "name")->valuestring;
			currently_playing.track_uri = cJSON_GetObjectItem(item, "uri")->valuestring;
		}
		ESP_LOGI(TAG, "Device ID: %s", player_details->device.id);
		ESP_LOGI(TAG, "Device Name: %s", player_details->device.name);
		ESP_LOGI(TAG, "Device Type: %s", player_details->device.type);
		ESP_LOGI(TAG, "Device is_active: %s", player_details->device.is_active ? "true" : "false");
		ESP_LOGI(TAG, "Device is_private_session: %s", player_details->device.is_private_session ? "true" : "false");
		ESP_LOGI(TAG, "Device volume_percent: %d", player_details->device.volume_percent);
		ESP_LOGI(TAG, "Player is_playing: %s", player_details->is_playing ? "true" : "false");
		ESP_LOGI(TAG, "Player shuffle_state: %s", player_details->shuffle_state ? "true" : "false");
		ESP_LOGI(TAG, "Track Name: %s", currently_playing.track_name);
		ESP_LOGI(TAG, "Track URI: %s", currently_playing.track_uri);
		ESP_LOGI(TAG, "Track Progress: %d", currently_playing.progress_ms);
		ESP_LOGI(TAG, "Track Durarion: %d", currently_playing.duration_ms);
	}
cleanup:
	if(response_json) cJSON_Delete(response_json);
	memset(local_response_buf, 0, RESPONSE_BUF_SIZE);
	memset(authorization_header, 0, 1024);
	esp_http_client_cleanup(client);
    // return response_data;
	return false;
}

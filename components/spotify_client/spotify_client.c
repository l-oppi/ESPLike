#include "spotify_client.h"

#define RESPONSE_BUF_SIZE (1024 * 12)
static const char *TAG = "NewSpotifyClient";
spotify_access_t spotify_access;
static char* local_response_buf = NULL;
static char* authorization_header = NULL;


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
	static char *output_buffer;
	static int output_len;
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

    if(spotify_access.is_fresh==false) {
       spotify_access.is_fresh = spotify_refresh_access_token();
    }
}

bool _check_response_error(cJSON* response_json)
{
	cJSON* error = cJSON_GetObjectItem(response_json, "error");
	if (error!=NULL) {
		ESP_LOGW(TAG, "Error on request");
		char* err_message = cJSON_GetObjectItem(error, "message")->valuestring;
		ESP_LOGW(TAG, "Error Message: %s", err_message);
		if (strcmp(err_message, "The access token expired") == 0) {
			ESP_LOGW(TAG, "The access token expired!");
		} else if (strcmp(err_message, "Only valid bearer authentication supported") == 0) {
			ESP_LOGW(TAG, "The access token is incorrect!");
		}
		return true;
	} else {
		return false;
	}
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
        } else {
            cJSON* access_token = cJSON_GetObjectItem(response_json, "access_token");
            cJSON* expires_in = cJSON_GetObjectItem(response_json, "expires_in");
            if (access_token!=NULL) {
				char* access_token_value = cJSON_GetObjectItem(response_json, "access_token")->valuestring;
                uint32_t expiration_time = cJSON_GetNumberValue(expires_in);
                if (access_token_value) {
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

	int content_length;
	esp_http_client_config_t config = {
		.host = SPOTIFY_HOST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
		.event_handler = _http_event_handler,
        .path = SPOTIFY_PLAYER_ENDPOINT,
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", authorization_header);
	esp_http_client_set_method(client, HTTP_METHOD_GET);
	esp_err_t err = esp_http_client_open(client, 0);
	int req_status = 0;
	int data_read = 0;
	cJSON* response_json = NULL;
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
		req_status = -1;
		goto cleanup;
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
			} else {
				ESP_LOGE(TAG, "Failed to read response");
				req_status = -1;
				goto cleanup;
			}
		}
	}
	esp_http_client_close(client);

	if (data_read > 0) {
		response_json = cJSON_Parse(local_response_buf);
		bool error = _check_response_error(response_json);
		if (error==true)
			goto cleanup;
		cJSON* device = cJSON_GetObjectItem(response_json, "device");
		if (device != NULL)
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
	}
cleanup:
	if(response_json) cJSON_Delete(response_json);
	memset(local_response_buf, 0, RESPONSE_BUF_SIZE);
	memset(authorization_header, 0, 1024);
	esp_http_client_cleanup(client);
    return !(req_status < 0);
}

bool spotify_get_current_playing(currently_playing_t *currently_playing)
{
	if (!spotify_is_access_token_fresh())
		if(!spotify_refresh_access_token())
			return false;

	snprintf(authorization_header, 1024, "Bearer %s", spotify_access.access_token);

	int content_length;
	esp_http_client_config_t config = {
		.host = SPOTIFY_HOST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
		.event_handler = _http_event_handler,
        .path = SPOTIFY_CURRENTLY_PLAYING_ENDPOINT,
		.timeout_ms = 3000,
	};
	
	esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", authorization_header);
	esp_http_client_set_method(client, HTTP_METHOD_GET);
	esp_err_t err = esp_http_client_open(client, 0);
	int req_status = 0;
	int data_read = 0;
	cJSON* response_json = NULL;
	if (err != ESP_OK) { 
		ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
		req_status = -1;
		goto cleanup;
	} else { 
		content_length = esp_http_client_fetch_headers(client); 
		if (content_length < 0) { 
			ESP_LOGE(TAG, "HTTP client fetch headers failed");
			req_status = -1;
			goto cleanup;
		} else { 
			data_read = esp_http_client_read_response(client, local_response_buf, RESPONSE_BUF_SIZE); 
			if (data_read > 0) { 
				ESP_LOGD(TAG, "HTTP GET Status = %d, content_length = %d", 
						esp_http_client_get_status_code(client), 
						esp_http_client_get_content_length(client));

				response_json = cJSON_Parse(local_response_buf);
				bool error = _check_response_error(response_json);
				if (error == true) {
					ESP_LOGD(TAG, "Found Some Error");
					goto cleanup;
				}
				cJSON* timestamp = cJSON_GetObjectItem(response_json, "timestamp");
				if (timestamp == NULL) {
					ESP_LOGW(TAG, "No timestamp found");
					goto cleanup;
				}
				currently_playing->is_playing = timestamp->valueint;
				currently_playing->is_playing = cJSON_IsTrue(cJSON_GetObjectItem(response_json, "is_playing"));
				currently_playing->progress_ms = cJSON_GetObjectItem(response_json, "progress_ms")->valueint;
				cJSON* item = cJSON_GetObjectItem(response_json, "item");
				if (item == NULL) {
					ESP_LOGW(TAG, "No item Found");
					goto cleanup;
				}
				currently_playing->duration_ms = cJSON_GetObjectItem(item, "duration_ms")->valueint;
				currently_playing->track_name = cJSON_GetObjectItem(item, "name")->valuestring;
				currently_playing->track_uri = cJSON_GetObjectItem(item, "uri")->valuestring;

				cJSON* current_element = NULL;
				cJSON* artists = cJSON_GetObjectItem(item, "artists");
				if (artists != NULL) {
					currently_playing->num_artists = cJSON_GetArraySize(artists);
					if (currently_playing->num_artists > SPOTIFY_MAX_NUM_ARTISTS)
						currently_playing->num_artists = SPOTIFY_MAX_NUM_ARTISTS;

					for (int i = 0; i < currently_playing->num_artists; i++) {
						current_element = cJSON_GetArrayItem(artists, i);
						currently_playing->artists[i].artist_name = cJSON_GetObjectItem(current_element, "name")->valuestring;
						currently_playing->artists[i].artist_uri = cJSON_GetObjectItem(current_element, "uri")->valuestring;
					}
				}
				cJSON* album = cJSON_GetObjectItem(item, "album");
				if (album != NULL) {
					current_element = NULL;
					cJSON* images = cJSON_GetObjectItem(album, "images");
					currently_playing->album.num_images = cJSON_GetArraySize(images);
					if (currently_playing->album.num_images > SPOTIFY_NUM_ALBUM_IMAGES)
						currently_playing->album.num_images = SPOTIFY_NUM_ALBUM_IMAGES;

					for (int i = 0; i < currently_playing->album.num_images; i++) {
						current_element = cJSON_GetArrayItem(images, i);
						currently_playing->album.album_images[i].height = cJSON_GetObjectItem(current_element, "height")->valueint;
						currently_playing->album.album_images[i].width = cJSON_GetObjectItem(current_element, "width")->valueint;
						currently_playing->album.album_images[i].url = cJSON_GetObjectItem(current_element, "url")->valuestring;
					}
				}	
			} else { 
				ESP_LOGE(TAG, "Failed to read response");
				req_status = -1;
				goto cleanup;
			}
		}
	}

cleanup:
	esp_http_client_close(client); 
	if(response_json) cJSON_Delete(response_json);
	memset(local_response_buf, 0, RESPONSE_BUF_SIZE);
	memset(authorization_header, 0, 1024);
	esp_http_client_cleanup(client);
	return !(req_status < 0);
}

bool spotify_play(const char *context_uri, int queue_pos, int position_ms, const char *device_id)
{
	if (!spotify_is_access_token_fresh())
		if(!spotify_refresh_access_token())
			return false;

	snprintf(authorization_header, 1024, "Bearer %s", spotify_access.access_token);

	int req_status = 0;
	esp_http_client_config_t config = {
		.host = SPOTIFY_HOST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
		.event_handler = _http_event_handler,
        .path = SPOTIFY_PLAY_ENDPOINT,
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", authorization_header);
	esp_http_client_set_method(client, HTTP_METHOD_PUT);

	cJSON* data = cJSON_CreateObject();

	if (context_uri[0] == '\0' || context_uri == NULL) {
		req_status = -1;
		goto cleanup;
	}
	cJSON_AddStringToObject(data, "context_uri", context_uri);
	if (queue_pos != 0) {
		cJSON* offset = cJSON_CreateObject();
		cJSON_AddNumberToObject(offset, "position", queue_pos);
		cJSON_AddItemToObject(data, "offset", offset);
	}
	cJSON_AddNumberToObject(data, "position_ms", position_ms);
	if (device_id != NULL)
		cJSON_AddStringToObject(data, "device_id", device_id);
	char *post_data = cJSON_Print(data);
	int data_len = strlen(post_data);
	esp_http_client_set_post_field(client, post_data, data_len);
	esp_err_t err = esp_http_client_perform(client);
	if (err != ESP_OK) {
		req_status = -1;
		goto cleanup;
	}
	cJSON_Delete(data);
	free(post_data);

cleanup:
	memset(authorization_header, 0, 1024);
	esp_http_client_cleanup(client);
	return !(req_status < 0);
}

void spotify_pause()
{
	if (!spotify_is_access_token_fresh())
		spotify_refresh_access_token();

	snprintf(authorization_header, 1024, "Bearer %s", spotify_access.access_token);

	esp_http_client_config_t config = {
		.host = SPOTIFY_HOST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
		.event_handler = _http_event_handler,
        .path = SPOTIFY_PAUSE_ENDPOINT,
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", authorization_header);
	esp_http_client_set_method(client, HTTP_METHOD_PUT);

	esp_err_t err = esp_http_client_perform(client);
	if (err == ESP_OK) {
		ESP_LOGD(TAG, "HTTP GET Status = %d, content_length = %d",
				esp_http_client_get_status_code(client),
				esp_http_client_get_content_length(client));
	} else {
		ESP_LOGW(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
	}
	
	memset(authorization_header, 0, 1024);
	esp_http_client_cleanup(client);
}

void spotify_change_volume(int volume_percent, const char *device_id)
{
	if (!spotify_is_access_token_fresh())
		spotify_refresh_access_token();

	snprintf(authorization_header, 1024, "Bearer %s", spotify_access.access_token);
	char endpoint[1024];
	if (device_id == NULL) {
   		snprintf(endpoint, 1024, "%s?volume_percent=%d", SPOTIFY_VOLUME_ENDPOINT, volume_percent);
	} else {
   		snprintf(endpoint, 1024, "%s?volume_percent=%d&device_id=%s", SPOTIFY_VOLUME_ENDPOINT, volume_percent, device_id);
	}

	esp_http_client_config_t config = {
		.host = SPOTIFY_HOST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
		.event_handler = _http_event_handler,
        .path = endpoint,
		// .user_data = local_response_buf,
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", authorization_header);
	esp_http_client_set_method(client, HTTP_METHOD_PUT);
	esp_err_t err = esp_http_client_perform(client);
	if (err == ESP_OK) {
		ESP_LOGD(TAG, "HTTP GET Status = %d, content_length = %d",
				esp_http_client_get_status_code(client),
				esp_http_client_get_content_length(client));
	} else {
		ESP_LOGW(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
	}
	// cJSON *response_json = cJSON_Parse(local_response_buf);
	// char* msg = cJSON_Print(response_json);
	// printf("%s\n", msg);
	// cJSON_Delete(response_json);
	// free(msg);
	memset(authorization_header, 0, 1024);
	esp_http_client_cleanup(client);
}
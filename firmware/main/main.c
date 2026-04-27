#include "app_config.h"
#include "bridge_url.h"
#include "button_controller.h"
#include "retry_policy.h"
#include "state_machine.h"
#include "wav_encoder.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define SERIAL_LINE_MAX 256U
#define URL_BUFFER_SIZE 256U
#define SYNTHETIC_SAMPLE_RATE_HZ 16000U
#define SYNTHETIC_BITS_PER_SAMPLE 16U
#define SYNTHETIC_CHANNEL_COUNT 1U
#define SYNTHETIC_MIN_RECORDING_MS 500U
#define PLAYBACK_MIN_MS 250U
#define PLAYBACK_MAX_MS 5000U

typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t size;
    bool truncated;
} http_response_accumulator_t;

typedef struct {
    app_config_t config;
    SemaphoreHandle_t lock;
    QueueHandle_t playback_queue;
    EventGroupHandle_t wifi_event_group;
    state_machine_state_t state;
    bool wifi_initialized;
    bool wifi_started;
    bool wifi_connected;
    bool health_ok;
    uint32_t wifi_retry_count;
    uint64_t recording_started_ms;
    size_t pending_audio_bytes;
    uint8_t *http_buffer;
} app_context_t;

static const char *TAG = "matrix-atom-echo";
static app_context_t s_app = { 0 };
static esp_event_handler_instance_t s_wifi_any_handler;
static esp_event_handler_instance_t s_wifi_ip_handler;
static bool s_wifi_handlers_registered = false;

static uint64_t now_ms(void)
{
    return (uint64_t) (esp_timer_get_time() / 1000LL);
}

static void copy_config_snapshot(app_config_t *config)
{
    xSemaphoreTake(s_app.lock, portMAX_DELAY);
    *config = s_app.config;
    xSemaphoreGive(s_app.lock);
}

static void set_wifi_connected(bool connected)
{
    xSemaphoreTake(s_app.lock, portMAX_DELAY);
    s_app.wifi_connected = connected;
    xSemaphoreGive(s_app.lock);
}

static void set_health_ok(bool health_ok)
{
    xSemaphoreTake(s_app.lock, portMAX_DELAY);
    s_app.health_ok = health_ok;
    xSemaphoreGive(s_app.lock);
}

static void set_pending_audio_bytes(size_t size_bytes)
{
    xSemaphoreTake(s_app.lock, portMAX_DELAY);
    s_app.pending_audio_bytes = size_bytes;
    xSemaphoreGive(s_app.lock);
}

static size_t get_pending_audio_bytes(void)
{
    size_t size_bytes = 0U;

    xSemaphoreTake(s_app.lock, portMAX_DELAY);
    size_bytes = s_app.pending_audio_bytes;
    xSemaphoreGive(s_app.lock);

    return size_bytes;
}

static void override_state(state_machine_state_t new_state, const char *reason)
{
    state_machine_state_t old_state = STATE_MACHINE_STATE_IDLE;

    xSemaphoreTake(s_app.lock, portMAX_DELAY);
    old_state = s_app.state;
    s_app.state = new_state;
    xSemaphoreGive(s_app.lock);

    ESP_LOGI(
        TAG,
        "state override: %s -> %s (%s)",
        state_machine_state_name(old_state),
        state_machine_state_name(new_state),
        reason);
}

static bool wifi_is_connected(void)
{
    bool connected = false;

    xSemaphoreTake(s_app.lock, portMAX_DELAY);
    connected = s_app.wifi_connected;
    xSemaphoreGive(s_app.lock);

    return connected;
}

static state_machine_state_t current_state(void)
{
    state_machine_state_t state = STATE_MACHINE_STATE_IDLE;

    xSemaphoreTake(s_app.lock, portMAX_DELAY);
    state = s_app.state;
    xSemaphoreGive(s_app.lock);

    return state;
}

static bool configure_uart_console(void)
{
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    if (uart_driver_install(UART_NUM_0, 2048, 0, 0, NULL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed");
        return false;
    }

    if (uart_param_config(UART_NUM_0, &uart_config) != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed");
        return false;
    }

    if (uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed");
        return false;
    }

    return true;
}

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    http_response_accumulator_t *accumulator = (http_response_accumulator_t *) event->user_data;

    if (event->event_id == HTTP_EVENT_ON_DATA &&
        accumulator != NULL &&
        accumulator->buffer != NULL &&
        event->data != NULL &&
        event->data_len > 0) {
        size_t writable = 0U;
        size_t copy_size = (size_t) event->data_len;

        if (accumulator->size < accumulator->capacity) {
            writable = accumulator->capacity - accumulator->size;
        }

        if (copy_size > writable) {
            copy_size = writable;
            accumulator->truncated = true;
        }

        if (copy_size > 0U) {
            memcpy(accumulator->buffer + accumulator->size, event->data, copy_size);
            accumulator->size += copy_size;
        }
    }

    return ESP_OK;
}

static esp_err_t bridge_http_request(
    bridge_route_t route,
    esp_http_client_method_t method,
    const char *content_type,
    const uint8_t *request_body,
    size_t request_body_size,
    int *status_code,
    size_t *response_body_size,
    bool *response_truncated)
{
    app_config_t config;
    char url[URL_BUFFER_SIZE];
    http_response_accumulator_t accumulator = {
        .buffer = s_app.http_buffer,
        .capacity = 0U,
        .size = 0U,
        .truncated = false,
    };
    esp_http_client_handle_t client = NULL;
    esp_http_client_config_t client_config = { 0 };
    esp_err_t error = ESP_OK;

    copy_config_snapshot(&config);

    if (bridge_url_build(url, sizeof(url), &config, route) != BRIDGE_URL_STATUS_OK) {
        ESP_LOGE(TAG, "failed to build bridge URL for route %d", (int) route);
        return ESP_ERR_INVALID_ARG;
    }

    accumulator.capacity = config.max_audio_bytes;

    client_config.url = url;
    client_config.timeout_ms = (int) config.http_timeout_ms;
    client_config.event_handler = http_event_handler;
    client_config.user_data = &accumulator;
    client_config.buffer_size = 1024;
    client_config.buffer_size_tx = 1024;

    client = esp_http_client_init(&client_config);
    if (client == NULL) {
        ESP_LOGE(TAG, "esp_http_client_init failed");
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_method(client, method);

    if (content_type != NULL) {
        esp_http_client_set_header(client, "Content-Type", content_type);
    }

    if (app_config_has_device_auth_token(&config)) {
        char header_value[APP_CONFIG_DEVICE_AUTH_TOKEN_MAX_LEN + 8U];
        int written = snprintf(
            header_value,
            sizeof(header_value),
            "Bearer %s",
            config.device_auth_token);
        if (written < 0 || (size_t) written >= sizeof(header_value)) {
            esp_http_client_cleanup(client);
            return ESP_ERR_INVALID_SIZE;
        }

        esp_http_client_set_header(client, "Authorization", header_value);
    }

    if (request_body != NULL && request_body_size > 0U) {
        esp_http_client_set_post_field(client, (const char *) request_body, (int) request_body_size);
    }

    error = esp_http_client_perform(client);
    if (error == ESP_OK && status_code != NULL) {
        *status_code = esp_http_client_get_status_code(client);
    }

    if (response_body_size != NULL) {
        *response_body_size = accumulator.size;
    }

    if (response_truncated != NULL) {
        *response_truncated = accumulator.truncated;
    }

    esp_http_client_cleanup(client);
    return error;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void) arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        app_config_t config;

        copy_config_snapshot(&config);
        set_wifi_connected(false);

        if (s_app.wifi_retry_count < config.wifi_max_retries) {
            s_app.wifi_retry_count += 1U;
            ESP_LOGW(TAG, "Wi-Fi disconnected, retry %u/%u", (unsigned) s_app.wifi_retry_count, (unsigned) config.wifi_max_retries);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_app.wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Wi-Fi connect failed after retries");
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *) event_data;

        s_app.wifi_retry_count = 0U;
        set_wifi_connected(true);
        xEventGroupSetBits(s_app.wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi connected with IP " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static esp_err_t wifi_initialize(void)
{
    esp_err_t error = ESP_OK;
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

    if (s_app.wifi_initialized) {
        return ESP_OK;
    }

    error = esp_netif_init();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) {
        return error;
    }

    error = esp_event_loop_create_default();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) {
        return error;
    }

    esp_netif_create_default_wifi_sta();

    error = esp_wifi_init(&wifi_init_config);
    if (error != ESP_OK) {
        return error;
    }

    if (!s_wifi_handlers_registered) {
        error = esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            &s_wifi_any_handler);
        if (error != ESP_OK) {
            return error;
        }

        error = esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL,
            &s_wifi_ip_handler);
        if (error != ESP_OK) {
            return error;
        }

        s_wifi_handlers_registered = true;
    }

    s_app.wifi_initialized = true;
    return ESP_OK;
}

static esp_err_t connect_wifi_now(void)
{
    app_config_t config;
    wifi_config_t wifi_config = { 0 };
    EventBits_t bits = 0;
    esp_err_t error = ESP_OK;

    copy_config_snapshot(&config);

    if (!app_config_has_wifi_credentials(&config)) {
        ESP_LOGW(TAG, "Wi-Fi SSID is not configured");
        return ESP_ERR_INVALID_STATE;
    }

    error = wifi_initialize();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "wifi_initialize failed: %s", esp_err_to_name(error));
        return error;
    }

    memset(&wifi_config, 0, sizeof(wifi_config));
    strncpy((char *) wifi_config.sta.ssid, config.wifi_ssid, sizeof(wifi_config.sta.ssid) - 1U);
    strncpy((char *) wifi_config.sta.password, config.wifi_password, sizeof(wifi_config.sta.password) - 1U);
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    xEventGroupClearBits(s_app.wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_app.wifi_retry_count = 0U;

    if (s_app.wifi_started) {
        esp_wifi_disconnect();
        esp_wifi_stop();
        s_app.wifi_started = false;
    }

    error = esp_wifi_set_mode(WIFI_MODE_STA);
    if (error != ESP_OK) {
        return error;
    }

    error = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (error != ESP_OK) {
        return error;
    }

    error = esp_wifi_start();
    if (error != ESP_OK) {
        return error;
    }

    s_app.wifi_started = true;
    bits = xEventGroupWaitBits(
        s_app.wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(config.wifi_connect_timeout_ms));

    if ((bits & WIFI_CONNECTED_BIT) != 0U) {
        return ESP_OK;
    }

    set_wifi_connected(false);
    return ESP_FAIL;
}

static void log_status(void)
{
    app_config_t config;
    bool connected = false;
    bool health_ok = false;
    state_machine_state_t state = STATE_MACHINE_STATE_IDLE;

    copy_config_snapshot(&config);

    xSemaphoreTake(s_app.lock, portMAX_DELAY);
    connected = s_app.wifi_connected;
    health_ok = s_app.health_ok;
    state = s_app.state;
    xSemaphoreGive(s_app.lock);

    ESP_LOGI(
        TAG,
        "status: state=%s wifi=%s health=%s bridge=%s device=%s ssid=%s auth=%s",
        state_machine_state_name(state),
        connected ? "connected" : "disconnected",
        health_ok ? "ok" : "unknown",
        config.bridge_base_url,
        config.device_id,
        config.wifi_ssid[0] != '\0' ? config.wifi_ssid : "<unset>",
        app_config_has_device_auth_token(&config) ? "set" : "unset");
}

static void print_help(void)
{
    ESP_LOGI(TAG, "serial commands:");
    ESP_LOGI(TAG, "  help");
    ESP_LOGI(TAG, "  status");
    ESP_LOGI(TAG, "  idle");
    ESP_LOGI(TAG, "  single | double | long");
    ESP_LOGI(TAG, "  health | poll | replay | thread-reset");
    ESP_LOGI(TAG, "  wifi connect");
    ESP_LOGI(TAG, "  set bridge <url>");
    ESP_LOGI(TAG, "  set device <id>");
    ESP_LOGI(TAG, "  set ssid <name>");
    ESP_LOGI(TAG, "  set pass <password>");
    ESP_LOGI(TAG, "  set token <device-auth-token>");
}

static uint32_t clamp_recording_duration_ms(uint64_t duration_ms, uint32_t max_duration_ms)
{
    uint32_t clamped = (uint32_t) duration_ms;

    if (clamped < SYNTHETIC_MIN_RECORDING_MS) {
        clamped = SYNTHETIC_MIN_RECORDING_MS;
    }

    if (clamped > max_duration_ms) {
        clamped = max_duration_ms;
    }

    return clamped;
}

static void fill_synthetic_pcm(int16_t *samples, size_t sample_count)
{
    size_t index = 0U;

    for (index = 0U; index < sample_count; index += 1U) {
        samples[index] = ((index / 48U) % 2U == 0U) ? 8000 : -8000;
    }
}

static esp_err_t upload_recording_now(uint32_t duration_ms)
{
    app_config_t config;
    size_t sample_count = 0U;
    size_t pcm_size_bytes = 0U;
    size_t total_size_bytes = 0U;
    uint8_t *wav_buffer = NULL;
    int status_code = 0;
    size_t response_body_size = 0U;
    bool response_truncated = false;
    esp_err_t error = ESP_OK;

    if (!wifi_is_connected()) {
        ESP_LOGW(TAG, "recording upload skipped because Wi-Fi is not connected");
        return ESP_ERR_INVALID_STATE;
    }

    copy_config_snapshot(&config);

    sample_count = (size_t) (((uint64_t) SYNTHETIC_SAMPLE_RATE_HZ * duration_ms) / 1000ULL);
    pcm_size_bytes = sample_count * sizeof(int16_t);
    total_size_bytes = wav_encoder_total_size(pcm_size_bytes);

    if (total_size_bytes > config.max_audio_bytes) {
        pcm_size_bytes = config.max_audio_bytes - WAV_ENCODER_HEADER_SIZE;
        sample_count = pcm_size_bytes / sizeof(int16_t);
        total_size_bytes = wav_encoder_total_size(pcm_size_bytes);
    }

    wav_buffer = (uint8_t *) malloc(total_size_bytes);
    if (wav_buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    fill_synthetic_pcm((int16_t *) (wav_buffer + WAV_ENCODER_HEADER_SIZE), sample_count);
    wav_encoder_write_header(
        wav_buffer,
        pcm_size_bytes,
        SYNTHETIC_SAMPLE_RATE_HZ,
        SYNTHETIC_CHANNEL_COUNT,
        SYNTHETIC_BITS_PER_SAMPLE);

    error = bridge_http_request(
        BRIDGE_ROUTE_RECORDING,
        HTTP_METHOD_POST,
        "audio/wav",
        wav_buffer,
        total_size_bytes,
        &status_code,
        &response_body_size,
        &response_truncated);

    free(wav_buffer);

    if (error != ESP_OK) {
        ESP_LOGE(TAG, "recording upload failed: %s", esp_err_to_name(error));
        return error;
    }

    if (response_truncated) {
        ESP_LOGW(TAG, "recording response was truncated");
    }

    if (status_code != 202) {
        ESP_LOGE(TAG, "recording upload returned status %d", status_code);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "recording upload accepted (%u ms, %u bytes)", (unsigned) duration_ms, (unsigned) total_size_bytes);
    return ESP_OK;
}

static esp_err_t post_simple_route(bridge_route_t route, const char *label)
{
    int status_code = 0;
    size_t response_body_size = 0U;
    bool response_truncated = false;
    esp_err_t error = ESP_OK;

    if (!wifi_is_connected()) {
        ESP_LOGW(TAG, "%s skipped because Wi-Fi is not connected", label);
        return ESP_ERR_INVALID_STATE;
    }

    error = bridge_http_request(
        route,
        HTTP_METHOD_POST,
        "application/json",
        NULL,
        0U,
        &status_code,
        &response_body_size,
        &response_truncated);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "%s failed: %s", label, esp_err_to_name(error));
        return error;
    }

    if (response_truncated) {
        ESP_LOGW(TAG, "%s response was truncated", label);
    }

    ESP_LOGI(TAG, "%s returned status %d", label, status_code);
    return status_code == 202 ? ESP_OK : ESP_FAIL;
}

static esp_err_t perform_health_check_now(void)
{
    int status_code = 0;
    size_t response_body_size = 0U;
    bool response_truncated = false;
    esp_err_t error = ESP_OK;

    if (!wifi_is_connected()) {
        set_health_ok(false);
        return ESP_ERR_INVALID_STATE;
    }

    error = bridge_http_request(
        BRIDGE_ROUTE_HEALTH,
        HTTP_METHOD_GET,
        NULL,
        NULL,
        0U,
        &status_code,
        &response_body_size,
        &response_truncated);
    if (error != ESP_OK) {
        set_health_ok(false);
        ESP_LOGW(TAG, "health check failed: %s", esp_err_to_name(error));
        return error;
    }

    set_health_ok(status_code == 200);

    if (response_truncated) {
        ESP_LOGW(TAG, "health response was truncated");
    }

    ESP_LOGI(TAG, "bridge health status=%d body=%u bytes", status_code, (unsigned) response_body_size);
    return status_code == 200 ? ESP_OK : ESP_FAIL;
}

static void handle_state_machine_event(state_machine_event_t event, const char *source);

static esp_err_t poll_next_audio_now(void)
{
    int status_code = 0;
    size_t response_body_size = 0U;
    bool response_truncated = false;
    esp_err_t error = ESP_OK;

    if (!wifi_is_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    error = bridge_http_request(
        BRIDGE_ROUTE_NEXT_AUDIO,
        HTTP_METHOD_GET,
        NULL,
        NULL,
        0U,
        &status_code,
        &response_body_size,
        &response_truncated);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "next-audio poll failed: %s", esp_err_to_name(error));
        return error;
    }

    if (status_code == 204) {
        if (current_state() == STATE_MACHINE_STATE_WAITING_FOR_REPLY) {
            ESP_LOGI(TAG, "next-audio returned 204");
        }
        return ESP_OK;
    }

    if (status_code != 200) {
        ESP_LOGW(TAG, "next-audio returned status %d", status_code);
        return ESP_FAIL;
    }

    if (response_body_size == 0U) {
        ESP_LOGW(TAG, "next-audio returned an empty body");
        return ESP_FAIL;
    }

    if (response_truncated) {
        ESP_LOGW(TAG, "next-audio response truncated to %u bytes", (unsigned) response_body_size);
    }

    set_pending_audio_bytes(response_body_size);
    ESP_LOGI(TAG, "received audio payload: %u bytes", (unsigned) response_body_size);
    handle_state_machine_event(STATE_MACHINE_EVENT_AUDIO_AVAILABLE, "bridge");
    return ESP_OK;
}

static void handle_action(state_machine_action_t action)
{
    uint64_t recording_started_ms = 0U;
    size_t playback_bytes = 0U;

    switch (action) {
    case STATE_MACHINE_ACTION_NONE:
        return;

    case STATE_MACHINE_ACTION_START_RECORDING:
        xSemaphoreTake(s_app.lock, portMAX_DELAY);
        s_app.recording_started_ms = now_ms();
        xSemaphoreGive(s_app.lock);
        ESP_LOGI(TAG, "recording started");
        return;

    case STATE_MACHINE_ACTION_STOP_RECORDING:
        xSemaphoreTake(s_app.lock, portMAX_DELAY);
        recording_started_ms = s_app.recording_started_ms;
        s_app.recording_started_ms = 0U;
        xSemaphoreGive(s_app.lock);

        if (upload_recording_now(
                clamp_recording_duration_ms(
                    now_ms() - recording_started_ms,
                    s_app.config.max_recording_ms)) != ESP_OK) {
            override_state(STATE_MACHINE_STATE_IDLE, "recording upload failed");
        }
        return;

    case STATE_MACHINE_ACTION_CANCEL_RECORDING:
        xSemaphoreTake(s_app.lock, portMAX_DELAY);
        s_app.recording_started_ms = 0U;
        xSemaphoreGive(s_app.lock);
        ESP_LOGI(TAG, "recording cancelled");
        return;

    case STATE_MACHINE_ACTION_START_NEW_THREAD:
        if (post_simple_route(BRIDGE_ROUTE_THREAD_RESET, "thread reset") != ESP_OK) {
            ESP_LOGW(TAG, "thread reset request failed");
        }
        return;

    case STATE_MACHINE_ACTION_REPLAY_LAST_AUDIO:
        if (post_simple_route(BRIDGE_ROUTE_REPLAY_LAST_AUDIO, "replay request") == ESP_OK) {
            override_state(STATE_MACHINE_STATE_WAITING_FOR_REPLY, "waiting for replay audio");
        }
        return;

    case STATE_MACHINE_ACTION_START_PLAYBACK:
        playback_bytes = get_pending_audio_bytes();
        xQueueReset(s_app.playback_queue);
        xQueueSend(s_app.playback_queue, &playback_bytes, 0);
        return;

    default:
        return;
    }
}

static void handle_state_machine_event(state_machine_event_t event, const char *source)
{
    state_machine_result_t result;
    state_machine_state_t old_state = STATE_MACHINE_STATE_IDLE;

    xSemaphoreTake(s_app.lock, portMAX_DELAY);
    old_state = s_app.state;
    result = state_machine_step(s_app.state, event);
    s_app.state = result.next_state;
    xSemaphoreGive(s_app.lock);

    ESP_LOGI(
        TAG,
        "%s event=%s transition=%s->%s action=%s",
        source,
        state_machine_event_name(event),
        state_machine_state_name(old_state),
        state_machine_state_name(result.next_state),
        state_machine_action_name(result.action));

    handle_action(result.action);
}

static void handle_button_event(button_event_t event, const char *source)
{
    if (event == BUTTON_EVENT_NONE) {
        return;
    }

    ESP_LOGI(TAG, "%s button=%s", source, button_event_name(event));

    switch (event) {
    case BUTTON_EVENT_SINGLE_PRESS:
        handle_state_machine_event(STATE_MACHINE_EVENT_SINGLE_PRESS, source);
        break;
    case BUTTON_EVENT_DOUBLE_PRESS:
        handle_state_machine_event(STATE_MACHINE_EVENT_DOUBLE_PRESS, source);
        break;
    case BUTTON_EVENT_LONG_PRESS:
        handle_state_machine_event(STATE_MACHINE_EVENT_LONG_PRESS, source);
        break;
    default:
        break;
    }
}

static const char *trim_leading_spaces(const char *value)
{
    while (*value != '\0' && isspace((unsigned char) *value)) {
        value += 1;
    }

    return value;
}

static void apply_string_setting(
    const char *label,
    app_config_status_t (*setter)(app_config_t *, const char *),
    const char *value)
{
    app_config_status_t status = APP_CONFIG_STATUS_INVALID_ARGUMENT;

    xSemaphoreTake(s_app.lock, portMAX_DELAY);
    status = setter(&s_app.config, value);
    xSemaphoreGive(s_app.lock);

    ESP_LOGI(TAG, "%s %s", label, status == APP_CONFIG_STATUS_OK ? "updated" : "invalid");
}

static void handle_serial_command(const char *command_line)
{
    const char *value = NULL;

    if (strcmp(command_line, "help") == 0) {
        print_help();
        return;
    }

    if (strcmp(command_line, "status") == 0) {
        log_status();
        return;
    }

    if (strcmp(command_line, "idle") == 0) {
        xSemaphoreTake(s_app.lock, portMAX_DELAY);
        s_app.recording_started_ms = 0U;
        s_app.pending_audio_bytes = 0U;
        xSemaphoreGive(s_app.lock);
        override_state(STATE_MACHINE_STATE_IDLE, "serial idle");
        return;
    }

    if (strcmp(command_line, "single") == 0) {
        handle_button_event(BUTTON_EVENT_SINGLE_PRESS, "serial");
        return;
    }

    if (strcmp(command_line, "double") == 0) {
        handle_button_event(BUTTON_EVENT_DOUBLE_PRESS, "serial");
        return;
    }

    if (strcmp(command_line, "long") == 0) {
        handle_button_event(BUTTON_EVENT_LONG_PRESS, "serial");
        return;
    }

    if (strcmp(command_line, "health") == 0) {
        perform_health_check_now();
        return;
    }

    if (strcmp(command_line, "poll") == 0) {
        poll_next_audio_now();
        return;
    }

    if (strcmp(command_line, "replay") == 0) {
        post_simple_route(BRIDGE_ROUTE_REPLAY_LAST_AUDIO, "replay request");
        return;
    }

    if (strcmp(command_line, "thread-reset") == 0) {
        post_simple_route(BRIDGE_ROUTE_THREAD_RESET, "thread reset");
        return;
    }

    if (strcmp(command_line, "wifi connect") == 0) {
        const esp_err_t error = connect_wifi_now();
        ESP_LOGI(TAG, "wifi connect -> %s", esp_err_to_name(error));
        return;
    }

    if (strncmp(command_line, "set bridge", 10U) == 0) {
        value = trim_leading_spaces(command_line + 10U);
        apply_string_setting("bridge URL", app_config_set_bridge_base_url, value);
        return;
    }

    if (strncmp(command_line, "set device", 10U) == 0) {
        value = trim_leading_spaces(command_line + 10U);
        apply_string_setting("device ID", app_config_set_device_id, value);
        return;
    }

    if (strncmp(command_line, "set ssid", 8U) == 0) {
        value = trim_leading_spaces(command_line + 8U);
        apply_string_setting("Wi-Fi SSID", app_config_set_wifi_ssid, value);
        return;
    }

    if (strncmp(command_line, "set pass", 8U) == 0) {
        value = trim_leading_spaces(command_line + 8U);
        apply_string_setting("Wi-Fi password", app_config_set_wifi_password, value);
        return;
    }

    if (strncmp(command_line, "set token", 9U) == 0) {
        value = trim_leading_spaces(command_line + 9U);
        apply_string_setting("device auth token", app_config_set_device_auth_token, value);
        return;
    }

    ESP_LOGW(TAG, "unknown command: %s", command_line);
    print_help();
}

static void serial_console_task(void *arg)
{
    char line_buffer[SERIAL_LINE_MAX];
    size_t line_length = 0U;

    (void) arg;
    memset(line_buffer, 0, sizeof(line_buffer));

    while (true) {
        uint8_t character = 0U;
        const int bytes_read = uart_read_bytes(UART_NUM_0, &character, 1U, pdMS_TO_TICKS(100));

        if (bytes_read <= 0) {
            continue;
        }

        if (character == '\r' || character == '\n') {
            if (line_length > 0U) {
                line_buffer[line_length] = '\0';
                handle_serial_command(line_buffer);
                line_length = 0U;
                memset(line_buffer, 0, sizeof(line_buffer));
            }
            continue;
        }

        if (line_length + 1U >= sizeof(line_buffer)) {
            ESP_LOGW(TAG, "serial command too long");
            line_length = 0U;
            memset(line_buffer, 0, sizeof(line_buffer));
            continue;
        }

        line_buffer[line_length] = (char) character;
        line_length += 1U;
    }
}

static void button_task(void *arg)
{
    app_config_t config;
    button_controller_t controller;
    bool previous_pressed = false;
    uint64_t pressed_since_ms = 0U;

    (void) arg;
    copy_config_snapshot(&config);

    gpio_config_t gpio_settings = {
        .pin_bit_mask = 1ULL << (uint32_t) config.button_gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    if (gpio_config(&gpio_settings) != ESP_OK) {
        ESP_LOGE(TAG, "button gpio configuration failed");
        vTaskDelete(NULL);
        return;
    }

    button_controller_init(
        &controller,
        config.long_press_threshold_ms,
        config.double_press_gap_ms);

    while (true) {
        const bool pressed = gpio_get_level((gpio_num_t) config.button_gpio) == 0;
        const uint64_t current_ms = now_ms();

        if (pressed && !previous_pressed) {
            pressed_since_ms = current_ms;
        } else if (!pressed && previous_pressed) {
            const button_event_t event = button_controller_on_release(
                &controller,
                current_ms,
                (uint32_t) (current_ms - pressed_since_ms));
            handle_button_event(event, "gpio");
        } else if (!pressed) {
            const button_event_t event = button_controller_flush(&controller, current_ms);
            handle_button_event(event, "gpio");
        }

        previous_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(config.button_poll_ms));
    }
}

static void playback_task(void *arg)
{
    size_t playback_bytes = 0U;

    (void) arg;

    while (true) {
        if (xQueueReceive(s_app.playback_queue, &playback_bytes, portMAX_DELAY) == pdTRUE) {
            uint32_t playback_ms = (uint32_t) (playback_bytes / 32U);

            if (playback_ms < PLAYBACK_MIN_MS) {
                playback_ms = PLAYBACK_MIN_MS;
            }
            if (playback_ms > PLAYBACK_MAX_MS) {
                playback_ms = PLAYBACK_MAX_MS;
            }

            ESP_LOGI(TAG, "playback start (%u bytes, %u ms)", (unsigned) playback_bytes, (unsigned) playback_ms);
            vTaskDelay(pdMS_TO_TICKS(playback_ms));
            ESP_LOGI(TAG, "playback done");
            handle_state_machine_event(STATE_MACHINE_EVENT_PLAYBACK_DONE, "playback");
        }
    }
}

static void health_task(void *arg)
{
    app_config_t config;

    (void) arg;

    while (true) {
        copy_config_snapshot(&config);
        if (wifi_is_connected()) {
            perform_health_check_now();
        }
        vTaskDelay(pdMS_TO_TICKS(config.health_interval_ms));
    }
}

static void poll_task(void *arg)
{
    app_config_t config;

    (void) arg;

    while (true) {
        copy_config_snapshot(&config);
        if (wifi_is_connected()) {
            poll_next_audio_now();
        }
        vTaskDelay(pdMS_TO_TICKS(config.poll_interval_ms));
    }
}

void app_main(void)
{
    esp_err_t error = ESP_OK;
    app_config_status_t config_status = APP_CONFIG_STATUS_OK;

    s_app.lock = xSemaphoreCreateMutex();
    s_app.playback_queue = xQueueCreate(1, sizeof(size_t));
    s_app.wifi_event_group = xEventGroupCreate();

    if (s_app.lock == NULL || s_app.playback_queue == NULL || s_app.wifi_event_group == NULL) {
        ESP_LOGE(TAG, "failed to allocate RTOS primitives");
        return;
    }

    config_status = app_config_load_defaults(&s_app.config);
    if (config_status != APP_CONFIG_STATUS_OK) {
        ESP_LOGE(TAG, "app_config_load_defaults failed");
        return;
    }

    s_app.http_buffer = (uint8_t *) malloc(s_app.config.max_audio_bytes);
    if (s_app.http_buffer == NULL) {
        ESP_LOGE(TAG, "failed to allocate HTTP buffer");
        return;
    }

    error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        error = nvs_flash_init();
    }
    ESP_ERROR_CHECK(error);

    s_app.state = STATE_MACHINE_STATE_IDLE;

    configure_uart_console();

    xTaskCreate(serial_console_task, "serial_console", 4096, NULL, 5, NULL);
    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
    xTaskCreate(playback_task, "playback_task", 4096, NULL, 5, NULL);
    xTaskCreate(health_task, "health_task", 4096, NULL, 4, NULL);
    xTaskCreate(poll_task, "poll_task", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "Firmware app ready");
    print_help();
    log_status();

    if (app_config_has_wifi_credentials(&s_app.config)) {
        const esp_err_t connect_error = connect_wifi_now();
        ESP_LOGI(TAG, "initial Wi-Fi connect -> %s", esp_err_to_name(connect_error));
    } else {
        ESP_LOGI(TAG, "Wi-Fi credentials are unset; use serial commands to configure them");
    }
}

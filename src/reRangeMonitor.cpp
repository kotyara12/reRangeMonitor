#include "reRangeMonitor.h"
#include "reNvs.h"
#include "reEsp32.h"
#include "rStrings.h"
#include <string.h>

reRangeMonitor::reRangeMonitor(float value_min, float value_max, float hysteresis, const char* nvs_space, cb_monitor_outofrange_t cb_status, cb_monitor_publish_t cb_publish)
{
  _notify      = true;
  _last_low    = 0;
  _last_high   = 0;
  _last_normal = 0;
  _last_value  = NAN;
  _value_min   = value_min; 
  _value_max   = value_max; 
  _hysteresis  = hysteresis;
  _status = TMS_EMPTY;
  _mqtt_topic  = nullptr;
  _nvs_space   = nvs_space;
  _out_of_range = cb_status;
  _mqtt_publish = cb_publish; 
}

reRangeMonitor::~reRangeMonitor()
{
  if (_mqtt_topic) free(_mqtt_topic);
  _mqtt_topic = nullptr;
}

// Monitoring value
range_monitor_status_t reRangeMonitor::checkValue(float value)
{
  if (value != NAN) {
    _last_value = value;
    if ((_status == TMS_EMPTY) || (_status == TMS_NORMAL)) {
      if (value < _value_min) {
        _status = TMS_TOO_LOW;
        _last_low = time(nullptr);
        if (_nvs_space) { nvsStore(_nvs_space); };
        mqttPublish(true);
        if (_out_of_range) {
          _out_of_range(this, _status, _notify, value, _value_min, _value_max);
        };
      } else if (value > _value_max) {
        _status = TMS_TOO_HIGH;
        _last_high = time(nullptr);
        if (_nvs_space) { nvsStore(_nvs_space); };
        mqttPublish(true);
        if (_out_of_range) {
          _out_of_range(this, _status, _notify, value, _value_min, _value_max);
        };
      } else if (_status == TMS_EMPTY) {
        _status = TMS_NORMAL;
        _last_normal = time(nullptr);
        if (_nvs_space) { nvsStore(_nvs_space); };
        mqttPublish(true);
      };
    } else {
      if ((value >= (_value_min + _hysteresis)) && (value <= (_value_max - _hysteresis))) {
        _status = TMS_NORMAL;
        _last_normal = time(nullptr);
        if (_nvs_space) { nvsStore(_nvs_space); };
        mqttPublish(true);
        if (_out_of_range) {
          _out_of_range(this, _status, _notify, value, _value_min, _value_max);
        };
      };
    };
  };
  return _status;
}

// Get current data
range_monitor_status_t reRangeMonitor::getStatus()
{
  return _status;
}

void reRangeMonitor::setStatusCallback(cb_monitor_outofrange_t cb_status)
{
  _out_of_range = cb_status;
}

float reRangeMonitor::getRangeMin()
{
  return _value_min;
}

float reRangeMonitor::getRangeMax()
{
  return _value_max;
}

float reRangeMonitor::getHysteresis()
{
  return _hysteresis;
}

// Parameters
void reRangeMonitor::paramsRegister(paramsGroupHandle_t root_group, const char* group_key, const char* group_topic, const char* group_friendly)
{
  paramsGroupHandle_t pgParams = paramsRegisterGroup(root_group, group_key, group_topic, group_friendly);

  paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U8, nullptr, pgParams,
    CONFIG_RANGE_MONITOR_NOTIFY_KEY, CONFIG_RANGE_MONITOR_NOTIFY_FRIENDLY,
    CONFIG_MQTT_PARAMS_QOS, (void*)&_notify);
  paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_FLOAT, nullptr, pgParams,
    CONFIG_RANGE_MONITOR_MIN_KEY, CONFIG_RANGE_MONITOR_MIN_FRIENDLY,
    CONFIG_MQTT_PARAMS_QOS, (void*)&_value_min);
  paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_FLOAT, nullptr, pgParams,
    CONFIG_RANGE_MONITOR_MAX_KEY, CONFIG_RANGE_MONITOR_MAX_FRIENDLY,
    CONFIG_MQTT_PARAMS_QOS, (void*)&_value_max);
  paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_FLOAT, nullptr, pgParams,
    CONFIG_RANGE_MONITOR_HYST_KEY, CONFIG_RANGE_MONITOR_HYST_FRIENDLY,
    CONFIG_MQTT_PARAMS_QOS, (void*)&_hysteresis);
}

// JSON
char* reRangeMonitor::getJSON()
{
  char str_low[CONFIG_FORMAT_STRFTIME_BUFFER_SIZE];
  memset(&str_low, 0, sizeof(str_low));
  if (_last_low > 0) {
    struct tm tm_low;
    localtime_r(&_last_low, &tm_low);
    strftime(str_low, sizeof(str_low), CONFIG_FORMAT_DTM, &tm_low);
  } else {
    strcpy(str_low, CONFIG_RANGE_MONITOR_TIME_EMPTY);
  };

  char str_high[CONFIG_FORMAT_STRFTIME_BUFFER_SIZE];
  memset(&str_high, 0, sizeof(str_high));
  if (_last_high > 0) {
    struct tm tm_high;
    localtime_r(&_last_high, &tm_high);
    strftime(str_high, sizeof(str_high), CONFIG_FORMAT_DTM, &tm_high);
  } else {
    strcpy(str_high, CONFIG_RANGE_MONITOR_TIME_EMPTY);
  };

  char str_norm[CONFIG_FORMAT_STRFTIME_BUFFER_SIZE];
  memset(&str_norm, 0, sizeof(str_norm));
  if (_last_normal > 0) {
    struct tm tm_norm;
    localtime_r(&_last_normal, &tm_norm);
    strftime(str_norm, sizeof(str_norm), CONFIG_FORMAT_DTM, &tm_norm);
  } else {
    strcpy(str_norm, CONFIG_RANGE_MONITOR_TIME_EMPTY);
  };

  return malloc_stringf("{\"status\":%d,\"value\":%f,\"last_normal\":\"%s\",\"last_min\":\"%s\",\"last_max\":\"%s\"}", 
    _status, _last_value, str_norm, str_low, str_high);
}

// MQTT
void reRangeMonitor::mqttSetCallback(cb_monitor_publish_t cb_publish)
{
  _mqtt_publish = cb_publish; 
}

char* reRangeMonitor::mqttTopicGet()
{
  return _mqtt_topic;
}

bool reRangeMonitor::mqttTopicSet(char* topic)
{
  if (_mqtt_topic) free(_mqtt_topic);
  _mqtt_topic = topic;
  return (_mqtt_topic != nullptr);
}

bool reRangeMonitor::mqttTopicCreate(bool primary, bool local, const char* topic1, const char* topic2, const char* topic3)
{
  return mqttTopicSet(mqttGetTopicDevice(primary, local, topic1, topic2, topic3));
}

void reRangeMonitor::mqttTopicFree()
{
  if (_mqtt_topic) free(_mqtt_topic);
  _mqtt_topic = nullptr;
}

bool reRangeMonitor::mqttPublish(bool forced)
{
  if ((_mqtt_topic) && (_mqtt_publish)) {
    return _mqtt_publish(this, _mqtt_topic, getJSON(), forced, false, true);
  };
  return false;
}

// NVS
void reRangeMonitor::nvsStore(const char* nvs_space)
{
  nvs_handle_t nvs_handle;
  if (nvsOpen(nvs_space, NVS_READWRITE, &nvs_handle)) {
    nvs_set_u8(nvs_handle, CONFIG_RANGE_MONITOR_STATUS, (uint8_t)_status);
    nvs_set_i64(nvs_handle, CONFIG_RANGE_MONITOR_LAST_NORMAL, (int64_t)_last_normal);
    nvs_set_i64(nvs_handle, CONFIG_RANGE_MONITOR_LAST_LOW, (int64_t)_last_low);
    nvs_set_i64(nvs_handle, CONFIG_RANGE_MONITOR_LAST_HIGH, (int64_t)_last_high);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
  };
}

void reRangeMonitor::nvsRestore(const char* nvs_space)
{
  nvs_handle_t nvs_handle;
  if (nvsOpen(nvs_space, NVS_READONLY, &nvs_handle)) {
    nvs_get_u8(nvs_handle, CONFIG_RANGE_MONITOR_STATUS, (uint8_t*)&_status);
    nvs_get_i64(nvs_handle, CONFIG_RANGE_MONITOR_LAST_NORMAL, (int64_t*)&_last_normal);
    nvs_get_i64(nvs_handle, CONFIG_RANGE_MONITOR_LAST_LOW, (int64_t*)&_last_low);
    nvs_get_i64(nvs_handle, CONFIG_RANGE_MONITOR_LAST_HIGH, (int64_t*)&_last_high);
    nvs_close(nvs_handle);
  };
}

void reRangeMonitor::nvsRestore()
{
  if (_nvs_space) { 
    nvsRestore(_nvs_space); 
  };
}

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
  _status      = TMS_EMPTY;
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
        mqttPublish();
        if (_out_of_range) {
          _out_of_range(this, _status, _notify, value, _value_min, _value_max);
        };
      } else if (value > _value_max) {
        _status = TMS_TOO_HIGH;
        _last_high = time(nullptr);
        if (_nvs_space) { nvsStore(_nvs_space); };
        mqttPublish();
        if (_out_of_range) {
          _out_of_range(this, _status, _notify, value, _value_min, _value_max);
        };
      } else if (_status == TMS_EMPTY) {
        _status = TMS_NORMAL;
        _last_normal = time(nullptr);
        if (_nvs_space) { nvsStore(_nvs_space); };
        mqttPublish();
      };
    } else {
      if ((value >= (_value_min + _hysteresis)) && (value <= (_value_max - _hysteresis))) {
        _status = TMS_NORMAL;
        _last_normal = time(nullptr);
        if (_nvs_space) { nvsStore(_nvs_space); };
        mqttPublish();
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
  char str_low[CONFIG_RANGE_MONITOR_TIMESTAMP_BUF_SIZE];
  char str_high[CONFIG_RANGE_MONITOR_TIMESTAMP_BUF_SIZE];
  char str_normal[CONFIG_RANGE_MONITOR_TIMESTAMP_BUF_SIZE];

  time2str_empty(CONFIG_RANGE_MONITOR_TIMESTAMP_FORMAT, &_last_low, &str_low[0], sizeof(str_low));
  time2str_empty(CONFIG_RANGE_MONITOR_TIMESTAMP_FORMAT, &_last_high, &str_high[0], sizeof(str_high));
  time2str_empty(CONFIG_RANGE_MONITOR_TIMESTAMP_FORMAT, &_last_normal, &str_normal[0], sizeof(str_normal));

  return malloc_stringf("{\"status\":%d,\"value\":%f,\"last_normal\":\"%s\",\"last_min\":\"%s\",\"last_max\":\"%s\"}", 
    _status, _last_value, str_normal, str_low, str_high);
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

bool reRangeMonitor::mqttPublish()
{
  if ((_mqtt_topic) && (_mqtt_publish)) {
    return _mqtt_publish(this, _mqtt_topic, getJSON(), false, true);
  };
  return false;
}

// NVS
void reRangeMonitor::nvsStore(const char* nvs_space)
{
  if ((nvs_space != nullptr) && (_status != TMS_EMPTY)) {
    nvs_handle_t nvs_handle;
    if (nvsOpen(nvs_space, NVS_READWRITE, &nvs_handle)) {
      nvs_set_i8(nvs_handle, CONFIG_RANGE_MONITOR_STATUS, (int8_t)_status);
      nvs_set_time(nvs_handle, CONFIG_RANGE_MONITOR_LAST_NORMAL, _last_normal);
      nvs_set_time(nvs_handle, CONFIG_RANGE_MONITOR_LAST_LOW, _last_low);
      nvs_set_time(nvs_handle, CONFIG_RANGE_MONITOR_LAST_HIGH, _last_high);
      nvs_commit(nvs_handle);
      nvs_close(nvs_handle);
    };
  };
}

void reRangeMonitor::nvsRestore(const char* nvs_space)
{
  if (nvs_space != nullptr) {
    nvs_handle_t nvs_handle;
    if (nvsOpen(nvs_space, NVS_READONLY, &nvs_handle)) {
      int8_t buf = (int8_t)_status;
      nvs_get_i8(nvs_handle, CONFIG_RANGE_MONITOR_STATUS, &buf);
      if ((buf >= TMS_TOO_LOW) && (buf <= TMS_TOO_HIGH)) {
        _status = (range_monitor_status_t)buf;
        nvs_get_time(nvs_handle, CONFIG_RANGE_MONITOR_LAST_NORMAL, &_last_normal);
        nvs_get_time(nvs_handle, CONFIG_RANGE_MONITOR_LAST_LOW, &_last_low);
        nvs_get_time(nvs_handle, CONFIG_RANGE_MONITOR_LAST_HIGH, &_last_high);
      };
      nvs_close(nvs_handle);
    };
  };
}

void reRangeMonitor::nvsRestore()
{
  nvsRestore(_nvs_space); 
}

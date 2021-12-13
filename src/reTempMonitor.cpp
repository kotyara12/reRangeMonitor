#include "reTempMonitor.h"
#include "rStrings.h"

reTempMonitor::reTempMonitor(float value_min, float value_max, float hysteresis, cb_monitor_outofrange_t cb_status, cb_monitor_publish_t cb_publish)
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
  _out_of_range = cb_status;
  _mqtt_publish = cb_publish; 
}

reTempMonitor::~reTempMonitor()
{
  if (_mqtt_topic) free(_mqtt_topic);
  _mqtt_topic = nullptr;
}

// Monitoring value
temp_monitor_status_t reTempMonitor::checkValue(float value)
{
  if (value != NAN) {
    _last_value = value;
    if ((_status == TMS_EMPTY) || (_status == TMS_NORMAL)) {
      if (value < _value_min) {
        _status = TMS_TOO_LOW;
        _last_low = time(nullptr);
        mqttPublish(true);
        if (_out_of_range) {
          _out_of_range(this, _status, _notify, value, _value_min, _value_max);
        };
      } else if (value > _value_max) {
        _status = TMS_TOO_HIGH;
        _last_high = time(nullptr);
        mqttPublish(true);
        if (_out_of_range) {
          _out_of_range(this, _status, _notify, value, _value_min, _value_max);
        };
      } else if (_status == TMS_EMPTY) {
        _status = TMS_NORMAL;
        _last_normal = time(nullptr);
        mqttPublish(true);
      };
    } else {
      if ((value >= (_value_min + _hysteresis)) && (value <= (_value_max - _hysteresis))) {
        _status = TMS_NORMAL;
        _last_normal = time(nullptr);
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
temp_monitor_status_t reTempMonitor::getStatus()
{
  return _status;
}

void reTempMonitor::setStatusCallback(cb_monitor_outofrange_t cb_status)
{
  _out_of_range = cb_status;
}

float reTempMonitor::getRangeMin()
{
  return _value_min;
}

float reTempMonitor::getRangeMax()
{
  return _value_max;
}

float reTempMonitor::getHysteresis()
{
  return _hysteresis;
}

// Parameters
void reTempMonitor::paramsRegister(paramsGroupHandle_t root_group, const char* group_key, const char* group_topic, const char* group_friendly)
{
  paramsGroupHandle_t pgParams = paramsRegisterGroup(root_group, group_key, group_topic, group_friendly);

  paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U8, nullptr, pgParams,
    CONFIG_TEMP_MONITOR_NOTIFY_KEY, CONFIG_TEMP_MONITOR_NOTIFY_FRIENDLY,
    CONFIG_MQTT_PARAMS_QOS, (void*)&_notify);
  paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_FLOAT, nullptr, pgParams,
    CONFIG_TEMP_MONITOR_MIN_KEY, CONFIG_TEMP_MONITOR_MIN_FRIENDLY,
    CONFIG_MQTT_PARAMS_QOS, (void*)&_value_min);
  paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_FLOAT, nullptr, pgParams,
    CONFIG_TEMP_MONITOR_MAX_KEY, CONFIG_TEMP_MONITOR_MAX_FRIENDLY,
    CONFIG_MQTT_PARAMS_QOS, (void*)&_value_max);
  paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_FLOAT, nullptr, pgParams,
    CONFIG_TEMP_MONITOR_HYST_KEY, CONFIG_TEMP_MONITOR_HYST_FRIENDLY,
    CONFIG_MQTT_PARAMS_QOS, (void*)&_hysteresis);
}

// JSON
char* reTempMonitor::getJSON()
{
  char* ret = nullptr;

  char* timeLow = nullptr;
  if (_last_low > 0) {
    timeLow = malloc_timestr(CONFIG_FORMAT_DTM, _last_low);
  } else {
    timeLow = malloc_string(CONFIG_TEMP_MONITOR_TIME_EMPTY);
  };

  char* timeHigh = nullptr;
  if (_last_high > 0) {
    timeHigh = malloc_timestr(CONFIG_FORMAT_DTM, _last_high);
  } else {
    timeHigh = malloc_string(CONFIG_TEMP_MONITOR_TIME_EMPTY);
  };

  char* timeNorm = nullptr;
  if (_last_normal > 0) {
    timeNorm = malloc_timestr(CONFIG_FORMAT_DTM, _last_normal);
  } else {
    timeNorm = malloc_string(CONFIG_TEMP_MONITOR_TIME_EMPTY);
  };

  if (timeLow && timeHigh && timeNorm) {
    ret = malloc_stringf("{\"status\":%d,\"value\":%f,\"last_normal\":\"%s\",\"last_min\":\"%s\",\"last_max\":\"%s\"}", 
      _status, _last_value, timeNorm, timeLow, timeHigh);
  };

  if (timeLow) free(timeLow);
  if (timeHigh) free(timeHigh);
  if (timeNorm) free(timeNorm);

  return ret;
}

// MQTT
void reTempMonitor::mqttSetCallback(cb_monitor_publish_t cb_publish)
{
  _mqtt_publish = cb_publish; 
}

char* reTempMonitor::mqttTopicGet()
{
  return _mqtt_topic;
}

bool reTempMonitor::mqttTopicSet(char* topic)
{
  if (_mqtt_topic) free(_mqtt_topic);
  _mqtt_topic = topic;
  return (_mqtt_topic != nullptr);
}

bool reTempMonitor::mqttTopicCreate(bool primary, bool local, const char* topic1, const char* topic2, const char* topic3)
{
  return mqttTopicSet(mqttGetTopicDevice(primary, local, topic1, topic2, topic3));
}

void reTempMonitor::mqttTopicFree()
{
  if (_mqtt_topic) free(_mqtt_topic);
  _mqtt_topic = nullptr;
}

bool reTempMonitor::mqttPublish(bool forced)
{
  if ((_mqtt_topic) && (_mqtt_publish)) {
    return _mqtt_publish(this, _mqtt_topic, getJSON(), forced, false, true);
  };
  return false;
}

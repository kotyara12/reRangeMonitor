/* 
   EN: Module for monitoring and maintaining temperature (or humidity) within specified limits
   RU: Модуль для мониторинга и поддержания темепературы (или влажности) в заданных пределах
   --------------------------
   (с) 2021 Разживин Александр | Razzhivin Alexander
   kotyara12@yandex.ru | https://kotyara12.ru | tg: @kotyara1971
   --------------------------
   Страница проекта: https://github.com/kotyara12/reTempMonitor
*/

#ifndef __RE_TEMPMONITOR_H__
#define __RE_TEMPMONITOR_H__

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include "project_config.h"
#include "def_consts.h"
#include "reParams.h"

typedef enum {
   TMS_EMPTY    = -2,
   TMS_TOO_LOW  = -1,
   TMS_NORMAL   = 0,
   TMS_TOO_HIGH = 1
} temp_monitor_status_t;

#ifdef __cplusplus
extern "C" {
#endif

class reTempMonitor;
typedef bool (*cb_monitor_publish_t) (reTempMonitor *monitor, char* topic, char* payload, bool forced, bool free_topic, bool free_payload);
typedef void (*cb_monitor_outofrange_t) (reTempMonitor *monitor, temp_monitor_status_t status, bool notify, float value, float min, float max);

class reTempMonitor {
   public:
      reTempMonitor(float value_min, float value_max, float hysteresis, cb_monitor_outofrange_t cb_status, cb_monitor_publish_t cb_publish);
      ~reTempMonitor();
      
      // Monitoring value
      temp_monitor_status_t checkValue(float value);

      // Get current data
      temp_monitor_status_t getStatus();
      void  setStatusCallback(cb_monitor_outofrange_t cb_status);
      float getRangeMin();
      float getRangeMax();
      float getHysteresis();

      // Parameters
      void paramsRegister(paramsGroupHandle_t root_group, const char* group_key, const char* group_topic, const char* group_friendly);

      // JSON
      char* getJSON();

      // MQTT
      void mqttSetCallback(cb_monitor_publish_t cb_publish);
      char* mqttTopicGet();
      bool mqttTopicSet(char* topic);
      bool mqttTopicCreate(bool primary, bool local, const char* topic1, const char* topic2, const char* topic3);
      void mqttTopicFree();
      bool mqttPublish(bool forced);
   private:
      bool   _notify      = true;
      time_t _last_low    = 0;
      time_t _last_high   = 0;
      time_t _last_normal = 0;
      float  _last_value  = NAN;
      float  _value_min   = 0.0; 
      float  _value_max   = 0.0; 
      float  _hysteresis  = 0.1;
      temp_monitor_status_t _status = TMS_EMPTY;

      char*  _mqtt_topic  = nullptr;

      cb_monitor_outofrange_t _out_of_range = nullptr;
      cb_monitor_publish_t    _mqtt_publish = nullptr; 
};

#ifdef __cplusplus
}
#endif

#endif // __RE_TEMPMONITOR_H__
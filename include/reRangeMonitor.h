/* 
   EN: Module for monitoring and maintaining values within specified limits
   RU: Модуль для мониторинга и поддержания значений в заданных пределах
   --------------------------
   (с) 2021-2022 Разживин Александр | Razzhivin Alexander
   kotyara12@yandex.ru | https://kotyara12.ru | tg: @kotyara1971
   --------------------------
   Страница проекта: https://github.com/kotyara12/reRangeMonitor
*/

#ifndef __RE_RANGE_MONITOR__
#define __RE_RANGE_MONITOR__

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
} range_monitor_status_t;

#ifdef __cplusplus
extern "C" {
#endif

class reRangeMonitor;
typedef bool (*cb_monitor_publish_t) (reRangeMonitor *monitor, char* topic, char* payload, bool free_topic, bool free_payload);
typedef void (*cb_monitor_outofrange_t) (reRangeMonitor *monitor, range_monitor_status_t status, bool notify, float value, float min, float max);

class reRangeMonitor {
   public:
      reRangeMonitor(float value_min, float value_max, float hysteresis, const char* nvs_space, cb_monitor_outofrange_t cb_status, cb_monitor_publish_t cb_publish);
      ~reRangeMonitor();
      
      // Monitoring value
      range_monitor_status_t checkValue(float value);

      // Get current data
      range_monitor_status_t getStatus();
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
      bool mqttPublish();

      // NVS
      void nvsStore(const char* nvs_space);
      void nvsRestore(const char* nvs_space);
      void nvsRestore();
   private:
      bool   _notify         = true;
      time_t _last_low       = 0;
      time_t _last_high      = 0;
      time_t _last_normal    = 0;
      float  _last_value     = NAN;
      float  _value_min      = 0.0; 
      float  _value_max      = 0.0; 
      float  _hysteresis     = 0.1;
      range_monitor_status_t _status = TMS_EMPTY;

      char*  _mqtt_topic     = nullptr;
      const char* _nvs_space = nullptr;

      cb_monitor_outofrange_t _out_of_range = nullptr;
      cb_monitor_publish_t    _mqtt_publish = nullptr; 
};

#ifdef __cplusplus
}
#endif

#endif // __RE_RANGE_MONITOR__
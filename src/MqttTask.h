#include <WiFiClient.h>
#include <PubSubClient.h>
#include <netif/etharp.h>
#include "HomeAssistantHelper.h"

WiFiClient espClient;
PubSubClient client(espClient);
HomeAssistantHelper haHelper;


class MqttTask : public CustomTask {
public:
  MqttTask(bool enabled = false, unsigned long interval = 0) : CustomTask(enabled, interval) {}

protected:
  unsigned long lastReconnectAttempt = 0;
  unsigned short int reconnectAttempts = 0;

  void setup() {
    client.setServer(settings.mqtt.server, settings.mqtt.port);
    client.setCallback(__callback);
    haHelper.setPrefix(settings.mqtt.prefix);
    haHelper.setDeviceVersion(OT_GATEWAY_VERSION);

    sprintf(buffer, CONFIG_URL, WiFi.localIP().toString().c_str());
    haHelper.setDeviceConfigUrl(buffer);
  }

  void loop() {
    if (!client.connected() && millis() - lastReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
      INFO_F("Mqtt not connected, state: %i, connecting to server %s...\n", client.state(), settings.mqtt.server);

      if (client.connect(settings.hostname, settings.mqtt.user, settings.mqtt.password)) {
        INFO("Connected to MQTT server");

        client.subscribe(getTopicPath("settings/set").c_str());
        client.subscribe(getTopicPath("state/set").c_str());
        publishHaEntities();
        publishNonStaticHaEntities(true);

        reconnectAttempts = 0;
        lastReconnectAttempt = 0;

      } else {
        INFO("Failed to connect to MQTT server\n");

        if (!vars.states.emergency && ++reconnectAttempts >= EMERGENCY_TRESHOLD) {
          vars.states.emergency = true;
          INFO("Emergency mode enabled");
        }

        forceARP();
        lastReconnectAttempt = millis();
      }
    }


    if (client.connected()) {
      if (vars.states.emergency) {
        vars.states.emergency = false;

        INFO("Emergency mode disabled");
      }

      client.loop();
      bool published = publishNonStaticHaEntities();
      publish(published);
    }
  }


  static void forceARP() {
    struct netif* netif = netif_list;
    while (netif) {
      etharp_gratuitous(netif);
      netif = netif->next;
    }
  }

  static bool updateSettings(JsonDocument& doc) {
    bool flag = false;

    if (!doc["debug"].isNull() && doc["debug"].is<bool>()) {
      settings.debug = doc["debug"].as<bool>();
      flag = true;
    }

    if (!doc["outdoorTempSource"].isNull() && doc["outdoorTempSource"].is<int>() && doc["outdoorTempSource"] >= 0 && doc["outdoorTempSource"] <= 2) {
      settings.outdoorTempSource = doc["outdoorTempSource"];
      flag = true;
    }

    if (!doc["mqtt"]["interval"].isNull() && doc["mqtt"]["interval"].is<int>() && doc["mqtt"]["interval"] >= 1000 && doc["mqtt"]["interval"] <= 120000) {
      settings.mqtt.interval = doc["mqtt"]["interval"].as<unsigned int>();
      flag = true;
    }

    // emergency
    if (!doc["emergency"]["enable"].isNull() && doc["emergency"]["enable"].is<bool>()) {
      settings.emergency.enable = doc["emergency"]["enable"].as<bool>();
      flag = true;
    }

    if (!doc["emergency"]["target"].isNull() && (doc["emergency"]["target"].is<float>() || doc["emergency"]["target"].is<int>())) {
      settings.emergency.target = round(doc["emergency"]["target"].as<float>() * 10) / 10;
      flag = true;
    }

    if (!doc["emergency"]["useEquitherm"].isNull() && doc["emergency"]["useEquitherm"].is<bool>()) {
      settings.emergency.useEquitherm = doc["emergency"]["useEquitherm"].as<bool>();
      flag = true;
    }

    // heating
    if (!doc["heating"]["enable"].isNull() && doc["heating"]["enable"].is<bool>()) {
      settings.heating.enable = doc["heating"]["enable"].as<bool>();
      flag = true;
    }

    if (!doc["heating"]["target"].isNull() && (doc["heating"]["target"].is<float>() || doc["heating"]["target"].is<int>())) {
      settings.heating.target = round(doc["heating"]["target"].as<float>() * 10) / 10;
      flag = true;
    }

    if (!doc["heating"]["hysteresis"].isNull() && (doc["heating"]["hysteresis"].is<float>() || doc["heating"]["hysteresis"].is<int>())) {
      settings.heating.hysteresis = round(doc["heating"]["hysteresis"].as<float>() * 10) / 10;
      flag = true;
    }

    // dhw
    if (!doc["dhw"]["enable"].isNull() && doc["dhw"]["enable"].is<bool>()) {
      settings.dhw.enable = doc["dhw"]["enable"].as<bool>();
      flag = true;
    }

    if (!doc["dhw"]["target"].isNull() && doc["dhw"]["target"].is<int>()) {
      settings.dhw.target = doc["dhw"]["target"].as<int>();
      flag = true;
    }

    // pid
    if (!doc["pid"]["enable"].isNull() && doc["pid"]["enable"].is<bool>()) {
      settings.pid.enable = doc["pid"]["enable"].as<bool>();
      flag = true;
    }

    if (!doc["pid"]["p_factor"].isNull() && (doc["pid"]["p_factor"].is<float>() || doc["pid"]["p_factor"].is<int>())) {
      settings.pid.p_factor = round(doc["pid"]["p_factor"].as<float>() * 1000) / 1000;
      flag = true;
    }

    if (!doc["pid"]["i_factor"].isNull() && (doc["pid"]["i_factor"].is<float>() || doc["pid"]["i_factor"].is<int>())) {
      settings.pid.i_factor = round(doc["pid"]["i_factor"].as<float>() * 1000) / 1000;
      flag = true;
    }

    if (!doc["pid"]["d_factor"].isNull() && (doc["pid"]["d_factor"].is<float>() || doc["pid"]["d_factor"].is<int>())) {
      settings.pid.d_factor = round(doc["pid"]["d_factor"].as<float>() * 1000) / 1000;
      flag = true;
    }

    // equitherm
    if (!doc["equitherm"]["enable"].isNull() && doc["equitherm"]["enable"].is<bool>()) {
      settings.equitherm.enable = doc["equitherm"]["enable"].as<bool>();
      flag = true;
    }

    if (!doc["equitherm"]["n_factor"].isNull() && (doc["equitherm"]["n_factor"].is<float>() || doc["equitherm"]["n_factor"].is<int>())) {
      settings.equitherm.n_factor = round(doc["equitherm"]["n_factor"].as<float>() * 1000) / 1000;
      flag = true;
    }

    if (!doc["equitherm"]["k_factor"].isNull() && (doc["equitherm"]["k_factor"].is<float>() || doc["equitherm"]["k_factor"].is<int>())) {
      settings.equitherm.k_factor = round(doc["equitherm"]["k_factor"].as<float>() * 1000) / 1000;
      flag = true;
    }

    if (!doc["equitherm"]["t_factor"].isNull() && (doc["equitherm"]["t_factor"].is<float>() || doc["equitherm"]["t_factor"].is<int>())) {
      settings.equitherm.t_factor = round(doc["equitherm"]["t_factor"].as<float>() * 1000) / 1000;
      flag = true;
    }

    if (flag) {
      eeSettings.update();
      publish(true);

      return true;
    }

    return false;
  }

  static bool updateVariables(const JsonDocument& doc) {
    bool flag = false;

    if (!doc["ping"].isNull() && doc["ping"]) {
      flag = true;
    }

    if (!doc["tuning"]["enable"].isNull() && doc["tuning"]["enable"].is<bool>()) {
      vars.tuning.enable = doc["tuning"]["enable"].as<bool>();
      flag = true;
    }

    if (!doc["tuning"]["regulator"].isNull() && doc["tuning"]["regulator"].is<int>() && doc["tuning"]["regulator"] >= 0 && doc["tuning"]["regulator"] <= 1) {
      vars.tuning.regulator = doc["tuning"]["regulator"];
      flag = true;
    }

    if (!doc["temperatures"]["indoor"].isNull() && (doc["temperatures"]["indoor"].is<float>() || doc["temperatures"]["indoor"].is<int>())) {
      vars.temperatures.indoor = round(doc["temperatures"]["indoor"].as<float>() * 100) / 100;
      flag = true;
    }

    if (!doc["temperatures"]["outdoor"].isNull() && (doc["temperatures"]["outdoor"].is<float>() || doc["temperatures"]["outdoor"].is<int>()) && settings.outdoorTempSource == 1) {
      vars.temperatures.outdoor = round(doc["temperatures"]["outdoor"].as<float>() * 100) / 100;
      flag = true;
    }

    if (!doc["restart"].isNull() && doc["restart"].is<bool>() && doc["restart"]) {
      eeSettings.updateNow();
      ESP.restart();
    }

    if (flag) {
      publish(true);

      return true;
    }

    return false;
  }

  static void publish(bool force = false) {
    static unsigned int prevPubVars = 0;
    static unsigned int prevPubSettings = 0;

    // publish variables and status
    if (force || millis() - prevPubVars > settings.mqtt.interval) {
      publishVariables(getTopicPath("state").c_str());

      if (vars.states.fault) {
        client.publish(getTopicPath("status").c_str(), "fault");
      } else {
        client.publish(getTopicPath("status").c_str(), vars.states.otStatus ? "online" : "offline");
      }

      forceARP();
      prevPubVars = millis();
    }

    // publish settings
    if (force || millis() - prevPubSettings > settings.mqtt.interval * 10) {
      publishSettings(getTopicPath("settings").c_str());
      prevPubSettings = millis();
    }
  }

  static void publishHaEntities() {
    // main
    haHelper.publishSelectOutdoorTempSource();
    haHelper.publishSwitchDebug(false);

    // emergency
    haHelper.publishSwitchEmergency();
    haHelper.publishNumberEmergencyTarget();
    haHelper.publishSwitchEmergencyUseEquitherm();

    // heating
    haHelper.publishSwitchHeating(false);
    //haHelper.publishNumberHeatingTarget(false);
    haHelper.publishNumberHeatingHysteresis();
    haHelper.publishSensorHeatingSetpoint(false);

    // dhw
    haHelper.publishSwitchDHW(false);
    //haHelper.publishNumberDHWTarget(false);

    // pid
    haHelper.publishSwitchPID();
    haHelper.publishNumberPIDFactorP();
    haHelper.publishNumberPIDFactorI();
    haHelper.publishNumberPIDFactorD();

    // equitherm
    haHelper.publishSwitchEquitherm();
    haHelper.publishNumberEquithermFactorN();
    haHelper.publishNumberEquithermFactorK();
    haHelper.publishNumberEquithermFactorT();

    // tuning
    haHelper.publishSwitchTuning();
    haHelper.publishSelectTuningRegulator();

    // states
    haHelper.publishBinSensorStatus();
    haHelper.publishBinSensorOtStatus();
    haHelper.publishBinSensorHeating();
    haHelper.publishBinSensorDHW();
    haHelper.publishBinSensorFlame();
    haHelper.publishBinSensorFault();
    haHelper.publishBinSensorDiagnostic();
    haHelper.publishSensorFaultCode();

    // sensors
    haHelper.publishSensorModulation(false);
    haHelper.publishSensorPressure(false);

    // temperatures
    haHelper.publishNumberIndoorTemp();
    //haHelper.publishNumberOutdoorTemp();
    haHelper.publishSensorHeatingTemp();
    haHelper.publishSensorDHWTemp();
  }

  static bool publishNonStaticHaEntities(bool force = false) {
    static byte _heatingMinTemp;
    static byte _heatingMaxTemp;
    static byte _dhwMinTemp;
    static byte _dhwMaxTemp;
    static bool _editableOutdoorTemp;

    bool published = false;
    bool isStupidMode = !settings.pid.enable && !settings.equitherm.enable;
    byte heatingMinTemp = isStupidMode ? vars.parameters.heatingMinTemp : 10;
    byte heatingMaxTemp = isStupidMode ? vars.parameters.heatingMaxTemp : 30;
    bool editableOutdoorTemp = settings.outdoorTempSource == 1;


    if (force || _heatingMinTemp != heatingMinTemp || _heatingMaxTemp != heatingMaxTemp) {
      if (settings.heating.target < heatingMinTemp || settings.heating.target > heatingMaxTemp) {
        settings.heating.target = constrain(settings.heating.target, heatingMinTemp, heatingMaxTemp);
      }

      _heatingMinTemp = heatingMinTemp;
      _heatingMaxTemp = heatingMaxTemp;

      haHelper.publishNumberHeatingTarget(heatingMinTemp, heatingMaxTemp, false);
      haHelper.publishClimateHeating(heatingMinTemp, heatingMaxTemp);

      published = true;
    }

    if (force || _dhwMinTemp != vars.parameters.dhwMinTemp || _dhwMaxTemp != vars.parameters.dhwMaxTemp) {
      _dhwMinTemp = vars.parameters.dhwMinTemp;
      _dhwMaxTemp = vars.parameters.dhwMaxTemp;

      haHelper.publishNumberDHWTarget(vars.parameters.dhwMinTemp, vars.parameters.dhwMaxTemp, false);
      haHelper.publishClimateDHW(vars.parameters.dhwMinTemp, vars.parameters.dhwMaxTemp);

      published = true;
    }

    if (force || _editableOutdoorTemp != editableOutdoorTemp) {
      _editableOutdoorTemp = editableOutdoorTemp;

      if (editableOutdoorTemp) {
        haHelper.deleteSensorOutdoorTemp();
        haHelper.publishNumberOutdoorTemp();
      } else {
        haHelper.deleteNumberOutdoorTemp();
        haHelper.publishSensorOutdoorTemp();
      }

      published = true;
    }

    return published;
  }

  static bool publishSettings(const char* topic) {
    StaticJsonDocument<2048> doc;

    doc["debug"] = settings.debug;
    doc["outdoorTempSource"] = settings.outdoorTempSource;

    doc["emergency"]["enable"] = settings.emergency.enable;
    doc["emergency"]["target"] = settings.emergency.target;
    doc["emergency"]["useEquitherm"] = settings.emergency.useEquitherm;

    doc["heating"]["enable"] = settings.heating.enable;
    doc["heating"]["target"] = settings.heating.target;
    doc["heating"]["hysteresis"] = settings.heating.hysteresis;

    doc["dhw"]["enable"] = settings.dhw.enable;
    doc["dhw"]["target"] = settings.dhw.target;

    doc["pid"]["enable"] = settings.pid.enable;
    doc["pid"]["p_factor"] = settings.pid.p_factor;
    doc["pid"]["i_factor"] = settings.pid.i_factor;
    doc["pid"]["d_factor"] = settings.pid.d_factor;

    doc["equitherm"]["enable"] = settings.equitherm.enable;
    doc["equitherm"]["n_factor"] = settings.equitherm.n_factor;
    doc["equitherm"]["k_factor"] = settings.equitherm.k_factor;
    doc["equitherm"]["t_factor"] = settings.equitherm.t_factor;

    client.beginPublish(topic, measureJson(doc), false);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  static bool publishVariables(const char* topic) {
    StaticJsonDocument<2048> doc;

    doc["tuning"]["enable"] = vars.tuning.enable;
    doc["tuning"]["regulator"] = vars.tuning.regulator;

    doc["states"]["otStatus"] = vars.states.otStatus;
    doc["states"]["heating"] = vars.states.heating;
    doc["states"]["dhw"] = vars.states.dhw;
    doc["states"]["flame"] = vars.states.flame;
    doc["states"]["fault"] = vars.states.fault;
    doc["states"]["diagnostic"] = vars.states.diagnostic;
    doc["states"]["faultCode"] = vars.states.faultCode;

    doc["sensors"]["modulation"] = vars.sensors.modulation;
    doc["sensors"]["pressure"] = vars.sensors.pressure;

    doc["temperatures"]["indoor"] = vars.temperatures.indoor;
    doc["temperatures"]["outdoor"] = vars.temperatures.outdoor;
    doc["temperatures"]["heating"] = vars.temperatures.heating;
    doc["temperatures"]["dhw"] = vars.temperatures.dhw;

    doc["parameters"]["heatingMinTemp"] = vars.parameters.heatingMinTemp;
    doc["parameters"]["heatingMaxTemp"] = vars.parameters.heatingMaxTemp;
    doc["parameters"]["heatingSetpoint"] = vars.parameters.heatingSetpoint;
    doc["parameters"]["dhwMinTemp"] = vars.parameters.dhwMinTemp;
    doc["parameters"]["dhwMaxTemp"] = vars.parameters.dhwMaxTemp;

    client.beginPublish(topic, measureJson(doc), false);
    //BufferingPrint bufferedClient(client, 32);
    //serializeJson(doc, bufferedClient);
    //bufferedClient.flush();
    serializeJson(doc, client);
    return client.endPublish();
  }

  static std::string getTopicPath(const char* topic) {
    return std::string(settings.mqtt.prefix) + "/" + std::string(topic);
  }

  static void __callback(char* topic, byte* payload, unsigned int length) {
    if (!length) {
      return;
    }

    if (settings.debug) {
      DEBUG_F("MQTT received message\n\r        Topic: %s\n\r        Data: ", topic);
      for (int i = 0; i < length; i++) {
        DEBUG_STREAM.print((char)payload[i]);
      }
      DEBUG_STREAM.print("\n");
    }

    StaticJsonDocument<2048> doc;
    DeserializationError dErr = deserializeJson(doc, (const byte*)payload, length);
    if (dErr != DeserializationError::Ok || doc.isNull()) {
      return;
    }

    if (getTopicPath("state/set").compare(topic) == 0) {
      updateVariables(doc);
      client.publish(getTopicPath("state/set").c_str(), NULL, true);

    } else if (getTopicPath("settings/set").compare(topic) == 0) {
      updateSettings(doc);
      client.publish(getTopicPath("settings/set").c_str(), NULL, true);
    }
  }
};
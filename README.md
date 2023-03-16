# ESP32-basedc Aranet proxy

Connect an ESP32 to a number of Aranet4 CO2 monitors via Bluetooth Low Energy and continously push the readings via MQTT.

Also see my [CO2 Monitor](https://github.com/oseiler2/CO2Monitor) which shares some of the same principles and code base.

# Why use a CO2 Monitor?

A large risk of Covid-19 transmission stems from airborne virus particles that can linger in poorly ventilated spaces for hours. This has been recognised by the WHO. To lower the risk of infection, particularly in busier indoor settings, good ventilation and regular air exchange are key.

This is where measuring CO2 levels in rooms can provide a direct and good indication of sufficient ventilation which correlates with reduced viral load and low risk of virus transmission. Good air quality is also important for creating a good learning or work environment.

Poorly ventilated rooms often feel stale and ‘stuffy’, but by the time we can feel that the air quality is already pretty poor; early indications are easily missed. A sensitive and consistent CO2 monitor can accurately measure CO2 levels and display an easy-to-understand and actionable traffic light indication with orange as an indication of worsening air quality and red as a reminder to open a window.

The data collected by the sensors should be logged and made available for further consumption. It can be visualised on a central school/organisation-wide dashboard and used to establish a baseline and understand patterns or identify rooms that are more difficult to ventilate, as well as providing potential alerting or to remotely checking a room before another group uses it.

# Hardware

Tested on ESP 32 Devkit (30 pin) and ESP32-S3, but should work just fine on most ESP32 based MCUs that support BLE and Wifi.
Supports an optional small 0.96 inch SSD1306 OLED I2C Display.

# Firmware

[![PlatformIO CI](https://github.com/oseiler2/ESP32-Aranet-Proxy/actions/workflows/pre-release.yml/badge.svg)](https://github.com/oseiler2/ESP32-Aranet-Proxy/actions/workflows/pre-release.yml)
[![Release](https://github.com/oseiler2/ESP32-Aranet-Proxy/actions/workflows/tagged-release.yml/badge.svg)](https://github.com/oseiler2/ESP32-Aranet-Proxy/actions/workflows/tagged-release.yml)

## Wifi

Supports [ESPAsync WiFiManager](https://github.com/khoih-prog/ESPAsync_WiFiManager) to set up wireless credentials and further configuration.

Pressing the `Boot` button for less than 2 seconds launches an AP using the SSID `Aranet-proxy-<ESP32mac>`. Connecting to this AP allows the Wifi credentials for the monitor to be set.

A password for the AP can be configured in the file `extra.ini` which needs to be created by copying [extra.template.ini](extra.template.ini) and applying the desired changes.

<img src="img/configuration.png">

## Configuration

The ESP32 is configured via the `config.json` file on the file system. There are 3 ways the configuration can be changed:

- via the web interface
- by directly editing [config.json](data/config.json) and uploading it via `Upload Filesystem Image`
- via MQTT (once connected)

## Backend using Mosquitto - Node-Red - InfluxDB - Grafana

[Docker compose file](./docker/docker.md) to set up the database and dashboards.

## MQTT

Sensor readings can be published via MQTT for centralised storage and visualition. Each node is configured with its own id and will then publish under `aranetproxy/<id>/up/sensors/<aranet-id>`. The top level topic `aranetproxy` is configurable. Downlink messages to nodes can be sent to each individual node using the id in the topic `aranetproxy/<id>/down/<command>`, or to all nodes when omitting the id part `aranetproxy/down/<command>`

Sending `aranetproxy/<id>/down/getConfig` will triger the node to reply with its current settings under `aranetproxy/<id>/up/config`

```
{
  "appVersion": "1.0.0",
  "mac": "xxxxxxxx",
  "ip": "xxx.yyy.zzz.www",
  "deviceId": 1,
  "mqttTopic": "aranetproxy",
  "mqttUsername": "aranetproxy",
  "mqttHost": "127.0.0.1",
  "mqttServerPort": 1883,
  "mqttUseTls": false,
  "mqttInsecure": false,
  "ssd1306Rows": 64,
  "scanInterval": 300
}
```

A message to `aranetproxy/<id>/down/setConfig` will set the node's configuration to the provided parameters. Note that changes to the hardware configuration will trigger a reboot.

```
{
  "deviceId": 1,
  "mqttTopic": "aranetproxy",
  "mqttUsername": "aranetproxy",
  "mqttPassword": "aranetproxy",
  "mqttHost": "127.0.0.1",
  "mqttServerPort": 1883,
  "mqttUseTls": false,
  "mqttInsecure": false,
  "ssd1306Rows": 64,
  "scanInterval": 300
}
```

A message to `aranetproxy/<id>/down/installMqttRootCa` will attempt to install the pem-based ca cert in the payload as root cert for tls enabled MQTT connections. A connection attempt will be made using the configured MQTT settings and the new cert, and if successful the cert will be persisted, otherwise discarded.

A message to `aranetproxy/<id>/down/installRootCa` will install the pem-based ca cert in the payload as root cert for OTA update requests.

A message to `aranetproxy/<id>/down/ota` will trigger the OTA polling mechnism if configured.

A message to `aranetproxy/<id>/down/forceota` will force an OTA update using the URL provided in the payload.

A message to `aranetproxy/<id>/down/reboot` will trigger a reset on the node.

A message to `aranetproxy/<id>/down/resetWifi` will wipe configured WiFi settings (SSID/password) and force a reboot.

### MQTT TLS support

To connect to an MQTT server using TLS (recommended) you need to enable TLS in the configuration by setting `mqttUseTls` to `true`. You also need to supply a root CA certificate in PEM format on the file system as `/mqtt_root_ca.pem` and/or a client certificate and key for using mTLS as `mqtt_client_cert.pem` and `mqtt_client_key.pem`. These files can be uploaded using the `Upload Filesystem Image` project task in PlatformIO. Alternatively you can set `mqttInsecure` to `true` to disable certificate validation altogether.

## Supported displays

- [128x32 OLED feather wing](https://www.adafruit.com/product/2900)
- [SSD1306 based displays](https://www.aliexpress.com/wholesale?SearchText=128X64+SSD1306)

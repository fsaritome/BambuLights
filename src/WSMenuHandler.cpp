#include <WSMenuHandler.h>

String WSMenuHandler::mqttMenu = "{\"2\": { \"url\" : \"mqtt.html\", \"title\" : \"Printer\" }}";
String WSMenuHandler::mqttHAMenu = "{\"3\": { \"url\" : \"mqtt_ha.html\", \"title\" : \"Homeassistant\" }}";
String WSMenuHandler::infoMenu = "{\"4\": { \"url\" : \"info.html\", \"title\" : \"Info\" }}";
// The key is the index into wsHandlers[] in main.cpp, not display order.
String WSMenuHandler::towerMenu = "{\"5\": { \"url\" : \"tower.html\", \"title\" : \"Tower\" }}";

void WSMenuHandler::handle(AsyncWebSocketClient *client, char *data) {
	String json("{\"type\":\"sv.init.menu\", \"value\":[");
	char *sep = "";
	for (int i=0; items[i] != 0; i++) {
		json.concat(sep);json.concat(*items[i]);sep=",";
	}
	json.concat("]}");
	client->text(json);
}

void WSMenuHandler::setItems(String **items) {
	this->items = items;
}


#include <homekit/homekit.h>
#include <homekit/characteristics.h>

void accessoryIdentify(homekit_value_t _value) {
	printf("accessory identify\n");
}

homekit_characteristic_t currentPosition = HOMEKIT_CHARACTERISTIC_(CURRENT_POSITION, 0, .format = homekit_format_int);
homekit_characteristic_t targetPosition = HOMEKIT_CHARACTERISTIC_(TARGET_POSITION, 0, .format = homekit_format_int);
homekit_characteristic_t positionState = HOMEKIT_CHARACTERISTIC_(POSITION_STATE, 0, .format = homekit_format_int);

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_window_covering, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Blind"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "KumIT"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0123456"),
            HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266/ESP32"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, accessoryIdentify),
            NULL
        }),
    		HOMEKIT_SERVICE(WINDOW_COVERING, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Blind"),
    		&currentPosition,
            &targetPosition,
            &positionState,
            NULL
    	}),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

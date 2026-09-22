#include "abox_mqtt_tls.h"
#include "ec800_async_fixture.h"
int main(void)
{
    MockCommand mock;
    ABoxEc800CommandPort p = mock_port(&mock);
    ABoxEc800Tls tls;
    ABoxMqttTls mqtt;
    ABoxTlsLease lease, none = {0, 0};
    CHECK(ABoxEc800Tls_Init(&tls, &p, 4));
    CHECK(ABoxMqttTls_Init(&mqtt, &tls, &p, 0));
    CHECK(!ABoxMqttTls_Start(&mqtt, 1, none, 0, 100));
    CHECK(ABoxMqttTls_Start(&mqtt, 0, none, 0, 100));
    ABoxMqttTls_Poll(&mqtt, 0); ABoxMqttTls_Poll(&mqtt, 1);
    CHECK(mqtt.status == ABOX_ASYNC_OK);
    CHECK(mock.count == 1 && !strcmp(mock.commands[0], "AT+QMTCFG=\"SSL\",0,0"));
    lease = prepared(&tls, 2); /* No OTA service involved. */
    CHECK(ABoxMqttTls_Start(&mqtt, 1, lease, 0, 100));
    CHECK(!ABoxEc800Tls_Release(&tls, lease));
    ABoxMqttTls_Poll(&mqtt, 0); ABoxMqttTls_Poll(&mqtt, 1);
    CHECK(mqtt.status == ABOX_ASYNC_OK);
    CHECK(!strcmp(mock.commands[6], "AT+QMTCFG=\"SSL\",0,1,2"));
    CHECK(!ABoxMqttTls_Start(&mqtt, 0, none, 0, 100));
    CHECK(ABoxMqttTls_Unbind(&mqtt, 0, 100));
    ABoxMqttTls_Poll(&mqtt, 0);
    CHECK(!ABoxEc800Tls_Release(&tls, lease));
    ABoxMqttTls_Poll(&mqtt, 1);
    CHECK(ABoxEc800Tls_Release(&tls, lease));
    lease = prepared(&tls, 2);
    CHECK(ABoxMqttTls_Start(&mqtt, 1, lease, 0, 10));
    mock.result = ABOX_ASYNC_PENDING;
    ABoxMqttTls_Poll(&mqtt, 0); ABoxMqttTls_Poll(&mqtt, 10);
    CHECK(mqtt.status == ABOX_ASYNC_TIMEOUT && mqtt.pinned);
    CHECK(!ABoxEc800Tls_Release(&tls, lease));
    CHECK(ABoxMqttTls_Unbind(&mqtt, 10, 100));
    mock.result = ABOX_ASYNC_ERROR;
    ABoxMqttTls_Poll(&mqtt, 10); ABoxMqttTls_Poll(&mqtt, 11);
    CHECK(mqtt.status == ABOX_ASYNC_ERROR && mqtt.pinned);
    CHECK(ABoxMqttTls_Unbind(&mqtt, 20, 100));
    mock.result = ABOX_ASYNC_OK;
    ABoxMqttTls_Poll(&mqtt, 20); ABoxMqttTls_Poll(&mqtt, 21);
    CHECK(!mqtt.pinned);
    CHECK(ABoxMqttTls_Start(&mqtt, 1, lease, 0, 10));
    ABoxMqttTls_Poll(&mqtt, 0); mock.drain = 0;
    ABoxMqttTls_Cancel(&mqtt);
    CHECK(mqtt.status == ABOX_ASYNC_QUARANTINED);
    CHECK(!ABoxMqttTls_Unbind(&mqtt, 0, 10));
    mock.current = 0; ABoxEc800Tls_OnModemReset(&tls);
    ABoxMqttTls_OnModemReset(&mqtt);
    CHECK(!ABoxMqttTls_Start(&mqtt, 1, lease, 0, 10));
    puts("MQTT TLS: explicit plaintext, independent prepare/bind, retained pins, unbind failure and reset passed");
    return 0;
}

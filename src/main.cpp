#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>

// --- Configuración de Red y Broker ---
const char* ssid = "TU_SSID";
const char* password = "TU_PASSWORD";
const char* mqtt_server = "IP_PUBLICA_DE_TU_EC2"; // La IP de tu Broker

// --- Variables de Control y Multitarea ---
TaskHandle_t Miner_Task;
QueueHandle_t miningQueue;
String minerId;

WiFiClient espClient;
PubSubClient client(espClient);

// Estructura para pasar datos entre núcleos
struct MiningJob {
    char blockData[64];
    int difficulty;
    uint32_t startNonce;
};

/* =================== LÓGICA DE MINERÍA (CORE 1) ===================== */

String runHash(String payload) {
    byte shaResult[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*) payload.c_str(), payload.length());
    mbedtls_md_finish(&ctx, shaResult);
    mbedtls_md_free(&ctx);

    String hashHex = "";
    for (int i = 0; i < 32; i++) {
        char buf[3];
        sprintf(buf, "%02x", shaResult[i]);
        hashHex += buf;
    }
    return hashHex;
}

void minerCore1(void * parameter) {
    MiningJob currentJob;
    uint32_t nonce = 0;
    bool miningActive = false;
    String targetPrefix = "";

    while (true) {
        // Revisar si hay un nuevo trabajo en la cola (sin bloquear si ya estamos minando)
        if (xQueueReceive(miningQueue, &currentJob, miningActive ? 0 : portMAX_DELAY)) {
            nonce = currentJob.startNonce;
            targetPrefix = "";
            for(int i=0; i<currentJob.difficulty; i++) targetPrefix += "0";
            miningActive = true;
            Serial.println(">>> Nuevo Bloque Recibido. Empezando minería...");
        }

        if (miningActive) {
            // EL SECRETO: Combinamos Datos + ID_UNICO + Nonce
            String payload = String(currentJob.blockData) + minerId + String(nonce);
            String result = runHash(payload);

            if (result.startsWith(targetPrefix)) {
                Serial.printf("!!! BLOQUE HALLADO !!! Nonce: %d\n", nonce);
                
                // Formatear solución para enviar al Core 0
                StaticJsonDocument<200> doc;
                doc["miner"] = minerId;
                doc["nonce"] = nonce;
                doc["hash"] = result;
                char buffer[200];
                serializeJson(doc, buffer);
                
                // En la simulación, enviamos el éxito por un topic específico
                client.publish("mining/solution", buffer);
                miningActive = false; // Parar hasta recibir nuevo bloque
            }

            nonce++;
            if (nonce % 500 == 0) yield(); // Alimentar al Watchdog
        }
    }
}

/* =================== COMUNICACIÓN (CORE 0) ===================== */

void callback(char* topic, byte* payload, unsigned int length) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload, length);

    MiningJob newJob;
    strlcpy(newJob.blockData, doc["data"] | "default", sizeof(newJob.blockData));
    newJob.difficulty = doc["diff"] | 4;
    newJob.startNonce = 0;

    // Enviamos el trabajo al Core 1 a través de la Queue
    xQueueSend(miningQueue, &newJob, portMAX_DELAY);
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Conectando a MQTT...");
        if (client.connect(minerId.c_str())) {
            Serial.println("Conectado");
            client.subscribe("mining/work");
        } else {
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    
    // Generar ID único basado en la MAC del ESP32
    uint64_t mac = ESP.getEfuseMac();
    minerId = "ESP_" + String((uint32_t)(mac >> 32), HEX) + String((uint32_t)mac, HEX);
    Serial.println("Miner ID: " + minerId);

    // Configurar WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    // Crear la cola de comunicación entre núcleos
    miningQueue = xQueueCreate(1, sizeof(MiningJob));

    // Lanzar la tarea de minería en el Core 1
    xTaskCreatePinnedToCore(minerCore1, "MinerTask", 10000, NULL, 1, &Miner_Task, 1);
}

void loop() {
    if (!client.connected()) reconnect();
    client.loop();
    delay(10); // Dejar aire para el sistema de red
}

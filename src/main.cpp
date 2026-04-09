#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>

// --- Configuración de Red y Broker ---
const char* ssid = "UA-Alumnos";
const char* password = "41umn05WLC";
const char* mqtt_server = "172.22.44.5";

// --- Variables y Colas ---
WiFiClient espClient;
PubSubClient client(espClient);
String minerId;

struct MiningJob {
    char blockData[64];
    int difficulty;
};

struct MiningSolution {
    uint32_t nonce;
    char hash[65];
};


//dos queues de comunicación
QueueHandle_t miningQueue;
QueueHandle_t solutionQueue;

/* =================== NÚCLEO 1: MINERÍA ===================== */
String calculateHash(String payload) {
    byte shaResult[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*) payload.c_str(), payload.length());
    mbedtls_md_finish(&ctx, shaResult);
    mbedtls_md_free(&ctx);

    String hashHex = "";
    hashHex.reserve(64);
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
    bool isMining = false;
    String target = "";

    while (true) {
        // Revisar si llegó un nuevo bloque
        if (xQueueReceive(miningQueue, &currentJob, isMining ? 0 : portMAX_DELAY)) {
            nonce = 0; // Reiniciamos el nonce para el nuevo bloque
            target = "";
            for(int i=0; i<currentJob.difficulty; i++) target += "0";
            isMining = true;
        }

        if (isMining) {
            String payload = String(currentJob.blockData) + minerId + String(nonce);
            String hash = calculateHash(payload);

            if (hash.startsWith(target)) {
                MiningSolution sol;
                sol.nonce = nonce;
                strlcpy(sol.hash, hash.c_str(), sizeof(sol.hash));
                
                // Enviar éxito al Core 0 y detener minería hasta el próximo bloque
                xQueueSend(solutionQueue, &sol, portMAX_DELAY);
                isMining = false; 
            }
            nonce++;
            if (nonce % 500 == 0) yield(); // Evitar reinicio por Watchdog
        }
    }
}

/* =================== NÚCLEO 0: COMUNICACIÓN ===================== */
void callback(char* topic, byte* payload, unsigned int length) {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, payload, length);

    MiningJob newJob;
    strlcpy(newJob.blockData, doc["data"] | "bloque_default", sizeof(newJob.blockData));
    newJob.difficulty = doc["diff"] | 4;

    // Pasar el nuevo trabajo al Core 1
    xQueueSend(miningQueue, &newJob, portMAX_DELAY);
}

void reconnect() {
    while (!client.connected()) {
        if (client.connect(minerId.c_str())) {
            client.subscribe("mining/work");
        } else {
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    
    // Generar ID único
    uint64_t mac = ESP.getEfuseMac();
    minerId = "ESP_" + String((uint32_t)mac, HEX);
    Serial.println("Iniciando Minero: " + minerId);

    // Colas de comunicación entre núcleos
    miningQueue = xQueueCreate(1, sizeof(MiningJob));
    solutionQueue = xQueueCreate(1, sizeof(MiningSolution));

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    xTaskCreatePinnedToCore(minerCore1, "MinerTask", 10000, NULL, 1, NULL, 1);
}

void loop() {
    if (!client.connected()) reconnect();
    client.loop();

    MiningSolution sol;
    // Revisar si el Core 1 encontró una solución
    if (xQueueReceive(solutionQueue, &sol, 0)) {
        StaticJsonDocument<256> doc;
        doc["miner"] = minerId;
        doc["nonce"] = sol.nonce;
        doc["hash"] = sol.hash;
        
        char buffer[256];
        serializeJson(doc, buffer);
        
        client.publish("mining/solution", buffer);
        Serial.println("¡Solución enviada a AWS!");
    }
    
    delay(10);
}

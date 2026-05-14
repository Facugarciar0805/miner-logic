#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>

// --- Configuración de Red y Broker ---
const char* ssid = "UA-Alumnos";
const char* password = "41umn05WLC";
const char* mqtt_server = "172.22.42.124"; 
const char* topicWork = "mining/work";
const char* topicConsensus = "mining/consensus";
const char* topicResolved = "mining/resolved";
const char* topicLog = "mining/log";

// Solo dejamos el máximo de miembros para los arrays de aprobación
const int maxMembers = 8;

// --- Variables y Colas ---
WiFiClient espClient;
PubSubClient client(espClient);
String minerId;
volatile bool miningActive = false;

String approvedMiners[maxMembers];

bool consensusOpen = false;
bool approvedByMe = false;
int approvalCount = 0;
int requiredApprovals = 0;
String proposalMiner;
String proposalHash;
uint32_t proposalWorkId = 0;
unsigned long proposalElapsedMs = 0;
uint32_t proposalNonce = 0;

struct MiningJob {
    uint32_t workId;
    char blockData[64];
    int difficulty;
    int requiredApprovals; // Ahora la cantidad requerida viaja con el trabajo
};

struct MiningSolution {
    MiningJob job;
    uint32_t workId;
    uint32_t nonce;
    char hash[65];
    unsigned long elapsedMs;
};

QueueHandle_t solutionQueue;
SemaphoreHandle_t workMutex;

struct WorkNode {
    MiningJob job;
    WorkNode* next;
};

WorkNode* workHead = nullptr;
WorkNode* workTail = nullptr;

bool enqueueWork(const MiningJob& job) {
    WorkNode* node = (WorkNode*) malloc(sizeof(WorkNode));
    if (!node) return false;
    node->job = job;
    node->next = nullptr;

    if (xSemaphoreTake(workMutex, portMAX_DELAY) != pdTRUE) {
        free(node);
        return false;
    }

    if (workTail) {
        workTail->next = node;
    } else {
        workHead = node;
    }
    workTail = node;
    xSemaphoreGive(workMutex);
    return true;
}

bool dequeueWork(MiningJob& job) {
    if (xSemaphoreTake(workMutex, 0) != pdTRUE) return false;
    if (!workHead) {
        xSemaphoreGive(workMutex);
        return false;
    }

    WorkNode* node = workHead;
    workHead = node->next;
    if (!workHead) workTail = nullptr;
    job = node->job;
    xSemaphoreGive(workMutex);
    free(node);
    return true;
}

void resetApprovalTrackers() {
    for (int i = 0; i < maxMembers; i++) {
        approvedMiners[i] = "";
    }
    approvalCount = 0;
    approvedByMe = false;
}

bool registerApproval(const String& id) {
    if (!id.length()) return false;
    for (int i = 0; i < maxMembers; i++) {
        if (approvedMiners[i] == id) return false;
    }
    for (int i = 0; i < maxMembers; i++) {
        if (!approvedMiners[i].length()) {
            approvedMiners[i] = id;
            return true;
        }
    }
    return false;
}

void publishConsensus(const JsonDocument& doc) {
    char buffer[256];
    size_t size = serializeJson(doc, buffer, sizeof(buffer));
    if (size > 0) client.publish(topicConsensus, buffer, size);
}

void publishLog(const char* msg, int workId = -1) {
    StaticJsonDocument<256> doc;
    doc["type"] = "log";
    doc["miner"] = minerId;
    doc["timestamp"] = millis();
    doc["msg"] = msg;
    if (workId >= 0) doc["workId"] = workId;
    char buffer[256];
    size_t size = serializeJson(doc, buffer, sizeof(buffer));
    if (size > 0) client.publish(topicLog, buffer, size);
}

void publishSolutionLog(uint32_t workId, uint32_t nonce, const char* hash, unsigned long elapsedMs) {
    StaticJsonDocument<256> doc;
    doc["type"] = "log";
    doc["miner"] = minerId;
    doc["timestamp"] = millis();
    doc["msg"] = "halle solucion, publicando al consenso";
    doc["workId"] = workId;
    doc["nonce"] = nonce;
    doc["hash"] = hash;
    doc["elapsedMs"] = elapsedMs;
    char buffer[256];
    size_t size = serializeJson(doc, buffer, sizeof(buffer));
    if (size > 0) client.publish(topicLog, buffer, size);
}

void publishApproval(const MiningJob& job) {
    StaticJsonDocument<256> doc;
    doc["type"] = "approval";
    doc["miner"] = minerId;
    doc["proposalMiner"] = proposalMiner;
    doc["workId"] = job.workId;
    doc["nonce"] = proposalNonce;
    doc["hash"] = proposalHash;
    doc["block"] = job.blockData;
    doc["diff"] = job.difficulty;
    doc["required"] = job.requiredApprovals;
    publishConsensus(doc);
}

void publishResolved(const MiningSolution& sol);

void publishProposal(const MiningSolution& sol) {
    proposalMiner = minerId;
    proposalWorkId = sol.workId;
    proposalElapsedMs = sol.elapsedMs;
    proposalNonce = sol.nonce;
    proposalHash = sol.hash;
    consensusOpen = true;
    resetApprovalTrackers();
    // Tomamos la cantidad requerida que vino en el trabajo original
    requiredApprovals = sol.job.requiredApprovals;
    miningActive = false;

    StaticJsonDocument<256> doc;
    doc["type"] = "proposal";
    doc["miner"] = minerId;
    doc["workId"] = sol.workId;
    doc["nonce"] = sol.nonce;
    doc["hash"] = sol.hash;
    doc["block"] = sol.job.blockData;
    doc["diff"] = sol.job.difficulty;
    doc["required"] = requiredApprovals;
    publishConsensus(doc);

    if (registerApproval(minerId)) {
        approvedByMe = true;
        approvalCount = 1;
        publishLog("aprobe la solucion", (int)sol.workId);
        publishApproval(sol.job);
        if (approvalCount >= requiredApprovals) {
            publishLog("publique la solucion aprobada", (int)sol.workId);
            publishResolved(sol);
            consensusOpen = false;
            miningActive = false;
        }
    }
}

void publishResolved(const MiningSolution& sol) {
    StaticJsonDocument<256> doc;
    doc["miner"] = minerId;
    doc["workId"] = sol.workId;
    doc["nonce"] = sol.nonce;
    doc["hash"] = sol.hash;
    doc["block"] = sol.job.blockData;
    doc["diff"] = sol.job.difficulty;
    doc["elapsedMs"] = sol.elapsedMs;
    char buffer[256];
    size_t size = serializeJson(doc, buffer, sizeof(buffer));
    if (size > 0) client.publish(topicResolved, buffer, size);
}

//=================== NÚCLEO 1: MINERÍA ===================== 
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
    unsigned long jobStartMs = 0;

    while (true) {
        if (!isMining && !consensusOpen && dequeueWork(currentJob)) {
            nonce = 0;
            target = "";
            for (int i = 0; i < currentJob.difficulty; i++) target += "0";
            isMining = true;
            miningActive = true;
            jobStartMs = millis();
        }

        if (isMining && miningActive) {
            String payload = String(currentJob.blockData) + minerId + String(nonce);
            String hash = runHash(payload);

            if (hash.startsWith(target)) {
                MiningSolution sol;
                sol.job = currentJob;
                sol.workId = currentJob.workId;
                sol.nonce = nonce;
                strlcpy(sol.hash, hash.c_str(), sizeof(sol.hash));
                sol.elapsedMs = millis() - jobStartMs;

                publishSolutionLog(sol.workId, sol.nonce, sol.hash, sol.elapsedMs);

                xQueueSend(solutionQueue, &sol, portMAX_DELAY);
                miningActive = false;
                isMining = false; 
            }
            nonce++;
            if (nonce % 500 == 0) yield(); 
        } else if (isMining && !miningActive) {
            isMining = false;
        } else {
            vTaskDelay(1);
        }
    }
}

// =================== NÚCLEO 0: COMUNICACIÓN ===================== 
void callback(char* topic, byte* payload, unsigned int length) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) return;

    if (strcmp(topic, topicWork) == 0) {
        MiningJob newJob;
        newJob.workId = doc["id"] | 0;
        strlcpy(newJob.blockData, doc["data"] | "bloque_default", sizeof(newJob.blockData));
        newJob.difficulty = doc["diff"] | 4;
        
        newJob.requiredApprovals = doc["required"] | 1; 

        consensusOpen = false;
        approvedByMe = false;
        approvalCount = 0;
        
        publishLog("recibi un trabajo", (int)newJob.workId);

        enqueueWork(newJob);
        return;
    }

    if (strcmp(topic, topicConsensus) != 0) return;

    const char* type = doc["type"] | "";
    const char* sender = doc["miner"] | "";

    if (strcmp(type, "proposal") == 0) {
        if (String(sender) == minerId) return;
        if (consensusOpen) return;

        proposalMiner = sender;
        proposalWorkId = doc["workId"] | 0;
        proposalNonce = doc["nonce"] | 0;
        proposalHash = doc["hash"] | "";
        
        requiredApprovals = doc["required"] | 1; 
        
        resetApprovalTrackers();
        consensusOpen = true;
        miningActive = false;

        MiningJob job;
        job.workId = doc["workId"] | 0;
        strlcpy(job.blockData, doc["block"] | "bloque_default", sizeof(job.blockData));
        job.difficulty = doc["diff"] | 4;
        job.requiredApprovals = requiredApprovals;

        String check = runHash(String(job.blockData) + proposalMiner + String(proposalNonce));
        
        if (check == proposalHash) {
            if (registerApproval(minerId)) {
                approvedByMe = true;
                approvalCount = 1;
                publishLog("aprobe la solucion", (int)job.workId);
                publishApproval(job);
            }
        } else {
            publishLog("no aprobe la solucion", (int)job.workId);
        }
        return;
    }

    if (strcmp(type, "approval") == 0) {
        if (!consensusOpen) return;
        if (String(doc["proposalMiner"] | "") != proposalMiner) return;
        if ((uint32_t)(doc["workId"] | 0) != proposalWorkId) return;
        if ((uint32_t)(doc["nonce"] | 0) != proposalNonce) return;
        if (String(doc["hash"] | "") != proposalHash) return;
        if (String(sender) == minerId) return;
        if (!registerApproval(sender)) return;

        approvalCount++;
        Serial.printf("Aprobaciones: %d/%d\n", approvalCount, requiredApprovals);

        if (proposalMiner == minerId && approvalCount >= requiredApprovals) {
            MiningSolution sol;
            strlcpy(sol.job.blockData, doc["block"] | "bloque_default", sizeof(sol.job.blockData));
            sol.job.difficulty = doc["diff"] | 4;
            sol.job.workId = proposalWorkId;
            sol.workId = proposalWorkId;
            sol.nonce = proposalNonce;
            strlcpy(sol.hash, proposalHash.c_str(), sizeof(sol.hash));
            sol.elapsedMs = proposalElapsedMs;
            publishLog("publique la solucion aprobada", (int)sol.workId);
            publishResolved(sol);
            consensusOpen = false;
            miningActive = false;
        }
        return;
    }

    if (strcmp(type, "resolved") == 0) {
        consensusOpen = false;
        miningActive = false;
        Serial.printf("Solucion resuelta por %s\n", sender);
        return;
    }
}

void reconnect() {
    while (!client.connected()) {
        Serial.println("Conectando a MQTT...");
        if (client.connect(minerId.c_str())) {
            Serial.println("MQTT conectado");
            client.subscribe(topicWork, 0);
            client.subscribe(topicConsensus, 0);
            
            // ¡NUEVO! Publicamos en el log apenas nos conectamos
            publishLog("minero conectado a la red");
        } else {
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    
    String macStr = WiFi.macAddress();
    macStr.replace(":", ""); // Le sacamos los dos puntos para que quede limpio
    minerId = "ESP_" + macStr;
    Serial.println("Iniciando Minero: " + minerId);

    solutionQueue = xQueueCreate(1, sizeof(MiningSolution));
    workMutex = xSemaphoreCreateMutex();

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    client.setBufferSize(1024);

    xTaskCreatePinnedToCore(minerCore1, "MinerTask", 10000, NULL, 1, NULL, 1);
}

void loop() {
    if (!client.connected()) reconnect();
    client.loop();

    MiningSolution sol;
    if (xQueueReceive(solutionQueue, &sol, 0)) {
        if (!consensusOpen) {
            publishProposal(sol);
        }
    }
    delay(10);
}


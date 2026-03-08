#include <Arduino.h>
#include <mbedtls/md.h>

// Variables Globales
String datos = "Bloque_Simulado_#106";
uint32_t nonce = 1;
uint32_t sessionStartTime = 0; // Para el tiempo total de la sesión

// Prototipos
String minarBloque(String datos, uint32_t nonce);

void setup() {
  Serial.begin(115200);
  // Capturamos el momento exacto en que el ESP32 arranca la simulación
  sessionStartTime = millis(); 
  Serial.println(">>> Sistema de Minería Iniciado <<<");
}

void loop() {
  Serial.println("--- Iniciando Proceso de Hashing ---");

  // 1. Tiempo de este hash individual
  uint32_t hashStartTime = micros();

  // 2. Ejecutamos la minería
  // Se utiliza mbedtls_md_update para procesar los datos
  String result = minarBloque(datos, nonce);

  uint32_t hashEndTime = micros();

  /* --- INICIO DEL IF DE SEGURIDAD --- */
  if (result.startsWith("000")) {
    uint32_t sessionEndTime = millis();
    
    // Calculamos tiempos
    float individualTime = (hashEndTime - hashStartTime) / 1000000.0;
    float totalTime = (sessionEndTime - sessionStartTime) / 1000.0;
    
    Serial.println("\n\n########################################");
    Serial.println("##        ¡BLOQUE MINADO!             ##");
    Serial.println("########################################");
    Serial.print("  Nonce Ganador: "); Serial.println(nonce);
    Serial.print("  Hash Resultante: "); Serial.println(result);
    Serial.println("----------------------------------------");
    Serial.print("  TIEMPO TOTAL:  "); Serial.print(totalTime, 2); Serial.println(" s");
    Serial.print("  ULTIMO HASH:   "); Serial.print(individualTime, 6); Serial.println(" s");
    Serial.print("  INTENTOS:      "); Serial.println(nonce);
    Serial.println("########################################");
    Serial.println("Simulación finalizada. Reinicie para otra prueba.");

    while(true) {
      delay(1000); 
    }
  }
  /* --- FIN DEL IF DE SEGURIDAD --- */

  // 4. Tiempo individual para el log común
  float timeTaken = (hashEndTime - hashStartTime) / 1000000.0;

  /* Formato Pretty */
  Serial.println("========================================");
  Serial.print("  NONCE:    "); Serial.println(nonce);
  Serial.print("  HASH:     "); Serial.println(result);
  Serial.print("  TIEMPO:   "); Serial.print(timeTaken, 6); Serial.println(" s");
  Serial.println("========================================\n");

  nonce += 1;
  delay(10); 
}

/**
 * Función que encapsula el ritual de mbedtls
 */
String minarBloque(String datos, uint32_t nonce){
    byte shaResult[32];
    mbedtls_md_context_t ctx;

    // Inicializa la estructura a cero
    mbedtls_md_init(&ctx);
    
    // Configura SHA256 y reserva memoria dinámica
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    
    // Prepara el contexto para un nuevo cálculo
    mbedtls_md_starts(&ctx);
    
    // Alimenta el hardware con los datos del bloque y el nonce
    String payload = datos + String(nonce);
    mbedtls_md_update(&ctx, (const unsigned char*) payload.c_str(), payload.length());
    
    // Finaliza el hash y escribe los 32 bytes en el buffer
    mbedtls_md_finish(&ctx, shaResult);
    
    // Libera la memoria interna para evitar memory leaks
    mbedtls_md_free(&ctx);

    // Conversión a Hexadecimal
    String hashHex = "";
    for (int i = 0; i < 32; i++) {
        char buf[3];
        sprintf(buf, "%02x", shaResult[i]);
        hashHex += buf;
    }
    return hashHex;
}
const aedesFactory = require('aedes');
const aedes = aedesFactory();
// Allow anonymous connections for local testing
aedes.authenticate = (client, username, password, cb) => {
  cb(null, true);
};
const net = require('net');
const server = net.createServer(aedes.handle);
const mqttService = require('./services/mqttService');
const blockchainService = require('./services/blockChainService');
let workIdCounter = 1;
let activeWork = null;

// 1. Levantamos el Broker MQTT en el puerto 1883
server.listen(1883, function () {
  console.log('🚀 Broker MQTT interno levantado en el puerto 1883');

  
  // 2. Conectamos cliente local para lógica del servidor
  mqttService.connect();
  mqttService.subscribe('mining/log');
  mqttService.subscribe('mining/resolved');

  mqttService.onMessage((topic, message) => {
    if (topic === 'mining/log') {
      try {
        const log = JSON.parse(message);
        const extra = log.hash ? ` hash=${log.hash}` : '';
        const nonce = log.nonce !== undefined ? ` nonce=${log.nonce}` : '';
        const workId = log.workId !== undefined ? ` work=${log.workId}` : '';
        const elapsed = log.elapsedMs !== undefined ? ` t=${log.elapsedMs}ms` : '';
        console.log(`📝 [${log.miner}]${workId} ${log.msg}${nonce}${extra}${elapsed}`);
      } catch (e) {
        console.log(`📝 Log: ${message}`);
      }
    }

    if (topic === 'mining/work') {
      try {
        const work = JSON.parse(message);
        console.log(`📡 Trabajo recibido [id=${work.id}]:`, message);
      } catch (e) {
        console.log('📡 Trabajo recibido:', message);
      }
    }

    if (topic === 'mining/resolved') {
      try {
        const resolved = JSON.parse(message);
        console.log(`✅ [${resolved.miner}] trabajo ${resolved.workId} resuelto en ${resolved.elapsedMs}ms nonce=${resolved.nonce} hash=${resolved.hash}`);
        blockchainService.addBlock(resolved);
      } catch (e) {
        console.log('✅ Resolución recibida:', message);
        blockchainService.addBlock(message);
      }
      activeWork = null;
    }
  });

  // Optional: emit test job if no external feed
  setInterval(() => {
    if (activeWork) return;

    const miningJob = JSON.stringify({
      id: workIdCounter++,
      data: 'bloque_test_' + Date.now(),
      diff: 4
    });
    activeWork = miningJob;
    mqttService.publish('mining/work', miningJob);
    console.log('📡 Emitiendo bloque de prueba...', miningJob);
  }, 15000);
});

const mqtt = require('mqtt');

let client;
let messageHandler = null;

function connect(brokerUrl = 'mqtt://localhost:1883') {
  client = mqtt.connect(brokerUrl);

  client.on('connect', () => {
    console.log('MQTT service conectado a', brokerUrl);
  });

  client.on('error', (err) => {
    console.log('MQTT service error:', err && err.message ? err.message : err);
  });

  client.on('message', (topic, message) => {
    if (messageHandler) {
      messageHandler(topic, message.toString());
    }
  });
}

function publish(topic, message) {
  if (!client) return;
  client.publish(topic, message);
}

function subscribe(topic) {
  if (!client) return;
  client.subscribe(topic, (err) => {
    if (err) console.log('Subscribe error', topic, err.message || err);
  });
}

function onMessage(handler) {
  messageHandler = handler;
}

module.exports = {
  connect,
  publish,
  subscribe,
  onMessage,
};

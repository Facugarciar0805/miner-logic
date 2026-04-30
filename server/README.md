Server for miner-logic

How to run locally (for testing):

1. Install dependencies

```bash
cd server
npm install
```

2. Start broker + server

```bash
npm start
```

This starts an Aedes MQTT broker on port 1883 and a local MQTT client that subscribes to `mining/log`, `mining/resolved`, and `mining/solution`.

Notes:
- In production you should host the broker on a server accessible by the ESP devices and secure it (authentication/TLS).
- The server writes blocks to `server/blockchain.json` when a `mining/resolved` message arrives.

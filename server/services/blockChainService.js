const fs = require('fs');
const path = require('path');
const dbFile = path.join(__dirname, '..', 'blockchain.json');

let currentIndex = 1;

async function addBlock(rawBlock) {
  let parsed = rawBlock;
  try {
    if (typeof rawBlock === 'string') {
      parsed = JSON.parse(rawBlock);
    }
  } catch (e) {
    parsed = { raw: rawBlock };
  }

  console.log('Agregando bloque a la blockchain:', parsed);
  const block = {
    index: currentIndex++,
    hash: `hash_${Date.now()}`,
    previousHash: `prev_${Date.now() - 1}`,
    miner: parsed.miner,
    workId: parsed.workId,
    nonce: parsed.nonce,
    elapsedMs: parsed.elapsedMs,
    hashSolved: parsed.hash,
    blockData: parsed.block,
    difficulty: parsed.diff,
    raw: parsed
  };

  let db = [];
  try {
    if (fs.existsSync(dbFile)) db = JSON.parse(fs.readFileSync(dbFile));
  } catch (e) {
    console.log('Error leyendo blockchain db', e.message || e);
  }

  db.push(block);
  fs.writeFileSync(dbFile, JSON.stringify(db, null, 2));
}

module.exports = {
  addBlock,
};

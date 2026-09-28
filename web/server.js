/* server.js — LVGL Web Bridge 服务端
 *  把支持 --net 模式的原生 LVGL 模拟器桥接到浏览器:
 *    exe stdout 帧 --> WebSocket --> 页面 canvas
 *    页面指针事件 --> WebSocket --> exe stdin
 *  启动: SIM_EXE=/path/to/main.exe node server.js
 */
const { spawn } = require('child_process');
const http = require('http');
const fs = require('fs');
const path = require('path');
const { WebSocketServer } = require('ws');

const PORT = process.env.PORT || 8090;
const EXE = process.env.SIM_EXE || path.join(__dirname, '..', 'sim', 'main.exe');

if (!fs.existsSync(EXE)) {
  console.error(`[server] 找不到模拟器: ${EXE}\n` +
                `        用环境变量 SIM_EXE 指向你编译出的 main.exe (需 --net 模式支持, 见 README)`);
  process.exit(1);
}

const child = spawn(EXE, ['--net'], { stdio: ['pipe', 'pipe', 'pipe'] });
child.stderr.on('data', d => process.stdout.write('[sim] ' + d));
child.on('exit', code => {
  console.log(`[server] 模拟器退出 (code=${code})，1 秒后退出服务`);
  setTimeout(() => process.exit(1), 1000);
});

const server = http.createServer((req, res) => {
  if (req.url === '/') {
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
    res.end(fs.readFileSync(path.join(__dirname, 'index.html')));
    return;
  }
  if (req.url === '/healthz') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ ok: true, frames: frameCount, clients: wss.clients.size }));
    return;
  }
  res.writeHead(404); res.end();
});

const wss = new WebSocketServer({ server });

let frameCount = 0;
let lastFrame = null;      // 新客户端连上时立即补发一帧

wss.on('connection', ws => {
  console.log('[ws] 客户端连接');
  if (lastFrame) ws.send(lastFrame);
  ws.on('message', data => {
    /* 页面发来的 5 字节指针事件, 原样写进 exe stdin */
    if (Buffer.isBuffer(data) && data.length === 5 && child.stdin.writable) {
      child.stdin.write(data);
    }
  });
  ws.on('close', () => console.log('[ws] 客户端断开'));
});

/* ---- 解析 exe stdout 的帧流并广播 ---- */
let buf = Buffer.alloc(0);
const MAGIC = 0x4c565331; // "LVS1"
child.stdout.on('data', chunk => {
  buf = Buffer.concat([buf, chunk]);
  for (;;) {
    if (buf.length < 12) break;
    if (buf.readUInt32LE(0) !== MAGIC) { buf = buf.subarray(1); continue; } // 失步则逐字节重同步
    const w = buf.readUInt32LE(4), h = buf.readUInt32LE(8);
    const len = 12 + w * h * 4;
    if (buf.length < len) break;
    lastFrame = Buffer.from(buf.subarray(0, len)); // 拷贝, buf 会被复用
    buf = buf.subarray(len);
    frameCount++;
    wss.clients.forEach(c => { if (c.readyState === 1) c.send(lastFrame); });
  }
});

server.listen(PORT, () => {
  console.log(`[server] LVGL Web Bridge: http://127.0.0.1:${PORT}`);
  console.log(`[server] 模拟器: ${EXE}`);
});
